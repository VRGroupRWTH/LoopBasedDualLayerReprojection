#include "worker.hpp"
#include "session.hpp"
#include "encoder.hpp"
#include "export.hpp"
#include "mesh_generator/mesh_generator.hpp"

#include <geometry_codec.hpp>
#include <filesystem>
#include <chrono>

bool WorkerPool::create(Server* server, uint32_t view_count, bool export_enabled)
{
    this->server = server;
    this->state = WORKER_STATE_ACTIVE;
    this->view_count = view_count;
    this->export_enabled = export_enabled;

    for (uint32_t view = 0; view < view_count; view++)
    {
        std::thread mesh_thread = std::thread([this, view]()
        {
            this->worker_mesh(view);
        });

        this->mesh_threads.push_back(std::move(mesh_thread));
    }

    this->submit_thread = std::thread([this]()
    {
        this->worker_submit();
    });
    
    return true;
}

void WorkerPool::destroy(std::vector<Frame*>& frames)
{
    this->state = WORKER_STATE_INACTIVE;
    this->input_condition.notify_all();
    this->mesh_condition.notify_all();

    for (std::thread& mesh_thread : this->mesh_threads)
    {
        mesh_thread.join();
    }

    this->submit_thread.join();
    this->mesh_threads.clear();

    for (WorkerFrame* worker_frame : this->input_queue)
    {
        frames.push_back(worker_frame->frame);

        delete worker_frame;
    }

    for (WorkerFrame* worker_frame : this->output_queue)
    {
        frames.push_back(worker_frame->frame);

        delete worker_frame;
    }

    this->input_queue.clear();
    this->output_queue.clear();
}

void WorkerPool::submit(Frame* frame)
{
    std::unique_lock<std::mutex> lock(this->input_mutex);

    WorkerFrame* worker_frame = new WorkerFrame;
    worker_frame->frame = frame;
    worker_frame->layer_data = this->server->allocate_layer_data();
    worker_frame->complete.fill(false);

    this->input_queue.push_back(worker_frame);
    this->input_condition.notify_all();
}

void WorkerPool::reclaim(std::vector<Frame*>& frames)
{
    std::unique_lock<std::mutex> lock(this->output_mutex);

    for (WorkerFrame* worker_frame : this->output_queue)
    {
        frames.push_back(worker_frame->frame);

        delete worker_frame;
    }

    this->output_queue.clear();
}

void WorkerPool::worker_mesh(uint32_t view)
{
    std::vector<MeshFeatureLine> feature_lines;

    while (true)
    {
        std::unique_lock<std::mutex> input_lock(this->input_mutex);
        WorkerFrame* worker_frame = nullptr;

        while (true)
        {
            for (WorkerFrame* input_frame : this->input_queue)
            {
                if (!input_frame->complete[view])
                {
                    worker_frame = input_frame;

                    break;
                }
            }

            if (worker_frame != nullptr)
            {
                break;
            }

            if (this->state == WORKER_STATE_INACTIVE)
            {
                return;
            }

            this->input_condition.wait(input_lock);
        }
        input_lock.unlock();

        const Frame* frame = worker_frame->frame;
        const ExportRequest& export_request = frame->export_request;
        MeshGeneratorFrame* mesh_generator_frame = frame->mesh_generator_frame[view];
        LayerData* layer_data = worker_frame->layer_data;

        mesh_generator_frame->triangulate(layer_data->vertices[view], layer_data->indices[view], layer_data->view_metadata[view], feature_lines, this->export_enabled);

        layer_data->view_metadata[view].time_layer = frame->time_layer[view];
        layer_data->view_metadata[view].time_image_encode = frame->encoder_frame->time_encode;
        memcpy(layer_data->view_matrices[view].data(), glm::value_ptr(frame->view_matrix[view]), sizeof(glm::mat4));
        
        if (this->export_enabled)
        {
            if (export_request.color_file_name.has_value())
            {
                std::string file_name = this->get_export_file_name(export_request.color_file_name.value(), view);

                glm::uvec2 image_resolution = frame->resolution;
                uint32_t image_size = image_resolution.x * image_resolution.y * sizeof(glm::u8vec3);

                export_color_image(file_name, image_resolution, frame->color_export_pointers[view], image_size);
            }

            if (export_request.depth_file_name.has_value())
            {
                std::string file_name = this->get_export_file_name(export_request.depth_file_name.value(), view);

                glm::uvec2 image_resolution = frame->resolution;
                uint32_t image_size = image_resolution.x * image_resolution.y * sizeof(float);

                export_depth_image(file_name, image_resolution, frame->depth_export_pointers[view], image_size);
            }

            if (export_request.mesh_file_name.has_value())
            {
                std::string file_name = this->get_export_file_name(export_request.mesh_file_name.value(), view);

                export_mesh(file_name, layer_data->vertices[view], layer_data->indices[view], frame->view_matrix[view], frame->projection_matrix, frame->resolution);
            }

            if (export_request.feature_lines_file_name.has_value())
            {
                std::string file_name = this->get_export_file_name(export_request.feature_lines_file_name.value(), view);

                export_feature_lines(file_name, feature_lines);
            }

            feature_lines.clear();
        }

        input_lock.lock();
        worker_frame->complete[view] = true;
        this->mesh_condition.notify_all();
        input_lock.unlock();
    }
}

void WorkerPool::worker_submit()
{
    std::vector<shared::Vertex> vertices;
    std::vector<shared::Index> indices;

    while (true)
    {
        std::unique_lock<std::mutex> input_lock(this->input_mutex);
        while (true)
        {
            if (!this->input_queue.empty())
            {
                WorkerFrame* worker_frame = this->input_queue.front();
                bool complete = true;

                for (uint32_t view = 0; view < this->view_count; view++)
                {
                    complete = complete && worker_frame->complete[view];
                }

                if (complete)
                {
                    break;
                }
            }

            if (this->state == WORKER_STATE_INACTIVE)
            {
                return;
            }

            this->mesh_condition.wait(input_lock);
        }

        WorkerFrame* worker_frame = this->input_queue.front();
        this->input_queue.erase(this->input_queue.begin());
        input_lock.unlock();

        const Frame* frame = worker_frame->frame;
        const EncoderFrame* encoder_frame = frame->encoder_frame;
        LayerData* layer_data = worker_frame->layer_data;

        vertices.clear();
        indices.clear();
        layer_data->geometry.clear();

        for (uint32_t view = 0; view < this->view_count; view++)
        {
            vertices.insert(vertices.end(), layer_data->vertices[view].begin(), layer_data->vertices[view].end());
            indices.insert(indices.end(), layer_data->indices[view].begin(), layer_data->indices[view].end());
        }

        std::chrono::high_resolution_clock::time_point geometry_encode_start = std::chrono::high_resolution_clock::now();
        shared::GeometryCodec::encode(indices, vertices, layer_data->geometry);
        std::chrono::high_resolution_clock::time_point geometry_encode_end = std::chrono::high_resolution_clock::now();
        double time_geometry_encode = std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(geometry_encode_end - geometry_encode_start).count();

        uint32_t image_buffer_size = encoder_frame->output_buffer_size + encoder_frame->output_parameter_buffer.size();
        uint32_t image_buffer_offset = 0;

        layer_data->image.resize(image_buffer_size);

        if (encoder_frame->config_changed)
        {
            memcpy(layer_data->image.data(), encoder_frame->output_parameter_buffer.data(), encoder_frame->output_parameter_buffer.size());
            image_buffer_offset += encoder_frame->output_parameter_buffer.size();
        }

        memcpy(layer_data->image.data() + image_buffer_offset, encoder_frame->output_buffer, encoder_frame->output_buffer_size);

        layer_data->request_id = frame->request_id;
        layer_data->layer_index = frame->layer_index;

        for (uint32_t view = 0; view < this->view_count; view++)
        {
            layer_data->view_metadata[view].time_geometry_encode = time_geometry_encode;
        }

        this->server->submit_layer_data(layer_data);
        worker_frame->layer_data = nullptr;

        std::unique_lock<std::mutex> output_lock(this->output_mutex);
        this->output_queue.push_back(worker_frame);
        output_lock.unlock();
    }
}

std::string WorkerPool::get_export_file_name(const std::string& request_file_name, uint32_t view)
{
    std::filesystem::path file_name = std::filesystem::path(this->server->get_study_directory()) / request_file_name;
    file_name.replace_filename(file_name.filename().string() + "_view_" + std::to_string(view));

    return file_name.string();
}
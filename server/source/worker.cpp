#include "worker.hpp"
#include "session.hpp"
#include "encoder.hpp"
#include "mesh_generator/mesh_generator.hpp"

bool WorkerPool::create(i3ds::StreamingServer* server)
{
    this->server = server;
    this->state = WORKER_STATE_ACTIVE;

    for (uint32_t view = 0; view < CAMERA_VIEW_COUNT; view++)
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
    worker_frame->layer_data = std::move(this->server->allocate_layer_data());
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
        MeshGeneratorFrame* mesh_generator_frame = frame->mesh_generator_frame[view];
        i3ds::LayerData& layer_data = worker_frame->layer_data;

        mesh_generator_frame->triangulate(worker_frame->vertices[view], worker_frame->indices[view], layer_data.view_statistic[view]);

        layer_data.vertex_counts[view] = worker_frame->vertices[view].size();
        layer_data.index_counts[view] = worker_frame->indices[view].size();
        layer_data.view_statistic[view].time_layer = frame->time_layer[view];

        memcpy(layer_data.view_matrices[view].data(), glm::value_ptr(frame->view_matrix[view]), sizeof(glm::mat4));
        
        input_lock.lock();
        worker_frame->complete[view] = true;
        this->mesh_condition.notify_all();
        input_lock.unlock();
    }
}

void WorkerPool::worker_submit()
{
    while (true)
    {
        std::unique_lock<std::mutex> input_lock(this->input_mutex);
        while (true)
        {
            if (!this->input_queue.empty())
            {
                WorkerFrame* worker_frame = this->input_queue.front();
                bool complete = true;

                for (uint32_t view = 0; view < CAMERA_VIEW_COUNT; view++)
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

        i3ds::LayerData& layer_data = worker_frame->layer_data;

        for (uint32_t view = 0; view < CAMERA_VIEW_COUNT; view++)
        {
            layer_data.vertices.insert(layer_data.vertices.end(), worker_frame->vertices[view].begin(), worker_frame->vertices[view].end());
            layer_data.indices.insert(layer_data.indices.end(), worker_frame->indices[view].begin(), worker_frame->indices[view].end());
        }

        const Frame* frame = worker_frame->frame;
        const EncoderFrame* encoder_frame = frame->encoder_frame;

        uint32_t encoder_buffer_size = encoder_frame->output_buffer_size + encoder_frame->output_parameter_buffer.size();
        uint32_t encoder_buffer_offset = 0;

        layer_data.image.resize(encoder_buffer_size);

        if (encoder_frame->config_changed)
        {
            memcpy(layer_data.image.data(), encoder_frame->output_parameter_buffer.data(), encoder_frame->output_parameter_buffer.size());
            encoder_buffer_offset += encoder_frame->output_parameter_buffer.size();
        }

        memcpy(layer_data.image.data() + encoder_buffer_offset, encoder_frame->output_buffer, encoder_frame->output_buffer_size);

        this->server->submit_mesh_layer(frame->request_id, frame->layer_index, std::move(layer_data));

        std::unique_lock<std::mutex> output_lock(this->output_mutex);
        this->output_queue.push_back(worker_frame);
        output_lock.unlock();
    }
}
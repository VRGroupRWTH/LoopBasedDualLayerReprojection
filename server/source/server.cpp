#include "server.hpp"
#include "scene.hpp"
#include "export.hpp"

#include <boost/json.hpp>
#include <spdlog/spdlog.h>
#include <App.h>
#include <filesystem>
#include <fstream>

Server::Server(std::string scene_directory, std::string study_directory) : scene_directory(scene_directory), study_directory(study_directory)
{

}

Server::~Server()
{
    this->destroy();
}

bool Server::create(uint32_t port)
{
    std::promise<uWS::Loop*> loop_promise;
    std::future<uWS::Loop*> loop_future = loop_promise.get_future();

    this->thread = std::thread(&Server::worker, this, port, std::move(loop_promise));
    this->loop = loop_future.get();

    return true;
}

void Server::destroy()
{
    if (this->loop != nullptr)
    {
        this->loop->defer([this]()
        {
            if (this->web_socket != nullptr)
            {
                this->web_socket->close();
                this->web_socket = nullptr;
            }

            if (this->listen_socket != nullptr)
            {
                us_listen_socket_close(0, this->listen_socket);
                this->listen_socket = nullptr;
            }
        });

        this->loop = nullptr;
    }

    if (this->thread.joinable())
    {
        this->thread.join();
    }

    for (LayerData* layer_data : this->layer_data_list)
    {
        delete layer_data;
    }

    this->layer_data_list.clear();
    this->layer_data_pool.clear();
}

LayerData* Server::allocate_layer_data()
{
    std::unique_lock<std::mutex> lock(this->layer_data_mutex);
    LayerData* layer_data = nullptr;

    if (this->layer_data_pool.empty())
    {
        layer_data = new LayerData;

        this->layer_data_list.push_back(layer_data);
    }

    else
    {
        layer_data = this->layer_data_pool.back();
        this->layer_data_pool.pop_back();
    }

    return layer_data;
}

void Server::submit_layer_data(LayerData* layer_data)
{
    if (this->loop == nullptr)
    {
        spdlog::error("Server: Can't send layer data since server is not running!");

        return;
    }

    this->loop->defer([this, layer_data]
    {
        uint32_t buffer_size = sizeof(shared::LayerResponsePacket);
        buffer_size += layer_data->geometry.size();
        buffer_size += layer_data->image.size();

        this->send_buffer.resize(buffer_size);

        shared::LayerResponsePacket* packet = (shared::LayerResponsePacket*)this->send_buffer.data();
        packet->type = shared::PACKET_TYPE_LAYER_RESPONSE;
        packet->request_id = layer_data->request_id;
        packet->layer_index = layer_data->layer_index;
        packet->geometry_bytes = layer_data->geometry.size();
        packet->image_bytes = layer_data->image.size();

        packet->view_metadata = layer_data->view_metadata;
        packet->view_matrices = layer_data->view_matrices;

        for (uint32_t index = 0; index < SHARED_VIEW_COUNT_MAX; index++)
        {
            packet->vertex_counts[index] = layer_data->vertices[index].size();
            packet->index_counts[index] = layer_data->indices[index].size();
        }

        uint8_t* geometry_pointer = this->send_buffer.data() + sizeof(shared::LayerResponsePacket);
        uint8_t* image_pointer = geometry_pointer + layer_data->geometry.size();

        memcpy(geometry_pointer, layer_data->geometry.data(), layer_data->geometry.size());
        memcpy(image_pointer, layer_data->image.data(), layer_data->image.size());

        std::string_view packet_view = std::string_view((const char*)this->send_buffer.data(), this->send_buffer.size());

        this->web_socket->send(packet_view);

        for (uint32_t index = 0; index < SHARED_VIEW_COUNT_MAX; index++)
        {
            layer_data->vertices[index].clear();
            layer_data->indices[index].clear();
        }

        layer_data->geometry.clear();
        layer_data->image.clear();

        std::unique_lock<std::mutex> lock(this->layer_data_mutex);
        this->layer_data_pool.push_back(layer_data);
    });
}

void Server::set_on_session_create(OnSessionCreate callback)
{
    std::unique_lock<std::mutex> lock(this->callback_mutex);

    this->on_session_create = callback;
}

void Server::set_on_session_destroy(OnSessionDestroy callback)
{
    std::unique_lock<std::mutex> lock(this->callback_mutex);

    this->on_session_destroy = callback;
}

void Server::set_on_render_request(OnRenderRequest callback)
{
    std::unique_lock<std::mutex> lock(this->callback_mutex);

    this->on_render_request = callback;
}

void Server::set_on_mesh_settings_change(OnMeshSettingsChange callback)
{
    std::unique_lock<std::mutex> lock(this->callback_mutex);

    this->on_mesh_settings_change = callback;
}

void Server::set_on_video_settings_change(OnVideoSettingsChange callback)
{
    std::unique_lock<std::mutex> lock(this->callback_mutex);

    this->on_video_settings_change = callback;
}

const std::string& Server::get_scene_directory() const
{
    return this->scene_directory;
}

const std::string& Server::get_study_directory() const
{
    return this->study_directory;
}

void Server::worker(uint32_t port, std::promise<uWS::Loop*> loop_promise)
{
    loop_promise.set_value(uWS::Loop::get());

    uWS::App::WebSocketBehavior<uint8_t> behaviour;
    behaviour.maxBackpressure = 100 * 1024 * 1024;
    behaviour.upgrade = std::bind_front(&Server::process_upgrade, this);
    behaviour.open = std::bind_front(&Server::process_open, this);
    behaviour.message = std::bind_front(&Server::process_message, this);
    behaviour.close = std::bind_front(&Server::process_close, this);

    uWS::App()
        .get("/scenes", std::bind_front(&Server::process_get_scenes, this))
        .get("/files/*", std::bind_front(&Server::process_get_files, this))
        .post("/files/*", std::bind_front(&Server::process_post_files, this))
        .ws("/*", std::move(behaviour))
        .listen(port, std::bind_front(&Server::process_listen, this, port))
        .run();
}

void Server::process_listen(uint32_t port, ListenSocket* socket)
{
    spdlog::info("Server: Listening for connections on port {}", port);

    this->listen_socket = socket;
}

void Server::process_upgrade(HttpResponse* response, HttpRequest* request, SocketContext* context)
{
    if (this->web_socket != nullptr)
    {
        spdlog::error("Server: Already connected!");

        return;
    }

    std::string_view websocket_key = request->getHeader("sec-websocket-key");
    std::string_view websocket_protocol = request->getHeader("sec-websocket-protocol");
    std::string_view websocket_extensions = request->getHeader("sec-websocket-extensions");

    response->upgrade<uint8_t>(0, websocket_key, websocket_protocol, websocket_extensions, context);
}

void Server::process_open(WebSocket* socket)
{
    if (this->web_socket != nullptr)
    {
        spdlog::error("Server: Already connected!");

        return;
    }

    this->web_socket = socket;
}

void Server::process_message(WebSocket* socket, std::string_view message, uWS::OpCode opcode)
{
    if (opcode != uWS::BINARY)
    {
        spdlog::error("Server: Invalid message type!");

        return;
    }

    if (message.size() < sizeof(shared::PacketType))
    {
        spdlog::error("Server: Invalid message size!");

        return;
    }

    shared::PacketType type = *(shared::PacketType*)message.data();

    switch (type)
    {
    case shared::PACKET_TYPE_SESSION_CREATE:
        if (!Server::parse_packet(message, this->on_session_create))
        {
            spdlog::error("Server: Can't parse session create packet!");
        }
        break;
    case shared::PACKET_TYPE_SESSION_DESTROY:
        if (!Server::parse_packet(message, this->on_session_destroy))
        {
            spdlog::error("Server: Can't parse session destroy packet!");
        }
        break;
    case shared::PACKET_TYPE_RENDER_REQUEST:
        if (!Server::parse_packet(message, this->on_render_request))
        {
            spdlog::error("Server: Can't parse render request packet!");
        }
        break;
    case shared::PACKET_TYPE_MESH_SETTINGS:
        if (!Server::parse_packet(message, this->on_mesh_settings_change))
        {
            spdlog::error("Server: Can't parse mesh settings packet!");
        }
        break;
    case shared::PACKET_TYPE_VIDEO_SETTINGS:
        if (!Server::parse_packet(message, this->on_video_settings_change))
        {
            spdlog::error("Server: Can't parse video settings packet!");
        }
        break;
    default:
        spdlog::error("Server: Invalid packet type!");
        break;
    }
}

void Server::process_close(WebSocket* socket, int code, std::string_view message)
{
    if (socket != this->web_socket)
    {
        spdlog::error("Server: Invalid socket!");

        return;
    }

    this->web_socket = nullptr;

    std::unique_lock<std::mutex> lock(this->callback_mutex);

    if (!this->on_session_destroy)
    {
        spdlog::warn("Server: No callback set!");

        return;
    }

    shared::SessionDestroyPacket packet;
    packet.type = shared::PACKET_TYPE_SESSION_DESTROY;

    this->on_session_destroy(packet);
}

void Server::process_get_scenes(HttpResponse* response, HttpRequest* request)
{
    boost::json::array scene_list;

    if (!std::filesystem::exists(this->scene_directory))
    {
        response->writeStatus("404 File not Found");
        response->end("");

        spdlog::error("Server: Can't find study directory '" + this->scene_directory + "' !");

        return;
    }

    for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(this->scene_directory))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        std::string extension = entry.path().extension().string();

        if (Scene::is_file_supported(extension))
        {
            scene_list.push_back(boost::json::string(entry.path().string()));
        }
    }

    response->end(boost::json::serialize(scene_list));
}

void Server::process_get_files(HttpResponse* response, HttpRequest* request)
{
    std::filesystem::path request_path = std::filesystem::relative("/files/", request->getFullUrl());
    std::filesystem::path file_path = std::filesystem::path(this->study_directory) / request_path;

    if (!std::filesystem::exists(file_path))
    {
        response->writeStatus("404 File not Found");
        response->end("");

        spdlog::error("Server: Can't find file '" + file_path.string() + "' !");

        return;
    }

    if (std::filesystem::is_directory(file_path))
    {
        boost::json::array file_list;

        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(file_path))
        {
            if (entry.is_regular_file())
            {
                file_list.push_back(boost::json::string(entry.path().string()));
            }
        }

        response->end(boost::json::serialize(file_list));
    }

    else if (std::filesystem::is_regular_file(file_path))
    {
        std::fstream file(file_path, std::ios::in | std::ios::binary);

        if (!file.good())
        {
            response->writeStatus("400 Bad Request");
            response->end("");

            spdlog::error("Server: Can't read file '" + file_path.string() + "' !");

            return;
        }

        file.seekg(0, std::ios::end);
        std::string file_content(file.tellg(), '\0');
        file.seekg(0, std::ios::beg);

        file.read(file_content.data(), file_content.size());
        file.close();

        response->end(file_content);
    }

    else
    {
        response->writeStatus("400 Bad Request");
        response->end("");

        spdlog::error("Server: Bad request for file '" + file_path.string() + "' !");
    }
}

void Server::process_post_files(HttpResponse* response, HttpRequest* request)
{
    std::filesystem::path request_path = std::filesystem::relative("/files/", request->getFullUrl());
    std::filesystem::path file_path = std::filesystem::path(this->study_directory) / request_path;
    std::string_view request_type = request->getQuery("type");

    if (request_type == "log")
    {
        std::filesystem::create_directories(file_path.parent_path());

        std::fstream file(file_path, std::ios::out | std::ios::binary | std::ios::app);

        if (!file.good())
        {
            response->writeStatus("400 Bad Request");
            response->end("");

            spdlog::error("Server: Can't write file '" + file_path.string() + "' !");

            return;
        }

        response->onData([file = std::move(file), response](std::string_view content, bool last) mutable
        {
            file.write(content.data(), content.size());
            file.flush();

            if (last)
            {
                file.close();
                response->end("");
            }
        });
 
        response->onAborted([file_path]()
        {
            spdlog::error("Server: Transmission aborted for file '" + file_path.string() + "' !");
        });
    }

    else if (request_type == "image")
    {
        std::filesystem::create_directories(file_path.parent_path());
        std::vector<uint8_t> buffer;

        response->onData([buffer = std::move(buffer), file_path, response](std::string_view content, bool last) mutable
        {
            buffer.insert(buffer.end(), content.begin(), content.end());

            if (last)
            {
                uint32_t image_width = *(uint32_t*)buffer.data();
                uint32_t image_height = *((uint32_t*)buffer.data() + 1);
                uint32_t image_size = image_width * image_height * sizeof(glm::u8vec3);
                uint8_t* image_data = buffer.data() + sizeof(uint32_t) * 2;

                if (export_color_image(file_path.string(), glm::uvec2(image_width, image_height), image_data, image_size) == 0)
                {
                    response->writeStatus("400 Bad Request");
                    response->end("");

                    spdlog::error("Server: Can't write image '" + file_path.string() + "' !");

                    return;
                }

                response->end("");
            }
        });

        response->onAborted([file_path]()
        {
            spdlog::error("Server: Transmission aborted for file '" + file_path.string() + "' !");
        });
    }

    else
    {
        response->writeStatus("400 Bad Request");
        response->end("");

        spdlog::error("Server: Bad request for file '" + file_path.string() + "' !");
    }
}

template<class PacketType>
bool Server::parse_packet(const std::string_view& message, std::function<void(const PacketType& packet)>& callback)
{
    std::unique_lock<std::mutex> lock(this->callback_mutex);

    if (message.size() != sizeof(PacketType))
    {
        spdlog::error("Server: Invalid packet size!");

        return false;
    }

    if (!callback)
    {
        spdlog::warn("Server: No callback set!");

        return false;
    }

    callback(*(PacketType*)message.data());

    return true;
}
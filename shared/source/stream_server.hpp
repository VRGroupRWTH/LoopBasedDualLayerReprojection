#pragma once

#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include <fstream>
#include "Loop.h"
#include "WebSocket.h"
#include "protocol.hpp"

namespace i3ds {

class StreamingServer {
    using NewSessionCallback = std::function<void(const SessionInitialization& session_settings)>;
    using CloseSessionCallback = std::function<void()>;
    using RequestMeshCallback = std::function<void(const MeshRequest& request)>;
    using VideoCompressionSettingsCallback =
        std::function<void(const VideoCompressionSettings& settings)>;
    using MeshGenerationSettingsCallback =
        std::function<void(const MeshGenerationSettings& settings)>;

   public:
    StreamingServer();
    ~StreamingServer();

    void set_new_session_callback(NewSessionCallback cb);
    void set_request_mesh_callback(RequestMeshCallback cb);
    void set_video_compression_settings_callback(VideoCompressionSettingsCallback cb);
    void set_mesh_generation_settings(MeshGenerationSettingsCallback cb);
    void set_close_session_callback(CloseSessionCallback cb);

    void start(std::uint16_t port = 9000);

    LayerData allocate_layer_data();
    void submit_mesh_layer(uint32_t request_id, uint32_t layer_index, LayerData data);
    void send_server_action(ServerAction action);

   private:
    std::mutex callbacks_mutex;
    NewSessionCallback new_session_callback;
    RequestMeshCallback request_mesh_callback;
    VideoCompressionSettingsCallback video_compression_settings_callback;
    MeshGenerationSettingsCallback mesh_generation_settings_callback;
    CloseSessionCallback close_session_callback;

    std::vector<uint8_t> send_buffer;
    std::vector<uint8_t> geometry_buffer;

    std::ofstream logs[3];

    std::mutex free_layer_data_mutex;
    std::vector<LayerData> free_layer_data;

    std::thread socket_thread;
    uWS::Loop* loop = nullptr;
    std::mutex session_socket_mutex;
    using WebSocket = uWS::WebSocket<false, true, std::nullptr_t>;
    WebSocket* session_socket = nullptr;
    us_listen_socket_t* session_listen_socket = nullptr;
    void run(uint16_t port);

    void process_message(MessageType type, std::span<const uint8_t> payload);
};

}  // namespace i3ds

#pragma once

#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>

#ifdef I3DS_EMSCRIPTEN
#include "emscripten/websocket.h"
#else
#include "ixwebsocket/IXWebSocket.h"
#endif

#include "protocol.hpp"

namespace i3ds {

struct DecodedLayer {
    uint32_t request_id;
    uint32_t layer_index;
    uint32_t encoded_size;
    uint32_t encoded_geometry_size;
    float geometry_encode_time;
    float geometry_decode_time;
    LayerData layer_data;
};

class SessionClient {
    using ConnectionErrorCallback = std::function<void(std::string_view error_message)>;
    using SessionEstablishedCallback = std::function<void()>;
    using ConnectionClosedCallback = std::function<void()>;
    using MeshLayerUpdateCallback
        = std::function<void(const LayerInfo&, std::span<const Vertex> vertices,
            std::span<const Index> indices, std::span<const uint8_t> encoded_frame)>;
    using ServerActionCallback = std::function<void(ServerAction action)>;

public:
    SessionClient(const SessionInitialization& settings,
        const std::string& server_uri = "ws://localhost:9000");
    SessionClient(const SessionClient&) = delete;
    SessionClient(SessionClient&&) = delete;
    ~SessionClient();

    SessionClient& operator=(const SessionClient&) = delete;
    SessionClient& operator=(SessionClient&&) = delete;

    void set_session_established_callback(SessionEstablishedCallback cb);
    void set_connection_closed_callback(ConnectionClosedCallback cb);
    // void set_mesh_layer_update_callback(MeshLayerUpdateCallback cb);
    void set_server_action_callback(ServerActionCallback cb);

    void adjust_video_encoding_settings(const VideoCompressionSettings& settings);
    void adjust_mesh_generation_settings(const MeshGenerationSettings& settings);
    uint32_t request_mesh(const std::array<Matrix, 6>& view_matrices);
    size_t get_received_bytes() const { return this->received_bytes.load(); }
    size_t get_and_reset_received_bytes() { return this->received_bytes.exchange(0); }
    void init_log(LogInterval interval, std::span<const std::string_view> entries);
    void write_log_entries(LogInterval interval, std::span<const float> values);
    std::optional<DecodedLayer> poll_layer_data();
    void log(int level, std::string_view message);

private:
#if I3DS_EMSCRIPTEN
    EMSCRIPTEN_WEBSOCKET_T socket;
#else
    ix::WebSocket socket;
#endif
    SessionEstablishedCallback session_established_callback = nullptr;
    ConnectionClosedCallback connection_closed_callback = nullptr;
    // MeshLayerUpdateCallback mesh_layer_update_callback = nullptr;
    ServerActionCallback server_action_callback = nullptr;

    SessionInitialization session_settings;

    void connection_established();
    template <typename T> void send_message(MessageType message_type, const T& data)
    {
        std::array<uint8_t, sizeof(T) + sizeof(MessageType)> packet;
        std::memcpy(packet.data(), &message_type, sizeof(message_type));
        std::memcpy(packet.data() + sizeof(MessageType), &data, sizeof(T));
        this->send(packet);
    }
    void send(std::span<const uint8_t> data);
    void process_message(std::span<const uint8_t> data);
    void connection_closed();

    void process_message(MessageType type, std::span<const uint8_t> payload);
    std::atomic<std::size_t> received_bytes;
    uint32_t request_id = 0;
    bool connected = false;

    std::thread decoder_worker;
    void decoder_thread();

    std::mutex decoder_mutex;
    std::condition_variable messages_to_decode_cond_var;
    std::deque<std::vector<std::uint8_t>> messages_to_decode;
    std::deque<DecodedLayer> decoded_layers;
    std::atomic_uint8_t shutdown = 0;
};

}

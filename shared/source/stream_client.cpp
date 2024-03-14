#include "session_client.hpp"

#include <cassert>
#include <mutex>
#include <optional>
#include <vector>
#include "fmt/format.h"
#include "fmt/ranges.h"
#include "geometry_codec.hpp"

#if I3DS_EMSCRIPTEN
#else
#include "ixwebsocket/IXNetSystem.h"
#endif

#include "spdlog/spdlog.h"

namespace i3ds {

SessionClient::SessionClient(const SessionInitialization& settings, const std::string& server_uri)
    : session_settings(settings) {
    this->decoder_worker = std::thread(&SessionClient::decoder_thread, this);
#if I3DS_EMSCRIPTEN
    EmscriptenWebSocketCreateAttributes socket_attributes = {
        .url = server_uri.c_str(),
        .createOnMainThread = true,
    };
    this->socket = emscripten_websocket_new(&socket_attributes);

    emscripten_websocket_set_onopen_callback(
        this->socket, this,
        [](int type, const EmscriptenWebSocketOpenEvent* event, void* user_data) {
            // assert(type == x);
            auto* this_ = reinterpret_cast<SessionClient*>(user_data);
            this_->connection_established();
            return 0;
        });

    emscripten_websocket_set_onmessage_callback(
        this->socket, this,
        [](int type, const EmscriptenWebSocketMessageEvent* event, void* user_data) {
            // assert(type == x);
            auto* this_ = reinterpret_cast<SessionClient*>(user_data);
            if (event->isText) {
                std::string_view str(reinterpret_cast<const char*>(event->data), event->numBytes);
                spdlog::error("Received invalid text message: {}", str);
            } else {
                this_->process_message({event->data, event->numBytes});
            }
            return 0;
        });

    emscripten_websocket_set_onclose_callback(
        this->socket, this,
        [](int type, const EmscriptenWebSocketCloseEvent* event, void* user_data) {
            // assert(type == x);
            auto* this_ = reinterpret_cast<SessionClient*>(user_data);
            if (!event->wasClean) {
                spdlog::error("{}", event->reason);
            }
            this_->connection_closed();
            return 0;
        });

#else
    ix::initNetSystem();

    this->socket.setUrl(server_uri);

    this->socket.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            if (!msg->binary) {
                spdlog::error("Received invalid text message: {}", msg->str);
            } else {
                this->process_message(
                    {reinterpret_cast<const uint8_t*>(msg->str.data()), msg->str.size()});
            }
        } else if (msg->type == ix::WebSocketMessageType::Open) {
            this->connection_established();

        } else if (msg->type == ix::WebSocketMessageType::Error) {
            spdlog::error("{}", msg->errorInfo.reason);
            this->connection_closed();
        }
    });

    this->socket.start();
#endif
}

SessionClient::~SessionClient() {
    this->shutdown.store(1);
    this->messages_to_decode_cond_var.notify_all();
    this->decoder_worker.join();
#if I3DS_EMSCRIPTEN
    emscripten_websocket_close(this->socket, 1000, "closing session");
    emscripten_websocket_delete(this->socket);
#endif
}

void SessionClient::set_session_established_callback(SessionEstablishedCallback cb) {
    this->session_established_callback = cb;
}

void SessionClient::set_connection_closed_callback(ConnectionClosedCallback cb) {
    this->connection_closed_callback = cb;
}

// void SessionClient::set_mesh_layer_update_callback(MeshLayerUpdateCallback cb) {
//     this->mesh_layer_update_callback = cb;
// }

void SessionClient::set_server_action_callback(ServerActionCallback cb) {
    this->server_action_callback = cb;
}

void SessionClient::adjust_video_encoding_settings(const VideoCompressionSettings& settings) {
    this->send_message(MessageType::SET_VIDEO_COMPRESSION_SETTINGS, settings);
}

void SessionClient::adjust_mesh_generation_settings(const MeshGenerationSettings& settings) {
    this->send_message(MessageType::SET_MESH_GENERATION_SETTINGS, settings);
}

uint32_t SessionClient::request_mesh(const std::array<Matrix, 6>& view_matrices) {
    this->send_message(MessageType::REQUEST_MESH,
                       MeshRequest{.id = request_id, .view_matrices = view_matrices});
    // spdlog::info("request view matrix {}: {}", request_id, view_matrix);
    request_id++;
    return request_id - 1;  // don't judge please
}

void SessionClient::init_log(LogInterval interval, std::span<const std::string_view> entries) {
    std::size_t buffer_size = sizeof(MessageType) + sizeof(LogInterval);
    for (const auto& entry : entries) {
        buffer_size += entry.size() + 1;
    }
    std::vector<uint8_t> message(buffer_size);

    std::size_t current_offset = 0;
    const auto type = MessageType::LOG_INIT;
    std::memcpy(message.data() + current_offset, &type, sizeof(MessageType));
    current_offset += sizeof(MessageType);

    std::memcpy(message.data() + current_offset, &interval, sizeof(LogInterval));
    current_offset += sizeof(LogInterval);

    for (const auto& entry : entries) {
        std::memcpy(message.data() + current_offset, entry.data(), entry.size());
        current_offset += entry.size();
        message[current_offset] = 0;
        current_offset += 1;
    }

    this->send(message);
}

void SessionClient::write_log_entries(LogInterval interval, std::span<const float> values) {
    const std::size_t buffer_size = sizeof(MessageType) + sizeof(LogInterval) + values.size_bytes();
    std::vector<uint8_t> message(buffer_size);

    std::size_t current_offset = 0;
    const auto type = MessageType::LOG_WRITE;
    std::memcpy(message.data() + current_offset, &type, sizeof(MessageType));
    current_offset += sizeof(MessageType);

    std::memcpy(message.data() + current_offset, &interval, sizeof(LogInterval));
    current_offset += sizeof(LogInterval);

    for (const auto& entry : values) {
        std::memcpy(message.data() + current_offset, &entry, sizeof(float));
        current_offset += sizeof(float);
    }

    this->send(message);
}

std::optional<DecodedLayer> SessionClient::poll_layer_data() {
    std::unique_lock<std::mutex> lock(this->decoder_mutex);
    if (this->decoded_layers.size() > 0) {
        DecodedLayer decoded_layer = std::move(this->decoded_layers.front());
        this->decoded_layers.pop_front();
        return std::move(decoded_layer);
    } else {
        return std::nullopt;
    }
}

void SessionClient::log(int level, std::string_view message) {
    std::vector<uint8_t> buffer(sizeof(MessageType) + sizeof(int) + message.length());
    MessageType type = MessageType::LOG;
    std::memcpy(buffer.data(), &type, sizeof(MessageType));
    std::memcpy(buffer.data() + sizeof(MessageType), &level, sizeof(int));
    std::memcpy(buffer.data() + sizeof(MessageType) + sizeof(int), message.data(), message.length());
    this->send(buffer);
}

void SessionClient::connection_established() {
    this->connected = true;
    spdlog::info("websocket connection to server established, sending initialization packet");
    this->send_message(MessageType::INITIALIZE_SESSION, this->session_settings);

    if (this->session_established_callback != nullptr) {
        this->session_established_callback();
    }
}

void SessionClient::send(std::span<const uint8_t> message) {
    if (!this->connected) {
        spdlog::warn("cannot send data when socket is not connected");
        return;
    }

#if I3DS_EMSCRIPTEN
    emscripten_websocket_send_binary(this->socket, const_cast<uint8_t*>(message.data()),
                                     message.size());
#else
    ix::IXWebSocketSendData send_data(reinterpret_cast<const char*>(message.data()),
                                      message.size());
    this->socket.sendBinary(send_data);
#endif
}

void SessionClient::process_message(std::span<const uint8_t> message) {
    this->received_bytes.fetch_add(message.size());

    MessageType type;
    if (message.size() < sizeof(MessageType)) {
        spdlog::error("invalid message of size: {}", message);
    }
    std::memcpy(&type, message.data(), sizeof(MessageType));
    this->process_message(type, message.subspan(sizeof(MessageType)));
}

void SessionClient::connection_closed() {
    spdlog::info("connection to server has been closed");
    if (this->connection_closed_callback) {
        this->connection_closed_callback();
    }
}

void SessionClient::process_message(MessageType type, std::span<const uint8_t> payload) {
    switch (type) {
        case MessageType::INITIALIZE_SESSION: {
            spdlog::error("invalid message type: initialize session");
            break;
        }

        case MessageType::SET_VIDEO_COMPRESSION_SETTINGS: {
            spdlog::error("invalid message type: set video compression setting");
            break;
        }

        case MessageType::SET_MESH_GENERATION_SETTINGS: {
            spdlog::error("invalid message type: set mesh generation settings");
            break;
        }

        case MessageType::REQUEST_MESH: {
            spdlog::error("invalid message type: request mesh");
            break;
        }

        case MessageType::UPDATE_MESH_LAYER: {
            std::vector<std::uint8_t> message(payload.size());
            std::memcpy(message.data(), payload.data(), payload.size());
            std::unique_lock<std::mutex> lock(this->decoder_mutex);
            this->messages_to_decode.push_back(message);
            lock.unlock();
            this->messages_to_decode_cond_var.notify_one();
            break;
        }

        case MessageType::SERVER_ACTION: {
            ServerAction action;
            if (!get_message_payload(payload, &action)) {
                spdlog::error("invalid message payload for server action");
                return;
            }
            if (this->server_action_callback) {
                this->server_action_callback(action);
            }
            break;
        }

        default:
            spdlog::error("invalid message type");
    }
}

void SessionClient::decoder_thread() {
    std::vector<Vertex> vertices;
    std::vector<Index> indices;

    while (!shutdown) {
        std::optional<std::vector<std::uint8_t>> message;
        {
            std::unique_lock<std::mutex> lock(this->decoder_mutex);
            this->messages_to_decode_cond_var.wait(
                lock, [this]() { return this->messages_to_decode.size() > 0 || this->shutdown.load(); });
            if (this->shutdown.load()) {
                return;
            }
            assert(this->messages_to_decode.size() > 0);
            message = std::move(this->messages_to_decode.front());
            this->messages_to_decode.pop_front();
        }
        assert(message);
        std::span<std::uint8_t> payload(*message);

        LayerInfo layer_info;
        if (!get_message_payload(payload.subspan(0, sizeof(LayerInfo)), &layer_info)) {
            spdlog::error("invalid message payload for mesh layer update");
            return;
        }

        // const auto vertex_size = sizeof(Vertex) * layer_info.vertex_count;
        // const auto index_size = sizeof(Index) * layer_info.index_count;
        const auto geometry_size = layer_info.geometry_size;
        const auto frame_size = layer_info.frame_size;
        const auto required_payload_size = sizeof(LayerInfo) + geometry_size + frame_size;

        if (payload.size() != required_payload_size) {
            spdlog::error(
                "invalid message payload for mesh layer update, expected {} bytes, got {}",
                required_payload_size, payload.size());
            return;
        }

        const auto geometry = payload.subspan(sizeof(LayerInfo), geometry_size);
        using Clock = std::chrono::steady_clock;
        const auto decode_start = Clock::now();
        GeometryCodec::decode(geometry, indices, vertices);
        const auto decode_end = Clock::now();
        const double decode_time =
            std::chrono::duration_cast<std::chrono::duration<double>>(decode_end - decode_start)
                .count() *
            1000.0f;
        spdlog::info("decode time: {}", decode_time);

        auto image = payload.subspan(sizeof(LayerInfo) + geometry_size);
        LayerData layer_data;
        layer_data.view_statistic = layer_info.view_statistics;
        layer_data.view_matrices = layer_info.view_matrices;
        layer_data.vertex_counts = layer_info.vertex_counts;
        layer_data.index_counts = layer_info.index_counts;
        layer_data.vertices = vertices;
        layer_data.indices = indices;
        layer_data.image = std::vector(image.begin(), image.end());

        DecodedLayer decoded_layer {
            .request_id = layer_info.request_id,
            .layer_index = layer_info.layer_index,
            .encoded_size = payload.size_bytes(),
            .encoded_geometry_size = geometry.size_bytes(),
            .geometry_encode_time = 0.0f,
            .geometry_decode_time = static_cast<float>(decode_time),
            .layer_data = std::move(layer_data),
        };

        std::unique_lock<std::mutex> lock(this->decoder_mutex);
        this->decoded_layers.push_back(std::move(decoded_layer));
    }
}

}  // namespace i3ds

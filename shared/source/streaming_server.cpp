#include "streaming_server.hpp"
#include <charconv>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <jsoncons/json.hpp>
#include <sstream>
#include "App.h"
#include "Loop.h"
#include "fmt/ranges.h"
#include "geometry_codec.hpp"
#include "spdlog/spdlog.h"

namespace i3ds {

template <typename T>
std::string_view value_view(const T* value) {
    return {reinterpret_cast<const char*>(value), sizeof(T)};
}

template <typename T>
std::string_view vector_view(const std::vector<T>* vector) {
    return {reinterpret_cast<const char*>(vector->data()), sizeof(T) * vector->size()};
}

StreamingServer::StreamingServer() {}

StreamingServer::~StreamingServer() {
    if (this->session_socket != nullptr) {
        this->session_socket->close();
    }

    if (this->session_listen_socket != nullptr) {
        us_listen_socket_close(0, this->session_listen_socket);
    }

    if (this->socket_thread.joinable()) {
        this->socket_thread.join();
    }
}

void StreamingServer::set_new_session_callback(NewSessionCallback cb) {
    std::lock_guard<std::mutex> lock_guard(this->callbacks_mutex);
    this->new_session_callback = cb;
}

void StreamingServer::set_request_mesh_callback(RequestMeshCallback cb) {
    std::lock_guard<std::mutex> lock_guard(this->callbacks_mutex);
    this->request_mesh_callback = cb;
}

void StreamingServer::set_video_compression_settings_callback(VideoCompressionSettingsCallback cb) {
    std::lock_guard<std::mutex> lock_guard(this->callbacks_mutex);
    this->video_compression_settings_callback = cb;
}
void StreamingServer::set_mesh_generation_settings(MeshGenerationSettingsCallback cb) {
    std::lock_guard<std::mutex> lock_guard(this->callbacks_mutex);
    this->mesh_generation_settings_callback = cb;
}

void StreamingServer::set_close_session_callback(CloseSessionCallback cb) {
    std::lock_guard<std::mutex> lock_guard(this->callbacks_mutex);
    this->close_session_callback = cb;
}

void StreamingServer::start(std::uint16_t port) {
    this->socket_thread = std::thread(&StreamingServer::run, this, port);
}

LayerData StreamingServer::allocate_layer_data() {
    std::unique_lock<std::mutex> lock_guard(this->free_layer_data_mutex);

    if (this->free_layer_data.size() > 0) {
        LayerData data = std::move(this->free_layer_data.back());
        this->free_layer_data.pop_back();
        return data;
    } else {
        return LayerData{};
    }
}

void StreamingServer::submit_mesh_layer(uint32_t request_id, uint32_t layer_index, LayerData data) {
    assert(this->loop);
    this->loop->defer([this, data = std::move(data), request_id, layer_index]() mutable {
        {
            GeometryCodec::encode(data.indices, data.vertices, this->geometry_buffer);
            const size_t total_size = sizeof(MessageType) + sizeof(LayerInfo) +
                                      this->geometry_buffer.size() + data.image.size();

            this->send_buffer.resize(0);
            this->send_buffer.reserve(total_size);

            auto send_buffer_append_bytes = [this](std::span<const uint8_t> bytes) {
                this->send_buffer.insert(this->send_buffer.end(), bytes.begin(), bytes.end());
            };
            auto send_buffer_append_value = [&send_buffer_append_bytes](const auto& value) {
                send_buffer_append_bytes(
                    std::span{reinterpret_cast<const uint8_t*>(&value), sizeof(value)});
            };

            MessageType type = MessageType::UPDATE_MESH_LAYER;
            LayerInfo layer_info = {
                .request_id = request_id,
                .layer_index = layer_index,
                .geometry_size = static_cast<uint32_t>(this->geometry_buffer.size()),
                .frame_size = static_cast<uint32_t>(data.image.size()),
            };
            std::copy(data.view_statistic.begin(), data.view_statistic.end(),
                      layer_info.view_statistics.begin());
            std::copy(data.view_matrices.begin(), data.view_matrices.end(),
                      layer_info.view_matrices.begin());
            std::copy(data.vertex_counts.begin(), data.vertex_counts.end(),
                      layer_info.vertex_counts.begin());
            std::copy(data.index_counts.begin(), data.index_counts.end(),
                      layer_info.index_counts.begin());

            send_buffer_append_value(type);
            send_buffer_append_value(layer_info);
            send_buffer_append_bytes(this->geometry_buffer);
            send_buffer_append_bytes(data.image);

            assert(this->send_buffer.size() == total_size);

            std::unique_lock<std::mutex> session_socket_lock_guard(this->session_socket_mutex);
            if (!this->session_socket) {
                return;
            }
            this->session_socket->send({reinterpret_cast<const char*>(this->send_buffer.data()),
                                        this->send_buffer.size()});
        }

        std::unique_lock<std::mutex> layer_lock_guard(this->free_layer_data_mutex);
        data.vertices.resize(0);
        data.indices.resize(0);
        data.image.resize(0);
        this->free_layer_data.push_back(std::move(data));
    });
}

void StreamingServer::send_server_action(ServerAction action) {
    assert(this->loop);
    this->loop->defer([this, action]() {
        std::unique_lock<std::mutex> session_socket_lock_guard(this->session_socket_mutex);
        if (!this->session_socket) {
            return;
        }
        struct {
            MessageType type;
            ServerAction action;
        } message{.type = MessageType::SERVER_ACTION, .action = action};
        this->session_socket->send({reinterpret_cast<const char*>(&message), sizeof(message)});
    });
}

namespace fs = std::filesystem;

fs::path participant_path(uint32_t participant_id) {
    return fs::path("/home/so225523/ar-streaming-study-results") / std::to_string(participant_id);
}

std::optional<uint32_t> parse_participant_id(uWS::HttpRequest* req) {
    const auto id = req->getQuery("participantId");
    std::uint32_t participant_id;
    auto [ptr, ec] = std::from_chars(id.data(), id.data() + id.size(), participant_id);
    if (ec == std::errc() && ptr != id.data()) {
        return participant_id;
    } else {
        return std::nullopt;
    }
}

std::optional<std::string> parse_condition(uWS::HttpRequest* req) {
    const auto condition = req->getQuery("condition");
    if (condition.empty()) {
        return std::nullopt;
    } else {
        return std::string(condition);
    }
}

enum class File {
    DEMOGRAPHIC,
    CONDITION_1_TRIAL,
    CONDITION_1,
    CONDITION_1_QUESTIONNAIRE,
    CONDITION_2_TRIAL,
    CONDITION_2,
    CONDITION_2_QUESTIONNAIRE,
    CONDITION_3_TRIAL,
    CONDITION_3,
    CONDITION_3_QUESTIONNAIRE,
    FINAL_QUESTIONNAIRE,
};

fs::path get_file_path(uint32_t participant_id, File file) {
    switch (file) {
        case File::CONDITION_1_TRIAL:
            return participant_path(participant_id) / "cond1-trial.json";
        case File::CONDITION_1:
            return participant_path(participant_id) / "cond1-run.json";
        case File::CONDITION_1_QUESTIONNAIRE:
            return participant_path(participant_id) / "cond1-questionnaire.json";
        case File::CONDITION_2_TRIAL:
            return participant_path(participant_id) / "cond2-trial.json";
        case File::CONDITION_2:
            return participant_path(participant_id) / "cond2-run.json";
        case File::CONDITION_2_QUESTIONNAIRE:
            return participant_path(participant_id) / "cond2-questionnaire.json";
        case File::CONDITION_3_TRIAL:
            return participant_path(participant_id) / "cond3-trial.json";
        case File::CONDITION_3:
            return participant_path(participant_id) / "cond3-run.json";
        case File::CONDITION_3_QUESTIONNAIRE:
            return participant_path(participant_id) / "cond3-questionnaire.json";
        case File::FINAL_QUESTIONNAIRE:
            return participant_path(participant_id) / "final.json";
    }
}

std::ofstream open_file(const fs::path& path) {
    fs::create_directories(path.parent_path());
    if (fs::exists(path)) {
        int i = 0;
        fs::path backup_path;
        do {
            backup_path = path;
            backup_path.replace_filename(path.filename().string() + ".bak" + std::to_string(i));
            ++i;
        } while (fs::exists(backup_path));
        spdlog::info("renaming {} to {}", path.string(), backup_path.string());
        fs::rename(path, backup_path);
    }
    return std::ofstream(path);
}

std::optional<std::string> read_file(const fs::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }

    file.seekg(0, std::ios::end);
    std::string content(file.tellg(), '\0');
    file.seekg(0, std::ios::beg);
    file.read(content.data(), content.size());
    return content;
}

void write_file(const fs::path& path, std::string_view content) {
    auto file = open_file(path);
    file.write(content.data(), content.size());
}

struct FileWriter {
    std::ofstream file;
    uWS::HttpResponse<false>* response;

    FileWriter(uWS::HttpResponse<false>* response, const fs::path& path)
        : file(open_file(path)), response(response) {}

    void operator()(std::string_view content, bool last) {
        spdlog::info("writing {} bytes", content.size());
        this->file.write(content.data(), content.size());
        if (last) {
            this->file.close();
            this->response->end("");
            spdlog::info("finished writing");
        }
    }
};

struct AbortHandler {
    uWS::HttpResponse<false>* response;

    AbortHandler(uWS::HttpResponse<false>* response) : response(response) {}

    void operator()() { spdlog::error("aborted"); }
};

#define POST_FILE_ENDPOINT(endpoint, file_type)                                            \
    post(endpoint, [](auto* res, auto* req) {                                              \
        if (const auto participant_id = parse_participant_id(req); participant_id) {       \
            spdlog::info("writing {} file for participant {}", endpoint, *participant_id); \
            res->onData(FileWriter(res, get_file_path(*participant_id, file_type)));       \
            res->onAborted(AbortHandler(res));                                             \
        } else {                                                                           \
            spdlog::error("failed to write {} file, no `participantId` found", endpoint);  \
            res->writeStatus("400 Bad Request");                                           \
            res->end("");                                                                  \
        }                                                                                  \
    })

#define GET_FILE_ENDPOINT(endpoint, file_type)                                                   \
    get(endpoint, [](auto* res, auto* req) {                                                     \
        if (const auto participant_id = parse_participant_id(req); participant_id) {             \
            spdlog::info("requested {} for participant {}", endpoint, *participant_id);          \
            const auto content = read_file(get_file_path(*participant_id, file_type));           \
            if (content) {                                                                       \
                res->end(*content);                                                              \
            } else {                                                                             \
                spdlog::error("failed to get {} for participant {}", endpoint, *participant_id); \
                res->writeStatus("404 File not Found");                                          \
                res->end("");                                                                    \
            }                                                                                    \
        } else {                                                                                 \
            spdlog::error("failed to get {}, no `participantId` found", endpoint);               \
            res->writeStatus("400 Bad Request");                                                 \
            res->end("");                                                                        \
        }                                                                                        \
    })

void StreamingServer::run(uint16_t port) {
    this->loop = uWS::Loop::get();
    uWS::App()
        .get(
            "/progress",
            [](auto* res, auto* req) {
                if (const auto participant_id = parse_participant_id(req); participant_id) {
                    spdlog::info("requested progress for participant {}", *participant_id);
                    using namespace jsoncons;
                    json data;
                    data["cond1-trial"] =
                        fs::exists(get_file_path(*participant_id, File::CONDITION_1_TRIAL));
                    data["cond1-run"] =
                        fs::exists(get_file_path(*participant_id, File::CONDITION_1));
                    data["cond1-questionnaire"] =
                        fs::exists(get_file_path(*participant_id, File::CONDITION_1_QUESTIONNAIRE));
                    data["cond2-trial"] =
                        fs::exists(get_file_path(*participant_id, File::CONDITION_2_TRIAL));
                    data["cond2-run"] =
                        fs::exists(get_file_path(*participant_id, File::CONDITION_2));
                    data["cond2-questionnaire"] =
                        fs::exists(get_file_path(*participant_id, File::CONDITION_2_QUESTIONNAIRE));
                    data["cond3-trial"] =
                        fs::exists(get_file_path(*participant_id, File::CONDITION_3_TRIAL));
                    data["cond3-run"] =
                        fs::exists(get_file_path(*participant_id, File::CONDITION_3));
                    data["cond3-questionnaire"] =
                        fs::exists(get_file_path(*participant_id, File::CONDITION_3_QUESTIONNAIRE));
                    data["final"] =
                        fs::exists(get_file_path(*participant_id, File::FINAL_QUESTIONNAIRE));
                    std::string s;
                    data.dump(s);
                    res->end(s);
                } else {
                    spdlog::error("failed to get progress, no `participantId` found");
                    res->writeStatus("400 Bad Request");
                    res->end("");
                }
            })
        .POST_FILE_ENDPOINT("/cond1-trial", File::CONDITION_1_TRIAL)
        .POST_FILE_ENDPOINT("/cond1-run", File::CONDITION_1)
        .POST_FILE_ENDPOINT("/cond1-questionnaire", File::CONDITION_1_QUESTIONNAIRE)
        .GET_FILE_ENDPOINT("/cond1-questionnaire", File::CONDITION_1_QUESTIONNAIRE)
        .POST_FILE_ENDPOINT("/cond2-trial", File::CONDITION_2_TRIAL)
        .POST_FILE_ENDPOINT("/cond2-run", File::CONDITION_2)
        .POST_FILE_ENDPOINT("/cond2-questionnaire", File::CONDITION_2_QUESTIONNAIRE)
        .GET_FILE_ENDPOINT("/cond2-questionnaire", File::CONDITION_2_QUESTIONNAIRE)
        .POST_FILE_ENDPOINT("/cond3-trial", File::CONDITION_3_TRIAL)
        .POST_FILE_ENDPOINT("/cond3-run", File::CONDITION_3)
        .POST_FILE_ENDPOINT("/cond3-questionnaire", File::CONDITION_3_QUESTIONNAIRE)
        .GET_FILE_ENDPOINT("/cond3-questionnaire", File::CONDITION_3_QUESTIONNAIRE)
        .POST_FILE_ENDPOINT("/final", File::FINAL_QUESTIONNAIRE)
        .GET_FILE_ENDPOINT("/final", File::FINAL_QUESTIONNAIRE)
        .ws<std::nullptr_t>(
            "/*",
            {
                .maxBackpressure = 100 * 1024 * 1024,
                .upgrade =
                    [this](auto* res, auto* req, auto* context) {
                        spdlog::info("new streaming client");
                        std::unique_lock<std::mutex> lock_guard(this->session_socket_mutex);
                        const auto participant_id = parse_participant_id(req);
                        const auto condition = parse_condition(req);

                        if (this->session_socket) {
                            spdlog::error(
                                "cannot start session: another session is already in progress");
                            res->writeStatus("529 Site is overloaded")->end();
                        } else if (!participant_id || !condition) {
                            spdlog::error(
                                "cannot start session: no participant id present or condition "
                                "present");
                            res->writeStatus("400 Bad Request")->end();
                        } else {
                            spdlog::info("starting session for participant {}", *participant_id);
                            const auto actual_condition = std::stoi(*condition) / 2;
                            const auto trial_infinx = (std::stoi(*condition) % 2 == 0) ? "-trial" : "";

                            this->logs[static_cast<uint32_t>(LogInterval::PER_FRAME)] =
                                open_file(participant_path(*participant_id) /
                                          fmt::format("{}{}-per-frame.csv", actual_condition, trial_infinx));
                            this->logs[static_cast<uint32_t>(LogInterval::PER_LAYER_UPDATE)] =
                                open_file(participant_path(*participant_id) /
                                          fmt::format("{}{}-per-layer-update.csv", actual_condition, trial_infinx));
                            this->logs[static_cast<uint32_t>(LogInterval::PER_SESSION)] =
                                open_file(participant_path(*participant_id) /
                                          fmt::format("{}{}-per-session.csv", actual_condition, trial_infinx));
                            res->upgrade(nullptr, req->getHeader("sec-websocket-key"),
                                         req->getHeader("sec-websocket-protocol"),
                                         req->getHeader("sec-websocket-extensions"), context);
                        }
                    },
                .open =
                    [this](auto* ws) {
                        spdlog::info("new websocket connection");
                        // this->session_socket_mutex is already locked by updgrade,
                        // maybe switch to recurive mutex
                        assert(this->session_socket == nullptr);
                        this->session_socket = ws;
                    },
                .message =
                    [this](auto* ws, std::string_view message, uWS::OpCode opcode) {
                        if (opcode != uWS::OpCode::BINARY) {
                            spdlog::error("invalid message with opcode {}: {}", opcode, message);
                        } else if (message.size() < sizeof(MessageType)) {
                            spdlog::error("invalid binary message of size {}", message.size());
                        } else {
                            MessageType type;
                            std::memcpy(&type, message.data(), sizeof(MessageType));
                            const auto message_view = message.substr(sizeof(MessageType));
                            this->process_message(
                                type, {reinterpret_cast<const uint8_t*>(message_view.data()),
                                       message_view.size()});
                        }
                    },
                .close =
                    [this](auto* ws, int code, std::string_view message) {
                        {
                            spdlog::warn("session has been closed {}: {}", code, message);
                            std::unique_lock<std::mutex> lock_guard(this->session_socket_mutex);
                            if (this->session_socket == ws) {
                                this->session_socket = nullptr;
                                spdlog::info("resetting session socket");
                            }
                        }
                        {
                            std::lock_guard<std::mutex> lock_guard(this->callbacks_mutex);
                            if (this->close_session_callback) {
                                this->close_session_callback();
                            } else {
                                spdlog::warn("no message handler registered");
                            }
                        }
                        this->logs[static_cast<uint32_t>(LogInterval::PER_FRAME)].close();
                        this->logs[static_cast<uint32_t>(LogInterval::PER_LAYER_UPDATE)].close();
                        this->logs[static_cast<uint32_t>(LogInterval::PER_SESSION)].close();
                    },
            })
        .listen(port,
                [this](auto* listenSocket) {
                    if (listenSocket) {
                        spdlog::info("http server open, waiting for connections");
                    }

                    this->session_listen_socket = listenSocket;
                })
        .run();
}

void StreamingServer::process_message(MessageType type, std::span<const uint8_t> payload) {
    switch (type) {
        case MessageType::INITIALIZE_SESSION: {
            std::lock_guard<std::mutex> lock_guard(this->callbacks_mutex);
            if (this->new_session_callback) {
                spdlog::info("received session initialization packet from client");
                SessionInitialization session_init;
                if (get_message_payload(payload, &session_init)) {
                    this->new_session_callback(session_init);
                }
            } else {
                spdlog::warn("no message handler registered");
            }
            break;
        }

        case MessageType::SET_VIDEO_COMPRESSION_SETTINGS: {
            std::lock_guard<std::mutex> lock_guard(this->callbacks_mutex);
            if (this->mesh_generation_settings_callback) {
                spdlog::info("received video compression settings packet");
                VideoCompressionSettings video_compression_settings;
                if (get_message_payload(payload, &video_compression_settings)) {
                    this->video_compression_settings_callback(video_compression_settings);
                }
            } else {
                spdlog::warn("no message handler registered");
            }
            break;
        }

        case MessageType::SET_MESH_GENERATION_SETTINGS: {
            std::lock_guard<std::mutex> lock_guard(this->callbacks_mutex);
            if (this->mesh_generation_settings_callback) {
                spdlog::info("received mesh generation settings packet");
                MeshGenerationSettings mesh_generation_settings;
                if (get_message_payload(payload, &mesh_generation_settings)) {
                    this->mesh_generation_settings_callback(mesh_generation_settings);
                }
            } else {
                spdlog::warn("no message handler registered");
            }
            break;
        }

        case MessageType::REQUEST_MESH: {
            std::lock_guard<std::mutex> lock_guard(this->callbacks_mutex);
            if (this->request_mesh_callback) {
                // spdlog::info("received request mesh packet");
                MeshRequest mesh_request;
                if (get_message_payload(payload, &mesh_request)) {
                    this->request_mesh_callback(mesh_request);
                }
            } else {
                spdlog::warn("no message handler registered");
            }
            break;
        }

        case MessageType::UPDATE_MESH_LAYER:
            spdlog::error("received update mesh layer packet from client");
            break;

        case MessageType::LOG_INIT: {
            LogInterval interval;
            std::memcpy(&interval, payload.data(), sizeof(LogInterval));

            const auto strings = payload.subspan(sizeof(LogInterval));
            size_t start = 0;
            size_t current = 0;
            for (const auto& v : strings) {
                current += 1;
                if (v == 0) {
                    this->logs[static_cast<uint32_t>(interval)].write(
                        reinterpret_cast<const char*>(strings.data() + start), current - start - 1); // -1 because of NULL terminator
                    this->logs[static_cast<uint32_t>(interval)].write(";", 1);
                    start = current;
                }
            }
            this->logs[static_cast<uint32_t>(interval)].write("\n", 1);
        } break;

        case MessageType::LOG_WRITE: {
            const auto interval = *reinterpret_cast<const LogInterval*>(payload.data());
            const auto value_bytes = payload.subspan(sizeof(LogInterval));
            const auto values = std::span(reinterpret_cast<const float*>(value_bytes.data()),
                                          value_bytes.size_bytes() / sizeof(float));
            std::array<char, 255> buffer;
            for (const auto& v : values) {
                const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), v,
                                                  std::chars_format::fixed);
                this->logs[static_cast<uint32_t>(interval)].write(buffer.data(),
                                                                  result.ptr - buffer.data());
                this->logs[static_cast<uint32_t>(interval)].write(";", 1);
            }
            this->logs[static_cast<uint32_t>(interval)].write("\n", 1);
            this->logs[static_cast<uint32_t>(interval)].flush();
        } break;

        case MessageType::LOG: {
            spdlog::level::level_enum level;
            std::memcpy(&level, payload.data(), sizeof(int));
            std::string_view message(reinterpret_cast<const char*>(payload.data() + 4),
                                     payload.size() - 4);
            spdlog::log(level, "client: {}", message);
        } break;
    }
}

}  // namespace i3ds

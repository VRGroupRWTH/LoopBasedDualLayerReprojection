#ifndef HEADER_SERVER
#define HEADER_SERVER

#include <WebSocket.h>
#include <HttpResponse.h>
#include <protocol.hpp>

#include <functional>
#include <thread>
#include <mutex>
#include <future>

struct LayerData
{
    uint32_t request_id = 0;
    uint32_t layer_index = 0;
    
    std::array<shared::ViewMetadata, SHARED_VIEW_COUNT_MAX> view_metadata;
    std::array<shared::Matrix, SHARED_VIEW_COUNT_MAX> view_matrices;

    std::array<std::vector<shared::Vertex>, SHARED_VIEW_COUNT_MAX> vertices;
    std::array<std::vector<shared::Index>, SHARED_VIEW_COUNT_MAX> indices;

    std::vector<uint8_t> geometry;
    std::vector<uint8_t> image;
};

class Server
{
public:
    typedef std::function<void (const shared::SessionCreatePacket& packet)> OnSessionCreate;
    typedef std::function<void (const shared::SessionDestroyPacket& packet)> OnSessionDestroy;
    typedef std::function<void (const shared::RenderRequestPacket& packet)> OnRenderRequest;
    typedef std::function<void (const shared::MeshSettingsPacket& packet)> OnMeshSettingsChange;
    typedef std::function<void (const shared::VideoSettingsPacket& packet)> OnVideoSettingsChange;

    typedef uWS::WebSocket<false, true, uint8_t> WebSocket;
    typedef uWS::HttpResponse<false> HttpResponse;
    typedef uWS::HttpRequest HttpRequest;

    typedef us_listen_socket_t ListenSocket;
    typedef us_socket_context_t SocketContext;

private:
    const std::string scene_directory = "./scene";
    const std::string study_directory = "./study";

    std::thread thread;
    uWS::Loop* loop = nullptr;             // Owned by main thread
    WebSocket* web_socket = nullptr;       // Owned by thread
    ListenSocket* listen_socket = nullptr; // Owned by thread
    std::vector<uint8_t> send_buffer;      // Owned by thread

    std::mutex callback_mutex;
    OnSessionCreate on_session_create;              // Protected by callback_mutex
    OnSessionDestroy on_session_destroy;            // Protected by callback_mutex
    OnRenderRequest on_render_request;              // Protected by callback_mutex
    OnMeshSettingsChange on_mesh_settings_change;   // Protected by callback_mutex
    OnVideoSettingsChange on_video_settings_change; // Protected by callback_mutex

    std::mutex layer_data_mutex;
    std::vector<LayerData*> layer_data_list; // Protected by layer_data_mutex
    std::vector<LayerData*> layer_data_pool; // Protected by layer_data_mutex

public:
    Server(std::string scene_directory, std::string study_directory);
    ~Server();

    bool create(uint32_t port = 9000);
    void destroy();

    LayerData* allocate_layer_data();
    void submit_layer_data(LayerData* layer_data);

    void set_on_session_create(OnSessionCreate callback);
    void set_on_session_destroy(OnSessionDestroy callback);
    void set_on_render_request(OnRenderRequest callback);
    void set_on_mesh_settings_change(OnMeshSettingsChange callback);
    void set_on_video_settings_change(OnVideoSettingsChange callback);

    const std::string& get_scene_directory() const;
    const std::string& get_study_directory() const;

private:
    void worker(uint32_t port, std::promise<uWS::Loop*> loop_promise);

    void process_listen(uint32_t port, ListenSocket* socket);
    void process_upgrade(HttpResponse* response, HttpRequest* request, SocketContext* context);
    void process_open(WebSocket* socket);
    void process_message(WebSocket* socket, std::string_view message, uWS::OpCode opcode);
    void process_close(WebSocket* socket, int code, std::string_view message);

    void process_get_scenes(HttpResponse* response, HttpRequest* request);
    void process_get_files(HttpResponse* response, HttpRequest* request);
    void process_post_files(HttpResponse* response, HttpRequest* request);
    
    template<class PacketType>
    bool parse_packet(const std::string_view& message, std::function<void(const PacketType& packet)>& callback);
};

#endif
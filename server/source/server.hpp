#ifndef HEADER_SERVER
#define HEADER_SERVER

#include <thread>
#include <mutex>
#include <functional>
#include <WebSocket.h>
#include <HttpResponse.h>
#include <protocol.hpp>

struct LayerData
{

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
    std::thread thread;
    std::mutex mutex;

    std::string scene_directory = "./scenes";
    std::string capture_directory = "./captures";

    WebSocket* web_socket = nullptr;       // Owned by thread
    ListenSocket* listen_socket = nullptr; // Owned by thread

    OnSessionCreate on_session_create;              // Protected by mutex
    OnSessionDestroy on_session_destroy;            // Protected by mutex
    OnRenderRequest on_render_request;              // Protected by mutex
    OnMeshSettingsChange on_mesh_settings_change;   // Protected by mutex
    OnVideoSettingsChange on_video_settings_change; // Protected by mutex

public:
    Server(std::string scene_directory, std::string capture_directory);
    ~Server();

    bool create(uint32_t port = 9000);
    void destroy();

    //TODO: Submit and request of layer data

    void set_on_session_create(OnSessionCreate callback);
    void set_on_session_destroy(OnSessionDestroy callback);
    void set_on_render_request(OnRenderRequest callback);
    void set_on_mesh_settings_change(OnMeshSettingsChange callback);
    void set_on_video_settings_change(OnVideoSettingsChange callback);

private:
    void worker(uint32_t port);

    void process_listen(ListenSocket* socket);
    void process_upgrade(HttpResponse* response, HttpRequest* request, SocketContext* context);
    void process_open(WebSocket* socket);
    void process_message(WebSocket* socket, std::string_view message, uWS::OpCode opcode);
    void process_close(WebSocket* socket, int code, std::string_view message);

    void process_get_scene(HttpResponse* response, HttpRequest* request);
    void process_get_motion_capture(HttpResponse* response, HttpRequest* request);
    void process_post_motion_capture(HttpResponse* response, HttpRequest* request);
    void process_post_log(HttpResponse* response, HttpRequest* request);
    void process_post_image(HttpResponse* response, HttpRequest* request);

    template<class PacketType>
    static bool parse_packet(const std::string_view& message, std::function<void(const PacketType& packet)>& callback);
};


/*



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
};*/

#endif
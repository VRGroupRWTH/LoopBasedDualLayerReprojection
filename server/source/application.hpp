#ifndef HEADER_APPLICATION
#define HEADER_APPLICATION

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <protocol.hpp>

#include "command_parser.hpp"
#include "server.hpp"
#include "camera.hpp"
#include "scene.hpp"
#include "session.hpp"
#include "shader.hpp"

enum ServerMessageType
{
    SERVER_MESSAGE_SESSION_CREATE,
    SERVER_MESSAGE_SESSION_DESTROY,
    SERVER_MESSAGE_RENDER_REQUEST,
    SERVER_MESSAGE_MESH_SETTINGS,
    SERVER_MESSAGE_VIDEO_SETTINGS
};

union ServerMessageData
{
public:
    shared::SessionCreatePacket session_create;
    shared::SessionDestroyPacket session_destroy;
    shared::RenderRequestPacket render_request;
    shared::MeshSettingsPacket mesh_settings;
    shared::VideoSettingsPacket video_settings;

public:
    ServerMessageData()
    {
        memset(this, 0, sizeof(ServerMessageData));
    }
};

struct ServerMessage
{
    ServerMessageType type;
    ServerMessageData data;
};

class Application
{
private:
    GLFWwindow* window = nullptr;

    CommandParser command_parser;
    Camera camera;
    Server* server = nullptr;
    Scene* scene = nullptr;
    Session* session = nullptr;

    std::mutex server_mutex;
    std::vector<ServerMessage> server_messages; //Protected by server_mutex

    Shader preview_shader = { "Application Preview Shader" };

public:
    Application() = default;

    bool create(uint32_t argument_count, const char** argument_list);
    void destroy();
    bool run();

private:
    bool create_window();
    bool create_shaders();
    bool create_server();

    bool process_session();

    void on_session_create(const shared::SessionCreatePacket& session_create);
    void on_session_destroy(const shared::SessionDestroyPacket& session_destroy);
    void on_render_request(const shared::RenderRequestPacket& render_request);
    void on_mesh_settings_change(const shared::MeshSettingsPacket& mesh_settings);
    void on_video_settings_change(const shared::VideoSettingsPacket& video_settings);

    static void GLAPIENTRY on_opengl_error(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
};

#endif

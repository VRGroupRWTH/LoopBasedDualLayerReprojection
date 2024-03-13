#ifndef HEADER_APPLICATION
#define HEADER_APPLICATION

#include <streaming_server.hpp>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "command_parser.hpp"
#include "camera.hpp"
#include "scene.hpp"
#include "session.hpp"
#include "shader.hpp"

enum ServerMessageType
{
    SERVER_MESSAGE_SESSION_CREATE,
    SERVER_MESSAGE_SESSION_DESTROY,
    SERVER_MESSAGE_MESH_REQUEST,
    SERVER_MESSAGE_MESH_SETTINGS,
    SERVER_MESSAGE_VIDEO_SETTINGS
};

union ServerMessageData
{
public:
    i3ds::SessionInitialization session_create;
    i3ds::MeshRequest mesh_request;
    i3ds::MeshGenerationSettings mesh_settings;
    i3ds::VideoCompressionSettings video_settings;

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
    Scene* scene = nullptr;
    Session* session = nullptr;

    i3ds::StreamingServer server;
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
    bool process_input();

    void on_session_create(const i3ds::SessionInitialization& session_settings);
    void on_session_destroy();
    void on_mesh_request(const i3ds::MeshRequest& mesh_request);
    void on_mesh_settings_change(const i3ds::MeshGenerationSettings& mesh_settings);
    void on_video_settings_change(const i3ds::VideoCompressionSettings& video_settings);

    static void GLAPIENTRY on_opengl_error(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
};

#endif

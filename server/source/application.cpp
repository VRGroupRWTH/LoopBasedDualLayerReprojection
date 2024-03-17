#include "application.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

bool Application::create(uint32_t argument_count, const char** argument_list)
{
    if (!this->command_parser.parse(argument_count, argument_list))
    {
        return false;
    }

    if (!this->create_window())
    {
        return false;
    }

    if (this->command_parser.get_scene_file_name().has_value())
    {
        this->scene = new Scene;

        if (!this->scene->create(this->command_parser.get_scene_file_name().value(), this->command_parser.get_scene_scale(), this->command_parser.get_scene_exposure(), this->command_parser.get_scene_indirect_intensity(), this->command_parser.get_sky_file_name(), this->command_parser.get_sky_intensity()))
        {
            return false;
        }
    }

    if (!this->create_shaders())
    {
        return false;
    }

    this->camera.update(this->window, true);

    if (!this->create_server())
    {
        return false;
    }

    return true;
}

void Application::destroy()
{
    if (this->server != nullptr)
    {
        this->server->destroy();

        delete this->server;
        this->server = nullptr;
    }

    if (this->session != nullptr)
    {
        this->session->destroy();

        delete this->session;
        this->session = nullptr;
    }

    if (this->scene != nullptr)
    {
        this->scene->destroy();

        delete this->scene;
        this->scene = nullptr;
    }

    this->preview_shader.clear_program();

    glfwMakeContextCurrent(nullptr);
    glfwDestroyWindow(this->window);

    this->window = nullptr;
}

bool Application::run()
{
    while (!glfwWindowShouldClose(this->window) && glfwGetKey(this->window, GLFW_KEY_ESCAPE) != GLFW_PRESS)
    {
        glfwPollEvents();

        if (!this->process_session())
        {
            return false;
        }

        if (this->scene != nullptr)
        {
            if (this->session == nullptr)
            {
                bool window_focused = glfwGetWindowAttrib(this->window, GLFW_FOCUSED);

                this->camera.update(window, window_focused);
            }

            glm::ivec2 window_size = glm::uvec2(0);
            glfwGetWindowSize(this->window, &window_size.x, &window_size.y);

            glViewport(0, 0, window_size.x, window_size.y);

            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
            glEnable(GL_FRAMEBUFFER_SRGB);

            glClearDepth(1.0f);
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

            this->preview_shader.use_shader();
            this->preview_shader["camera_view_projection_matrix"] = this->camera.get_projection_matrix() * this->camera.get_view_matrix();
            this->preview_shader["camera_position"] = this->camera.get_position();

            this->scene->render(this->preview_shader);

            this->preview_shader.use_default();

            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glDisable(GL_FRAMEBUFFER_SRGB);
        }
        
        glfwSwapBuffers(this->window);
    }

    return true;
}

bool Application::create_window()
{
    if (!glfwInit())
    {
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);

    this->window = glfwCreateWindow(1280, 800, "Depth Discontinuity Trace", NULL, NULL);

    if (this->window == nullptr)
    {
        return false;
    }

    glfwMakeContextCurrent(window);

    if (glewInit() != GLEW_OK)
    {
        return false;
    }

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(Application::on_opengl_error, 0);

    return true;
}

bool Application::create_shaders()
{
    ShaderDefines defines;
    defines.set_define_from_file("#include \"shared_defines.glsl\"", SHADER_DIRECTORY"shared_defines.glsl");
    defines.set_define_from_file("#include \"shared_math_library.glsl\"", SHADER_DIRECTORY"shared_math_library.glsl");
    defines.set_define_from_file("#include \"shared_indirect_library.glsl\"", SHADER_DIRECTORY"shared_indirect_library.glsl");
    defines.set_define_from_file("#include \"shared_light_library.glsl\"", SHADER_DIRECTORY"shared_light_library.glsl");

    if (!this->preview_shader.load_shader(SHADER_DIRECTORY"application_preview_shader.vert", SHADER_TYPE_VERTEX, defines))
    {
        return false;
    }

    if (!this->preview_shader.load_shader(SHADER_DIRECTORY"application_preview_shader.frag", SHADER_TYPE_FRAGMENT, defines))
    {
        return false;
    }

    if (!this->preview_shader.link_program())
    {
        return false;
    }

    return true;
}

bool Application::create_server()
{
    this->server = new Server(this->command_parser.get_scene_directory(), this->command_parser.get_study_directory());

    this->server->set_on_session_create([this](const shared::SessionCreatePacket& session_create)
    {
        this->on_session_create(session_create);
    });

    this->server->set_on_session_destroy([this](const shared::SessionDestroyPacket& session_destroy)
    {
        this->on_session_destroy(session_destroy);
    });

    this->server->set_on_render_request([this](const shared::RenderRequestPacket& render_request)
    {
        this->on_render_request(render_request);
    });

    this->server->set_on_mesh_settings_change([this](const shared::MeshSettingsPacket& mesh_settings)
    {
        this->on_mesh_settings_change(mesh_settings);
    });

    this->server->set_on_video_settings_change([this](const shared::VideoSettingsPacket& video_settings)
    {
        this->on_video_settings_change(video_settings);
    });

    if (!this->server->create())
    {
        return false;
    }

    return true;
}

bool Application::process_session()
{
    std::unique_lock<std::mutex> lock(this->server_mutex);
    std::vector<ServerMessage> messages;
    messages.swap(this->server_messages);
    lock.unlock();

    if (this->session != nullptr)
    {
        this->session->check_frames();
    }

    std::optional<ServerMessage> latest_request;

    for (const ServerMessage& message : messages)
    {
        if (message.type == SERVER_MESSAGE_SESSION_CREATE)
        {
            const shared::SessionCreatePacket& session_create = message.data.session_create;

            if (this->session != nullptr)
            {
                spdlog::error("Application: Session already open!");

                return false;
            }

            if (this->scene != nullptr)
            {
                this->scene->destroy();

                delete this->scene;
                this->scene = nullptr;
            }

            std::string scene_file_name;
            std::optional<std::string> sky_file_name;

            uint32_t scene_file_length = strnlen(session_create.scene_file_name.data(), SHARED_STRING_LENGTH_MAX);
            uint32_t sky_file_length = strnlen(session_create.sky_file_name.data(), SHARED_STRING_LENGTH_MAX);

            if (scene_file_length > 0)
            {
                scene_file_name = std::string(session_create.scene_file_name.data(), scene_file_length);
            }

            else
            {
                spdlog::error("Application: No scene specified!");

                return false;
            }

            if (sky_file_length > 0)
            {
                sky_file_name = std::string(session_create.sky_file_name.data(), sky_file_length);
            }

            this->scene = new Scene();

            if (!this->scene->create(scene_file_name, session_create.scene_scale, session_create.scene_exposure, session_create.scene_indirect_intensity, sky_file_name, session_create.sky_intensity))
            {
                spdlog::error("Application: Can't create scene!");

                return false;
            }

            glm::uvec2 resolution = glm::uvec2(0);
            resolution.x = session_create.resolution_width;
            resolution.y = session_create.resolution_height;

            MeshGeneratorType mesh_generator_type = MESH_GENERATOR_TYPE_LINE_BASED;

            switch (session_create.mesh_generator)
            {
            case shared::MESH_GENERATOR_TYPE_LINE:
                mesh_generator_type = MESH_GENERATOR_TYPE_LINE_BASED;
                break;
            case shared::MESH_GENERATOR_TYPE_LOOP:
                mesh_generator_type = MESH_GENERATOR_TYPE_LOOP_BASED;
                break;
            default:
                spdlog::error("Application: Unknown mesh generation method!");
                return false;
            }

            EncoderCodec codec = ENCODER_CODEC_H264;

            switch (session_create.video_codec)
            {
            case shared::VIDEO_CODEC_TYPE_H264:
                codec = ENCODER_CODEC_H264;
                break;
            case shared::VIDEO_CODEC_TYPE_H265:
                codec = ENCODER_CODEC_H265;
                break;
            case shared::VIDEO_CODEC_TYPE_AV1:
                codec = ENCODER_CODEC_AV1;
                break;
            default:
                spdlog::error("Application: Unknown encoding!");
                return false;
            }

            this->session = new Session();

            if (!this->session->create(this->server, mesh_generator_type, codec, resolution, session_create.layer_count, session_create.view_count, session_create.video_use_chroma_subsampling, session_create.export_enabled))
            {
                spdlog::error("Application: Can't create session!");

                return false;
            }

            glm::mat4 view_matrix = glm::mat4(1.0f);
            glm::mat4 projection_matrix = glm::make_mat4(session_create.projection_matrix.data());

            for (uint32_t index = 0; index < SHARED_VIEW_COUNT_MAX; index++)
            {
                this->camera.set_view_matrix(index, view_matrix);
            }

            this->camera.set_projection_matrix(projection_matrix);
        }

        else if (message.type == SERVER_MESSAGE_SESSION_DESTROY)
        {
            if (this->session == nullptr)
            {
                continue; //Ignore Message
            }

            this->session->destroy();

            delete this->session;
            this->session = nullptr;

            this->camera.update(this->window, true);

            latest_request.reset();
        }

        else if (message.type == SERVER_MESSAGE_RENDER_REQUEST)
        {
            const shared::RenderRequestPacket& render_request = message.data.render_request;

            if (this->session == nullptr)
            {
                continue; //Ignore Message
            }

            if (latest_request.has_value())
            {
                if (latest_request->data.render_request.request_id < render_request.request_id)
                {
                    latest_request = message;
                }
            }

            else
            {
                latest_request = message;
            }
        }

        else if (message.type == SERVER_MESSAGE_MESH_SETTINGS)
        {
            const shared::MeshSettingsPacket& mesh_settings = message.data.mesh_settings;

            if (this->session == nullptr)
            {
                continue; //Ignore Message
            }

            this->session->set_layer_depth_base_threshold(mesh_settings.layer.depth_base_threshold);
            this->session->set_layer_depth_slope_threshold(mesh_settings.layer.depth_slope_threshold);
            this->session->set_layer_use_object_ids(mesh_settings.layer.use_object_ids);

            this->session->set_mesh_settings(mesh_settings.mesh);
        }

        else if (message.type == SERVER_MESSAGE_VIDEO_SETTINGS)
        {
            const shared::VideoSettingsPacket video_settings = message.data.video_settings;

            if (this->session == nullptr)
            {
                continue; //Ignore Message
            }

            switch (video_settings.mode)
            {
            case shared::VIDEO_CODEC_MODE_CONSTANT_BITRATE:
                this->session->set_encoder_mode(ENCODER_MODE_CONSTANT_BITRATE);
                break;
            case shared::VIDEO_CODEC_MODE_CONSTANT_QUALITY:
                this->session->set_encoder_mode(ENCODER_MODE_CONSTANT_QUALITY);
                break;
            default:
                spdlog::error("Application: Unknown video mode!");
                return false;
            }

            this->session->set_encoder_frame_rate(video_settings.framerate);
            this->session->set_encoder_bitrate(video_settings.bitrate);
            this->session->set_encoder_quality(video_settings.quality);
        }

        else
        {
            spdlog::error("Application: Unknown server message!");

            return false;
        }
    }

    if (latest_request.has_value())
    {
        const shared::RenderRequestPacket& render_request = latest_request->data.render_request;

        if (this->session == nullptr)
        {
            return true; //Ignore Message
        }

        std::array<std::optional<std::string>, SHARED_EXPORT_COUNT_MAX> export_file_names;

        for (uint32_t index = 0; index < SHARED_EXPORT_COUNT_MAX; index++)
        {
            uint32_t file_name_length = strnlen(render_request.export_file_names[index].data(), SHARED_STRING_LENGTH_MAX);

            if (file_name_length > 0)
            {
                export_file_names[index] = std::string(render_request.export_file_names[index].data(), file_name_length);
            }
        }

        ExportRequest export_request;
        export_request.color_file_name = export_file_names[shared::EXPORT_TYPE_COLOR];
        export_request.depth_file_name = export_file_names[shared::EXPORT_TYPE_DEPTH];
        export_request.mesh_file_name = export_file_names[shared::EXPORT_TYPE_MESH];
        export_request.feature_lines_file_name = export_file_names[shared::EXPORT_TYPE_FEATURE_LINES];

        for (uint32_t view = 0; view < SHARED_VIEW_COUNT_MAX; view++)
        {
            glm::mat4 view_matrix = glm::make_mat4(render_request.view_matrices[view].data());

            this->camera.set_view_matrix(view, view_matrix);
        }

        glm::mat4 view_matrix = glm::make_mat4(render_request.view_matrices[0].data());
        this->camera.set_position(glm::inverse(view_matrix) * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

        if (!this->session->render_frame(this->camera, *this->scene, render_request.request_id, export_request))
        {
            lock.lock(); //Reinsert render request
            this->server_messages.insert(this->server_messages.begin(), latest_request.value());
            lock.unlock();
        }
    }

    return true;
}

void Application::on_session_create(const shared::SessionCreatePacket& session_create)
{
    std::unique_lock<std::mutex> lock(this->server_mutex);

    ServerMessage message;
    message.type = SERVER_MESSAGE_SESSION_CREATE;
    message.data.session_create = session_create;

    this->server_messages.push_back(message);
}

void Application::on_session_destroy(const shared::SessionDestroyPacket& session_destroy)
{
    std::unique_lock<std::mutex> lock(this->server_mutex);

    ServerMessage message;
    message.type = SERVER_MESSAGE_SESSION_DESTROY;
    message.data.session_destroy = session_destroy;

    this->server_messages.push_back(message);
}

void Application::on_render_request(const shared::RenderRequestPacket& render_request)
{
    std::unique_lock<std::mutex> lock(this->server_mutex);

    ServerMessage message;
    message.type = SERVER_MESSAGE_RENDER_REQUEST;
    message.data.render_request = render_request;

    this->server_messages.push_back(message);
}

void Application::on_mesh_settings_change(const shared::MeshSettingsPacket& mesh_settings)
{
    std::unique_lock<std::mutex> lock(this->server_mutex);

    ServerMessage message;
    message.type = SERVER_MESSAGE_MESH_SETTINGS;
    message.data.mesh_settings = mesh_settings;

    this->server_messages.push_back(message);
}

void Application::on_video_settings_change(const shared::VideoSettingsPacket& video_settings)
{
    std::unique_lock<std::mutex> lock(this->server_mutex);

    ServerMessage message;
    message.type = SERVER_MESSAGE_VIDEO_SETTINGS;
    message.data.video_settings = video_settings;

    this->server_messages.push_back(message);
}

void GLAPIENTRY Application::on_opengl_error(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
    if (type == GL_DEBUG_TYPE_ERROR)
    {
        spdlog::error("OpenGL: {}", message);
    }
}
#include "session.hpp"

bool Session::create(Server* server, MeshGeneratorType mesh_generator_type, EncoderCodec codec, const glm::uvec2& resolution, uint32_t layer_count, bool chroma_subsampling)
{
    if (!this->worker_pool.create(server))
    {
        return false;
    }

    this->mesh_generator = make_mesh_generator(mesh_generator_type);

    if (!this->mesh_generator->create(resolution))
    {
        return false;
    }

    if (!this->encoder_context.create())
    {
        return false;
    }

    glm::uvec2 encoder_resolution = glm::uvec2(resolution.x * 3, resolution.y * 2);

    for(uint32_t layer = 0; layer < layer_count; layer++)
    {
        Encoder* encoder = new Encoder();

        if (!encoder->create(&this->encoder_context, codec, encoder_resolution, chroma_subsampling))
        {
            return false;
        }

        this->encoders.push_back(encoder);
    }

    if (!this->create_shaders())
    {
        return false;
    }

    if (!this->create_frames(resolution, layer_count))
    {
        return false;
    }

    this->resolution = resolution;
    this->layer_count = layer_count;

    return true;
}

void Session::destroy()
{
    std::vector<Frame*> aborted_frames;
    this->worker_pool.destroy(aborted_frames);

    for (Frame* frame : aborted_frames)
    {
        uint32_t layer = frame->layer_index;

        for (uint32_t view = 0; view < CAMERA_VIEW_COUNT; view++)
        {
            this->mesh_generator->unmap_frame(frame->mesh_generator_frame[view]);
        }

        this->encoders[layer]->unmap_frame(frame->encoder_frame);

        frame->mesh_generator_complete.fill(false);
        frame->encoder_complete = false;

        this->empty_frames[layer].push_back(frame);
    }

    this->destroy_frames();

    if (this->mesh_generator != nullptr)
    {
        this->mesh_generator->destroy();

        delete this->mesh_generator;
    }
    
    for (Encoder* encoder : this->encoders)
    {
        encoder->destroy();

        delete encoder;
    }

    this->encoders.clear();
    this->encoder_context.destroy();
}

bool Session::render_frame(const Camera& camera, const Scene& scene, uint32_t request_id)
{
    for (uint32_t layer = 0; layer < this->layer_count; layer++)
    {
        if (this->empty_frames[layer].empty())
        {
            return false;
        }
    }

    Frame* previous_layer = nullptr;

    for (uint32_t layer = 0; layer < this->layer_count; layer++)
    {
        Frame* current_layer = this->empty_frames[layer].back();
        this->empty_frames[layer].pop_back();

        current_layer->request_id = request_id;
        
        glViewport(0, 0, this->resolution.x, this->resolution.y);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glEnable(GL_FRAMEBUFFER_SRGB);

        glClearDepth(1.0f);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

        this->layer_shader.use_shader();
        this->layer_shader["camera_projection_matrix"] = camera.get_projection_matrix();
        this->layer_shader["camera_position"] = camera.get_position();
        this->layer_shader["layer_depth_base_threshold"] = this->layer_depth_base_threshold;
        this->layer_shader["layer_depth_slope_threshold"] = this->layer_depth_slope_threshold;
        this->layer_shader["layer_use_object_ids"] = this->layer_use_object_ids;
        this->layer_shader["layer"] = layer;

        for (uint32_t view = 0; view < CAMERA_VIEW_COUNT; view++)
        {
            current_layer->view_matrix[view] = camera.get_view_matrix(view);

            current_layer->layer_timer[view].begin();

            if (previous_layer != nullptr)
            {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, previous_layer->mesh_generator_frame[view]->get_depth_buffer());
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, previous_layer->mesh_generator_frame[view]->get_object_id_buffer());

                glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
            }

            glBindFramebuffer(GL_FRAMEBUFFER, current_layer->frame_buffers[view]);

            glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

            this->layer_shader["camera_view_projection_matrix"] = camera.get_projection_matrix() * camera.get_view_matrix(view);
            this->layer_shader["camera_view_matrix"] = camera.get_view_matrix(view);

            scene.render(this->layer_shader);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, 0);

            glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);

            glm::uvec2 view_offset = glm::uvec2(0);
            view_offset.x = (view % 3) * this->resolution.x;
            view_offset.y = (view / 3) * this->resolution.y;

            glCopyImageSubData(current_layer->color_view_buffer, GL_TEXTURE_2D, 0, 0, 0, 0, current_layer->encoder_frame->color_buffer, GL_TEXTURE_2D, 0, view_offset.x, view_offset.y, 0, this->resolution.x, this->resolution.y, 1);

            current_layer->layer_timer[view].end();
        }

        this->layer_shader.use_default();

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_FRAMEBUFFER_SRGB);

        for (uint32_t view = 0; view < CAMERA_VIEW_COUNT; view++)
        {
            if (!this->mesh_generator->submit_frame(current_layer->mesh_generator_frame[view]))
            {
                return false;
            }
        }

        if (!this->encoders[layer]->submit_frame(current_layer->encoder_frame))
        {
            return false;
        }

        this->active_frames[layer].push_back(current_layer);
        previous_layer = current_layer;
    }

    return true;
}

void Session::check_frames()
{
    for (uint32_t layer = 0; layer < this->layer_count; layer++)
    {
        while(!this->active_frames[layer].empty())
        {
            Frame* frame = this->active_frames[layer].front();

            bool mesh_generator_complete = true;
            bool layer_complete = true;

            for (uint32_t view = 0; view < CAMERA_VIEW_COUNT; view++)
            {
                if (!frame->mesh_generator_complete[view])
                {
                    frame->mesh_generator_complete[view] = this->mesh_generator->map_frame(frame->mesh_generator_frame[view]);
                }

                mesh_generator_complete = mesh_generator_complete && frame->mesh_generator_complete[view];
            }

            if (mesh_generator_complete)
            {
                for (uint32_t view = 0; view < CAMERA_VIEW_COUNT; view++)
                {
                    if (!frame->layer_timer[view].get_time(frame->time_layer[view], TIMER_UNIT_MILLISECONDS))
                    {
                        layer_complete = false;

                        break;
                    }
                }
            }

            if (!frame->encoder_complete)
            {
                frame->encoder_complete = this->encoders[layer]->map_frame(frame->encoder_frame);
            }

            if (mesh_generator_complete && layer_complete && frame->encoder_complete)
            {
                this->worker_pool.submit(frame);

                this->active_frames[layer].erase(this->active_frames[layer].begin());
            }

            else
            {
                break;
            }
        }
    }

    std::vector<Frame*> complete_frames;
    this->worker_pool.reclaim(complete_frames);

    for (Frame* frame : complete_frames)
    {
        uint32_t layer = frame->layer_index;

        for (uint32_t view = 0; view < CAMERA_VIEW_COUNT; view++)
        {
            this->mesh_generator->unmap_frame(frame->mesh_generator_frame[view]);
        }

        this->encoders[layer]->unmap_frame(frame->encoder_frame);

        frame->mesh_generator_complete.fill(false);
        frame->encoder_complete = false;

        this->empty_frames[layer].push_back(frame);
    }
}

void Session::set_layer_depth_base_threshold(float depth_base_threshold)
{
    this->layer_depth_base_threshold = depth_base_threshold;
}

void Session::set_layer_depth_slope_threshold(float depth_slope_threshold)
{
    this->layer_depth_slope_threshold = depth_slope_threshold;
}

void Session::set_layer_use_object_ids(bool use_object_ids)
{
    this->layer_use_object_ids = use_object_ids;
}

void Session::set_mesh_settings(const shared::MeshSettings& settings)
{
    this->mesh_generator->apply(settings);
}

void Session::set_encoder_mode(EncoderMode mode)
{
    for (Encoder* encoder : this->encoders)
    {
        encoder->set_mode(mode);
    }
}

void Session::set_encoder_frame_rate(uint32_t frame_rate)
{
    for (Encoder* encoder : this->encoders)
    {
        encoder->set_frame_rate(frame_rate);
    }
}

void Session::set_encoder_bitrate(double bitrate)
{
    for (Encoder* encoder : this->encoders)
    {
        encoder->set_bitrate(bitrate);
    }
}

void Session::set_encoder_quality(double quality)
{
    for (Encoder* encoder : this->encoders)
    {
        encoder->set_quality(quality);
    }
}

bool Session::create_shaders()
{
    ShaderDefines defines;
    defines.set_define_from_file("#include \"shared_defines.glsl\"", SHADER_DIRECTORY"shared_defines.glsl");
    defines.set_define_from_file("#include \"shared_math_library.glsl\"", SHADER_DIRECTORY"shared_math_library.glsl");
    defines.set_define_from_file("#include \"shared_indirect_library.glsl\"", SHADER_DIRECTORY"shared_indirect_library.glsl");
    defines.set_define_from_file("#include \"shared_light_library.glsl\"", SHADER_DIRECTORY"shared_light_library.glsl");

    if (!this->layer_shader.load_shader(SHADER_DIRECTORY"session_layer_shader.vert", SHADER_TYPE_VERTEX, defines))
    {
        return false;
    }

    if (!this->layer_shader.load_shader(SHADER_DIRECTORY"session_layer_shader.geom", SHADER_TYPE_GEOMETRY, defines))
    {
        return false;
    }

    if (!this->layer_shader.load_shader(SHADER_DIRECTORY"session_layer_shader.frag", SHADER_TYPE_FRAGMENT, defines))
    {
        return false;
    }

    if (!this->layer_shader.link_program())
    {
        return false;
    }

    return true;
}

bool Session::create_frames(const glm::uvec2& resolution, uint32_t layer_count)
{
    this->empty_frames.resize(layer_count);
    this->active_frames.resize(layer_count);

    for (uint32_t layer = 0; layer < layer_count; layer++)
    {
        for (uint32_t index = 0; index < SESSION_FRAME_COUNT; index++)
        {
            Frame* frame = new Frame;
            frame->layer_index = layer;

            frame->encoder_frame = this->encoders[layer]->create_frame();

            if (frame->encoder_frame == nullptr)
            {
                return false;
            }

            glGenTextures(1, &frame->color_view_buffer);
            glBindTexture(GL_TEXTURE_2D, frame->color_view_buffer);

            glTexStorage2D(GL_TEXTURE_2D, 1, GL_SRGB8_ALPHA8, resolution.x, resolution.y);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            glBindTexture(GL_TEXTURE_2D, 0);

            for (uint32_t view = 0; view < CAMERA_VIEW_COUNT; view++)
            {
                if (!frame->layer_timer[view].create())
                {
                    return false;
                }

                frame->mesh_generator_frame[view] = this->mesh_generator->create_frame();

                if (frame->mesh_generator_frame[view] == nullptr)
                {
                    return false;
                }

                glGenFramebuffers(1, &frame->frame_buffers[view]);
                glBindFramebuffer(GL_FRAMEBUFFER, frame->frame_buffers[view]);

                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, frame->mesh_generator_frame[view]->get_depth_buffer(), 0);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, frame->mesh_generator_frame[view]->get_normal_buffer(), 0);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, frame->mesh_generator_frame[view]->get_object_id_buffer(), 0);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, frame->color_view_buffer, 0);

                std::vector<GLenum> draw_buffers =
                {
                    GL_COLOR_ATTACHMENT0,
                    GL_COLOR_ATTACHMENT1,
                    GL_COLOR_ATTACHMENT2
                };

                glDrawBuffers(draw_buffers.size(), draw_buffers.data());

                GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

                if (status != GL_FRAMEBUFFER_COMPLETE)
                {
                    return false;
                }

                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }

            this->empty_frames[layer].push_back(frame);
        }
    }

    return true;
}

void Session::destroy_frames()
{
    for (uint32_t layer = 0; layer < this->layer_count; layer++)
    {
        for (Frame* frame : this->empty_frames[layer])
        {
            for (uint32_t view = 0; view < CAMERA_VIEW_COUNT; view++)
            {
                frame->layer_timer[view].destroy();

                glDeleteFramebuffers(1, &frame->frame_buffers[view]);

                this->mesh_generator->destroy_frame(frame->mesh_generator_frame[view]);
            }

            glDeleteTextures(1, &frame->color_view_buffer);

            this->encoders[layer]->destroy_frame(frame->encoder_frame);

            delete frame;
        }

        for (Frame* frame : this->active_frames[layer])
        {
            for (uint32_t view = 0; view < CAMERA_VIEW_COUNT; view++)
            {
                frame->layer_timer[view].destroy();

                glDeleteFramebuffers(1, &frame->frame_buffers[view]);

                this->mesh_generator->destroy_frame(frame->mesh_generator_frame[view]);
            }

            glDeleteTextures(1, &frame->color_view_buffer);

            this->encoders[layer]->destroy_frame(frame->encoder_frame);

            delete frame;
        }

        this->empty_frames[layer].clear();
        this->active_frames[layer].clear();
    }

    this->empty_frames.clear();
    this->active_frames.clear();
}
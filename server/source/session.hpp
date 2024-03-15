#ifndef HEADER_SESSION
#define HEADER_SESSION

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>
#include <array>
#include <cstdint>
#include <types.hpp>

#include "mesh_generator/mesh_generator.hpp"
#include "server.hpp"
#include "camera.hpp"
#include "scene.hpp"
#include "worker.hpp"
#include "encoder.hpp"
#include "shader.hpp"
#include "timer.hpp"

#define SESSION_FRAME_COUNT 8

struct Frame
{
    std::array<GLuint, CAMERA_VIEW_COUNT> frame_buffers;
    GLuint color_view_buffer = 0;
    
    std::array<MeshGeneratorFrame*, CAMERA_VIEW_COUNT> mesh_generator_frame;
    EncoderFrame* encoder_frame = nullptr;

    std::array<bool, CAMERA_VIEW_COUNT> mesh_generator_complete;
    bool encoder_complete = false;

    std::array<Timer, CAMERA_VIEW_COUNT> layer_timer;
    std::array<double, CAMERA_VIEW_COUNT> time_layer;

    std::array<glm::mat4, CAMERA_VIEW_COUNT> view_matrix;
    uint32_t request_id = 0;
    uint32_t layer_index = 0;
};

class Session
{
private:
    Shader layer_shader = { "Session Layer Shader" };
    
    WorkerPool worker_pool;
    MeshGenerator* mesh_generator = nullptr;
    EncoderContext encoder_context;
    std::vector<Encoder*> encoders;

    std::vector<std::vector<Frame*>> empty_frames;
    std::vector<std::vector<Frame*>> active_frames;

    glm::uvec2 resolution = glm::uvec2(0);
    uint32_t layer_count = 0;

    float layer_depth_base_threshold = 0.5f;
    float layer_depth_slope_threshold = 0.0f;
    bool layer_use_object_ids = false;

public:
    Session() = default;

    bool create(Server* server, MeshGeneratorType mesh_generator_type, EncoderCodec codec, const glm::uvec2& resolution, uint32_t layer_count, bool chroma_subsampling);
    void destroy();

    bool render_frame(const Camera& camera, const Scene& scene, uint32_t request_id);
    void check_frames();

    void set_layer_depth_base_threshold(float depth_base_threshold);
    void set_layer_depth_slope_threshold(float depth_slope_threshold);
    void set_layer_use_object_ids(bool use_object_ids);

    void set_mesh_settings(const shared::MeshSettings& settings);

    void set_encoder_mode(EncoderMode mode);
    void set_encoder_frame_rate(uint32_t frame_rate);
    void set_encoder_bitrate(double bitrate);
    void set_encoder_quality(double quality);

private:
    bool create_shaders();
    bool create_frames(const glm::uvec2& resolution, uint32_t layer_count);
    void destroy_frames();
};

#endif
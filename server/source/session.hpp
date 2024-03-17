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

struct ExportRequest
{
    std::optional<std::string> color_file_name;
    std::optional<std::string> depth_file_name;
    std::optional<std::string> mesh_file_name;
    std::optional<std::string> feature_lines_file_name;
};

struct Frame
{
    std::array<GLuint, SHARED_VIEW_COUNT_MAX> frame_buffers;
    GLuint color_view_buffer = 0;
    
    std::array<GLuint, SHARED_VIEW_COUNT_MAX> color_export_buffers;
    std::array<GLuint, SHARED_VIEW_COUNT_MAX> depth_export_buffers;
    std::array<uint8_t*, SHARED_VIEW_COUNT_MAX> color_export_pointers;
    std::array<uint8_t*, SHARED_VIEW_COUNT_MAX> depth_export_pointers;
    ExportRequest export_request;

    std::array<MeshGeneratorFrame*, SHARED_VIEW_COUNT_MAX> mesh_generator_frame;
    EncoderFrame* encoder_frame = nullptr;

    std::array<bool, SHARED_VIEW_COUNT_MAX> mesh_generator_complete;
    bool encoder_complete = false;

    std::array<Timer, SHARED_VIEW_COUNT_MAX> layer_timer;
    std::array<double, SHARED_VIEW_COUNT_MAX> time_layer;

    std::array<glm::mat4, SHARED_VIEW_COUNT_MAX> view_matrix;
    glm::mat4 projection_matrix = glm::mat4(1.0f);

    glm::uvec2 resolution = glm::uvec2(0);
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
    uint32_t view_count = 0;
    bool export_enabled = false;

    float layer_depth_base_threshold = 0.5f;
    float layer_depth_slope_threshold = 0.0f;
    bool layer_use_object_ids = false;

public:
    Session() = default;

    bool create(Server* server, MeshGeneratorType mesh_generator_type, EncoderCodec codec, const glm::uvec2& resolution, uint32_t layer_count, uint32_t view_count, bool chroma_subsampling, bool export_enabled);
    void destroy();

    bool render_frame(const Camera& camera, const Scene& scene, uint32_t request_id, ExportRequest& export_request);
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
    bool create_frames(const glm::uvec2& resolution, uint32_t layer_count, uint32_t view_count, bool export_enabled);
    void destroy_frames();
    void destroy_frame(Frame* frame);
};

#endif
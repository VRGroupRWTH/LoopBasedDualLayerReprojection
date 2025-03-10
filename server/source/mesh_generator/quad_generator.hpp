#ifndef HEADER_QUAD_GENERATOR
#define HEADER_QUAD_GENERATOR

#include "mesh_generator.hpp"
#include "shader.hpp"
#include "timer.hpp"

namespace glsl
{
    using namespace glm;
    typedef uint32_t uint;
#include "../shaders/shared_defines.glsl"
}

class QuadGeneratorFrame : public MeshGeneratorFrame
{
public:
    GLuint depth_buffer = 0;
    GLuint normal_buffer = 0;
    GLuint object_id_buffer = 0;

    GLuint count_buffer = 0;
    GLuint vertex_buffer = 0;
    GLuint index_buffer = 0;

    const glsl::QuadCount* count_pointer = nullptr;
    const glsl::QuadVertex* vertex_pointer = nullptr;
    const uint32_t* index_pointer = nullptr;

    GLsync fence = 0;

    Timer copy_timer;
    Timer delta_timer;
    Timer refine_timer;
    Timer corner_timer;
    Timer write_timer;

    //Time measurements in millisconds
    double time_copy = 0.0;
    double time_delta = 0.0;
    double time_refine = 0.0;
    double time_corner = 0.0;
    double time_write = 0.0;

public:
    QuadGeneratorFrame() = default;

    bool triangulate(std::vector<shared::Vertex>& vertices, std::vector<shared::Index>& indices, shared::ViewMetadata& metadata, std::vector<MeshFeatureLine>& feature_lines, bool export_feature_lines);

    GLuint get_depth_buffer() const;
    GLuint get_normal_buffer() const;
    GLuint get_object_id_buffer() const;
};

class QuadGenerator : public MeshGenerator
{
private:
    Shader copy_shader = { "Quad Copy Shader" };
    Shader delta_shader = { "Quad Delta Shader" };
    Shader refine_shader = { "Quad Refine Shader" };
    Shader corner_shader = { "Quad Corner Shader" };
    Shader write_shader = { "Quad Write Shader" };
    
    std::array<GLuint, 2> refine_indirect_buffers = { 0, 0 };
    std::array<GLuint, 2> refine_buffers = { 0, 0 };
    GLuint quad_indirect_buffer = 0;
    GLuint quad_buffer = 0;
    GLuint setup_buffer = 0;
    GLuint delta_buffer = 0;
    GLuint corner_buffer = 0;
    
    glm::uvec2 resolution = glm::uvec2(0);
    uint32_t delta_buffer_levels = 0;
    uint32_t setup_buffer_count = 0;
    float depth_max = 0.995f;
    float depth_threshold = 0.001f;
    
public:
    QuadGenerator() = default;

    bool create(const glm::uvec2& resolution);
    void destroy();
    void apply(const shared::MeshSettings& settings);

    MeshGeneratorFrame* create_frame();
    void destroy_frame(MeshGeneratorFrame* frame);
    bool submit_frame(MeshGeneratorFrame* frame);
    bool map_frame(MeshGeneratorFrame* frame);
    bool unmap_frame(MeshGeneratorFrame* frame);

private:
    bool create_buffers(const glm::uvec2& resolution);
    bool create_shaders();
    void destroy_buffers();

    void perform_copy_pass(QuadGeneratorFrame* quad_frame);
    void perform_delta_pass(QuadGeneratorFrame* quad_frame);
    void perform_refine_pass(QuadGeneratorFrame* quad_frame);
    void perform_corner_pass(QuadGeneratorFrame* quad_frame);
    void perform_write_pass(QuadGeneratorFrame* quad_frame);
};

#endif
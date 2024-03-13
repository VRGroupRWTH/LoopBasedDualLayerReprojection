#ifndef HEADER_LINE_GENERATOR
#define HEADER_LINE_GENERATOR

#include <vector>

#include "mesh_generator.hpp"
#include "line_triangulation.hpp"
#include "shader.hpp"
#include "timer.hpp"

namespace glsl
{
    using namespace glm;
    typedef uint32_t uint;
#include "../shaders/shared_defines.glsl"
}

class LineQuadTreeLevel
{
private:
    glm::uvec2 resolution = glm::uvec2(0);
    GLuint level_buffer = 0;
    uint8_t* level_pointer = nullptr;

public:
    LineQuadTreeLevel() = default;
    
    bool create(const glm::uvec2& resolution);
    void destroy();

    bool set_pixel(const glm::ivec2& coord, uint8_t value);
    bool get_pixel(const glm::ivec2& coord, uint8_t& value) const;

    const glm::uvec2& get_resolution() const;
    GLuint get_level_buffer() const;
};

class LineQuadTree
{
private:
    std::vector<LineQuadTreeLevel> levels;

public:
    LineQuadTree() = default;

    bool create(const glm::uvec2& resolution);
    void destroy();

    bool fill(GLuint buffer);
    bool remove(const glm::ivec2& coord);

    bool find_global(glm::ivec2& coord) const;
    bool find_local(const glm::ivec2& center_coord, std::vector<glm::ivec2>& coords) const;
    bool find_local_max(const glm::ivec2& center_coord, glm::ivec2& coord) const;

    bool get_pixel(const glm::ivec2& coord, uint8_t& value) const;

    uint32_t get_level_count() const;
};

class LineGeneratorFrame : public MeshGeneratorFrame
{
public:
    LineQuadTree quad_tree;
    LineTriangulation triangulation;

    glm::uvec2 resolution;
    float depth_max = 0.995f;
    uint32_t line_length_min = 10;

    GLuint depth_buffer = 0;
    GLuint normal_buffer = 0;
    GLuint object_id_buffer = 0;

    GLuint depth_copy_buffer = 0;
    const float* depth_copy_pointer = nullptr;

    GLsync fence = 0;

    //Time measurements in millisconds
    Timer edge_timer;
    Timer quad_tree_timer;

    double time_edge = 0.0;
    double time_quad_tree = 0.0;

public:
    LineGeneratorFrame() = default;

    bool triangulate(std::vector<i3ds::Vertex>& vertices, std::vector<i3ds::Index>& indices, i3ds::ViewStatistic& statistic);

    GLuint get_depth_buffer() const;
    GLuint get_normal_buffer() const;
    GLuint get_object_id_buffer() const;
};

class LineGenerator : public MeshGenerator
{
private:
    Shader edge_shader = { "Line Generator Edge Shader" };
    Shader quad_tree_shader = { "Line Generator Quad Tree Shader" };

    GLuint edge_buffer = 0;
    uint32_t edge_buffer_levels = 0;

    glm::uvec2 resolution = glm::uvec2(0);
    float depth_max = 0.995f;
    float laplace_threshold = 0.005f;
    float normal_scale = glm::pi<float>() * 0.25f;
    uint32_t line_length_min = 10;

public:
    LineGenerator() = default;

    bool create(const glm::uvec2& resolution);
    void destroy();
    void apply(const i3ds::MeshSettings& settings);

    MeshGeneratorFrame* create_frame();
    void destroy_frame(MeshGeneratorFrame* frame);
    bool submit_frame(MeshGeneratorFrame* frame);
    bool map_frame(MeshGeneratorFrame* frame);
    bool unmap_frame(MeshGeneratorFrame* frame);

private:
    bool create_buffers(const glm::uvec2& resolution);
    bool create_shaders();

    void perform_edge_pass(LineGeneratorFrame* line_frame);
    void perform_quad_tree_pass(LineGeneratorFrame* line_frame);
};

#endif
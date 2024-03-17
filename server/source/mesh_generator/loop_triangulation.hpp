#ifndef HEADER_LOOP_TRIANGULATION
#define HEADER_LOOP_TRIANGULATION

#include <glm/glm.hpp>
#include <vector>
#include <array>
#include <cstdint>
#include <types.hpp>

#include "mesh_generator.hpp"

namespace glsl
{
    using namespace glm;
    typedef uint32_t uint;
#include "../shaders/shared_defines.glsl"
}

struct LoopPoint
{
    glm::u16vec2 point;
    float depth = 0.0f;

    uint32_t vertex_index = 0;
    uint32_t previous_segment = 0;
    uint32_t next_segment = 0;
};

struct LoopPointHandle
{
    uint16_t loop_index = 0;
    uint16_t point_index = 0;
};

enum ContourSide
{
    CONTOUR_SIDE_LEFT,
    CONTOUR_SIDE_RIGHT
};

struct ContourPoint
{
    ContourSide side;
    uint32_t side_index = 0;

    LoopPoint point;
    LoopPoint next;
    LoopPoint previous;
};

struct Contour
{
    std::vector<LoopPoint> left;
    std::vector<LoopPoint> right;
};

struct Interval
{
    glm::u16vec2 left = glm::u16vec2(0);
    glm::u16vec2 right = glm::u16vec2(0);

    uint32_t left_loop_index = 0;
    uint32_t left_segment_index = 0;
    uint32_t left_base_point_index = 0;
    uint32_t left_next_point_index = 0;

    uint32_t right_loop_index = 0;
    uint32_t right_segment_index = 0;
    uint32_t right_base_point_index = 0;
    uint32_t right_next_point_index = 0;

    uint32_t last_loop_index = 0;
    uint32_t last_point_index = 0;
    bool last_is_merge = false;

    Contour* left_contour = nullptr;
    Contour* right_contour = nullptr;
};

class LoopTriangulation
{
private:
    std::vector<Interval> intervals;

    std::vector<std::vector<LoopPoint>> loop_points;
    std::vector<std::vector<LoopPoint>> loop_point_cache;
    std::vector<LoopPointHandle> loop_point_handles;

    std::vector<Contour*> contours;
    std::vector<Contour*> contour_cache;
    std::vector<ContourPoint> contour_points;
    std::vector<ContourPoint> contour_reflex_chain;

    uint32_t vertex_counter = 0;

public:
    LoopTriangulation() = default;
    ~LoopTriangulation();

    void process(const glm::uvec2& resolution, float triangle_scale, const glsl::Loop* loop_pointer, const glsl::LoopCount* loop_count_pointer, const glsl::LoopSegment* loop_segment_pointer, std::vector<shared::Vertex>& vertices, std::vector<shared::Index>& indices, shared::ViewMetadata& metadata, std::vector<MeshFeatureLine>& feature_lines, bool export_feature_lines);

private:
    void compute_loop_points(uint32_t segment_count, const glsl::LoopSegment* segment_pointer, std::vector<LoopPoint>& points);
    static void compute_segment(const glm::ivec2& last_coord, const glm::ivec2& current_coord, glm::ivec2& segment_direction, uint32_t& segment_length);

    void compute_triangulation(const glm::uvec2& resolution, float triangle_scale, const glsl::Loop* loop_pointer, const glsl::LoopSegment* loop_segment_pointer, std::vector<shared::Vertex>& vertices, std::vector<shared::Index>& indices, shared::ViewMetadata& metadata);
    bool check_inside(const LoopPoint& point, const glsl::Loop* loop_pointer, const glsl::LoopSegment* loop_segment_pointer, uint32_t& interval_index);
    bool process_adjacent_two_intervals(const LoopPointHandle& point_handle, const LoopPoint& point);
    bool process_adjacent_one_interval(const LoopPointHandle& point_handle, const LoopPoint& point);
    void process_inside_interval(const LoopPointHandle& point_handle, const LoopPoint& point, uint32_t interval_index);
    void process_outside_interval(const LoopPointHandle& point_handle, const LoopPoint& point);

    void triangulate_contour(const Contour* contour, std::vector<shared::Index>& indices);
    static bool is_reflex(const LoopPoint& previous, const LoopPoint& current, const LoopPoint& next);

    void clear_state();
    std::vector<LoopPoint> allocate_points();
    Contour* allocate_contour();

    static uint32_t previous_point_index(uint32_t point_index, uint32_t loop_size);
    static uint32_t next_point_index(uint32_t point_index, uint32_t loop_size);
    static uint32_t offset_point_index(uint32_t point_index, uint32_t offset, uint32_t loop_size); //Where 0 <= offset <= loop_size
};

#endif
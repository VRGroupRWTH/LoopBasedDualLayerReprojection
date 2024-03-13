#ifndef HEADER_LINE_TRIANGULATION
#define HEADER_LINE_TRIANGULATION

#include <streaming_server.hpp>
#include <glm/glm.hpp>
#include <vector>

#define LINE_TRIANGULATION_BORDER_POINTS 10

class LineQuadTree;

struct PointSequence
{
    glm::ivec2 start_point;
    glm::ivec2 direction;
    uint32_t length;
};

struct LineSegment
{
    glm::ivec2 start;
    glm::ivec2 end;

    bool is_end = false;
};

class LineTriangulation
{
private:
    std::vector<glm::ivec2> line_coords;
    std::vector<LineSegment> line_segments;
    std::vector<PointSequence> point_sequences;

public:
    LineTriangulation() = default;

    void process(const glm::uvec2& resolution, float depth_max, uint32_t line_length_min, const float* depth_copy_pointer, LineQuadTree& quad_tree, std::vector<i3ds::Vertex>& vertices, std::vector<i3ds::Index>& indices, i3ds::ViewStatistic& statistic);

private:
    void compute_line_segments();
};

#endif
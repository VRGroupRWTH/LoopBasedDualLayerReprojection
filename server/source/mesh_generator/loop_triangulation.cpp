#include "loop_triangulation.hpp"

#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>

LoopTriangulation::~LoopTriangulation()
{
    for (Contour* contour : this->contours)
    {
        delete contour;
    }

    for (Contour* contour : this->contour_cache)
    {
        delete contour;
    }

    this->contours.clear();
    this->contour_cache.clear();
}

void LoopTriangulation::process(const glm::uvec2& resolution, float triangle_scale, const glsl::Loop* loop_pointer, const glsl::LoopCount* loop_count_pointer, const glsl::LoopSegment* loop_segment_pointer, std::vector<shared::Vertex>& vertices, std::vector<shared::Index>& indices, shared::ViewMetadata& metadata, std::vector<MeshFeatureLine>& feature_lines, bool export_feature_lines)
{
    this->clear_state();

    uint32_t loop_count = loop_count_pointer->loop_counter;
    uint32_t segment_count = loop_count_pointer->segment_counter;

    if (loop_count > LOOP_GENERATOR_MAX_LOOP_COUNT)
    {
        vertices.clear();
        indices.clear();

        spdlog::error("LoopTriangulation: Loop count exceeds buffer limit!");

        return;
    }

    if (segment_count > LOOP_GENERATOR_MAX_LOOP_SEGMENT_COUNT)
    {
        vertices.clear();
        indices.clear();

        spdlog::error("LoopTriangulation: Loop segement count exceeds buffer limit!");

        return;
    }

    metadata.loop.loop_count = loop_count;
    metadata.loop.segment_count = segment_count;

    for (uint32_t loop_index = 0; loop_index < loop_count; loop_index++)
    {
        const glsl::Loop* loop = loop_pointer + loop_index;
        const glsl::LoopSegment* segment_pointer = loop_segment_pointer + loop->segment_offset;
        uint32_t segment_count = loop->segment_count;

        std::vector<LoopPoint> points = this->allocate_points();

        std::chrono::high_resolution_clock::time_point loop_segments_start = std::chrono::high_resolution_clock::now();
        this->compute_loop_points(segment_count, segment_pointer, points);
        std::chrono::high_resolution_clock::time_point loop_segments_end = std::chrono::high_resolution_clock::now();
        metadata.loop.time_loop_points += std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(loop_segments_end - loop_segments_start).count();

        metadata.loop.point_count += points.size();

        this->loop_points.push_back(std::move(points));
    }

    if (export_feature_lines)
    {
        feature_lines.clear();
        
        for (uint32_t loop_index = 0; loop_index < loop_count; loop_index++)
        {
            const std::vector<LoopPoint>& points = this->loop_points[loop_index];

            for (uint32_t point_index = 0; point_index < points.size(); point_index++)
            {
                const LoopPoint& line_start = points[point_index];
                const LoopPoint& line_end = points[(point_index + 1) % points.size()];

                MeshFeatureLine feature_line;
                feature_line.start = line_start.point;
                feature_line.end = line_end.point;
                feature_line.id = loop_index;

                feature_lines.push_back(feature_line);
            }
        }
    }

    std::chrono::high_resolution_clock::time_point triangulation_start = std::chrono::high_resolution_clock::now();
    this->compute_triangulation(resolution, triangle_scale, loop_pointer, loop_segment_pointer, vertices, indices, metadata);
    std::chrono::high_resolution_clock::time_point triangulation_end = std::chrono::high_resolution_clock::now();
    metadata.loop.time_triangulation = std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(triangulation_end - triangulation_start).count();
}

void LoopTriangulation::compute_loop_points(uint32_t segment_count, const glsl::LoopSegment* segment_pointer, std::vector<LoopPoint>& points)
{
    //Related to Inverse Bersenham Algorithm introduced in "Pseudo-Immersive Real-Time Display of 3D Scenes on Mobile Devices" by "Ming Li, Arne Schmitz, Leif Kobbelt"

    glm::ivec2 start_point = segment_pointer->end_coord;
    uint32_t start_offset = 0;

    for (uint32_t index = 1; index < segment_count; index++)
    {
        const glsl::LoopSegment& segment = segment_pointer[index];

        if (segment.end_coord.y < start_point.y || (segment.end_coord.y == start_point.y && segment.end_coord.x < start_point.x))
        {
            start_point = segment.end_coord;
            start_offset = index;
        }
    }

    if (segment_count <= 4)
    {
        for (uint32_t index = 0; index < segment_count; index++)
        {
            const glsl::LoopSegment& segment = segment_pointer[(index + start_offset) % segment_count];

            LoopPoint point;
            point.point = segment.end_coord;
            point.depth = segment.end_coord_depth;
            point.previous_segment = (index + start_offset) % segment_count;
            point.next_segment = (index + 1 + start_offset) % segment_count;
            point.vertex_index = this->vertex_counter;

            points.push_back(point);
            this->vertex_counter++;
        }

        return;
    }

    glm::ivec2 last_coord = segment_pointer[segment_count - 1].end_coord;

    for (uint32_t index = 0; index < segment_count;)
    {
        const glsl::LoopSegment& current_segment = segment_pointer[(index + start_offset) % segment_count];
        glm::ivec2 current_direction = glm::ivec2(0);
        uint32_t current_length = 0;

        LoopTriangulation::compute_segment(last_coord, current_segment.end_coord, current_direction, current_length);

        last_coord = current_segment.end_coord;
        index++;

        if (current_length > 2 || index >= segment_count)
        {
            LoopPoint point;
            point.point = current_segment.end_coord;
            point.depth = current_segment.end_coord_depth;
            point.previous_segment = (index + start_offset + segment_count - 1) % segment_count;
            point.next_segment = (index + start_offset) % segment_count;
            point.vertex_index = this->vertex_counter;

            points.push_back(point);
            this->vertex_counter++;
        }

        else
        {
            const glsl::LoopSegment& next_segment = segment_pointer[(index + start_offset) % segment_count];
            glm::ivec2 next_direction = glm::ivec2(0);
            uint32_t next_length = 0;

            LoopTriangulation::compute_segment(last_coord, next_segment.end_coord, next_direction, next_length);

            last_coord = next_segment.end_coord;
            index++;

            glm::u16vec2 line_end_coord = next_segment.end_coord;
            float line_end_coord_depth = next_segment.end_coord_depth;
            bool line_depth_step = next_segment.end_coord_depth < 0.0f;
            float line_slope = next_length;

            while (index < segment_count)
            {
                const glsl::LoopSegment& slope_segment = segment_pointer[(index + start_offset) % segment_count];
                glm::ivec2 slope_direction = glm::ivec2(0);
                uint32_t slope_length = 0;

                LoopTriangulation::compute_segment(last_coord, slope_segment.end_coord, slope_direction, slope_length);

                if (glm::all(glm::equal(slope_direction, current_direction)))
                {
                    if (slope_length > 2)
                    {
                        break;
                    }

                    line_end_coord = slope_segment.end_coord;
                    line_end_coord_depth = slope_segment.end_coord_depth;
                    line_depth_step = line_depth_step || (slope_segment.end_coord_depth < 0.0f);

                    last_coord = slope_segment.end_coord;
                    index++;
                }

                else if (glm::all(glm::equal(slope_direction, next_direction)))
                {
                    if (std::abs(line_slope - slope_length) > 2.0f)
                    {
                        break;
                    }

                    line_slope = (line_slope + slope_length) / 2.0f;
                    line_end_coord = slope_segment.end_coord;
                    line_end_coord_depth = slope_segment.end_coord_depth;
                    line_depth_step = line_depth_step || (slope_segment.end_coord_depth < 0.0f);

                    last_coord = slope_segment.end_coord;
                    index++;
                }

                else
                {
                    break;
                }
            }

            if (line_depth_step)
            {
                line_end_coord_depth = -glm::abs(line_end_coord_depth);
            }

            LoopPoint point;
            point.point = line_end_coord;
            point.depth = line_end_coord_depth;
            point.previous_segment = (index + start_offset + segment_count - 1) % segment_count;
            point.next_segment = (index + start_offset) % segment_count;
            point.vertex_index = this->vertex_counter;

            points.push_back(point);
            this->vertex_counter++;
        }
    }
}

void LoopTriangulation::compute_segment(const glm::ivec2& last_coord, const glm::ivec2& current_coord, glm::ivec2& segment_direction, uint32_t& segment_length)
{
    glm::ivec2 direction = current_coord - last_coord;

    segment_direction = glm::sign(direction);
    segment_length = glm::max(glm::abs(direction.x), glm::abs(direction.y));
}

void LoopTriangulation::compute_triangulation(const glm::uvec2& resolution, float triangle_scale, const glsl::Loop* loop_pointer, const glsl::LoopSegment* loop_segment_pointer, std::vector<shared::Vertex>& vertices, std::vector<shared::Index>& indices, shared::ViewMetadata& metadata)
{
    //Related to "Real-time Image Vectorization on GPU" by "Xiaoliang Xiong, Jie Feng and Bingfeng Zhou"
    //Related to "CMSC 754: Lecture 5 Polygon Triangulation" by "Dave Mount"

    std::chrono::high_resolution_clock::time_point loop_info_start = std::chrono::high_resolution_clock::now();
    for (uint32_t loop_index = 0; loop_index < this->loop_points.size(); loop_index++)
    {
        const std::vector<LoopPoint>& points = this->loop_points[loop_index];

        for (uint32_t point_index = 0; point_index < points.size(); point_index++)
        {
            LoopPointHandle point_handle;
            point_handle.loop_index = loop_index;
            point_handle.point_index = point_index;

            this->loop_point_handles.push_back(point_handle);
        }
    }
    std::chrono::high_resolution_clock::time_point loop_info_end = std::chrono::high_resolution_clock::now();
    metadata.loop.time_loop_info = std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(loop_info_end - loop_info_start).count();

    std::chrono::high_resolution_clock::time_point loop_sort_start = std::chrono::high_resolution_clock::now();
    std::sort(this->loop_point_handles.begin(), this->loop_point_handles.end(), [&](const LoopPointHandle& point_handle1, const LoopPointHandle& point_handle2)
    {
        const LoopPoint& point1 = this->loop_points[point_handle1.loop_index][point_handle1.point_index];
        const LoopPoint& point2 = this->loop_points[point_handle2.loop_index][point_handle2.point_index];

        if (point1.point.y == point2.point.y)
        {
            return point1.point.x < point2.point.x;
        }

        return point1.point.y < point2.point.y;
    });
    std::chrono::high_resolution_clock::time_point loop_sort_end = std::chrono::high_resolution_clock::now();
    metadata.loop.time_loop_sort = std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(loop_sort_end - loop_sort_start).count();

    std::chrono::high_resolution_clock::time_point sweep_line_start = std::chrono::high_resolution_clock::now();
    for (const LoopPointHandle& point_handle : this->loop_point_handles)
    {
        const LoopPoint& point = this->loop_points[point_handle.loop_index][point_handle.point_index];

        std::chrono::high_resolution_clock::time_point adjacent_two_start = std::chrono::high_resolution_clock::now();
        if (this->process_adjacent_two_intervals(point_handle, point))
        {
            continue;
        }
        std::chrono::high_resolution_clock::time_point adjacent_two_end = std::chrono::high_resolution_clock::now();
        metadata.loop.time_adjacent_two += std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(adjacent_two_end - adjacent_two_start).count();

        std::chrono::high_resolution_clock::time_point adjacent_one_start = std::chrono::high_resolution_clock::now();
        if (this->process_adjacent_one_interval(point_handle, point))
        {
            continue;
        }
        std::chrono::high_resolution_clock::time_point adjacent_one_end = std::chrono::high_resolution_clock::now();
        metadata.loop.time_adjacent_one += std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(adjacent_one_end - adjacent_one_start).count();

        std::chrono::high_resolution_clock::time_point interval_update_start = std::chrono::high_resolution_clock::now();
        std::chrono::high_resolution_clock::time_point interval_update_end;
        uint32_t interval_index = 0;

        if (this->check_inside(point, loop_pointer, loop_segment_pointer, interval_index))
        {
            interval_update_end = std::chrono::high_resolution_clock::now();

            std::chrono::high_resolution_clock::time_point inside_outside_start = std::chrono::high_resolution_clock::now();
            this->process_inside_interval(point_handle, point, interval_index);
            std::chrono::high_resolution_clock::time_point inside_outside_end = std::chrono::high_resolution_clock::now();
            metadata.loop.time_inside_outside += std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(inside_outside_end - inside_outside_start).count();
        }

        else
        {
            interval_update_end = std::chrono::high_resolution_clock::now();

            std::chrono::high_resolution_clock::time_point inside_outside_start = std::chrono::high_resolution_clock::now();
            this->process_outside_interval(point_handle, point);
            std::chrono::high_resolution_clock::time_point inside_outside_end = std::chrono::high_resolution_clock::now();
            metadata.loop.time_inside_outside += std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(inside_outside_end - inside_outside_start).count();
        }

        metadata.loop.time_interval_update += std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(interval_update_end - interval_update_start).count();
    }
    std::chrono::high_resolution_clock::time_point sweep_line_end = std::chrono::high_resolution_clock::now();
    metadata.loop.time_sweep_line = std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(sweep_line_end - sweep_line_start).count();

    indices.clear();
    vertices.clear();
    vertices.reserve(this->vertex_counter);

    if (triangle_scale != 0.0f)
    {
        for (uint32_t loop_index = 0; loop_index < this->loop_points.size(); loop_index++)
        {
            const std::vector<LoopPoint>& points = this->loop_points[loop_index];

            for (uint32_t current_index = 0; current_index < points.size(); current_index++)
            {
                uint32_t previous_index = LoopTriangulation::previous_point_index(current_index, points.size());
                uint32_t next_index = LoopTriangulation::next_point_index(current_index, points.size());

                const LoopPoint& previous_point = points[previous_index];
                const LoopPoint& current_point = points[current_index];
                const LoopPoint& next_point = points[next_index];

                glm::vec2 offset = glm::vec2(0.0f);

                if (previous_point.depth > 0.0f && current_point.depth > 0.0f && next_point.depth > 0.0f) //Not a depth discontinuity. Adjust position to close gaps
                {
                    glm::vec2 direction1 = glm::vec2(previous_point.point) - glm::vec2(current_point.point);
                    glm::vec2 direction2 = glm::vec2(next_point.point) - glm::vec2(current_point.point);

                    float angle1 = glm::atan(direction1.y, direction1.x);
                    float angle2 = glm::atan(direction2.y, direction2.x);
                    float center_angle = 0.0f;

                    if (angle1 < angle2)
                    {
                        center_angle = (angle1 + angle2) / 2.0f;
                    }

                    else
                    {
                        center_angle = angle1 + ((glm::two_pi<float>() - angle1) + angle2) / 2.0f;
                    }

                    offset.x = triangle_scale * glm::cos(center_angle);
                    offset.y = triangle_scale * glm::sin(center_angle);
                }

                glm::vec2 position = glm::vec2((current_point.point + glm::u16vec2(1)) / glm::u16vec2(2)) + offset;
                position = glm::clamp(position, glm::vec2(0.0f), glm::vec2(resolution));

                shared::Vertex vertex;
                vertex.x = (uint16_t)position.x;
                vertex.y = (uint16_t)position.y;
                vertex.z = glm::abs(current_point.depth);

                vertices.push_back(vertex);
            }
        }
    }

    else
    {
        for (uint32_t loop_index = 0; loop_index < this->loop_points.size(); loop_index++)
        {
            const std::vector<LoopPoint>& points = this->loop_points[loop_index];

            for (const LoopPoint& point : points)
            {
                shared::Vertex vertex;
                vertex.x = (point.point.x + 1) / 2;
                vertex.y = (point.point.y + 1) / 2;
                vertex.z = glm::abs(point.depth);

                vertices.push_back(vertex);
            }
        }
    }

    std::chrono::high_resolution_clock::time_point contour_start = std::chrono::high_resolution_clock::now();
    for (Contour* contour : this->contours)
    {
        triangulate_contour(contour, indices);
    }
    std::chrono::high_resolution_clock::time_point contour_end = std::chrono::high_resolution_clock::now();
    metadata.loop.time_contour = std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(contour_end - contour_start).count();
}

bool LoopTriangulation::check_inside(const LoopPoint& point, const glsl::Loop* loop_pointer, const glsl::LoopSegment* loop_segment_pointer, uint32_t& interval_index)
{
    for (uint32_t index = 0; index < this->intervals.size(); index++)
    {
        Interval& interval = this->intervals[index];

        const glsl::Loop* left_loop = loop_pointer + interval.left_loop_index;
        const glsl::LoopSegment* left_segments = loop_segment_pointer + left_loop->segment_offset;
        uint32_t left_segment_count = left_loop->segment_count;

        const glsl::Loop* right_loop = loop_pointer + interval.right_loop_index;
        const glsl::LoopSegment* right_segments = loop_segment_pointer + right_loop->segment_offset;
        uint32_t right_segment_count = right_loop->segment_count;

        while (interval.left.y != point.point.y)
        {
            const glsl::LoopSegment& segment = left_segments[interval.left_segment_index];

            if (segment.end_coord.y > point.point.y)
            {
                break;
            }

            interval.left = segment.end_coord;
            interval.left_segment_index = LoopTriangulation::previous_point_index(interval.left_segment_index, left_segment_count);
        }

        while (interval.right.y != point.point.y)
        {
            const glsl::LoopSegment& segment = right_segments[interval.right_segment_index];

            if (segment.end_coord.y > point.point.y)
            {
                break;
            }

            interval.right = segment.end_coord;
            interval.right_segment_index = LoopTriangulation::next_point_index(interval.right_segment_index, right_segment_count);
        }

        if (interval.left.x <= point.point.x && point.point.x <= interval.right.x)
        {
            interval_index = index;

            return true;
        }
    }

    return false;
}

bool LoopTriangulation::process_adjacent_two_intervals(const LoopPointHandle& point_handle, const LoopPoint& point)
{
    const Interval* left_interval = nullptr;
    const Interval* right_interval = nullptr;

    uint32_t left_index = 0;
    uint32_t right_index = 0;

    for (uint32_t index = 0; index < this->intervals.size(); index++)
    {
        const Interval& interval = this->intervals[index];

        if (interval.left_loop_index == point_handle.loop_index && interval.left_next_point_index == point_handle.point_index)
        {
            right_interval = &interval;
            right_index = index;
        }

        if (interval.right_loop_index == point_handle.loop_index && interval.right_next_point_index == point_handle.point_index)
        {
            left_interval = &interval;
            left_index = index;
        }

        if (left_interval != nullptr && right_interval != nullptr)
        {
            break;
        }
    }

    if (left_interval == nullptr || right_interval == nullptr)
    {
        return false;
    }

    if (left_interval == right_interval) //Is End
    {
        if (left_interval->last_is_merge)
        {
            left_interval->left_contour->right.push_back(point);
            left_interval->right_contour->left.push_back(point);

            this->contours.push_back(left_interval->left_contour);
            this->contours.push_back(left_interval->right_contour);
        }

        else
        {
            left_interval->left_contour->right.push_back(point);

            this->contours.push_back(left_interval->left_contour);
        }

        this->intervals.erase(intervals.begin() + left_index);
    }

    else
    {
        Interval interval;
        interval.left = left_interval->left;
        interval.left_loop_index = left_interval->left_loop_index;
        interval.left_segment_index = left_interval->left_segment_index;
        interval.left_base_point_index = left_interval->left_base_point_index;
        interval.left_next_point_index = left_interval->left_next_point_index;

        interval.right = right_interval->right;
        interval.right_loop_index = right_interval->right_loop_index;
        interval.right_segment_index = right_interval->right_segment_index;
        interval.right_base_point_index = right_interval->right_base_point_index;
        interval.right_next_point_index = right_interval->right_next_point_index;

        interval.last_loop_index = point_handle.loop_index;
        interval.last_point_index = point_handle.point_index;
        interval.last_is_merge = true;

        if (left_interval->last_is_merge)
        {
            interval.left_contour = left_interval->left_contour;
            left_interval->right_contour->right.push_back(point);

            this->contours.push_back(left_interval->right_contour);
        }

        else
        {
            interval.left_contour = left_interval->left_contour;
        }

        if (right_interval->last_is_merge)
        {
            interval.right_contour = right_interval->right_contour;
            right_interval->left_contour->right.push_back(point);

            this->contours.push_back(right_interval->left_contour);
        }

        else
        {
            interval.right_contour = right_interval->left_contour;
        }

        interval.left_contour->right.push_back(point);
        interval.right_contour->left.push_back(point);

        if (left_index < right_index)
        {
            this->intervals.erase(intervals.begin() + right_index);
            this->intervals.erase(intervals.begin() + left_index);
        }

        else
        {
            this->intervals.erase(intervals.begin() + left_index);
            this->intervals.erase(intervals.begin() + right_index);
        }

        this->intervals.push_back(interval);
    }

    return true;
}

bool LoopTriangulation::process_adjacent_one_interval(const LoopPointHandle& point_handle, const LoopPoint& point)
{
    Interval* interval = nullptr;

    for (Interval& loop_interval : this->intervals)
    {
        if (loop_interval.left_loop_index == point_handle.loop_index && loop_interval.left_next_point_index == point_handle.point_index)
        {
            interval = &loop_interval;
            break;
        }

        if (loop_interval.right_loop_index == point_handle.loop_index && loop_interval.right_next_point_index == point_handle.point_index)
        {
            interval = &loop_interval;
            break;
        }
    }

    if (interval == nullptr)
    {
        return false;
    }

    if (interval->left_loop_index == point_handle.loop_index && interval->left_next_point_index == point_handle.point_index)
    {
        if (interval->last_is_merge)
        {
            interval->left_contour->left.push_back(point);
            interval->right_contour->left.push_back(point);

            this->contours.push_back(interval->left_contour);
            interval->left_contour = interval->right_contour;
            interval->right_contour = nullptr;
        }

        else
        {
            interval->left_contour->left.push_back(point);
        }

        interval->left = point.point;
        interval->left_loop_index = point_handle.loop_index;
        interval->left_segment_index = point.previous_segment;
        interval->left_base_point_index = point_handle.point_index;
        interval->left_next_point_index = (point_handle.point_index + loop_points[point_handle.loop_index].size() - 1) % loop_points[point_handle.loop_index].size();
    }

    else
    {
        if (interval->last_is_merge)
        {
            interval->left_contour->right.push_back(point);
            interval->right_contour->right.push_back(point);

            this->contours.push_back(interval->right_contour);
            interval->right_contour = nullptr;
        }

        else
        {
            interval->left_contour->right.push_back(point);
        }

        interval->right = point.point;
        interval->right_loop_index = point_handle.loop_index;
        interval->right_segment_index = point.next_segment;
        interval->right_base_point_index = point_handle.point_index;
        interval->right_next_point_index = (point_handle.point_index + 1) % loop_points[point_handle.loop_index].size();
    }

    interval->last_loop_index = point_handle.loop_index;
    interval->last_point_index = point_handle.point_index;
    interval->last_is_merge = false;

    return true;
}

void LoopTriangulation::process_inside_interval(const LoopPointHandle& point_handle, const LoopPoint& point, uint32_t interval_index)
{
    const Interval& interval = this->intervals[interval_index];

    Interval left_interval;
    left_interval.left = interval.left;
    left_interval.left_loop_index = interval.left_loop_index;
    left_interval.left_segment_index = interval.left_segment_index;
    left_interval.left_base_point_index = interval.left_base_point_index;
    left_interval.left_next_point_index = interval.left_next_point_index;

    left_interval.right = point.point;
    left_interval.right_loop_index = point_handle.loop_index;
    left_interval.right_segment_index = point.next_segment;
    left_interval.right_base_point_index = point_handle.point_index;
    left_interval.right_next_point_index = (point_handle.point_index + 1) % loop_points[point_handle.loop_index].size();

    left_interval.last_loop_index = point_handle.loop_index;
    left_interval.last_point_index = point_handle.point_index;
    left_interval.last_is_merge = false;

    Interval right_interval;
    right_interval.left = point.point;
    right_interval.left_loop_index = point_handle.loop_index;
    right_interval.left_segment_index = point.previous_segment;
    right_interval.left_base_point_index = point_handle.point_index;
    right_interval.left_next_point_index = (point_handle.point_index + loop_points[point_handle.loop_index].size() - 1) % loop_points[point_handle.loop_index].size();

    right_interval.right = interval.right;
    right_interval.right_loop_index = interval.right_loop_index;
    right_interval.right_segment_index = interval.right_segment_index;
    right_interval.right_base_point_index = interval.right_base_point_index;
    right_interval.right_next_point_index = interval.right_next_point_index;

    right_interval.last_loop_index = point_handle.loop_index;
    right_interval.last_point_index = point_handle.point_index;
    right_interval.last_is_merge = false;

    if (interval.last_is_merge)
    {
        left_interval.left_contour = interval.left_contour;
        left_interval.left_contour->right.push_back(point);

        right_interval.left_contour = interval.right_contour;
        right_interval.left_contour->left.push_back(point);
    }

    else
    {
        const LoopPoint& last_point = loop_points[interval.last_loop_index][interval.last_point_index];

        if (interval.last_loop_index == interval.left_loop_index && interval.last_point_index == interval.left_base_point_index)
        {
            left_interval.left_contour = this->allocate_contour();
            left_interval.left_contour->left.push_back(last_point);
            left_interval.left_contour->right.push_back(point);

            right_interval.left_contour = interval.left_contour;
            right_interval.left_contour->left.push_back(point);
        }

        else
        {
            left_interval.left_contour = interval.left_contour;
            left_interval.left_contour->right.push_back(point);

            right_interval.left_contour = this->allocate_contour();
            right_interval.left_contour->right.push_back(last_point);
            right_interval.left_contour->left.push_back(point);
        }
    }

    this->intervals.erase(this->intervals.begin() + interval_index);
    this->intervals.push_back(left_interval);
    this->intervals.push_back(right_interval);
}

void LoopTriangulation::process_outside_interval(const LoopPointHandle& point_handle, const LoopPoint& point)
{
    Interval interval;
    interval.left = point.point;
    interval.left_loop_index = point_handle.loop_index;
    interval.left_segment_index = point.previous_segment;
    interval.left_base_point_index = point_handle.point_index;
    interval.left_next_point_index = (point_handle.point_index + loop_points[point_handle.loop_index].size() - 1) % loop_points[point_handle.loop_index].size();

    interval.right = point.point;
    interval.right_loop_index = point_handle.loop_index;
    interval.right_segment_index = point.next_segment;
    interval.right_base_point_index = point_handle.point_index;
    interval.right_next_point_index = (point_handle.point_index + 1) % loop_points[point_handle.loop_index].size();

    interval.last_loop_index = point_handle.loop_index;
    interval.last_point_index = point_handle.point_index;
    interval.last_is_merge = false;

    interval.left_contour = this->allocate_contour();
    interval.left_contour->left.push_back(point);

    this->intervals.push_back(interval);
}

void LoopTriangulation::triangulate_contour(const Contour* contour, std::vector<shared::Index>& indices)
{
    this->contour_points.clear();
    this->contour_reflex_chain.clear();

    for (int32_t index = contour->left.size() - 1; index >= 0; index--)
    {
        const LoopPoint& loop_point = contour->left[index];

        ContourPoint point;
        point.point = loop_point;
        point.side = CONTOUR_SIDE_LEFT;
        point.side_index = index;

        this->contour_points.push_back(point);
    }

    for (uint32_t index = 0; index < contour->right.size(); index++)
    {
        const LoopPoint& loop_point = contour->right[index];

        ContourPoint point;
        point.point = loop_point;
        point.side = CONTOUR_SIDE_RIGHT;
        point.side_index = index;

        this->contour_points.push_back(point);
    }

    for (uint32_t index = 0; index < this->contour_points.size(); index++)
    {
        uint32_t previous_index = LoopTriangulation::previous_point_index(index, this->contour_points.size());
        uint32_t next_index = LoopTriangulation::next_point_index(index, this->contour_points.size());

        ContourPoint& current = this->contour_points[index];
        const ContourPoint& previous = this->contour_points[previous_index];
        const ContourPoint& next = this->contour_points[next_index];

        current.previous = previous.point;
        current.next = next.point;
    }

    if (this->contour_points.size() < 3)
    {
        return;
    }

    std::sort(this->contour_points.begin(), this->contour_points.end(), [](const ContourPoint& point1, const ContourPoint& point2)
    {
        if (point1.point.point.y == point2.point.point.y)
        {
            if (point1.side == point2.side)
            {
                return point1.side_index < point2.side_index;
            }

            return point1.side < point2.side;
        }

        return point1.point.point.y < point2.point.point.y;
    });

    this->contour_reflex_chain.push_back(this->contour_points[0]);
    this->contour_reflex_chain.push_back(this->contour_points[1]);

    for (uint32_t index = 2; index < this->contour_points.size(); index++)
    {
        const ContourPoint& chain_end = this->contour_reflex_chain.back();
        ContourPoint& current = this->contour_points[index];

        if (current.side != this->contour_reflex_chain.back().side) //Case 1
        {
            for (uint32_t chain_index = 0; chain_index < this->contour_reflex_chain.size() - 1; chain_index++)
            {
                const ContourPoint& point1 = this->contour_reflex_chain[chain_index];
                const ContourPoint& point2 = this->contour_reflex_chain[chain_index + 1];

                indices.push_back(point1.point.vertex_index);
                indices.push_back(point2.point.vertex_index);
                indices.push_back(current.point.vertex_index);
            }

            current.next = this->contour_reflex_chain.back().point;
            this->contour_reflex_chain.back().previous = current.point;

            this->contour_reflex_chain.erase(this->contour_reflex_chain.begin(), this->contour_reflex_chain.end() - 1);
            this->contour_reflex_chain.push_back(current);
        }

        else if (!LoopTriangulation::is_reflex(chain_end.previous, chain_end.point, chain_end.next)) //Case 2a
        {
            while (this->contour_reflex_chain.size() > 1)
            {
                const ContourPoint& point1 = this->contour_reflex_chain[this->contour_reflex_chain.size() - 1];
                const ContourPoint& point2 = this->contour_reflex_chain[this->contour_reflex_chain.size() - 2];

                if (current.side == CONTOUR_SIDE_RIGHT)
                {
                    if (LoopTriangulation::is_reflex(point2.point, point1.point, current.point))
                    {
                        break;
                    }
                }

                else
                {
                    if (!LoopTriangulation::is_reflex(point2.point, point1.point, current.point))
                    {
                        break;
                    }
                }

                indices.push_back(point1.point.vertex_index);
                indices.push_back(point2.point.vertex_index);
                indices.push_back(current.point.vertex_index);

                this->contour_reflex_chain.pop_back();
            }

            current.previous = this->contour_reflex_chain.back().point;
            this->contour_reflex_chain.back().next = current.point;

            this->contour_reflex_chain.push_back(current);
        }

        else //Case 2b
        {
            this->contour_reflex_chain.push_back(current);
        }
    }
}

inline bool LoopTriangulation::is_reflex(const LoopPoint& previous, const LoopPoint& current, const LoopPoint& next)
{
    glm::i16vec2 direction1 = glm::i16vec2(current.point) - glm::i16vec2(previous.point);
    glm::i16vec2 direction2 = glm::i16vec2(next.point) - glm::i16vec2(current.point);
    glm::i16vec2 outside_direction = glm::i16vec2(direction1.y, -direction1.x);

    if (direction2.x * outside_direction.x + direction2.y * outside_direction.y > 0)
    {
        return true;
    }

    return false;
}

void LoopTriangulation::clear_state()
{
    for (std::vector<LoopPoint>& points : this->loop_points)
    {
        points.clear();

        this->loop_point_cache.push_back(std::move(points));
    }

    for (Contour* contour : this->contours)
    {
        contour->left.clear();
        contour->right.clear();

        this->contour_cache.push_back(contour);
    }

    this->vertex_counter = 0;

    this->loop_points.clear();
    this->loop_point_handles.clear();
    this->intervals.clear();
    this->contours.clear();
}

std::vector<LoopPoint> LoopTriangulation::allocate_points()
{
    if (this->loop_point_cache.empty())
    {
        return std::vector<LoopPoint>();
    }

    std::vector<LoopPoint> points = std::move(this->loop_point_cache.back());
    this->loop_point_cache.pop_back();

    return points;
}

Contour* LoopTriangulation::allocate_contour()
{
    if (this->contour_cache.empty())
    {
        return new Contour;
    }

    Contour* contour = this->contour_cache.back();
    this->contour_cache.pop_back();

    return contour;
}

inline uint32_t LoopTriangulation::previous_point_index(uint32_t point_index, uint32_t loop_size)
{
    if (point_index <= 0)
    {
        return loop_size - 1;
    }

    return point_index - 1;
}

inline uint32_t LoopTriangulation::next_point_index(uint32_t point_index, uint32_t loop_size)
{
    uint32_t next_index = point_index + 1;

    if (next_index >= loop_size)
    {
        return 0;
    }

    return next_index;
}

inline uint32_t LoopTriangulation::offset_point_index(uint32_t point_index, uint32_t offset, uint32_t loop_size) //Where 0 <= offset <= loop_size
{
    uint32_t next_index = point_index + offset;

    if (next_index >= loop_size)
    {
        return next_index - loop_size;
    }

    return next_index;
}
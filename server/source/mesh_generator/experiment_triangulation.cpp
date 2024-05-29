#include "experiment_triangulation.hpp"

#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>

#define ENABLE_SWEEP_LINE_PROFILE 1

namespace experiment
{
    inline void Contour::push_left(const LoopPoint& point)
    {
        ContourPoint& contour_point = this->left_points.emplace_back();
        contour_point.side = CONTOUR_SIDE_LEFT;
        contour_point.side_index = this->left_points.size() - 1;
        contour_point.vertex_index = point.vertex_index;

        contour_point.point = point.point;
        contour_point.is_edge = point.is_edge;
        contour_point.is_bridge = point.is_bridge;
    }

    inline void Contour::push_right(const LoopPoint& point)
    {
        ContourPoint& contour_point = this->right_points.emplace_back();
        contour_point.side = CONTOUR_SIDE_RIGHT;
        contour_point.side_index = this->right_points.size() - 1;
        contour_point.vertex_index = point.vertex_index;

        contour_point.point = point.point;
        contour_point.is_edge = point.is_edge;
        contour_point.is_bridge = point.is_bridge;
    }

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

    void LoopTriangulation::process(const glm::uvec2& resolution, float triangle_scale, uint32_t min_loop_length, const glsl::Loop* loop_pointer, const glsl::LoopCount* loop_count_pointer, const glsl::LoopSegment* loop_segment_pointer, std::vector<shared::Vertex>& vertices, std::vector<shared::Index>& indices, shared::ViewMetadata& statistic, std::vector<MeshFeatureLine>& features_lines, bool export_feature_lines)
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

        double time_loop_points = 0.0;

        for (uint32_t loop_index = 0; loop_index < loop_count; loop_index++)
        {
            const glsl::Loop* loop = loop_pointer + loop_index;
            const glsl::LoopSegment* segment_pointer = loop_segment_pointer + loop->segment_offset;
            uint32_t segment_count = loop->segment_count;
            bool is_edge = (loop->loop_flag & EXPERIMENT_GENERATOR_LOOP_EGDE) != 0;

            std::vector<LoopPoint> points = this->allocate_points();

            std::chrono::high_resolution_clock::time_point loop_segments_start = std::chrono::high_resolution_clock::now();
            this->compute_loop_points(segment_count, segment_pointer, is_edge, points);
            std::chrono::high_resolution_clock::time_point loop_segments_end = std::chrono::high_resolution_clock::now();
            time_loop_points += std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(loop_segments_end - loop_segments_start).count();
            
            if (export_feature_lines)
            {
                for (uint32_t index = 0; index < points.size(); index++)
                {
                    const LoopPoint& point1 = points[index];
                    const LoopPoint& point2 = points[(index + 1) % points.size()];

                    MeshFeatureLine feature_line;
                    feature_line.start = point1.point; //TODO: !!!!!!!!!!!!!!!!!!!!!!!!! convert space !!!!!!!!
                    feature_line.end = point2.point;
                    feature_line.id = point1.is_edge ? 6 : 0;

                    features_lines.push_back(feature_line);
                }
            }

            this->loop_points.push_back(std::move(points));
        }

        statistic.loop.time_loop_points = time_loop_points;

        std::chrono::high_resolution_clock::time_point triangulation_start = std::chrono::high_resolution_clock::now();
        this->compute_triangulation(resolution, triangle_scale, loop_pointer, loop_segment_pointer, vertices, indices, statistic, features_lines);
        std::chrono::high_resolution_clock::time_point triangulation_end = std::chrono::high_resolution_clock::now();
        double time_triangulation = std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(triangulation_end - triangulation_start).count();
    }

    bool skip_invalid_segments(const glsl::LoopSegment* segment_pointer, uint32_t segment_count, uint32_t start_offset, uint32_t& index)
    {
        while (index < segment_count)
        {
            const glsl::LoopSegment& segment = segment_pointer[(index + start_offset) % segment_count];

            if (segment.end_coord_depth < -1.0f)
            {
                index++; //Invalid segment as the bridge neighbour was discared
            }

            else
            {
                return true;
            }
        }

        return false;
    }

    void LoopTriangulation::compute_loop_points(uint32_t segment_count, const glsl::LoopSegment* segment_pointer, bool is_edge, std::vector<LoopPoint>& points)
    {
        //Related to Inverse Bersenham Algorithm introduced in "Pseudo-Immersive Real-Time Display of 3D Scenes on Mobile Devices" by "Ming Li, Arne Schmitz, Leif Kobbelt"

        glm::ivec2 start_point = segment_pointer->end_coord;
        uint32_t start_offset = 0;

        uint32_t ignored_segment_count = 0;

        for (uint32_t index = 1; index < segment_count; index++)
        {
            const glsl::LoopSegment& segment = segment_pointer[index];

            if (segment.end_coord_depth < -1.0f)
            {
                continue;
            }
            
            if (segment.end_coord.y < start_point.y || (segment.end_coord.y == start_point.y && segment.end_coord.x < start_point.x))
            {
                start_point = segment.end_coord;
                start_offset = index;
            }

            ignored_segment_count++;
        }

        if (ignored_segment_count <= 4)
        {
            for (uint32_t index = 0; index < segment_count; index++)
            {
                const glsl::LoopSegment& segment = segment_pointer[(index + start_offset) % segment_count];

                if (segment.end_coord_depth < -1.0f)
                {
                    continue;
                }

                LoopPoint point;
                point.point = segment.end_coord;
                point.depth = glm::abs(segment.end_coord_depth);
                point.is_bridge = segment.end_coord_depth < 0.0f;
                point.is_edge = is_edge;
                point.previous_segment = (index + start_offset) % segment_count;
                point.next_segment = (index + 1 + start_offset) % segment_count;
                point.vertex_index = this->vertex_counter;

                points.push_back(point);
                this->vertex_counter++;
            }

            return;
        }

        glm::ivec2 last_coord = glm::ivec2(0);

        for (int32_t index = segment_count - 1; index >= 0; index--)
        {
            const glsl::LoopSegment& segment = segment_pointer[(index + start_offset) % segment_count];

            if (segment.end_coord_depth < -1.0f)
            {
                continue;
            }

            last_coord = segment.end_coord;

            break;
        }

        for (uint32_t index = 0; index < segment_count;)
        {
            if (!skip_invalid_segments(segment_pointer, segment_count, start_offset, index))
            {
                break;
            }

            const glsl::LoopSegment& current_segment = segment_pointer[(index + start_offset) % segment_count];
            glm::ivec2 current_direction = glm::ivec2(0);
            uint32_t current_length = 0;

            // The lenght of the segment is influenced by bridge points. Therefore not the same behaviour as if there are no birdge points.
            LoopTriangulation::compute_segment(last_coord, current_segment.end_coord, current_direction, current_length);

            last_coord = current_segment.end_coord;
            index++;

            if (current_length > 2 || index >= segment_count)
            {
                LoopPoint point;
                point.point = current_segment.end_coord;
                point.depth = glm::abs(current_segment.end_coord_depth);
                point.is_bridge = current_segment.end_coord_depth < 0.0f;
                point.is_edge = is_edge;
                point.previous_segment = (index + start_offset + segment_count - 1) % segment_count;
                point.next_segment = (index + start_offset) % segment_count;
                point.vertex_index = this->vertex_counter;

                points.push_back(point);
                this->vertex_counter++;
            }

            else
            {
                if (current_segment.end_coord_depth < 0.0f) //Keep even if the line simplification algorithm would have removed it
                {
                    LoopPoint point;
                    point.point = current_segment.end_coord;
                    point.depth = glm::abs(current_segment.end_coord_depth);
                    point.is_bridge = current_segment.end_coord_depth < 0.0f;
                    point.is_edge = is_edge;
                    point.previous_segment = (index + start_offset + segment_count - 1) % segment_count;
                    point.next_segment = (index + start_offset) % segment_count;
                    point.vertex_index = this->vertex_counter;

                    points.push_back(point);
                    this->vertex_counter++;
                }

                if (!skip_invalid_segments(segment_pointer, segment_count, start_offset, index))
                {
                    break;
                }

                const glsl::LoopSegment& next_segment = segment_pointer[(index + start_offset) % segment_count];
                glm::ivec2 next_direction = glm::ivec2(0);
                uint32_t next_length = 0;

                LoopTriangulation::compute_segment(last_coord, next_segment.end_coord, next_direction, next_length);

                last_coord = next_segment.end_coord;
                index++;

                glm::u16vec2 line_end_coord = next_segment.end_coord;
                float line_end_coord_depth = next_segment.end_coord_depth;
                float line_slope = next_length;
                uint32_t line_segment_index = index;

                glsl::LoopSegment last_segment = next_segment;
                uint32_t last_index = index;

                while (index < segment_count)
                {
                    if (!skip_invalid_segments(segment_pointer, segment_count, start_offset, index))
                    {
                        break;
                    }

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

                        if (last_segment.end_coord_depth < 0.0f) //Keep even if the line simplification algorithm would have removed it
                        {
                            LoopPoint point;
                            point.point = last_segment.end_coord;
                            point.depth = glm::abs(last_segment.end_coord_depth);
                            point.is_bridge = last_segment.end_coord_depth < 0.0f;
                            point.is_edge = is_edge;
                            point.previous_segment = (last_index + start_offset + segment_count - 1) % segment_count;
                            point.next_segment = (last_index + start_offset) % segment_count;
                            point.vertex_index = this->vertex_counter;

                            points.push_back(point);
                            this->vertex_counter++;
                        }

                        line_end_coord = slope_segment.end_coord;
                        line_end_coord_depth = slope_segment.end_coord_depth;
                        
                        last_coord = slope_segment.end_coord;
                        index++;

                        last_segment = slope_segment;
                        last_index = index;
                    }

                    else if (glm::all(glm::equal(slope_direction, next_direction)))
                    {
                        if (std::abs(line_slope - slope_length) > 2.0f)
                        {
                            break;
                        }

                        if (last_segment.end_coord_depth < 0.0f) //Keep even if the line simplification algorithm would have removed it
                        {
                            LoopPoint point;
                            point.point = last_segment.end_coord;
                            point.depth = glm::abs(last_segment.end_coord_depth);
                            point.is_bridge = last_segment.end_coord_depth < 0.0f;
                            point.is_edge = is_edge;
                            point.previous_segment = (last_index + start_offset + segment_count - 1) % segment_count;
                            point.next_segment = (last_index + start_offset) % segment_count;
                            point.vertex_index = this->vertex_counter;

                            points.push_back(point);
                            this->vertex_counter++;
                        }

                        line_slope = (line_slope + slope_length) / 2.0f;
                        line_end_coord = slope_segment.end_coord;
                        line_end_coord_depth = slope_segment.end_coord_depth;
                        
                        last_coord = slope_segment.end_coord;
                        index++;

                        last_segment = slope_segment;
                        last_index = index;
                    }

                    else
                    {
                        break;
                    }
                }

                LoopPoint point;
                point.point = line_end_coord;
                point.depth = glm::abs(line_end_coord_depth);
                point.is_bridge = line_end_coord_depth < 0.0f;
                point.is_edge = is_edge;
                point.previous_segment = (last_index + start_offset + segment_count - 1) % segment_count;
                point.next_segment = (last_index + start_offset) % segment_count;
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

    void LoopTriangulation::compute_triangulation(const glm::uvec2& resolution, float triangle_scale, const glsl::Loop* loop_pointer, const glsl::LoopSegment* loop_segment_pointer, std::vector<shared::Vertex>& vertices, std::vector<shared::Index>& indices, shared::ViewMetadata& statistic, std::vector<MeshFeatureLine>& features_lines)
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
        //statistic.add("Time Loop Info", std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(loop_info_end - loop_info_start).count());

        std::chrono::high_resolution_clock::time_point loop_sort_start = std::chrono::high_resolution_clock::now();
        std::sort(this->loop_point_handles.begin(), this->loop_point_handles.end(), [&](const LoopPointHandle& point_handle1, const LoopPointHandle& point_handle2)
        {
            const LoopPoint& point1 = this->loop_points[point_handle1.loop_index][point_handle1.point_index];
            const LoopPoint& point2 = this->loop_points[point_handle2.loop_index][point_handle2.point_index];

            if (point1.point.y == point2.point.y)
            {
                if (point1.point.y % 2 == 0) //Change the sorting of the points alternating based on the y coordinate to left to right or right to left
                {
                    return point1.point.x < point2.point.x;
                }

                return point1.point.x > point2.point.x;
            }

            return point1.point.y < point2.point.y;
        });
        std::chrono::high_resolution_clock::time_point loop_sort_end = std::chrono::high_resolution_clock::now();
        //statistic.add("Time Loop Sort", std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(loop_sort_end - loop_sort_start).count());

#if ENABLE_SWEEP_LINE_PROFILE
        double time_interval_update = 0.0;
        double time_adjacent_two = 0.0;
        double time_adjacent_one = 0.0;
        double time_inside_outside = 0.0;
        double time_interval_search = 0.0;
#endif

        std::chrono::high_resolution_clock::time_point sweep_line_start = std::chrono::high_resolution_clock::now();
        for(const LoopPointHandle& point_handle : this->loop_point_handles)
        {
#if ENABLE_SWEEP_LINE_PROFILE
            std::chrono::high_resolution_clock::time_point interval_search_start = std::chrono::high_resolution_clock::now();
#endif
            AdjacentIntervals adjacent_intervals;
            this->check_intervals(point_handle, adjacent_intervals);
#if ENABLE_SWEEP_LINE_PROFILE
            std::chrono::high_resolution_clock::time_point interval_search_end = std::chrono::high_resolution_clock::now();
            time_interval_search += std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(interval_search_end - interval_search_start).count();
#endif

            const LoopPoint& point = this->loop_points[point_handle.loop_index][point_handle.point_index];
            
            if (point.is_edge)
            {
#if ENABLE_SWEEP_LINE_PROFILE
                std::chrono::high_resolution_clock::time_point adjacent_two_start = std::chrono::high_resolution_clock::now();
#endif
                if (adjacent_intervals.middle_index != INVALID_INTERVAL_INDEX)
                {
                    this->process_adjacent_one_interval_middle(point_handle, point, adjacent_intervals.middle_index);
                    this->process_adjacent_two_intervals(point_handle, point, adjacent_intervals.left_index, adjacent_intervals.right_index);

                    this->remove_intervals(adjacent_intervals);

                    continue;
                }
#if ENABLE_SWEEP_LINE_PROFILE
                std::chrono::high_resolution_clock::time_point adjacent_two_end = std::chrono::high_resolution_clock::now();
                time_adjacent_two += std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(adjacent_two_end - adjacent_two_start).count();
#endif

#if ENABLE_SWEEP_LINE_PROFILE
                std::chrono::high_resolution_clock::time_point adjacent_one_start = std::chrono::high_resolution_clock::now();
#endif
                if (adjacent_intervals.left_index != INVALID_INTERVAL_INDEX)
                {
                    this->process_adjacent_one_interval_left(point_handle, point, adjacent_intervals.left_index);
                    this->process_adjacent_one_interval_right(point_handle, point, adjacent_intervals.right_index);

                    continue;
                }
#if ENABLE_SWEEP_LINE_PROFILE
                std::chrono::high_resolution_clock::time_point adjacent_one_end = std::chrono::high_resolution_clock::now();
                time_adjacent_one += std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(adjacent_one_end - adjacent_one_start).count();
#endif

#if ENABLE_SWEEP_LINE_PROFILE
                std::chrono::high_resolution_clock::time_point interval_update_start = std::chrono::high_resolution_clock::now();
#endif
                uint32_t interval_index = 0;

                if (!this->check_inside(point, loop_pointer, loop_segment_pointer, interval_index))
                {
                    spdlog::error("LoopTriangulation: Error during triangulation");
                }

#if ENABLE_SWEEP_LINE_PROFILE
                std::chrono::high_resolution_clock::time_point interval_update_end = std::chrono::high_resolution_clock::now();
                time_interval_update += std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(interval_update_end - interval_update_start).count();
#endif

                LoopWinding local_winding = this->check_winding_local(point_handle, point, loop_pointer, loop_segment_pointer);
                bool winding_reverse = false;

                if (local_winding != LOOP_WINDING_COUNTER_CLOCKWISE)
                {
                    winding_reverse = true;
                }

#if ENABLE_SWEEP_LINE_PROFILE
                std::chrono::high_resolution_clock::time_point inside_outside_start = std::chrono::high_resolution_clock::now();
#endif
                this->process_inside_interval(point_handle, point, interval_index, !winding_reverse);
                this->process_outside_interval(point_handle, point, winding_reverse);
#if ENABLE_SWEEP_LINE_PROFILE
                std::chrono::high_resolution_clock::time_point inside_outside_end = std::chrono::high_resolution_clock::now();
                time_inside_outside += std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(inside_outside_end - inside_outside_start).count();
#endif
            }

            else
            {
#if ENABLE_SWEEP_LINE_PROFILE
                std::chrono::high_resolution_clock::time_point adjacent_two_start = std::chrono::high_resolution_clock::now();
#endif
                if (adjacent_intervals.middle_index != INVALID_INTERVAL_INDEX)
                {
                    this->process_adjacent_one_interval_middle(point_handle, point, adjacent_intervals.middle_index);

                    this->remove_intervals(adjacent_intervals);

                    continue;
                }

                if (adjacent_intervals.left_index != INVALID_INTERVAL_INDEX && adjacent_intervals.right_index != INVALID_INTERVAL_INDEX)
                {
                    this->process_adjacent_two_intervals(point_handle, point, adjacent_intervals.left_index, adjacent_intervals.right_index);

                    this->remove_intervals(adjacent_intervals);

                    continue;
                }
#if ENABLE_SWEEP_LINE_PROFILE
                std::chrono::high_resolution_clock::time_point adjacent_two_end = std::chrono::high_resolution_clock::now();
                time_adjacent_two += std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(adjacent_two_end - adjacent_two_start).count();
#endif

#if ENABLE_SWEEP_LINE_PROFILE
                std::chrono::high_resolution_clock::time_point adjacent_one_start = std::chrono::high_resolution_clock::now();
#endif
                if (adjacent_intervals.left_index != INVALID_INTERVAL_INDEX)
                {
                    this->process_adjacent_one_interval_left(point_handle, point, adjacent_intervals.left_index);

                    continue;
                }

                if (adjacent_intervals.right_index != INVALID_INTERVAL_INDEX)
                {
                    this->process_adjacent_one_interval_right(point_handle, point, adjacent_intervals.right_index);

                    continue;
                }
#if ENABLE_SWEEP_LINE_PROFILE
                std::chrono::high_resolution_clock::time_point adjacent_one_end = std::chrono::high_resolution_clock::now();
                time_adjacent_one += std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(adjacent_one_end - adjacent_one_start).count();

                std::chrono::high_resolution_clock::time_point interval_update_start = std::chrono::high_resolution_clock::now();
                std::chrono::high_resolution_clock::time_point interval_update_end;
#endif
                uint32_t interval_index = 0;

                if (this->check_inside(point, loop_pointer, loop_segment_pointer, interval_index))
                {
#if ENABLE_SWEEP_LINE_PROFILE
                    interval_update_end = std::chrono::high_resolution_clock::now();

                    std::chrono::high_resolution_clock::time_point inside_outside_start = std::chrono::high_resolution_clock::now();
#endif
                    this->process_inside_interval(point_handle, point, interval_index, false);

#if ENABLE_SWEEP_LINE_PROFILE
                    std::chrono::high_resolution_clock::time_point inside_outside_end = std::chrono::high_resolution_clock::now();
                    time_inside_outside += std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(inside_outside_end - inside_outside_start).count();
#endif
                }

                else
                {
#if ENABLE_SWEEP_LINE_PROFILE
                    interval_update_end = std::chrono::high_resolution_clock::now();

                    std::chrono::high_resolution_clock::time_point inside_outside_start = std::chrono::high_resolution_clock::now();
#endif

                    this->process_outside_interval(point_handle, point, false);

#if ENABLE_SWEEP_LINE_PROFILE
                    std::chrono::high_resolution_clock::time_point inside_outside_end = std::chrono::high_resolution_clock::now();
                    time_inside_outside += std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(inside_outside_end - inside_outside_start).count();
#endif
                }

#if ENABLE_SWEEP_LINE_PROFILE
                time_interval_update += std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(interval_update_end - interval_update_start).count();
#endif
            }
        }
        std::chrono::high_resolution_clock::time_point sweep_line_end = std::chrono::high_resolution_clock::now();
        //statistic.add("Time Sweep Line", std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(sweep_line_end - sweep_line_start).count());

#if ENABLE_SWEEP_LINE_PROFILE
        //statistic.add("Time Interval Search", time_interval_search);
        //statistic.add("Time Interval Update", time_interval_update);
        //statistic.add("Time Adjacent Two", time_adjacent_two);
        //statistic.add("Time Adjacent One", time_adjacent_one);
        //statistic.add("Time Inside Outside", time_inside_outside);
#endif

        indices.clear();
        vertices.clear();
        vertices.reserve(this->vertex_counter);

        for (uint32_t loop_index = 0; loop_index < this->loop_points.size(); loop_index++)
        {
            const std::vector<LoopPoint>& points = this->loop_points[loop_index];

            for (const LoopPoint& point : points)
            {
                shared::Vertex vertex;
                vertex.x = (point.point.x + (uint16_t)1) / (uint16_t)2;
                vertex.y = (point.point.y + (uint16_t)1) / (uint16_t)2;
                vertex.z = point.depth;

                vertices.push_back(vertex);
            }
        }

        std::chrono::high_resolution_clock::time_point contour_split_start = std::chrono::high_resolution_clock::now();
        this->split_contours();
        std::chrono::high_resolution_clock::time_point contour_split_end = std::chrono::high_resolution_clock::now();
        //statistic.add("Time Contour Split", std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(contour_split_end - contour_split_start).count());

        std::chrono::high_resolution_clock::time_point contour_start = std::chrono::high_resolution_clock::now();
        for (Contour* contour : this->contours)
        {
            this->triangulate_contour(contour, indices);
        }
        std::chrono::high_resolution_clock::time_point contour_end = std::chrono::high_resolution_clock::now();
        //statistic.add("Time Contour", std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(contour_end - contour_start).count());
    }

    bool LoopTriangulation::check_inside(const LoopPoint& point, const glsl::Loop* loop_pointer, const glsl::LoopSegment* loop_segment_pointer, uint32_t& inside_index)
    {
        std::vector<Interval>::iterator interval_iter = this->intervals.begin();
        std::vector<Interval>::iterator interval_end = this->intervals.end();
        uint32_t interval_index = 0;

        uint32_t point_x = point.point.x;
        uint32_t point_y = point.point.y;

        while (interval_iter != interval_end)
        {
            Interval& interval = *interval_iter;

            const glsl::Loop* left_loop = loop_pointer + interval.left_loop_index;
            const glsl::LoopSegment* left_segments = loop_segment_pointer + left_loop->segment_offset;

            bool left_winding_reverse = interval.left_winding_reverse;
            uint32_t left_segment_count = left_loop->segment_count;
            uint32_t left_segment_index = interval.left_segment_index;
            uint32_t left_coord_x = interval.left.x;
            uint32_t left_coord_y = interval.left.y;

            if (left_winding_reverse)
            {
                while (left_coord_y != point_y)
                {
                    const glsl::LoopSegment& segment = left_segments[left_segment_index];
                    uint32_t segment_end_coord_y = segment.end_coord.y;

                    if (segment_end_coord_y > point_y)
                    {
                        break;
                    }

                    left_coord_x = segment.end_coord.x;
                    left_coord_y = segment_end_coord_y;
                    left_segment_index = LoopTriangulation::next_point_index(left_segment_index, left_segment_count);
                }
            }

            else
            {
                while (left_coord_y != point_y)
                {
                    const glsl::LoopSegment& segment = left_segments[left_segment_index];
                    uint32_t segment_end_coord_y = segment.end_coord.y;

                    if (segment_end_coord_y > point_y)
                    {
                        break;
                    }

                    left_coord_x = segment.end_coord.x;
                    left_coord_y = segment_end_coord_y;
                    left_segment_index = LoopTriangulation::previous_point_index(left_segment_index, left_segment_count);
                }
            }

            interval.left.x = left_coord_x;
            interval.left.y = left_coord_y;
            interval.left_segment_index = left_segment_index;

            if (point_x < left_coord_x)
            {
                interval_iter++;
                interval_index++;

                continue;
            }

            const glsl::Loop* right_loop = loop_pointer + interval.right_loop_index;
            const glsl::LoopSegment* right_segments = loop_segment_pointer + right_loop->segment_offset;

            bool right_winding_reverse = interval.right_winding_reverse;
            uint32_t right_segment_count = right_loop->segment_count;
            uint32_t right_segment_index = interval.right_segment_index;
            uint32_t right_coord_x = interval.right.x;
            uint32_t right_coord_y = interval.right.y;

            if (right_winding_reverse)
            {
                while (right_coord_y != point_y)
                {
                    const glsl::LoopSegment& segment = right_segments[right_segment_index];
                    uint32_t segment_end_coord_y = segment.end_coord.y;

                    if (segment_end_coord_y > point_y)
                    {
                        break;
                    }

                    right_coord_x = segment.end_coord.x;
                    right_coord_y = segment_end_coord_y;
                    right_segment_index = LoopTriangulation::previous_point_index(right_segment_index, right_segment_count);
                }
            }

            else
            {
                while (right_coord_y != point_y)
                {
                    const glsl::LoopSegment& segment = right_segments[right_segment_index];
                    uint32_t segment_end_coord_y = segment.end_coord.y;

                    if (segment_end_coord_y > point_y)
                    {
                        break;
                    }

                    right_coord_x = segment.end_coord.x;
                    right_coord_y = segment_end_coord_y;
                    right_segment_index = LoopTriangulation::next_point_index(right_segment_index, right_segment_count);
                }
            }

            interval.right.x = right_coord_x;
            interval.right.y = right_coord_y;
            interval.right_segment_index = right_segment_index;

            if (point_x > right_coord_x)
            {
                interval_iter++;
                interval_index++;

                continue;
            }

            inside_index = interval_index;

            return true;
        }

        return false;
    }

    LoopWinding LoopTriangulation::check_winding_local(const LoopPointHandle& point_handle, const LoopPoint& point, const glsl::Loop* loop_pointer, const glsl::LoopSegment* loop_segment_pointer)
    {
        const glsl::Loop* loop = loop_pointer + point_handle.loop_index;
        const glsl::LoopSegment* segments = loop_segment_pointer + loop->segment_offset;

        const glsl::LoopSegment& previous_segment = segments[LoopTriangulation::previous_point_index(point.previous_segment, loop->segment_count)];
        const glsl::LoopSegment& next_segment = segments[point.next_segment];

        if (previous_segment.end_coord.x < next_segment.end_coord.x)
        {
            return LOOP_WINDING_COUNTER_CLOCKWISE;
        }

        return LOOP_WINDING_CLOCKWISE;
    }

    //Assumes all cleared
    void LoopTriangulation::check_intervals(const LoopPointHandle& point_handle, AdjacentIntervals& adjacent_intervals)
    {
        std::vector<Interval>::iterator interval_iter = this->intervals.begin();
        std::vector<Interval>::iterator interval_end = this->intervals.end();

        uint64_t interval_value = ((uint64_t)point_handle.point_index << 32) | ((uint64_t)point_handle.loop_index);
        uint32_t interval_count = 0;
        uint32_t interval_index = 0;

        while (interval_iter != interval_end && interval_count < 3)
        {
            uint64_t left_value = *((uint64_t*)&(*interval_iter));
            uint64_t right_value = *(((uint64_t*)&(*interval_iter)) + 1);

            uint32_t adjacent_index = (uint32_t)(interval_value == left_value);
            adjacent_index |= (uint32_t)(interval_value == right_value) << 1;

            if (adjacent_index > 0)
            {
                ((uint32_t*)&adjacent_intervals)[adjacent_index - 1] = interval_index;
                interval_count++;
            }

            interval_index++;
            interval_iter++;
        }
    }

    void LoopTriangulation::remove_intervals(AdjacentIntervals& adjacent_intervals)
    {
        std::array<uint32_t, 3> remove_list;
        uint32_t remove_count = 0;

        if (adjacent_intervals.left_index != INVALID_INTERVAL_INDEX)
        {
            remove_list[remove_count] = adjacent_intervals.left_index;
            remove_count++;
        }

        if (adjacent_intervals.middle_index != INVALID_INTERVAL_INDEX)
        {
            remove_list[remove_count] = adjacent_intervals.middle_index;
            remove_count++;
        }

        if (adjacent_intervals.right_index != INVALID_INTERVAL_INDEX)
        {
            remove_list[remove_count] = adjacent_intervals.right_index;
            remove_count++;
        }

        std::sort(remove_list.begin(), remove_list.begin() + remove_count, std::greater());

        for(uint32_t index = 0; index < remove_count; index++)
        {
            this->intervals.erase(this->intervals.begin() + remove_list[index]);
        }
    }

    void LoopTriangulation::process_adjacent_two_intervals(const LoopPointHandle& point_handle, const LoopPoint& point, uint32_t left_index, uint32_t right_index)
    {
        Interval& interval = this->intervals.emplace_back();
        const Interval& left_interval = this->intervals[right_index];
        const Interval& right_interval = this->intervals[left_index];

        interval.left = left_interval.left;
        interval.left_loop_index = left_interval.left_loop_index;
        interval.left_segment_index = left_interval.left_segment_index;
        interval.left_base_point_index = left_interval.left_base_point_index;
        interval.left_next_point_index = left_interval.left_next_point_index;
        interval.left_winding_reverse = left_interval.left_winding_reverse;

        interval.right = right_interval.right;
        interval.right_loop_index = right_interval.right_loop_index;
        interval.right_segment_index = right_interval.right_segment_index;
        interval.right_base_point_index = right_interval.right_base_point_index;
        interval.right_next_point_index = right_interval.right_next_point_index;
        interval.right_winding_reverse = right_interval.right_winding_reverse;

        interval.last_loop_index = point_handle.loop_index;
        interval.last_point_index = point_handle.point_index;
        interval.last_is_merge = true;

        if (left_interval.last_is_merge)
        {
            interval.left_contour = left_interval.left_contour;
            interval.left_contour->push_right(point);

            left_interval.right_contour->push_left(point);

            this->contours.push_back(left_interval.right_contour);
        }

        else
        {
            interval.left_contour = left_interval.left_contour;
            interval.left_contour->push_right(point);
        }

        if (right_interval.last_is_merge)
        {
            interval.right_contour = right_interval.right_contour;
            interval.right_contour->push_left(point);

            right_interval.left_contour->push_right(point);

            this->contours.push_back(right_interval.left_contour);
        }

        else
        {
            interval.right_contour = right_interval.left_contour;
            interval.right_contour->push_left(point);
        }
    }

    void LoopTriangulation::process_adjacent_one_interval_middle(const LoopPointHandle& point_handle, const LoopPoint& point, uint32_t middle_index)
    {
        const Interval& middle_interval = this->intervals[middle_index];

        if (middle_interval.last_is_merge)
        {
            middle_interval.left_contour->push_right(point);
            middle_interval.right_contour->push_left(point);

            this->contours.push_back(middle_interval.left_contour);
            this->contours.push_back(middle_interval.right_contour);
        }

        else
        {
            middle_interval.left_contour->push_right(point);

            this->contours.push_back(middle_interval.left_contour);
        }
    }

    void LoopTriangulation::process_adjacent_one_interval_left(const LoopPointHandle& point_handle, const LoopPoint& point, uint32_t left_index)
    {
        Interval& left_interval = this->intervals[left_index];
        uint32_t left_segment_index = point.previous_segment;
        
        if (left_interval.left_winding_reverse)
        {
            //Flip starting segments
            left_segment_index = point.next_segment;
        }

        if (left_interval.last_is_merge)
        {
            left_interval.left_contour->push_right(point);
            left_interval.right_contour->push_left(point);

            this->contours.push_back(left_interval.left_contour);
            left_interval.left_contour = left_interval.right_contour;
            left_interval.right_contour = nullptr;
        }

        else
        {
            left_interval.left_contour->push_left(point);
        }

        left_interval.left = point.point;
        left_interval.left_loop_index = point_handle.loop_index;
        left_interval.left_segment_index = left_segment_index;
        left_interval.left_base_point_index = point_handle.point_index;
        left_interval.left_next_point_index = LoopTriangulation::above_left_index(point_handle.point_index, loop_points[point_handle.loop_index].size(), left_interval);

        left_interval.last_loop_index = point_handle.loop_index;
        left_interval.last_point_index = point_handle.point_index;
        left_interval.last_is_merge = false;
    }

    void LoopTriangulation::process_adjacent_one_interval_right(const LoopPointHandle& point_handle, const LoopPoint& point, uint32_t right_index)
    {
        Interval& right_interval = this->intervals[right_index];
        uint32_t right_segment_index = point.next_segment;

        if (right_interval.right_winding_reverse)
        {
            //Flip starting segments
            right_segment_index = point.previous_segment;
        }

        if (right_interval.last_is_merge)
        {
            right_interval.left_contour->push_right(point);
            right_interval.right_contour->push_left(point);

            this->contours.push_back(right_interval.right_contour);
            right_interval.right_contour = nullptr;
        }

        else
        {
            right_interval.left_contour->push_right(point);
        }

        right_interval.right = point.point;
        right_interval.right_loop_index = point_handle.loop_index;
        right_interval.right_segment_index = right_segment_index;
        right_interval.right_base_point_index = point_handle.point_index;
        right_interval.right_next_point_index = LoopTriangulation::above_right_index(point_handle.point_index, loop_points[point_handle.loop_index].size(), right_interval);

        right_interval.last_loop_index = point_handle.loop_index;
        right_interval.last_point_index = point_handle.point_index;
        right_interval.last_is_merge = false;
    }

    void LoopTriangulation::process_inside_interval(const LoopPointHandle& point_handle, const LoopPoint& point, uint32_t interval_index, bool winding_reverse)
    {
        Interval& left_interval = this->intervals.emplace_back();
        Interval& interval = this->intervals[interval_index];
        
        uint32_t left_segment_index = point.previous_segment;
        uint32_t right_segment_index = point.next_segment;

        if (winding_reverse)
        {
            //Flip starting segments
            left_segment_index = point.next_segment;
            right_segment_index = point.previous_segment;
        }

        left_interval.left = interval.left;
        left_interval.left_loop_index = interval.left_loop_index;
        left_interval.left_segment_index = interval.left_segment_index;
        left_interval.left_base_point_index = interval.left_base_point_index;
        left_interval.left_next_point_index = interval.left_next_point_index;
        left_interval.left_winding_reverse = interval.left_winding_reverse;

        left_interval.right = point.point;
        left_interval.right_loop_index = point_handle.loop_index;
        left_interval.right_segment_index = right_segment_index;
        left_interval.right_base_point_index = point_handle.point_index;
        left_interval.right_winding_reverse = winding_reverse;

        left_interval.last_loop_index = point_handle.loop_index;
        left_interval.last_point_index = point_handle.point_index;
        left_interval.last_is_merge = false;
        
        Interval right_interval;
        right_interval.left = point.point;
        right_interval.left_loop_index = point_handle.loop_index;
        right_interval.left_segment_index = left_segment_index;
        right_interval.left_base_point_index = point_handle.point_index;
        right_interval.left_winding_reverse = winding_reverse;

        right_interval.right = interval.right;
        right_interval.right_loop_index = interval.right_loop_index;
        right_interval.right_segment_index = interval.right_segment_index;
        right_interval.right_base_point_index = interval.right_base_point_index;
        right_interval.right_next_point_index = interval.right_next_point_index;
        right_interval.right_winding_reverse = interval.right_winding_reverse;

        right_interval.last_loop_index = point_handle.loop_index;
        right_interval.last_point_index = point_handle.point_index;
        right_interval.last_is_merge = false;

        left_interval.right_next_point_index = LoopTriangulation::above_right_index(point_handle.point_index, loop_points[point_handle.loop_index].size(), left_interval);
        right_interval.left_next_point_index = LoopTriangulation::above_left_index(point_handle.point_index, loop_points[point_handle.loop_index].size(), right_interval);

        if (interval.last_is_merge)
        {
            left_interval.left_contour = interval.left_contour;
            left_interval.left_contour->push_right(point);

            right_interval.left_contour = interval.right_contour;
            right_interval.left_contour->push_left(point);
        }

        else
        {
            const LoopPoint& last_point = this->loop_points[interval.last_loop_index][interval.last_point_index];
            
            if (interval.last_loop_index == interval.left_loop_index && interval.last_point_index == interval.left_base_point_index)
            {
                left_interval.left_contour = this->allocate_contour();
                left_interval.left_contour->push_right(last_point);
                left_interval.left_contour->push_right(point);
                
                right_interval.left_contour = interval.left_contour;
                right_interval.left_contour->push_left(point);
            }

            else
            {
                left_interval.left_contour = interval.left_contour;
                left_interval.left_contour->push_right(point);

                right_interval.left_contour = this->allocate_contour();
                right_interval.left_contour->push_left(last_point);
                right_interval.left_contour->push_left(point);
            }
        }

        memcpy(this->intervals.data() + interval_index, &right_interval, sizeof(right_interval));
    }

    void LoopTriangulation::process_outside_interval(const LoopPointHandle& point_handle, const LoopPoint& point, bool winding_reverse)
    {
        uint32_t left_segment_index = point.previous_segment;
        uint32_t right_segment_index = point.next_segment;

        if (winding_reverse)
        {
            //Flip starting segments
            left_segment_index = point.next_segment;
            right_segment_index = point.previous_segment;
        }

        Interval& interval = this->intervals.emplace_back();
        interval.left = point.point;
        interval.left_loop_index = point_handle.loop_index;
        interval.left_segment_index = left_segment_index;
        interval.left_base_point_index = point_handle.point_index;
        interval.left_winding_reverse = winding_reverse;
            
        interval.right = point.point;
        interval.right_loop_index = point_handle.loop_index;
        interval.right_segment_index = right_segment_index;
        interval.right_base_point_index = point_handle.point_index;
        interval.right_winding_reverse = winding_reverse;
        
        interval.last_loop_index = point_handle.loop_index;
        interval.last_point_index = point_handle.point_index;
        interval.last_is_merge = false;

        interval.left_contour = this->allocate_contour();
        interval.left_contour->push_left(point);

        interval.left_next_point_index = LoopTriangulation::above_left_index(point_handle.point_index, loop_points[point_handle.loop_index].size(), interval);
        interval.right_next_point_index = LoopTriangulation::above_right_index(point_handle.point_index, loop_points[point_handle.loop_index].size(), interval);
    }

    std::array<glm::ivec2, 4> forward_closed =
    {
        glm::ivec2(-1, 0),
        glm::ivec2(0, -1),
        glm::ivec2(1, 0),
        glm::ivec2(0, 1)
    };

    glm::ivec2 LoopTriangulation::get_bridge_match_position(const ContourPoint& point)
    {
        glm::ivec2 local_position = (glm::ivec2(point.point) + 1) % 2;
        uint32_t local_index = 0;

        if (local_position.y > 0) // Circular indexing
        {
            local_index = 3 - local_position.x;
        }

        else
        {
            local_index = local_position.x;
        }

        glm::ivec2 match_position = glm::ivec2(0);

        if (point.is_edge)
        {
            match_position = glm::ivec2(point.point) + forward_closed[(local_index + 1) % 4];
        }

        else
        {
            match_position = glm::ivec2(point.point) + forward_closed[local_index];
        }

        return match_position;
    }

    void LoopTriangulation::split_contours()
    {
        for (uint32_t contour_index = 0; contour_index < this->contours.size(); contour_index++)
        {
            Contour* contour = this->contours[contour_index];
            contour->right_points.insert(contour->right_points.end(), contour->left_points.rbegin(), contour->left_points.rend());
            
            if (contour->right_points.size() <= 3)
            {
                continue;
            }

            for (uint32_t point_index = 0; point_index < contour->right_points.size(); point_index++)
            {
                const ContourPoint& point = contour->right_points[point_index];
                
                if (!point.is_bridge)
                {
                    continue;
                }

                glm::ivec2 match_position = this->get_bridge_match_position(point);

                int32_t start_index = point_index;
                int32_t end_index = -1;

                for (uint32_t offset = point_index + 1; offset < contour->right_points.size(); offset++)
                {
                    const ContourPoint& other = contour->right_points[offset];

                    if (glm::all(glm::equal(glm::ivec2(other.point), match_position)))
                    {
                        end_index = offset;

                        break;
                    }
                }

                if (end_index == -1 || end_index == start_index + 1 || (start_index == 0 && end_index == contour->right_points.size() - 1))
                {
                    if (end_index == start_index + 1)
                    {
                        point_index++; //Jump the next one as this is the end of the bridge
                    }

                    continue;
                }

                Contour* new_contour = this->allocate_contour();
                new_contour->right_points.clear();
                new_contour->right_points.insert(new_contour->right_points.end(), contour->right_points.begin() + start_index, contour->right_points.begin() + end_index + 1);

                this->contours.push_back(new_contour);

                contour->right_points.erase(contour->right_points.begin() + start_index + 1, contour->right_points.begin() + end_index);

                point_index++; //Jump the next one as this is the end of the bridge
            }
        }
    }

    void LoopTriangulation::triangulate_contour(Contour* contour, std::vector<shared::Index>& indices)
    {
        if (contour->right_points.size() < 3)
        {
            return;
        }

        std::sort(contour->right_points.begin(), contour->right_points.end(), [](const ContourPoint& point1, const ContourPoint& point2)
        {
            if (point1.point.y == point2.point.y)
            {
                if (point1.side == point2.side)
                {
                    return point1.side_index < point2.side_index;
                }

                return point1.side < point2.side;
            }

            return point1.point.y < point2.point.y;
        });

        this->contour_reflex_chain.clear();
        this->contour_reflex_chain.push_back(contour->right_points[0]);
        this->contour_reflex_chain.push_back(contour->right_points[1]);

        std::vector<ContourPoint>::iterator contour_point_iter = contour->right_points.begin() + 2;
        std::vector<ContourPoint>::iterator contour_point_end = contour->right_points.end();

        while(contour_point_iter != contour_point_end)
        {
            ContourPoint& current = *contour_point_iter++;

            if (current.side != this->contour_reflex_chain.back().side) //Case 1
            {
                std::vector<ContourPoint>::iterator chain_point_iter = this->contour_reflex_chain.begin();
                std::vector<ContourPoint>::iterator chain_point_end = this->contour_reflex_chain.end() - 1;

                while(chain_point_iter != chain_point_end)
                {
                    const ContourPoint& point1 = *chain_point_iter;
                    const ContourPoint& point2 = *(chain_point_iter + 1);

                    indices.push_back(point1.vertex_index);
                    indices.push_back(point2.vertex_index);
                    indices.push_back(current.vertex_index);

                    chain_point_iter++;
                }

                this->contour_reflex_chain.erase(this->contour_reflex_chain.begin(), this->contour_reflex_chain.end() - 1);
            }

            else
            {
                const ContourPoint& chain_end = this->contour_reflex_chain.back();
                const ContourPoint& chain_previous = *(this->contour_reflex_chain.end() - 2);
                bool non_reflex = false;

                if (current.side == CONTOUR_SIDE_RIGHT)
                {
                    non_reflex = !LoopTriangulation::is_reflex(chain_previous.point, chain_end.point, current.point);
                }

                else
                {
                    non_reflex = !LoopTriangulation::is_reflex(chain_end.point, chain_previous.point, current.point);
                }

                if (non_reflex) //Case 2a
                {
                    std::vector<ContourPoint>::reverse_iterator chain_point_iter = this->contour_reflex_chain.rbegin();
                    std::vector<ContourPoint>::reverse_iterator chain_point_end = this->contour_reflex_chain.rend() - 1;

                    while (chain_point_iter != chain_point_end)
                    {
                        const ContourPoint& point1 = *chain_point_iter;
                        const ContourPoint& point2 = *(chain_point_iter + 1);

                        if (current.side == CONTOUR_SIDE_RIGHT)
                        {
                            if (LoopTriangulation::is_reflex(point2.point, point1.point, current.point))
                            {
                                break;
                            }
                        }

                        else
                        {
                            if (LoopTriangulation::is_reflex(point1.point, point2.point, current.point))
                            {
                                break;
                            }
                        }

                        indices.push_back(point1.vertex_index);
                        indices.push_back(point2.vertex_index);
                        indices.push_back(current.vertex_index);

                        chain_point_iter++;
                    }

                    this->contour_reflex_chain.erase(chain_point_iter.base(), this->contour_reflex_chain.end());
                }
            }

            //Case 1
            //Case 2a
            //Case 2b
            this->contour_reflex_chain.push_back(current);
        }
    }

    inline bool LoopTriangulation::is_reflex(const glm::i16vec2& previous, const glm::i16vec2& current, const glm::i16vec2& next)
    {
        glm::i16vec2 direction1 = current - previous;
        glm::i16vec2 direction2 = next - current;
        
        if (direction2.x * direction1.y > direction2.y * direction1.x)
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
            contour->left_points.clear();
            contour->right_points.clear();

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

    inline uint32_t LoopTriangulation::above_left_index(uint32_t point_index, uint32_t loop_size, const Interval& interval)
    {
        if (interval.left_winding_reverse) //Inside
        {
            return LoopTriangulation::next_point_index(point_index, loop_size);
        }

        return LoopTriangulation::previous_point_index(point_index, loop_size);
    }

    inline uint32_t LoopTriangulation::above_right_index(uint32_t point_index, uint32_t loop_size, const Interval& interval)
    {
        if (interval.right_winding_reverse)
        {
            return LoopTriangulation::previous_point_index(point_index, loop_size);
        }

        return LoopTriangulation::next_point_index(point_index, loop_size);
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
}
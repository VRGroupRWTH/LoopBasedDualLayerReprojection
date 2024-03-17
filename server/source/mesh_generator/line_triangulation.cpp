#include "line_triangulation.hpp"
#include "line_generator.hpp"

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Triangulation_vertex_base_with_info_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <chrono>

typedef CGAL::Exact_predicates_inexact_constructions_kernel Kernel;
typedef CGAL::Triangulation_vertex_base_with_info_2<uint32_t, Kernel> VertexBase;
typedef CGAL::Triangulation_face_base_with_info_2<uint32_t, Kernel> FaceBase;
typedef CGAL::Constrained_triangulation_face_base_2<Kernel, FaceBase> ConstraintFaceBase;
typedef CGAL::Triangulation_data_structure_2<VertexBase, ConstraintFaceBase> TriangulationDataStructure;
typedef CGAL::Constrained_Delaunay_triangulation_2<Kernel, TriangulationDataStructure, CGAL::Exact_predicates_tag> ConstrainedTriangulation;
typedef ConstrainedTriangulation::Point_2 Point;
typedef ConstrainedTriangulation::Edge Edge;
typedef ConstrainedTriangulation::Vertex_handle VertexHandle;
typedef ConstrainedTriangulation::Face_handle FaceHandle;

void LineTriangulation::process(const glm::uvec2& resolution, float depth_max, uint32_t line_length_min, const float* depth_copy_pointer, LineQuadTree& quad_tree, std::vector<shared::Vertex>& vertices, std::vector<shared::Index>& indices, shared::ViewMetadata& metadata, std::vector<MeshFeatureLine>& feature_lines, bool export_feature_lines)
{
    this->line_coords.clear();
    this->line_segments.clear();

    std::chrono::high_resolution_clock::time_point line_trace_start = std::chrono::high_resolution_clock::now();
    while (true)
    {
        glm::ivec2 current_coord = glm::ivec2(0);

        if (!quad_tree.find_global(current_coord))
        {
            break;
        }

        if (!quad_tree.remove(current_coord))
        {
            return;
        }

        this->line_coords.clear();
        this->line_coords.push_back(current_coord);

        glm::ivec2 direction = glm::ivec2(0);

        while (true)
        {
            glm::ivec2 next_coord = glm::ivec2(0);

            if (!quad_tree.find_local_max(current_coord, next_coord))
            {
                break;
            }

            glm::ivec2 next_direction = next_coord - current_coord;

            if (direction.x == 0)
            {
                direction.x = next_direction.x;
            }

            else if (next_direction.x != 0 && next_direction.x != direction.x)
            {
                break;
            }

            if (direction.y == 0)
            {
                direction.y = next_direction.y;
            }

            else if (next_direction.y != 0 && next_direction.y != direction.y)
            {
                break;
            }

            if (!quad_tree.remove(next_coord))
            {
                return;
            }

            current_coord = next_coord;
            this->line_coords.push_back(current_coord);

            if (next_coord.x == 0 || next_coord.y == 0)
            {
                break;
            }

            if (next_coord.x == resolution.x - 1 || next_coord.y == resolution.y - 1)
            {
                break;
            }
        }

        if (this->line_coords.size() < line_length_min)
        {
            continue;
        }

        this->compute_line_segments();
    }
    std::chrono::high_resolution_clock::time_point line_trace_end = std::chrono::high_resolution_clock::now();
    metadata.line.time_line_trace = std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(line_trace_end - line_trace_start).count();

    std::vector<glm::ivec2> broder_start =
    {
        glm::ivec2(0),
        glm::ivec2(resolution.x, 0),
        glm::ivec2(resolution),
        glm::ivec2(0, resolution.y)
    };

    std::vector<glm::ivec2> broder_direction =
    {
        glm::ivec2(1, 0),
        glm::ivec2(0, 1),
        glm::ivec2(-1, 0),
        glm::ivec2(0, -1)
    };

    if (export_feature_lines)
    {
        feature_lines.clear();
        
        for (uint32_t border = 0; border < 4; border++)
        {
            for (uint32_t index = 0; index < LINE_TRIANGULATION_BORDER_POINTS - 1; index++)
            {
                glm::ivec2 line_step = (glm::ivec2(resolution) / (LINE_TRIANGULATION_BORDER_POINTS - 1)) * broder_direction[border];
                glm::ivec2 line_start = broder_start[border] + line_step * glm::ivec2(index);
                glm::ivec2 line_end = line_start + line_end;

                MeshFeatureLine feature_line;
                feature_line.start = line_start;
                feature_line.end = line_end;
                feature_line.id = 0;

                feature_lines.push_back(feature_line);
            }
        }

        uint32_t line_index = 1;

        for (const LineSegment& line_segment : this->line_segments)
        {
            MeshFeatureLine feature_line;
            feature_line.start = line_segment.start;
            feature_line.end = line_segment.end;
            feature_line.id = line_index;

            if (line_segment.is_end)
            {
                line_index++;
            }
        }
    }

    std::chrono::high_resolution_clock::time_point triangulation_start = std::chrono::high_resolution_clock::now();
    ConstrainedTriangulation triangulation;

    VertexHandle broder_start_vertex = nullptr;
    VertexHandle border_last_vertex = nullptr;

    for (uint32_t border = 0; border < 4; border++)
    {
        for (uint32_t index = 0; index < LINE_TRIANGULATION_BORDER_POINTS - 1; index++)
        {
            glm::ivec2 position = broder_start[border] + (glm::ivec2(index * resolution) / (LINE_TRIANGULATION_BORDER_POINTS - 1)) * broder_direction[border];

            VertexHandle border_vertex = triangulation.insert(Point(position.x, position.y));

            if (broder_start_vertex == nullptr)
            {
                broder_start_vertex = border_vertex;
            }

            else
            {
                triangulation.insert_constraint(border_last_vertex, border_vertex);
            }

            border_last_vertex = border_vertex;
        }
    }

    triangulation.insert_constraint(border_last_vertex, broder_start_vertex);

    VertexHandle last_vertex = nullptr;
    glm::ivec2 last_end = glm::ivec2(0);

    for (const LineSegment& line_segment : this->line_segments)
    {
        VertexHandle vertex = triangulation.insert(Point(line_segment.start.x, line_segment.start.y));

        if (line_segment.is_end)
        {
            VertexHandle end_vertex = triangulation.insert(Point(line_segment.end.x, line_segment.end.y));

            if (last_vertex != nullptr)
            {
                triangulation.insert_constraint(last_vertex, vertex);
            }

            triangulation.insert_constraint(vertex, end_vertex);

            last_vertex = nullptr;
            last_end = glm::ivec2(0);
        }

        else
        {
            if (last_vertex != nullptr)
            {
                triangulation.insert_constraint(last_vertex, vertex);
            }

            last_vertex = vertex;
            last_end = line_segment.end;
        }
    }

    vertices.clear();
    indices.clear();

    uint32_t vertex_index = 0;

    for (VertexHandle vertex_handle : triangulation.all_vertex_handles())
    {
        glm::ivec2 position = glm::ivec2(0);
        position.x = vertex_handle->point().x();
        position.y = vertex_handle->point().y();

        position = glm::clamp(position, glm::ivec2(0), glm::ivec2(resolution) - 1);

        shared::Vertex vertex;
        vertex.x = position.x;
        vertex.y = position.y;
        vertex.z = glm::min(depth_copy_pointer[position.y * resolution.x + position.x], depth_max);

        vertices.push_back(vertex);

        vertex_handle->info() = vertex_index;
        vertex_index++;
    }

    for (FaceHandle face_handle : triangulation.finite_face_handles())
    {
        VertexHandle vertex1 = face_handle->vertex(0);
        VertexHandle vertex2 = face_handle->vertex(1);
        VertexHandle vertex3 = face_handle->vertex(2);

        indices.push_back(vertex1->info());
        indices.push_back(vertex2->info());
        indices.push_back(vertex3->info());
    }
    std::chrono::high_resolution_clock::time_point triangulation_end = std::chrono::high_resolution_clock::now();
    metadata.line.time_triangulation = std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(triangulation_end - triangulation_start).count();
}

void LineTriangulation::compute_line_segments()
{
    this->point_sequences.clear();

    glm::ivec2 last_coord = this->line_coords.front();

    for (uint32_t index = 1; index < this->line_coords.size(); index++)
    {
        glm::ivec2 current_coord = this->line_coords[index];
        glm::ivec2 delta = current_coord - last_coord;

        if ((delta.x != 0 && delta.y != 0) || index == this->line_coords.size() - 1)
        {
            PointSequence sequence;
            sequence.start_point = last_coord;
            sequence.length = glm::max(glm::abs(delta.x), glm::abs(delta.y));
            
            if (glm::abs(delta.x) == sequence.length)
            {
                sequence.direction.x = glm::sign(delta.x);
                sequence.direction.y = 0;
            }

            else
            {
                sequence.direction.x = 0;
                sequence.direction.y = glm::sign(delta.y);
            }

            this->point_sequences.push_back(sequence);

            last_coord = last_coord + sequence.direction * glm::ivec2(sequence.length);
        }
    }

    for(uint32_t index = 0; index < this->point_sequences.size();)
    {
        const PointSequence& current_sequence = this->point_sequences[index];
        index++;

        if (current_sequence.length > 1 || index >= this->point_sequences.size())
        {
            LineSegment segment;
            segment.start = current_sequence.start_point;
            segment.end = current_sequence.start_point + current_sequence.direction * glm::ivec2(current_sequence.length);

            this->line_segments.push_back(segment);
        }

        else
        {
            const PointSequence& next_sequence = this->point_sequences[index];
            index++;

            glm::ivec2 segment_end_coord = next_sequence.start_point + current_sequence.direction * glm::ivec2(current_sequence.length);
            float segment_slope = next_sequence.length;

            while (index < this->point_sequences.size())
            {
                const PointSequence& slope_sequence = this->point_sequences[index];
                index++;

                if (glm::all(glm::equal(slope_sequence.direction, current_sequence.direction)))
                {
                    if (slope_sequence.length > 1)
                    {
                        break;
                    }

                    segment_end_coord = slope_sequence.start_point + slope_sequence.direction * glm::ivec2(slope_sequence.length);
                    index++;
                }

                else if (glm::all(glm::equal(slope_sequence.direction, next_sequence.direction)))
                {
                    if (std::abs(segment_slope - slope_sequence.length) > 2.0f)
                    {
                        break;
                    }

                    segment_slope = (segment_slope + slope_sequence.length) / 2.0f;
                    segment_end_coord = slope_sequence.start_point + slope_sequence.direction * glm::ivec2(slope_sequence.length);
                    index++;
                }

                else
                {
                    break;
                }
            }

            LineSegment segment;
            segment.start = current_sequence.start_point;
            segment.end = segment_end_coord;

            this->line_segments.push_back(segment);
        }
    }

    this->line_segments.back().is_end = true;
}
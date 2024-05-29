#ifndef HEADER_EXPERIMENT_GENERATOR
#define HEADER_EXPERIMENT_GENERATOR

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <vector>

#include "mesh_generator.hpp"
#include "experiment_triangulation.hpp"
#include "shader.hpp"
#include "timer.hpp"

namespace glsl
{
    using namespace glm;
    typedef uint32_t uint;
#include "../shaders/shared_defines.glsl"
}

namespace experiment
{
    struct LoopGeneratorLevel
    {
        glm::uvec2 level_resolution;
        uint32_t cell_buffer_size;

        GLuint loop_range_buffer;
        GLuint loop_range_count_buffer;
    };

    class LoopGeneratorFrame : public MeshGeneratorFrame
    {
    public:
        LoopTriangulation triangulation;
        glm::uvec2 resolution = glm::uvec2(0);
        uint32_t min_loop_length = 0;
        float triangle_scale = 0.0f;

        GLuint depth_buffer = 0;
        GLuint normal_buffer = 0;
        GLuint object_id_buffer = 0;

        GLuint loop_buffer = 0;
        GLuint loop_count_buffer = 0;
        GLuint loop_segment_buffer = 0;

        glsl::Loop* loop_pointer = nullptr;
        glsl::LoopCount* loop_count_pointer = nullptr;
        glsl::LoopSegment* loop_segment_pointer = nullptr;

        GLsync fence = 0;

        Timer vector_timer;
        Timer base_timer;
        Timer combine_timer;
        Timer distribute_timer;
        Timer write_timer;

        //Time measurements in millisconds
        double time_vector = 0.0;
        double time_base = 0.0;
        double time_combine = 0.0;
        double time_distribute = 0.0;
        double time_write = 0.0;

    public:
        LoopGeneratorFrame() = default;

        bool triangulate(std::vector<shared::Vertex>& vertices, std::vector<shared::Index>& indices, shared::ViewMetadata& statistic, std::vector<MeshFeatureLine>& features_lines, bool export_feature_lines = false);

        GLuint get_depth_buffer() const;
        GLuint get_normal_buffer() const;
        GLuint get_object_id_buffer() const;
    };

    class LoopGenerator : public MeshGenerator
    {
    private:
        Shader vector_shader = { "Experiment Generator Vector Shader" };
        Shader split_shader = { "Experiment Generator Split Shader" };
        Shader base_shader = { "Loop Generator Base Shader" };
        Shader combine_shader = { "Loop Generator Combine Shader" };
        Shader distribute_shader = { "Loop Generator Distribute Shader" };
        Shader discard_shader = { "Experiment Generator Discard Shader" };
        Shader write_shader = { "Experiment Generator Write Shader" };

        std::vector<LoopGeneratorLevel> levels;
        GLuint vector_buffer = 0;
        GLuint junction_buffer = 0;
        GLuint loop_buffer = 0;
        GLuint loop_count_buffer = 0;
        GLuint loop_segment_buffer = 0;

        glm::uvec2 resolution;
        float depth_max = 0.995f;
        float depth_base_threshold = 0.005f;
        float depth_slope_threshold = 0.005f;
        float normal_threshold = glm::pi<float>() / 4.0f;
        float triangle_scale = 0.0f;
        uint32_t loop_min_length = 100;
        bool use_normals = true;
        bool use_object_ids = true;

    public:
        LoopGenerator() = default;

        bool create(const glm::uvec2& resolution);
        void destroy();
        void apply(const shared::MeshSettings& settings);

        MeshGeneratorFrame* create_frame();
        void destroy_frame(MeshGeneratorFrame* frame);
        bool submit_frame(MeshGeneratorFrame* frame);
        bool map_frame(MeshGeneratorFrame* frame);
        bool unmap_frame(MeshGeneratorFrame* frame);
        bool is_frame_empty(MeshGeneratorFrame* frame);

    private:
        bool create_buffers(const glm::uvec2& resolution);
        bool create_shaders();
        void destroy_buffers();

        void perform_vector_pass(LoopGeneratorFrame* loop_frame);
        void perform_split_pass(LoopGeneratorFrame* loop_frame);
        void perform_base_pass(LoopGeneratorFrame* loop_frame);
        void perform_combine_pass(LoopGeneratorFrame* loop_frame);
        void perform_distribute_pass(LoopGeneratorFrame* loop_frame);
        void perform_discard_pass(LoopGeneratorFrame* loop_frame);
        void perform_write_pass(LoopGeneratorFrame* loop_frame);

        std::vector<uint32_t> compute_combine_work_group_sizes() const;
        std::vector<uint32_t> compute_distribute_work_group_sizes() const;
    };
}

#endif
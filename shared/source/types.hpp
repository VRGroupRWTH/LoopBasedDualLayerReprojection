#pragma once

#include <array>
#include <cstdint>

#define SHARED_VIEW_COUNT_MAX    6
#define SHARED_EXPORT_COUNT_MAX  4
#define SHARED_STRING_LENGTH_MAX 1024
#define SHARED_PI                3.14159265358f

namespace shared
{
    enum MeshGeneratorType : uint32_t
    {
        MESH_GENERATOR_TYPE_QUAD = 0x00,
        MESH_GENERATOR_TYPE_LINE = 0x01,
        MESH_GENERATOR_TYPE_LOOP = 0x02
    };

    enum VideoCodecType : uint32_t
    {
        VIDEO_CODEC_TYPE_H264 = 0x00,
        VIDEO_CODEC_TYPE_H265 = 0x01,
        VIDEO_CODEC_TYPE_AV1  = 0x02
    };

    enum VideoCodecMode : uint32_t
    {
        VIDEO_CODEC_MODE_CONSTANT_BITRATE = 0x00,
        VIDEO_CODEC_MODE_CONSTANT_QUALITY = 0x01
    };

    enum ExportType : uint32_t
    {
        EXPORT_TYPE_COLOR         = 0x00,
        EXPORT_TYPE_DEPTH         = 0x01,
        EXPORT_TYPE_MESH          = 0x02,
        EXPORT_TYPE_FEATURE_LINES = 0x03,
    };

    typedef uint32_t Index;
    typedef std::array<float, 16> Matrix;
    typedef std::array<char, SHARED_STRING_LENGTH_MAX> String;

    struct Vertex
    {
        uint16_t x = 0;
        uint16_t y = 0;
        float z = 0.0f;
    };

    struct LayerSettings
    {
        float depth_base_threshold = 0.5f;
        float depth_slope_threshold = 0.5f;
        std::uint8_t use_object_ids = true;
    };

    struct QuadSettings
    {
        float depth_threshold = 0.001f;
    };

    struct LineSettings
    {
        float laplace_threshold = 0.003f;
        float normal_scale = 0.5f;
        uint32_t line_length_min = 10;
    };

    struct LoopSettings
    {
        float depth_base_threshold = 0.001f;
        float depth_slope_threshold = 0.007f;
        float normal_threshold = SHARED_PI * 0.22222222f; //40°
        float triangle_scale = 2.0f;
        uint32_t loop_length_min = 80;
        std::uint8_t use_normals = true;
        std::uint8_t use_object_ids = true;
    };

    struct MeshSettings
    {
    public:
        float depth_max = 0.995f;

        union
        {
            QuadSettings quad;
            LineSettings line;
            LoopSettings loop;
        };

    public:
        MeshSettings(MeshGeneratorType type = MESH_GENERATOR_TYPE_QUAD)
        {
            switch (type)
            {
            case MESH_GENERATOR_TYPE_QUAD:
                this->quad = QuadSettings();
                break;
            case MESH_GENERATOR_TYPE_LINE:
                this->line = LineSettings();
                break;
            case MESH_GENERATOR_TYPE_LOOP:
                this->loop = LoopSettings();
                break;
            default:
                break;
            }
        }
    };

    struct QuadViewMetadata // All time measurements in milliseconds
    {
        float time_copy = 0.0f;
        float time_delta = 0.0f;
        float time_refine = 0.0f;
        float time_corner = 0.0f;
        float time_write = 0.0f;
    };

    struct LineViewMetadata // All time measurements in milliseconds
    {
        // Time required for the generation of the mesh on the GPU
        float time_edge_detection = 0.0f;
        float time_quad_tree = 0.0f;

        // Time required for the generation of the mesh on the CPU
        float time_cpu = 0.0f;
        float time_line_trace = 0.0f;
        float time_triangulation = 0.0f;
        uint32_t line_count = 0;
    };

    struct LoopViewMetadata // All time measurements in milliseconds
    {
        // Time required for the generation of the mesh on the GPU
        float time_vector = 0.0f;
        float time_split = 0.0f;
        float time_base = 0.0f;
        float time_combine = 0.0f;
        float time_distribute = 0.0f;
        float time_discard = 0.0f;
        float time_write = 0.0f;

        // Time required for the generation of the mesh on the CPU
        float time_cpu = 0.0f;
        float time_loop_simplification = 0.0f; // Time required for the inverse Bresenham step
        float time_triangulation = 0.0f;       // Time required for the entire traiangulation process not including the inverse Bresenham step
        float time_loop_info = 0.0f;
        float time_loop_sort = 0.0f;
        float time_sweep_line = 0.0f;
        float time_adjacent_two = 0.0f;
        float time_adjacent_one = 0.0f;
        float time_interval_search = 0.0f;
        float time_interval_update = 0.0f;
        float time_inside_outside = 0.0f;
        float time_contour_split = 0.0f;
        float time_contour = 0.0f;
        uint32_t loop_count = 0;
        uint32_t segment_count = 0;
        uint32_t point_count = 0;
    };

    struct ViewMetadata // All time measurements in milliseconds
    {
    public:
        float time_layer = 0.0f;           // Time required for the rendering of the scene
        float time_image_encode = 0.0f;    // Time required for the encoding of the image of the view
        float time_geometry_encode = 0.0f; // Time required for the encoding of the geometry of the view

        union
        {
            QuadViewMetadata quad;
            LineViewMetadata line;
            LoopViewMetadata loop;
        };

    public:
        ViewMetadata()
        {
            memset(this, 0, sizeof(ViewMetadata));
        }
    };
}
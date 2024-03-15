#pragma once

#include <array>
#include <cstdint>

#define SHARED_VIEW_COUNT_MAX             6
#define SHARED_SCENE_FILE_NAME_LENGTH_MAX 1024
#define SHARED_SKY_FILE_NAME_LENGTH_MAX   1024
#define SHARED_PI                         3.14159265358f

namespace shared
{
    enum MeshGeneratorType : uint32_t
    {
        MESH_GENERATOR_TYPE_LINE = 0x00,
        MESH_GENERATOR_TYPE_LOOP = 0x01
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

    enum MeshRequestExport : uint32_t
    {
        MESH_REQUEST_EXPORT_MESH          = 0x01,
        MESH_REQUEST_EXPORT_FEATURE_LINES = 0x02,
        MESH_REQUEST_EXPORT_COLOR         = 0x04,
        MESH_REQUEST_EXPORT_DEPTH         = 0x08
    };

    typedef uint32_t Index;
    typedef std::array<float, 16> Matrix;

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

    struct LineSettings
    {
        float laplace_threshold = 0.005f;
        float normal_scale = SHARED_PI * 0.25f;
        uint32_t line_length_min = 10;
    };

    struct LoopSettings
    {
        float depth_base_threshold = 0.005f;
        float depth_slope_threshold = 0.005f;
        float normal_threshold = SHARED_PI * 0.25f;
        float triangle_scale = 0.0f;
        std::uint8_t use_normals = true;
        std::uint8_t use_object_ids = true;
    };

    struct MeshSettings
    {
    public:
        float depth_max = 0.995f;

        union
        {
            LineSettings line;
            LoopSettings loop;
        };

    public:
        MeshSettings(MeshGeneratorType type = MESH_GENERATOR_TYPE_LINE)
        {
            switch (type)
            {
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

    struct LineViewMetadata // All time measurements in milliseconds
    {
        // Time required for the generation of the mesh on the GPU
        float time_edge_detection = 0.0f;
        float time_quad_tree = 0.0f;

        // Time required for the generation of the mesh on the CPU
        float time_line_trace = 0.0f;
        float time_triangulation = 0.0f;
    };

    struct LoopViewMetadata // All time measurements in milliseconds
    {
        // Time required for the generation of the mesh on the GPU
        float time_vector = 0.0f;
        float time_base = 0.0f;
        float time_combine = 0.0f;
        float time_distribute = 0.0f;
        float time_write = 0.0f;

        // Time required for the generation of the mesh on the CPU
        float time_loop_points = 0.0f;    // Time required for the inverse Bresenham step
        float time_triangulation = 0.0f;  // Time required for the entire traiangulation process not including the inverse Bresenham step
        float time_loop_info = 0.0f;
        float time_loop_sort = 0.0f;
        float time_sweep_line = 0.0f;
        float time_adjacent_two = 0.0f;
        float time_adjacent_one = 0.0f;
        float time_interval_update = 0.0f;
        float time_inside_outside = 0.0f;
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
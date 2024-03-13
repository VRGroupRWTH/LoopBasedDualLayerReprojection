#pragma once

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace i3ds {
const float pi = 3.14159265358f;
enum class MessageType : uint32_t {
    INITIALIZE_SESSION = 0,
    SET_VIDEO_COMPRESSION_SETTINGS = 1,
    SET_MESH_GENERATION_SETTINGS = 2,
    REQUEST_MESH = 3,
    UPDATE_MESH_LAYER = 4,
    LOG_INIT = 5,
    LOG_WRITE = 6,
    SERVER_ACTION = 7,
    LOG = 8,
};

using Index = uint32_t;

struct Vertex {
    uint16_t x;
    uint16_t y;
    float z;  // [0, 1]
};

enum class VideoCodec : uint32_t {
    H264 = 0,
    H265 = 1,
    AV1 = 2,
};

enum class MeshGenerationMethod : uint32_t { LINE_BASED = 0, LOOP_BASED = 1 };

using Matrix = std::array<float, 16>;

struct SessionInitialization {
    uint32_t resolution_width;
    uint32_t resolution_height;
    MeshGenerationMethod mesh_generation_method;
    VideoCodec video_codec;
    std::uint8_t video_use_chroma_subsampling = true;
    std::uint8_t layer_count = 2;
    Matrix projection_matrix;

    char scene_filename[1024];
    float scene_scale = 1.0f;
    float scene_exposure = 1.0f;
    float scene_indirect_intensity = 1.0f;

    char sky_filename[1024];
    float sky_intensity = 1.0f;
};

struct MeshRequest {
    uint32_t id;
    std::array<Matrix, 6> view_matrices;
};

enum class VideoCompressionMode : uint32_t {
    CONSTANT_BITRATE = 0,
    CONSTANT_QUALITY = 1,
};

struct VideoCompressionSettings {
    VideoCompressionMode mode = VideoCompressionMode::CONSTANT_QUALITY;
    float bitrate = 1.0;
    float quality = 0.5;
    uint32_t framerate = 10;
};

// Settings controlling the gneration of the layers
struct LayerSettings {
    float depth_base_threshold;
    float depth_slope_threshold;
    std::uint8_t use_object_ids;

    static LayerSettings defaults() {
        return {
            .depth_base_threshold = 0.5f,
            .depth_slope_threshold = 0.5f,
            .use_object_ids = 1,
        };
    }
};

// Settings controlling the line based meshing algorithm
struct LineBasedSettings {
    float laplace_threshold;
    float normal_scale;
    uint32_t line_length_min;

    static LineBasedSettings defaults() {
        return {
            .laplace_threshold = 0.005f,
            .normal_scale = pi * 0.25f,
            .line_length_min = 10,
        };
    }
};

// Settings controlling the loop based meshing algorithm
struct LoopBasedSettings {
    float depth_base_threshold;
    float depth_slope_threshold;
    float normal_threshold;
    float triangle_scale;
    std::uint8_t use_normals;
    std::uint8_t use_object_ids;

    static LoopBasedSettings defaults() {
        return {
            .depth_base_threshold = 0.005f,
            .depth_slope_threshold = 0.005f,
            .normal_threshold = pi * 0.25f,
            .use_normals = true,
            .use_object_ids = true,
        };
    }
};

// Settings controlling the meshing
struct MeshSettings {
    float depth_max;

    union {
        LineBasedSettings line_based;
        LoopBasedSettings loop_based;
    };

    static MeshSettings defaults(MeshGenerationMethod method) {
        switch (method) {
            case MeshGenerationMethod::LINE_BASED:
                return {
                    .depth_max = 0.995f,
                    .line_based = LineBasedSettings::defaults(),
                };

            case MeshGenerationMethod::LOOP_BASED:
                return {
                    .depth_max = 0.995f,
                    .loop_based = LoopBasedSettings::defaults(),
                };
        }
    }
};

struct MeshGenerationSettings {
    LayerSettings layer;
    MeshSettings mesh;

    MeshGenerationSettings() {
        layer.depth_base_threshold = 0.5f;
        layer.depth_slope_threshold = 0.5f;
        mesh.depth_max = 0.995f;
        mesh.loop_based.depth_base_threshold = 0.005f;
        mesh.loop_based.depth_slope_threshold = 0.005f;
        mesh.loop_based.normal_threshold = pi * 0.25f;
        mesh.loop_based.use_normals = true;
        mesh.loop_based.use_object_ids = true;
    }

    static MeshGenerationSettings defaults(MeshGenerationMethod method) {
        MeshGenerationSettings settings;
        settings.layer = LayerSettings::defaults();
        const auto mesh_settings = MeshSettings::defaults(method);
        std::memcpy(&settings.mesh, &mesh_settings, sizeof(MeshSettings));
        return settings;
    }

    static MeshGenerationSettings create_line_based(const LayerSettings& layer_settings, float depth_max, const LineBasedSettings& line_settings) {
        MeshGenerationSettings settings;
        settings.layer = layer_settings,
        settings.mesh = {
            .depth_max = depth_max,
            .line_based = line_settings,
        };
        return settings;
    }

    static MeshGenerationSettings create_loop_based(const LayerSettings& layer_settings, float depth_max, const LoopBasedSettings& loop_settings) {
        MeshGenerationSettings settings;
        settings.layer = layer_settings,
        settings.mesh = {
            .depth_max = depth_max,
            .loop_based = loop_settings,
        };
        return settings;
    }
};

enum class LogInterval : std::uint32_t {
    PER_FRAME = 0,
    PER_LAYER_UPDATE = 1,
    PER_SESSION = 2,
};

struct LineBasedStatistic {  // All time measurements in milliseconds
    // Time required for the generation of the mesh on the CPU
    float time_line_trace;
    float time_triangulation;

    // Time required for the generation of the mesh on the GPU
    float time_edge_detection;
    float time_quad_tree;
};

struct LoopBasedStatistic {  // All time measurements in milliseconds
    // Time required for the generation of the mesh on the CPU
    float time_loop_points;    // Time required for the inverse Bresenham step
    float time_triangulation;  // Time required for the entire traiangulation process not including
                               // the inverse Bresenham step
    float time_loop_info;
    float time_loop_sort;
    float time_sweep_line;
    float time_adjacent_two;
    float time_adjacent_one;
    float time_interval_update;
    float time_inside_outside;
    float time_contour;
    uint32_t loop_count;
    uint32_t segment_count;
    uint32_t point_count;

    // Time required for the generation of the mesh on the GPU
    float time_vector;
    float time_base;
    float time_combine;
    float time_distribute;
    float time_write;
};

struct ViewStatistic {  // All time measurements in milliseconds
    float time_layer;   // Time required for the rendering of the scene

    // Method dependent measurements
    union {
        LineBasedStatistic line_based;
        LoopBasedStatistic loop_based;
    };
};

struct LayerInfo {
    uint32_t request_id;
    uint32_t layer_index;
    uint32_t geometry_size;
    uint32_t frame_size;
    std::array<ViewStatistic, 6> view_statistics;
    std::array<Matrix, 6> view_matrices;
    std::array<uint32_t, 6> vertex_counts;
    std::array<uint32_t, 6> index_counts;
};

enum class ServerAction : std::uint32_t {
    NEXT_CONDITION = 0,
    PREVIOUS_CONDITION = 1,
};

struct LayerData {
    LayerData() = default;

    // Ensure the struct is never copied by accident
    LayerData(LayerData&&) = default;
    LayerData& operator=(LayerData&&) = default;

    LayerData(const LayerData&) = delete;
    LayerData& operator=(const LayerData&) = delete;

    // The measurements that were taken by the server
    std::array<ViewStatistic, 6> view_statistic;

    // The view matrices as received in the request
    std::array<Matrix, 6> view_matrices;

    // The number of vertices that correspond to the mesh of view_matrices[i].
    // (should sum up to vertices.size())
    std::array<uint32_t, 6> vertex_counts;

    // The number of indices that correspond to the mesh of view_matrices[i].
    // (should sum up to indices.size())
    std::array<uint32_t, 6> index_counts;

    // Vertices and indices of all meshes should be transmitted in one buffer.
    // First, all vertices that correnspond to the mesh of view_matrices[0], 
    // then all the belong to view_matrices[1], etc.
    std::vector<Vertex> vertices;
    std::vector<Index> indices;

    // The image should have a resolution of (3*resolution_width)x(2* resolution_height) and the
    // images for the corresponding view matrices of the request should be as follows:
    // +---+---+---+
    // | 0 | 1 | 2 |
    // +---+---+---+
    // | 3 | 4 | 5 |
    // +---+---+---+
    std::vector<uint8_t> image;
};

template <typename T>
bool get_message_payload(std::span<const uint8_t> payload, T* destination) {
    if (payload.size() != sizeof(T)) {
        return false;
    } else {
        std::memcpy(destination, payload.data(), sizeof(T));
        return true;
    }
}

}  // namespace i3ds

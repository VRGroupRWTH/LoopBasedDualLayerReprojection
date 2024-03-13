#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <chrono>
#include <string>
#include "geometry_codec.hpp"
#include "protocol.hpp"

using namespace emscripten;

class Uint8Array {
   public:
    Uint8Array(const val& buffer) : buffer(val::global("Uint8Array").new_(buffer)) {}
    Uint8Array(size_t length) : buffer(val::global("Uint8Array").new_(length)) {}
    template <typename T>
    Uint8Array(i3ds::MessageType message_type, T message)
        : Uint8Array(sizeof(i3ds::MessageType) + sizeof(T)) {
        this->write(0, message_type);
        this->write(sizeof(i3ds::MessageType), message);
    }

    Uint8Array(const Uint8Array&) = delete;
    Uint8Array(Uint8Array&&) = default;
    Uint8Array& operator=(const Uint8Array&) = delete;
    Uint8Array& operator=(Uint8Array&&) = default;

    template <typename T>
    static val create_buffer(i3ds::MessageType message_type, T message) {
        return Uint8Array(message_type, message).get_buffer();
    }

    void read(size_t offset, void* data, size_t size) const {
        assert(offset + size <= length());
        uint8_t* bytes = reinterpret_cast<uint8_t*>(data);
        for (size_t i = 0; i < size; ++i) {
            bytes[i] = buffer[offset + i].as<uint8_t>();
        }
    }

    template <typename T>
    T read(size_t offset) const {
        T value;
        this->read(offset, &value, sizeof(T));
        return value;
    }

    void write(size_t offset, const void* data, size_t size) {
        assert(offset + size <= length());
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);
        for (size_t i = 0; i < size; ++i) {
            buffer.set(offset + i, bytes[i]);
        }
    }

    template <typename T>
    void write(size_t offset, T value) {
        write(offset, &value, sizeof(T));
    }

    val slice(size_t start, size_t count) const {
        return this->buffer.call<val>("slice", start, start + count);
    }

    size_t length() const { return buffer["length"].as<size_t>(); }

    // do not use class afterwards
    val get_buffer() { return std::move(this->buffer); }

   private:
    val buffer;
};

template <typename T, typename K>
T get_property(const val& object, const K& key) {
    auto property = object[key];
    assert(property != val::undefined());
    return property.template as<T>();
}

// template <typename K, typename T>
// T get_property(const val& object, const K& key, const T& def) {
//     auto property = object[key];
//     if (property != val::undefined()) {
//         return property.template as<T>();
//     } else {
//         return def;
//     }
// }

val create_log_packet(const std::string& level, const std::string& message) {
    Uint8Array buffer(4 + 4 + message.length());
    buffer.write(0, i3ds::MessageType::LOG);
    if (level == "trace") {
        buffer.write<int>(sizeof(i3ds::MessageType), 0);
    } else if (level == "debug") {
        buffer.write<int>(sizeof(i3ds::MessageType), 1);
    } else if (level == "info") {
        buffer.write<int>(sizeof(i3ds::MessageType), 2);
    } else if (level == "warn") {
        buffer.write<int>(sizeof(i3ds::MessageType), 3);
    } else if (level == "error") {
        buffer.write<int>(sizeof(i3ds::MessageType), 4);
    } else if (level == "critical") {
        buffer.write<int>(sizeof(i3ds::MessageType), 5);
    }
    buffer.write(8, message.data(), message.size());
    return buffer.get_buffer();
}

val create_log_init_packet(const std::string& interval, const val& entries) {
    std::size_t packet_size = sizeof(i3ds::MessageType) + sizeof(i3ds::LogInterval);
    const std::vector<std::string> entry_strings = vecFromJSArray<std::string>(entries);
    for (const auto& entry : entry_strings) {
        packet_size += entry.size() + 1;
    }

    Uint8Array buffer(packet_size);
    buffer.write(0, i3ds::MessageType::LOG_INIT);
    if (interval == "perSession") {
        buffer.write(sizeof(i3ds::MessageType), i3ds::LogInterval::PER_SESSION);
    } else if (interval == "perLayerUpdate") {
        buffer.write(sizeof(i3ds::MessageType), i3ds::LogInterval::PER_LAYER_UPDATE);
    } else if (interval == "perFrame") {
        buffer.write(sizeof(i3ds::MessageType), i3ds::LogInterval::PER_FRAME);
    }

    std::size_t offset = sizeof(i3ds::MessageType) + sizeof(i3ds::LogInterval);
    for (const auto& entry : entry_strings) {
        buffer.write(offset, entry.data(), entry.size() + 1);
        offset += entry.size() + 1;
    }

    return buffer.get_buffer();
}

val create_log_write_packet(const std::string& interval, const val& entries, const val& values) {
    // std::cout << entries["length"].as<unsigned int>() << std::endl;
    std::size_t packet_size = sizeof(i3ds::MessageType) + sizeof(i3ds::LogInterval) +
                              entries["length"].as<unsigned int>() * sizeof(float);
    // const std::vector<float> value_vector = convertJSArrayToNumberVector<float>(entries);
    // for (const auto& entry : entry_strings) {
    //     packet_size += entry.size() + 1;
    // }

    Uint8Array buffer(packet_size);
    buffer.write(0, i3ds::MessageType::LOG_WRITE);
    if (interval == "perSession") {
        buffer.write(sizeof(i3ds::MessageType), i3ds::LogInterval::PER_SESSION);
    } else if (interval == "perLayerUpdate") {
        buffer.write(sizeof(i3ds::MessageType), i3ds::LogInterval::PER_LAYER_UPDATE);
    } else if (interval == "perFrame") {
        buffer.write(sizeof(i3ds::MessageType), i3ds::LogInterval::PER_FRAME);
    }

    std::size_t offset = sizeof(i3ds::MessageType) + sizeof(i3ds::LogInterval);
    for (const auto& entry : entries) {
        buffer.write(offset, values[entry].as<float>());
        offset += sizeof(float);
    }

    return buffer.get_buffer();
}

i3ds::Matrix create_projection_matrix(float n, float f) {
    const float fn = 1.0f / (f - n);
    return {
        1.0f,         0.0f, 0.0f, 0.0f, 0.0f,
        1.0f,         0.0f, 0.0f, 0.0f, 0.0f,
        (f + n) * fn, 1.0f, 0.0f, 0.0f, -2 * n * f * fn,
        0.0f,
    };
}

i3ds::Matrix create_view_matrix(float x, float y, float z) {
    return {
        1.0f, 0.0f, 0.0f, x, 0.0f, 1.0f, 0.0f, y, 0.0f, 0.0f, 1.0f, z, 0.0f, 0.0f, 0.0f, 1.0f,
    };
}

val create_session_init_packet(const val& settings) {
    i3ds::SessionInitialization init;

    const uint32_t resolution = get_property<std::uint32_t>(settings, "resolution");
    init.resolution_width = resolution;
    init.resolution_height = resolution;

    init.projection_matrix = create_projection_matrix(get_property<float>(settings, "nearPlane"),
                                                      get_property<float>(settings, "farPlane"));
    // std::cout <<
    //     init.projection_matrix[0] << " " << init.projection_matrix[1] << " " <<
    //     init.projection_matrix[2] << " " << init.projection_matrix[3] << "\n" <<
    //     init.projection_matrix[4] << " " << init.projection_matrix[5] << " " <<
    //     init.projection_matrix[6] << " " << init.projection_matrix[7] << "\n" <<
    //     init.projection_matrix[8] << " " << init.projection_matrix[9] << " " <<
    //     init.projection_matrix[10] << " " << init.projection_matrix[11] << "\n" <<
    //     init.projection_matrix[12] << " " << init.projection_matrix[13] << " " <<
    //     init.projection_matrix[14] << " " << init.projection_matrix[15] << std::endl;

    const std::string mesh_generation_method =
        get_property<std::string>(settings, "meshGeneration");
    if (mesh_generation_method == "line") {
        init.mesh_generation_method = i3ds::MeshGenerationMethod::LINE_BASED;
    } else if (mesh_generation_method == "loop") {
        init.mesh_generation_method = i3ds::MeshGenerationMethod::LOOP_BASED;
    }

    const std::string video_codec = get_property<std::string>(settings, "videoCodec");
    if (video_codec == "h264") {
        init.video_codec = i3ds::VideoCodec::H264;
    } else if (video_codec == "h265") {
        init.video_codec = i3ds::VideoCodec::H265;
    } else if (video_codec == "av1") {
        init.video_codec = i3ds::VideoCodec::AV1;
    }
    init.video_use_chroma_subsampling = get_property<bool>(settings, "chromaSubsampling");
    // std::memcpy(&init.projection_matrix, &projection_matrix, sizeof(float) * 16);

    const std::string scene = get_property<std::string>(settings, "scene");
    std::memcpy(&init.scene_filename, scene.data(),
                std::min(sizeof(init.scene_filename), scene.length()));
    init.scene_scale = get_property<float>(settings, "sceneScale");
    init.layer_count = get_property<float>(settings, "layerCount");
    init.scene_exposure = get_property<float>(settings, "sceneExposure");
    init.scene_indirect_intensity = get_property<float>(settings, "sceneIndirectIntensity");
    init.sky_filename[0] = '\0';

    return Uint8Array::create_buffer(i3ds::MessageType::INITIALIZE_SESSION, init);
}

i3ds::LayerSettings create_layer_settings(const val& settings) {
    return {
        .depth_base_threshold = get_property<float>(settings, "depthBaseThreshold"),
        .depth_slope_threshold = get_property<float>(settings, "depthSlopeThreshold"),
        .use_object_ids = get_property<bool>(settings, "useObjectIds"),
    };
}

i3ds::LineBasedSettings create_line_based_settings(const val& settings) {
    return {
        .laplace_threshold = get_property<float>(settings, "laplaceThreshold"),
        .normal_scale = get_property<float>(settings, "normalScale"),
        .line_length_min = get_property<uint32_t>(settings, "lineLengthMin"),
    };
}

i3ds::LoopBasedSettings create_loop_based_settings(const val& settings) {
    return {
        .depth_base_threshold = get_property<float>(settings, "depthBaseThreshold"),
        .depth_slope_threshold = get_property<float>(settings, "depthSlopeThreshold"),
        .normal_threshold = get_property<float>(settings, "normalThreshold"),
        .triangle_scale = get_property<float>(settings, "triangleScale"),
        .use_normals = get_property<bool>(settings, "useNormals"),
        .use_object_ids = get_property<bool>(settings, "useObjectIds"),
    };
}

i3ds::MeshSettings create_mesh_settings(const val& settings) {
    i3ds::MeshSettings mesh;
    mesh.depth_max = get_property<float>(settings, "depthMax");
    if (settings["lineBased"] != val::undefined()) {
        mesh.line_based = create_line_based_settings(settings["lineBased"]);
    } else if (settings["loopBased"] != val::undefined()) {
        mesh.loop_based = create_loop_based_settings(settings["loopBased"]);
    } else {
        mesh.loop_based = i3ds::LoopBasedSettings::defaults();
    }
    return mesh;
}

val create_mesh_generation_settings(const val& settings) {
    i3ds::MeshGenerationSettings mesh;
    mesh.layer = create_layer_settings(settings["layer"]);
    mesh.mesh = create_mesh_settings(settings["mesh"]);
    return Uint8Array(i3ds::MessageType::SET_MESH_GENERATION_SETTINGS, mesh).get_buffer();
}

val create_video_compression_settings(const val& settings) {
    i3ds::VideoCompressionSettings video;
    video.framerate = get_property<std::uint32_t>(settings, "framerate");
    if (settings["bitrate"] != val::undefined()) {
        video.mode = i3ds::VideoCompressionMode::CONSTANT_BITRATE;
        video.bitrate = settings["bitrate"].as<float>();
    } else if (settings["quality"] != val::undefined()) {
        video.mode = i3ds::VideoCompressionMode::CONSTANT_QUALITY;
        video.bitrate = settings["quality"].as<float>();
    } else {
        video.mode = i3ds::VideoCompressionMode::CONSTANT_BITRATE;
        video.bitrate = 1000;
    }
    return Uint8Array(i3ds::MessageType::SET_VIDEO_COMPRESSION_SETTINGS, video).get_buffer();
}

val create_request_package(uint32_t id, float x, float y, float z) {
    i3ds::MeshRequest request;
    request.id = id;
    for (auto& view_matrix : request.view_matrices) {
        view_matrix = {
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f, -x,   -y,   -z,   1.0f,
        };
    }

    request.view_matrices[2] = {
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -x, -y, -z, 1.0f,
    };
    request.view_matrices[1] = {
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, z, -y, -x, 1.0f,
    };
    request.view_matrices[0] = {
        -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, x, -y, z, 1.0f,
    };
    request.view_matrices[3] = {
        0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -z, -y, x, 1.0f,
    };
    request.view_matrices[4] = {
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -x, z, -y, 1.0f,
    };
    request.view_matrices[5] = {
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, -x, -z, y, 1.0f,
    };
    //     request.view_matrices[1] = {
    //         1.0f, 0.0f, 0.0f, x,
    //         0.0f, 1.0f, 0.0f, y,
    //         0.0f, 0.0f, 1.0f, z,
    //         0.0f, 0.0f, 0.0f, 1.0f,
    //     };
    //     request.view_matrices[2] = {
    //         1.0f, 0.0f, 0.0f, x,
    //         0.0f, 1.0f, 0.0f, y,
    //         0.0f, 0.0f, 1.0f, z,
    //         0.0f, 0.0f, 0.0f, 1.0f,
    //     };
    //     request.view_matrices[3] = {
    //         1.0f, 0.0f, 0.0f, x,
    //         0.0f, 1.0f, 0.0f, y,
    //         0.0f, 0.0f, 1.0f, z,
    //         0.0f, 0.0f, 0.0f, 1.0f,
    //     };
    //     request.view_matrices[4] = {
    //         1.0f, 0.0f, 0.0f, x,
    //         0.0f, 1.0f, 0.0f, y,
    //         0.0f, 0.0f, 1.0f, z,
    //         0.0f, 0.0f, 0.0f, 1.0f,
    //     };
    //     request.view_matrices[5] = {
    //         1.0f, 0.0f, 0.0f, x,
    //         0.0f, 1.0f, 0.0f, y,
    //         0.0f, 0.0f, 1.0f, z,
    //         0.0f, 0.0f, 0.0f, 1.0f,
    //     };
    return Uint8Array::create_buffer(i3ds::MessageType::REQUEST_MESH, request);
}

template <typename T, const size_t N, typename F>
val parse_array(const std::array<T, N>& array, const F& f) {
    val result = val::array();

    for (const auto& value : array) {
        result.call<void>("push", f(value));
    }

    return result;
}

val parse_matrix(const i3ds::Matrix& matrix) {
    val js_matrix = val::array();
    for (const auto& value : matrix) {
        js_matrix.call<void>("push", value);
    }
    return js_matrix;
}

val parse_line_statistic(const i3ds::LineBasedStatistic& statistic) {
    val obj = val::object();
    obj.set("timeQuadTree", statistic.time_quad_tree);
    obj.set("timeLineTrace", statistic.time_line_trace);
    obj.set("timeTriangulation", statistic.time_triangulation);
    obj.set("timeEdgeDetection", statistic.time_edge_detection);
    return obj;
}

val parse_loop_statistic(const i3ds::LoopBasedStatistic& statistic) {
    val obj = val::object();

    obj.set("timeLoopPoints", statistic.time_loop_points);
    obj.set("timeTriangulation", statistic.time_triangulation);
    obj.set("timeLoopInfo", statistic.time_loop_info);
    obj.set("timeLoopSort", statistic.time_loop_sort);
    obj.set("timeSweepLine", statistic.time_sweep_line);
    obj.set("timeAdjacentTwo", statistic.time_adjacent_two);
    obj.set("timeAdjacentOne", statistic.time_adjacent_one);
    obj.set("timeIntervalUpdate", statistic.time_interval_update);
    obj.set("timeInsideOutside", statistic.time_inside_outside);
    obj.set("timeContour", statistic.time_contour);
    obj.set("loopCount", statistic.loop_count);
    obj.set("segmentCount", statistic.segment_count);
    obj.set("pointCount", statistic.point_count);

    obj.set("timeVector", statistic.time_vector);
    obj.set("timeBase", statistic.time_base);
    obj.set("timeCombine", statistic.time_combine);
    obj.set("timeDistribute", statistic.time_distribute);
    obj.set("timeWrite", statistic.time_write);

    return obj;
}

val parse_view_statistic(const i3ds::ViewStatistic& view_statistic) {
    val statistic = val::object();
    statistic.set("timeLayer", view_statistic.time_layer);
    statistic.set("loop", parse_loop_statistic(view_statistic.loop_based));
    statistic.set("line", parse_line_statistic(view_statistic.line_based));
    return statistic;
}

val parse_view_statistics(const std::array<i3ds::ViewStatistic, 6>& view_statistics) {
    val statistics = val::array();
    for (const auto& statistic : view_statistics) {
        statistics.call<void>("push", parse_view_statistic(statistic));
    }
    return statistics;
}

val decompose_mesh_layer(const Uint8Array& request_data) {
    const auto info = request_data.read<i3ds::LayerInfo>(sizeof(i3ds::MessageType));

    // std::array<ViewStatistic, 6> view_statistics;
    // std::array<Matrix, 6> view_matrices;
    // std::array<uint32_t, 6> vertex_counts;
    // std::array<uint32_t, 6> index_counts;

    val result = val::object();
    result.set("requestId", info.request_id);
    result.set("layerIndex", info.layer_index);
    result.set("vertexCounts", parse_array(info.vertex_counts, [](uint32_t i) { return val(i); }));
    result.set("indexCounts", parse_array(info.index_counts, [](uint32_t i) { return val(i); }));
    result.set("viewMatrices", parse_array(info.view_matrices, parse_matrix));
    result.set("viewStatistics", parse_view_statistics(info.view_statistics));

    const size_t geometry_start = sizeof(i3ds::MessageType) + sizeof(i3ds::LayerInfo);
    result.set("geometry", request_data.slice(geometry_start, info.geometry_size));

    const size_t frame_start = geometry_start + info.geometry_size;
    result.set("frame", request_data.slice(frame_start, info.frame_size));

    return result;
}

val parse_response(const val& response) {
    Uint8Array request_data(response);
    val result = val::object();

    switch (request_data.read<i3ds::MessageType>(0)) {
        case i3ds::MessageType::UPDATE_MESH_LAYER:
            result.set("type", "meshLayer");
            result.set("meshLayer", decompose_mesh_layer(request_data));
            break;

        default:
            return val::null();
    }

    return result;
}

static std::vector<i3ds::Vertex> vertices;
static std::vector<i3ds::Index> indices;
val decode_geometry(const val& geometry) {
    const std::vector<uint8_t> geometry_buffer = convertJSArrayToNumberVector<uint8_t>(geometry);
    const auto t0 = std::chrono::steady_clock::now();
    if (i3ds::GeometryCodec::decode(geometry_buffer, indices, vertices)) {
        const auto t1 = std::chrono::steady_clock::now();
        const double raw_decode_time =
            std::chrono::duration_cast<std::chrono::duration<double>>(t1 - t0).count() * 1000.0f;

        val result = val::object();

        // There are two options to transfer data to the worker
        // If you comment in another alternative, make sure to also change it in the
        // GeometryDecodeWorker.ts OPTION A The vertices and indices buffers are global to avoid
        // unnecessary allocations. The vertex_array and index_array are created for each call as
        // they will be moved out of anyway.

        static_assert(sizeof(i3ds::Vertex) == 8);
        auto vertex_array = val::global("Uint32Array").new_(vertices.size() * 2);
        auto vertex_data = reinterpret_cast<const uint32_t*>(vertices.data());
        for (size_t i = 0; i < vertices.size() * 2; ++i) {
            vertex_array.set(i, vertex_data[i]);
        }

        static_assert(sizeof(i3ds::Index) == 4);
        auto index_array = val::global("Uint32Array").new_(indices.size());
        auto index_data = reinterpret_cast<const uint32_t*>(indices.data());
        for (size_t i = 0; i < indices.size(); ++i) {
            index_array.set(i, index_data[i]);
        }

        result.set("vertices", vertex_array);
        result.set("indices", index_array);
        result.set("rawDecodeTime", raw_decode_time);

        // OPTION B
        // result.set("verticesPointer", reinterpret_cast<size_t>(vertices.data()));
        // result.set("verticesByteLength", vertices.size() * sizeof(i3ds::Vertex));
        // result.set("indicesPointer", reinterpret_cast<size_t>(indices.data()));
        // result.set("indicesByteLength", indices.size() * sizeof(i3ds::Index));
        // result.set("rawDecodeTime", raw_decode_time);

        return result;
    } else {
        return val::null();
    }
}

EMSCRIPTEN_BINDINGS(my_module) {
    function("createLogPacket", &create_log_packet);
    function("createLogInitPacket", &create_log_init_packet);
    function("createLogWritePacket", &create_log_write_packet);
    function("createSessionInitPacket", &create_session_init_packet);
    function("createRequestPacket", &create_request_package);
    function("createVideoCompressionSettingsPacket", &create_video_compression_settings);
    function("createMeshGenerationSettingsPacket", &create_mesh_generation_settings);
    function("parseResponse", &parse_response);
    function("decodeGeometry", &decode_geometry);
}

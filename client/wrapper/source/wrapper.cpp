#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <optional>
#include <vector>
#include <string>
#include "../../shared/source/protocol.hpp"
#include "../../shared/source/geometry_codec.hpp"

struct MeshSettings
{
    float depth_max;

    std::optional<shared::QuadSettings> quad;
    std::optional<shared::LineSettings> line;
    std::optional<shared::LoopSettings> loop;
};

struct ViewMetadata
{
    float time_layer;
    float time_image_encode;
    float time_geometry_encode;

    std::optional<shared::QuadViewMetadata> quad;
    std::optional<shared::LineViewMetadata> line;
    std::optional<shared::LoopViewMetadata> loop;
};

struct SessionCreateForm
{
    shared::MeshGeneratorType mesh_generator;
    shared::VideoCodecType video_codec;
    bool video_use_chroma_subsampling;

    shared::Matrix projection_matrix;
    uint32_t resolution_width;
    uint32_t resolution_height;
    uint32_t layer_count;
    uint32_t view_count;
    
    std::string scene_file_name;
    float scene_scale;
    float scene_exposure;
    float scene_indirect_intensity;

    std::string sky_file_name;
    float sky_intensity;

    bool export_enabled;
};

struct SessionDestroyForm
{

};

struct RenderRequestForm
{
    uint32_t request_id;
    std::array<std::string, SHARED_EXPORT_COUNT_MAX> export_file_names;
    std::array<shared::Matrix, SHARED_VIEW_COUNT_MAX> view_matrices;
};

struct MeshSettingsForm
{
    shared::LayerSettings layer;
    MeshSettings mesh;
};

struct VideoSettingsForm
{
    shared::VideoCodecMode mode;
    uint32_t framerate;
    float bitrate;
    float quality;
};

struct LayerResponseForm
{
    uint32_t request_id;
    uint32_t layer_index;

    uint32_t geometry_bytes;
    uint32_t image_bytes;

    std::array<ViewMetadata, SHARED_VIEW_COUNT_MAX> view_metadata;
    std::array<shared::Matrix, SHARED_VIEW_COUNT_MAX> view_matrices;
    std::array<uint32_t, SHARED_VIEW_COUNT_MAX> vertex_counts;
    std::array<uint32_t, SHARED_VIEW_COUNT_MAX> index_counts;
};

struct Geometry
{
    emscripten::val indices;
    emscripten::val vertices;
};

template<class Type>
emscripten::val build_array(const Type& packet)
{
    emscripten::val view = emscripten::val(emscripten::typed_memory_view(sizeof(packet), (uint8_t*)&packet));
    emscripten::val array = emscripten::val::global("Uint8Array").new_(view);

    return array;
}

template<class Type>
std::vector<Type> build_vector(const emscripten::val& value, uint32_t offset, uint32_t size)
{
    emscripten::val subarray = value.call<emscripten::val>("subarray", offset, offset + size);

    return convertJSArrayToNumberVector<Type>(subarray);
}

shared::String build_string(const std::string& src_string)
{
    shared::String dst_string;
    memset(dst_string.data(), 0, dst_string.size());
    memcpy(dst_string.data(), src_string.data(), std::min(src_string.size(), dst_string.size()));

    return dst_string;
}

MeshSettingsForm default_mesh_settings()
{
    MeshSettingsForm form;
    form.layer = shared::LayerSettings();
    form.mesh.depth_max = shared::MeshSettings().depth_max;
    form.mesh.quad = shared::QuadSettings();
    form.mesh.line = shared::LineSettings();
    form.mesh.loop = shared::LoopSettings();

    return form;    
}

VideoSettingsForm default_video_settings()
{
    shared::VideoSettingsPacket packet = shared::VideoSettingsPacket();

    VideoSettingsForm form;
    form.mode = packet.mode;
    form.framerate = packet.framerate;
    form.bitrate = packet.bitrate;
    form.quality = packet.quality;

    return form;
}

emscripten::val build_session_create_packet(SessionCreateForm form)
{
    shared::SessionCreatePacket packet;
    packet.type = shared::PACKET_TYPE_SESSION_CREATE;
    packet.mesh_generator = form.mesh_generator;
    packet.video_codec = form.video_codec;
    packet.video_use_chroma_subsampling = form.video_use_chroma_subsampling;
    packet.projection_matrix = form.projection_matrix;
    packet.resolution_width = form.resolution_width;
    packet.resolution_height = form.resolution_height;
    packet.layer_count = form.layer_count;
    packet.view_count = form.view_count;
    packet.scene_file_name = build_string(form.scene_file_name);
    packet.scene_scale = form.scene_scale;
    packet.scene_exposure = form.scene_exposure;
    packet.scene_indirect_intensity = form.scene_indirect_intensity;
    packet.sky_file_name = build_string(form.sky_file_name);
    packet.sky_intensity = form.sky_intensity;
    packet.export_enabled = form.export_enabled;    

    return build_array(packet);
}

emscripten::val build_session_destroy_packet(SessionDestroyForm form)
{
    shared::SessionDestroyPacket packet;
    packet.type = shared::PACKET_TYPE_SESSION_DESTROY;

    return build_array(packet);
}

emscripten::val build_render_request_packet(RenderRequestForm form)
{
    shared::RenderRequestPacket packet;
    packet.type = shared::PACKET_TYPE_RENDER_REQUEST;
    packet.request_id = form.request_id;
    packet.view_matrices = form.view_matrices;

    for(uint32_t index = 0; index < SHARED_EXPORT_COUNT_MAX; index++)
    {
        packet.export_file_names[index] = build_string(form.export_file_names[index]);
    }

    return build_array(packet);
}

emscripten::val build_mesh_settings_packet(MeshSettingsForm form)
{
    shared::MeshSettingsPacket packet;
    packet.type = shared::PACKET_TYPE_MESH_SETTINGS;
    packet.layer = form.layer;
    packet.mesh.depth_max = form.mesh.depth_max;

    uint32_t settings = 0;
    settings += form.mesh.quad.has_value() ? 1 : 0;
    settings += form.mesh.line.has_value() ? 1 : 0;
    settings += form.mesh.loop.has_value() ? 1 : 0;

    if(settings > 1)
    {
        assert(true);
    }

    if (form.mesh.quad.has_value())
    {
        packet.mesh.quad = form.mesh.quad.value();
    }

    if (form.mesh.line.has_value())
    {
        packet.mesh.line = form.mesh.line.value();
    }

    if (form.mesh.loop.has_value())
    {
        packet.mesh.loop = form.mesh.loop.value();
    }

    return build_array(packet);
}

emscripten::val build_video_settings_packet(VideoSettingsForm form)
{
    shared::VideoSettingsPacket packet;
    packet.type = shared::PACKET_TYPE_VIDEO_SETTINGS;
    packet.mode = form.mode;
    packet.framerate = form.framerate;
    packet.bitrate = form.bitrate;
    packet.quality = form.quality;

    return build_array(packet);
}

//Assumes that data is an Uint8Array
shared::PacketType parse_packet_type(emscripten::val data)
{
    const std::vector<uint8_t> packet_data = build_vector<uint8_t>(data, 0, sizeof(shared::PacketType));
    shared::PacketType* packet_type = (shared::PacketType*)packet_data.data();

    return *packet_type;
}

//Assumes that data is an Uint8Array
LayerResponseForm parse_layer_response_packet(emscripten::val data)
{
    const std::vector<uint8_t> packet_data = build_vector<uint8_t>(data, 0, sizeof(shared::LayerResponsePacket));
    const shared::LayerResponsePacket* packet = (const shared::LayerResponsePacket*)packet_data.data();

    LayerResponseForm form;
    form.request_id = packet->request_id;
    form.layer_index = packet->layer_index;
    form.geometry_bytes = packet->geometry_bytes;
    form.image_bytes = packet->image_bytes;
    form.view_matrices = packet->view_matrices;
    form.vertex_counts = packet->vertex_counts;
    form.index_counts = packet->index_counts; 

    for(uint32_t index = 0; index < SHARED_VIEW_COUNT_MAX; index++)
    {
        const shared::ViewMetadata& packet_metadata = packet->view_metadata[index];
        ViewMetadata& form_metadata = form.view_metadata[index];

        form_metadata.time_layer = packet_metadata.time_layer;
        form_metadata.time_image_encode = packet_metadata.time_image_encode;
        form_metadata.time_geometry_encode = packet_metadata.time_geometry_encode;
        form_metadata.quad = packet_metadata.quad; // Don't know which one is correct so use both
        form_metadata.line = packet_metadata.line; // Don't know which one is correct so use both
        form_metadata.loop = packet_metadata.loop; // Don't know which one is correct so use both
    }

    return form;
}

//Try took keep them local to avoid reallocations
std::vector<shared::Index> local_indices;
std::vector<shared::Vertex> local_vertices;

//Assumes that data, indices and vertices are Uint8Arrays
std::optional<Geometry> decode_geoemtry(emscripten::val data, emscripten::val indices, emscripten::val vertices)
{
    const std::vector<uint8_t> local_data = emscripten::convertJSArrayToNumberVector<uint8_t>(data); //copy here

    if (!shared::GeometryCodec::decode(local_data, local_indices, local_vertices))
    {
        return std::optional<Geometry>();
    }

    uint32_t index_bytes = sizeof(shared::Index) * local_indices.size();
    uint32_t vertex_bytes = sizeof(shared::Vertex) * local_vertices.size();

    emscripten::val index_buffer = indices["buffer"];
    emscripten::val vertex_buffer = vertices["buffer"];

    uint32_t index_buffer_size = index_buffer["byteLength"].as<uint32_t>();
    uint32_t vertex_buffer_size = vertex_buffer["byteLength"].as<uint32_t>();

    Geometry geometry;

    if(index_buffer_size < index_bytes)
    {
        geometry.indices = emscripten::val::global("Uint8Array").new_(index_bytes);
    }

    else
    {
        geometry.indices = emscripten::val::global("Uint8Array").new_(index_buffer, 0, index_bytes);
    }

    if(vertex_buffer_size < vertex_bytes)
    {
        geometry.vertices = emscripten::val::global("Uint8Array").new_(vertex_bytes);
    }

    else
    {
        geometry.vertices = emscripten::val::global("Uint8Array").new_(vertex_buffer, 0, vertex_bytes);
    }

    emscripten::val local_index_view = emscripten::val(emscripten::typed_memory_view(index_bytes, (uint8_t*)local_indices.data()));
    emscripten::val local_vertex_view = emscripten::val(emscripten::typed_memory_view(vertex_bytes, (uint8_t*)local_vertices.data()));

    geometry.indices.call<void>("set", local_index_view, 0);   //copy here
    geometry.vertices.call<void>("set", local_vertex_view, 0); //copy here

    return geometry;
}

EMSCRIPTEN_BINDINGS(wrapper) 
{
    emscripten::constant("INDEX_SIZE", sizeof(shared::Index));
    emscripten::constant("VERTEX_SIZE", sizeof(shared::Vertex));
    emscripten::constant("VERTEX_OFFSET_X", offsetof(shared::Vertex, x));
    emscripten::constant("VERTEX_OFFSET_Y", offsetof(shared::Vertex, y));
    emscripten::constant("VERTEX_OFFSET_Z", offsetof(shared::Vertex, z));

    emscripten::enum_<shared::PacketType>("PacketType")
        .value("PACKET_TYPE_SESSION_CREATE", shared::PACKET_TYPE_SESSION_CREATE)
        .value("PACKET_TYPE_SESSION_DESTROY", shared::PACKET_TYPE_SESSION_DESTROY)
        .value("PACKET_TYPE_RENDER_REQUEST", shared::PACKET_TYPE_RENDER_REQUEST)
        .value("PACKET_TYPE_MESH_SETTINGS", shared::PACKET_TYPE_MESH_SETTINGS)
        .value("PACKET_TYPE_VIDEO_SETTINGS", shared::PACKET_TYPE_VIDEO_SETTINGS)
        .value("PACKET_TYPE_LAYER_RESPONSE", shared::PACKET_TYPE_LAYER_RESPONSE);

    emscripten::enum_<shared::MeshGeneratorType>("MeshGeneratorType")
        .value("MESH_GENERATOR_TYPE_QUAD", shared::MESH_GENERATOR_TYPE_QUAD)
        .value("MESH_GENERATOR_TYPE_LINE", shared::MESH_GENERATOR_TYPE_LINE)
        .value("MESH_GENERATOR_TYPE_LOOP", shared::MESH_GENERATOR_TYPE_LOOP);
        
    emscripten::enum_<shared::VideoCodecType>("VideoCodecType")
        .value("VIDEO_CODEC_TYPE_H264", shared::VIDEO_CODEC_TYPE_H264)
        .value("VIDEO_CODEC_TYPE_H265", shared::VIDEO_CODEC_TYPE_H265)
        .value("VIDEO_CODEC_TYPE_AV1", shared::VIDEO_CODEC_TYPE_AV1);

    emscripten::enum_<shared::VideoCodecMode>("VideoCodecMode")
        .value("VIDEO_CODEC_MODE_CONSTANT_BITRATE", shared::VIDEO_CODEC_MODE_CONSTANT_BITRATE)
        .value("VIDEO_CODEC_MODE_CONSTANT_QUALITY", shared::VIDEO_CODEC_MODE_CONSTANT_QUALITY);

    emscripten::enum_<shared::ExportType>("ExportType")
        .value("EXPORT_TYPE_COLOR", shared::EXPORT_TYPE_COLOR)
        .value("EXPORT_TYPE_DEPTH", shared::EXPORT_TYPE_DEPTH)
        .value("EXPORT_TYPE_MESH", shared::EXPORT_TYPE_MESH)
        .value("EXPORT_TYPE_FEATURE_LINES", shared::EXPORT_TYPE_FEATURE_LINES);

    emscripten::value_array<shared::Matrix>("Matrix")
        .element(emscripten::index<0>()).element(emscripten::index<1>()).element(emscripten::index<2>()).element(emscripten::index<3>())
        .element(emscripten::index<4>()).element(emscripten::index<5>()).element(emscripten::index<6>()).element(emscripten::index<7>())
        .element(emscripten::index<8>()).element(emscripten::index<9>()).element(emscripten::index<10>()).element(emscripten::index<11>())
        .element(emscripten::index<12>()).element(emscripten::index<13>()).element(emscripten::index<14>()).element(emscripten::index<15>());    

    emscripten::value_object<shared::Vertex>("Vertex")
        .field("x", &shared::Vertex::x)
        .field("y", &shared::Vertex::y)
        .field("z", &shared::Vertex::z);

    emscripten::value_object<shared::LayerSettings>("LayerSettings")
        .field("depth_base_threshold", &shared::LayerSettings::depth_base_threshold)
        .field("depth_slope_threshold", &shared::LayerSettings::depth_slope_threshold)
        .field("use_object_ids", &shared::LayerSettings::use_object_ids);

    emscripten::value_object<shared::QuadSettings>("QuadSettings")
        .field("depth_threshold", &shared::QuadSettings::depth_threshold);

    emscripten::value_object<shared::LineSettings>("LineSettings")
        .field("laplace_threshold", &shared::LineSettings::laplace_threshold)
        .field("normal_scale", &shared::LineSettings::normal_scale)
        .field("line_length_min", &shared::LineSettings::line_length_min);

    emscripten::value_object<shared::LoopSettings>("LoopSettings")
        .field("depth_base_threshold", &shared::LoopSettings::depth_base_threshold)
        .field("depth_slope_threshold", &shared::LoopSettings::depth_slope_threshold)
        .field("normal_threshold", &shared::LoopSettings::normal_threshold)
        .field("triangle_scale", &shared::LoopSettings::triangle_scale)
        .field("loop_length_min", &shared::LoopSettings::loop_length_min)
        .field("use_normals", &shared::LoopSettings::use_normals)
        .field("use_object_ids", &shared::LoopSettings::use_object_ids);

    emscripten::register_optional<shared::QuadSettings>();
    emscripten::register_optional<shared::LineSettings>();
    emscripten::register_optional<shared::LoopSettings>();

    emscripten::value_object<MeshSettings>("MeshSettings")
        .field("depth_max", &MeshSettings::depth_max)
        .field("quad", &MeshSettings::quad)
        .field("line", &MeshSettings::line)
        .field("loop", &MeshSettings::loop);

    emscripten::value_object<shared::QuadViewMetadata>("QuadViewMetadata")
        .field("time_copy", &shared::QuadViewMetadata::time_copy)
        .field("time_delta", &shared::QuadViewMetadata::time_delta)
        .field("time_refine", &shared::QuadViewMetadata::time_refine)
        .field("time_corner", &shared::QuadViewMetadata::time_corner)
        .field("time_write", &shared::QuadViewMetadata::time_write);

    emscripten::value_object<shared::LineViewMetadata>("LineViewMetadata")
        .field("time_edge_detection", &shared::LineViewMetadata::time_edge_detection)
        .field("time_quad_tree", &shared::LineViewMetadata::time_quad_tree)
        .field("time_cpu", &shared::LineViewMetadata::time_cpu)
        .field("time_line_trace", &shared::LineViewMetadata::time_line_trace)
        .field("time_triangulation", &shared::LineViewMetadata::time_triangulation)
        .field("line_count", &shared::LineViewMetadata::line_count);

    emscripten::value_object<shared::LoopViewMetadata>("LoopViewMetadata")
        .field("time_vector", &shared::LoopViewMetadata::time_vector)
        .field("time_split", &shared::LoopViewMetadata::time_split)
        .field("time_base", &shared::LoopViewMetadata::time_base)
        .field("time_combine", &shared::LoopViewMetadata::time_combine)
        .field("time_distribute", &shared::LoopViewMetadata::time_distribute)
        .field("time_discard", &shared::LoopViewMetadata::time_discard)
        .field("time_write", &shared::LoopViewMetadata::time_write)
        .field("time_cpu", &shared::LoopViewMetadata::time_cpu)
        .field("time_loop_simplification", &shared::LoopViewMetadata::time_loop_simplification)
        .field("time_triangulation", &shared::LoopViewMetadata::time_triangulation)
        .field("time_loop_info", &shared::LoopViewMetadata::time_loop_info)
        .field("time_loop_sort", &shared::LoopViewMetadata::time_loop_sort)
        .field("time_sweep_line", &shared::LoopViewMetadata::time_sweep_line)
        .field("time_adjacent_two", &shared::LoopViewMetadata::time_adjacent_two)
        .field("time_adjacent_one", &shared::LoopViewMetadata::time_adjacent_one)
        .field("time_interval_search", &shared::LoopViewMetadata::time_interval_search)
        .field("time_interval_update", &shared::LoopViewMetadata::time_interval_update)
        .field("time_inside_outside", &shared::LoopViewMetadata::time_inside_outside)
        .field("time_contour_split", &shared::LoopViewMetadata::time_contour_split)
        .field("time_contour", &shared::LoopViewMetadata::time_contour)
        .field("loop_count", &shared::LoopViewMetadata::loop_count)
        .field("segment_count", &shared::LoopViewMetadata::segment_count)
        .field("point_count", &shared::LoopViewMetadata::point_count);

    emscripten::register_optional<shared::QuadViewMetadata>();
    emscripten::register_optional<shared::LineViewMetadata>();
    emscripten::register_optional<shared::LoopViewMetadata>();

    emscripten::value_object<ViewMetadata>("ViewMetadata")
        .field("time_layer", &ViewMetadata::time_layer)
        .field("time_image_encode", &ViewMetadata::time_image_encode)
        .field("time_geometry_encode", &ViewMetadata::time_geometry_encode)
        .field("quad", &ViewMetadata::quad)
        .field("line", &ViewMetadata::line)
        .field("loop", &ViewMetadata::loop);

    emscripten::value_array<std::array<std::string, SHARED_EXPORT_COUNT_MAX>>("FileNameArray")
        .element(emscripten::index<0>()).element(emscripten::index<1>())
        .element(emscripten::index<2>()).element(emscripten::index<3>());

    emscripten::value_array<std::array<shared::Matrix, SHARED_VIEW_COUNT_MAX>>("MatrixArray")
        .element(emscripten::index<0>()).element(emscripten::index<1>())
        .element(emscripten::index<2>()).element(emscripten::index<3>())
        .element(emscripten::index<4>()).element(emscripten::index<5>());

    emscripten::value_array<std::array<ViewMetadata, SHARED_VIEW_COUNT_MAX>>("ViewMetadataArray")
        .element(emscripten::index<0>()).element(emscripten::index<1>())
        .element(emscripten::index<2>()).element(emscripten::index<3>())
        .element(emscripten::index<4>()).element(emscripten::index<5>());

    emscripten::value_array<std::array<uint32_t, SHARED_VIEW_COUNT_MAX>>("CountArray")
        .element(emscripten::index<0>()).element(emscripten::index<1>())
        .element(emscripten::index<2>()).element(emscripten::index<3>())
        .element(emscripten::index<4>()).element(emscripten::index<5>());

    emscripten::value_object<SessionCreateForm>("SessionCreateForm")
        .field("mesh_generator", &SessionCreateForm::mesh_generator)
        .field("video_codec", &SessionCreateForm::video_codec)
        .field("video_use_chroma_subsampling", &SessionCreateForm::video_use_chroma_subsampling)
        .field("projection_matrix", &SessionCreateForm::projection_matrix)
        .field("resolution_width", &SessionCreateForm::resolution_width)
        .field("resolution_height", &SessionCreateForm::resolution_height)
        .field("layer_count", &SessionCreateForm::layer_count)
        .field("view_count", &SessionCreateForm::view_count)
        .field("scene_file_name", &SessionCreateForm::scene_file_name)
        .field("scene_scale", &SessionCreateForm::scene_scale)
        .field("scene_exposure", &SessionCreateForm::scene_exposure)
        .field("scene_indirect_intensity", &SessionCreateForm::scene_indirect_intensity)
        .field("sky_file_name", &SessionCreateForm::sky_file_name)
        .field("sky_intensity", &SessionCreateForm::sky_intensity)
        .field("export_enabled", &SessionCreateForm::export_enabled);

    emscripten::value_object<SessionDestroyForm>("SessionDestroyForm");

    emscripten::value_object<RenderRequestForm>("RenderRequestForm")
        .field("request_id", &RenderRequestForm::request_id)
        .field("export_file_names", &RenderRequestForm::export_file_names)
        .field("view_matrices", &RenderRequestForm::view_matrices);

    emscripten::value_object<MeshSettingsForm>("MeshSettingsForm")
        .field("layer", &MeshSettingsForm::layer)
        .field("mesh", &MeshSettingsForm::mesh);

    emscripten::value_object<VideoSettingsForm>("VideoSettingsForm")
        .field("mode", &VideoSettingsForm::mode)
        .field("framerate", &VideoSettingsForm::framerate)
        .field("bitrate", &VideoSettingsForm::bitrate)
        .field("quality", &VideoSettingsForm::quality);

    emscripten::value_object<LayerResponseForm>("LayerResponseForm")
        .field("request_id", &LayerResponseForm::request_id)
        .field("layer_index", &LayerResponseForm::layer_index)
        .field("geometry_bytes", &LayerResponseForm::geometry_bytes)
        .field("image_bytes", &LayerResponseForm::image_bytes)
        .field("view_metadata", &LayerResponseForm::view_metadata)
        .field("view_matrices", &LayerResponseForm::view_matrices)
        .field("vertex_counts", &LayerResponseForm::vertex_counts)
        .field("index_counts", &LayerResponseForm::index_counts);

    emscripten::value_object<Geometry>("Geometry")
        .field("indices", &Geometry::indices)
        .field("vertices", &Geometry::vertices);

    emscripten::register_optional<Geometry>();

    emscripten::function("default_mesh_settings", &default_mesh_settings);
    emscripten::function("default_video_settings", &default_video_settings);
    emscripten::function("build_session_create_packet(form)", &build_session_create_packet);
    emscripten::function("build_session_destroy_packet(form)", &build_session_destroy_packet);
    emscripten::function("build_render_request_packet(form)", &build_render_request_packet);
    emscripten::function("build_mesh_settings_packet(form)", &build_mesh_settings_packet);
    emscripten::function("build_video_settings_packet(form)", &build_video_settings_packet);
    emscripten::function("parse_packet_type(data)", &parse_packet_type);
    emscripten::function("parse_layer_response_packet(data)", &parse_layer_response_packet);
    emscripten::function("decode_geoemtry(data, indices, vertices)", &decode_geoemtry);
}
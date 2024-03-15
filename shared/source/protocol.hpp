#pragma once

#include "types.hpp"

namespace shared
{
    // In addition to the websocket stream described by the following packets, the server also provides several functions based on HTTP Gets and Posts
    // [GET]  /scene                      -> Requests the avaliable scenes that the server could load.
    // [GET]  /motion_capture             -> Requests the avaliable motion capture that could be requested by the client.
    // [GET]  /motion_capture?id=<number> -> Requests the id-th motion capture in the list returned by "/motion_capture". 
    //                                       Server will respond with a json file that contains triplets of time, source perspective and destination perspective.
    // [POST] /motion_capture             -> Server will store the given motion capture.
    // [POST] /log                        -> Server will store the given table row in the specified file.
    // [POST] /image                      -> Server will store the given uncompressed RGBA8 image file.

    enum PacketType : uint32_t
    {
        PACKET_TYPE_SESSION_CREATE  = 0x00,
        PACKET_TYPE_SESSION_DESTROY = 0x01,
        PACKET_TYPE_RENDER_REQUEST  = 0x02,
        PACKET_TYPE_MESH_SETTINGS   = 0x03,
        PACKET_TYPE_VIDEO_SETTINGS  = 0x04,
        PACKET_TYPE_LAYER_RESPONSE  = 0x05
    };

    struct SessionCreatePacket
    {
        PacketType type = PACKET_TYPE_SESSION_CREATE;

        MeshGeneratorType mesh_generator = MESH_GENERATOR_TYPE_LOOP;
        VideoCodecType video_codec = VIDEO_CODEC_TYPE_H264;
        uint8_t video_use_chroma_subsampling = true;

        Matrix view_projection_matrix;
        uint32_t view_resolution_width = 1024;
        uint32_t view_resolution_height = 1024;
        uint32_t view_count = 1;
        uint32_t layer_count = 1;
        
        char scene_filename[SHARED_SCENE_FILE_NAME_LENGTH_MAX];
        float scene_scale = 1.0f;
        float scene_exposure = 1.0f;
        float scene_indirect_intensity = 1.0f;

        char sky_filename[SHARED_SKY_FILE_NAME_LENGTH_MAX];
        float sky_intensity = 1.0f;
    };

    struct SessionDestroyPacket
    {
        PacketType type = PACKET_TYPE_SESSION_DESTROY;

        // Nothing
    };

    struct RenderRequestPacket
    {
        PacketType type = PACKET_TYPE_RENDER_REQUEST;

        uint32_t request_id = 0;
        uint32_t request_exports = 0; // Bitfield defined by MeshRequestExport

        std::array<Matrix, SHARED_VIEW_COUNT_MAX> view_matrices;
    };

    struct MeshSettingsPacket
    {
        PacketType type = PACKET_TYPE_MESH_SETTINGS;

        LayerSettings layer;
        MeshSettings mesh;
    };

    struct VideoSettingsPacket
    {
        PacketType type = PACKET_TYPE_VIDEO_SETTINGS;

        VideoCodecMode mode = VIDEO_CODEC_MODE_CONSTANT_QUALITY;
        uint32_t framerate = 10;
        float bitrate = 1.0f;
        float quality = 0.5f;
    };

    struct LayerResponsePacket
    {
        PacketType type = PACKET_TYPE_LAYER_RESPONSE;

        uint32_t request_id = 0;
        uint32_t layer_index = 0;

        uint32_t geometry_bytes = 0;
        uint32_t image_bytes = 0;

        std::array<ViewMetadata, SHARED_VIEW_COUNT_MAX> view_metadata; // Measurements taken by the server for each view
        std::array<Matrix, SHARED_VIEW_COUNT_MAX> view_matrices;       // View matrices as received in the request
        std::array<uint32_t, SHARED_VIEW_COUNT_MAX> vertex_counts;     // Number of vertices for each view
        std::array<uint32_t, SHARED_VIEW_COUNT_MAX> index_counts;      // Number of indicies for each view

        // Followed by the encoded geometry of the layer consisting of geometry_bytes
        // Followed by the encoded image of the layer consisting of image_bytes

        // The package combines the geometry of all view into a single vertex array and index array.
        // The vertices and indices of all view are concatenated meaning that the vertex and index 
        // array start with the geometry of the first view and end with the geometry of the last view.
        // The sum of vertex_counts is equal to the length of the vertex array encoded in this packet.
        // The sum of index_counts is equal to the length of the index array encoded in this packet.

        // The package only contains a single encoded image that combines the images of all views.
        // The resolution of this combined image is (n * view_resolution_with) x (m * view_resolution_height), 
        // where n = view_count if view_count <= 3 else n = 3
        // where m = 1 if view_count <= 3 else m = 2.
        // The location of each view within the combined image is as follows:
        // +---+---+---+
        // | 0 | 1 | 2 |
        // +---+---+---+
        // | 3 | 4 | 5 |
        // +---+---+---+
    };
}
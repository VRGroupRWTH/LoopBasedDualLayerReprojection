#include "geometry_codec.hpp"
#include "huffman.hpp"
#include <limits>

using namespace i3ds;

bool GeometryCodec::encode(std::span<const Index> indices, std::span<const Vertex> vertices, std::vector<uint8_t>& buffer)
{
    std::vector<uint32_t> packet_indices;
    std::vector<uint16_t> packet_vertices;

    uint32_t last_index = 0;
    uint16_t last_vertex_x = 0;
    uint16_t last_vertex_y = 0;
    uint16_t last_vertex_depth = 0;

    for (const Index& index : indices)
    {
        uint32_t encoded_index = GeometryCodec::encode_delta((int32_t)index - (int32_t)last_index);

        packet_indices.push_back(encoded_index);

        last_index = index;
    }

    for (const Vertex& vertex : vertices)
    {
        uint16_t vertex_depth = (uint16_t)(vertex.z * 0x7FFF);

        uint16_t encoded_vertex_x = GeometryCodec::encode_delta((int16_t)vertex.x - (int16_t) last_vertex_x);
        uint16_t encoded_vertex_y = GeometryCodec::encode_delta((int16_t)vertex.y - (int16_t) last_vertex_y);
        uint16_t encoded_vertex_depth = GeometryCodec::encode_delta((int16_t)vertex_depth - (int16_t) last_vertex_depth);

        packet_vertices.push_back(encoded_vertex_x);
        packet_vertices.push_back(encoded_vertex_y);
        packet_vertices.push_back(encoded_vertex_depth);

        last_vertex_x = vertex.x;
        last_vertex_y = vertex.y;
        last_vertex_depth = vertex_depth;
    }

    std::vector<std::span<const uint8_t>> input_lists =
    {
        std::span<uint8_t>((uint8_t*)packet_indices.data(), packet_indices.size() * sizeof(uint32_t)),
        std::span<uint8_t>((uint8_t*)packet_vertices.data(), packet_vertices.size() * sizeof(uint16_t))
    };

    HuffmanCode huffman_code;

    if (!huffman_code.create(input_lists))
    {
        return false;
    }

    std::vector<uint8_t> index_bytes;
    std::vector<uint8_t> vertex_bytes;

    if (!huffman_code.encode(input_lists[0], index_bytes))
    {
        return false;
    }

    if (!huffman_code.encode(input_lists[1], vertex_bytes))
    {
        return false;
    }

    uint32_t header_offset = 0;
    uint32_t index_offset = sizeof(GeometryHeader);
    uint32_t vertex_offset = sizeof(GeometryHeader) + index_bytes.size();
    uint32_t buffer_size = sizeof(GeometryHeader) + index_bytes.size() + vertex_bytes.size();

    buffer.resize(buffer_size);

    GeometryHeader* header = (GeometryHeader*)(buffer.data() + header_offset);
    header->index_count = indices.size();
    header->index_bytes = index_bytes.size();
    header->vertex_count = vertices.size();
    header->vertex_bytes = vertex_bytes.size();
    huffman_code.export_code(header->huffman_lengths);

    memcpy(buffer.data() + index_offset, index_bytes.data(), index_bytes.size());
    memcpy(buffer.data() + vertex_offset, vertex_bytes.data(), vertex_bytes.size());

    return true;
}

bool GeometryCodec::decode(std::span<const uint8_t> buffer, std::vector<Index>& indices, std::vector<Vertex>& vertices)
{
    GeometryHeader* header = (GeometryHeader*)buffer.data();
    
    HuffmanCode huffman_code;

    if (!huffman_code.import_code(header->huffman_lengths))
    {
        return false;
    }

    uint32_t index_offset = sizeof(GeometryHeader);
    uint32_t vertex_offset = sizeof(GeometryHeader) + header->index_bytes;

    std::vector<uint32_t> packet_indices;
    std::vector<uint16_t> packet_vertices;

    packet_indices.resize(header->index_count);
    packet_vertices.resize(header->vertex_count * 3);

    std::span<uint8_t> index_bytes = std::span((uint8_t*)packet_indices.data(), packet_indices.size() * sizeof(uint32_t));
    std::span<uint8_t> vertex_bytes = std::span((uint8_t*)packet_vertices.data(), packet_vertices.size() * sizeof(uint16_t));

    if (!huffman_code.decode(std::span<const uint8_t>(buffer.data() + index_offset, header->index_bytes), index_bytes))
    {
        return false;
    }

    if (!huffman_code.decode(std::span(buffer.data() + vertex_offset, header->vertex_bytes), vertex_bytes))
    {
        return false;
    }
    
    indices.clear();
    vertices.clear();

    indices.reserve(header->index_count);
    vertices.reserve(header->vertex_count);

    uint32_t last_index = 0;
    uint16_t last_vertex_x = 0;
    uint16_t last_vertex_y = 0;
    uint16_t last_vertex_depth = 0;

    for (uint32_t offset = 0; offset < packet_indices.size(); offset++)
    {
        uint32_t encoded_index = packet_indices[offset];
        uint32_t index = GeometryCodec::decode_delta(encoded_index) + last_index;

        indices.push_back((Index)index);

        last_index = index;
    }

    for (uint32_t offset = 0; offset < packet_vertices.size(); offset += 3)
    {
        uint16_t encoded_vertex_x = packet_vertices[offset + 0];
        uint16_t encoded_vertex_y = packet_vertices[offset + 1];
        uint16_t encoded_vertex_depth = packet_vertices[offset + 2];

        uint16_t vertex_x = GeometryCodec::decode_delta(encoded_vertex_x) + last_vertex_x;
        uint16_t vertex_y = GeometryCodec::decode_delta(encoded_vertex_y) + last_vertex_y;
        uint16_t vertex_depth = GeometryCodec::decode_delta(encoded_vertex_depth) + last_vertex_depth;

        Vertex vertex;
        vertex.x = vertex_x;
        vertex.y = vertex_y;
        vertex.z = (float)vertex_depth / (float)0x7FFF;

        vertices.push_back(vertex);

        last_vertex_x = vertex_x;
        last_vertex_y = vertex_y;
        last_vertex_depth = vertex_depth;
    }

    return true;
}

uint16_t GeometryCodec::encode_delta(int16_t delta)
{
    uint16_t encoded_delta = 0;

    if (delta < 0)
    {
        encoded_delta = ((uint16_t)-delta) << 0x01;
        encoded_delta |= 0x01;
    }

    else
    {
        encoded_delta = ((uint16_t)delta) << 0x01;
    }

    return encoded_delta;
}

uint32_t GeometryCodec::encode_delta(int32_t delta)
{
    uint32_t encoded_delta = 0;

    if (delta < 0)
    {
        encoded_delta = ((uint32_t)-delta) << 0x01;
        encoded_delta |= 0x01;
    }

    else
    {
        encoded_delta = ((uint32_t)delta) << 0x01;
    }

    return encoded_delta;
}

int16_t GeometryCodec::decode_delta(uint16_t encoded_delta)
{
    int16_t delta = encoded_delta >> 0x01;

    if ((encoded_delta & 0x01) != 0)
    {
        delta = -delta;
    }

    return delta;
}

int32_t GeometryCodec::decode_delta(uint32_t encoded_delta)
{
    int32_t delta = encoded_delta >> 0x01;

    if ((encoded_delta & 0x01) != 0)
    {
        delta = -delta;
    }

    return delta;
}

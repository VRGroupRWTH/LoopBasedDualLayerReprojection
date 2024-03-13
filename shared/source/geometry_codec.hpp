#pragma once

#include <cstdint>
#include <vector>
#include <span>
#include <array>
#include "protocol.hpp"

namespace i3ds
{
    struct GeometryHeader
    {
        std::array<uint8_t, 256> huffman_lengths;
        uint32_t index_count = 0;
        uint32_t index_bytes = 0;
        uint32_t vertex_count = 0;
        uint32_t vertex_bytes = 0;
    };

    class GeometryCodec
    {
    public:
        GeometryCodec() = delete;

        static bool encode(std::span<const Index> indices, std::span<const Vertex> vertices, std::vector<uint8_t>& buffer);
        static bool decode(std::span<const uint8_t> buffer, std::vector<Index>& indices, std::vector<Vertex>& vertices);

    private:
        static uint16_t encode_delta(int16_t delta);
        static uint32_t encode_delta(int32_t delta);
        static int16_t decode_delta(uint16_t encoded_delta);
        static int32_t decode_delta(uint32_t encoded_delta);
    };
}

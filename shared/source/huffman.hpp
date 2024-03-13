#pragma once

#include <vector>
#include <array>
#include <span>
#include <memory>
#include <cstdint>

namespace i3ds
{
    struct HuffmanNode
    {
        uint32_t symbol = 0;
        uint64_t code = 0;
        uint32_t code_length = 0;
        float probability = 0.0f;

        HuffmanNode* left = nullptr;
        HuffmanNode* right = nullptr;
    };

    class HuffmanCode
    {
    private:
        HuffmanNode* root_node = nullptr;
        std::vector<HuffmanNode*> leave_nodes;

    public:
        HuffmanCode() = default;
        ~HuffmanCode();

        bool create(const std::vector<std::span<const uint8_t>>& input_lists);
        void destroy();

        bool import_code(const std::array<uint8_t, 256>& huffman_lengths);
        void export_code(std::array<uint8_t, 256>& huffman_lengths) const;

        bool encode(std::span<const uint8_t> input_list, std::vector<uint8_t>& output_list) const;
        bool decode(std::span<const uint8_t> input_list, std::span<uint8_t> output_list) const;

    private:
        void destroy(HuffmanNode* node);
        bool assign_codes(HuffmanNode* node, uint64_t code, uint32_t code_length);
    };
}

#include "huffman.hpp"
#include <algorithm>

using namespace i3ds;

HuffmanCode::~HuffmanCode()
{
    this->destroy();
}

bool HuffmanCode::create(const std::vector<std::span<const uint8_t>>& input_lists)
{
    if (this->root_node != nullptr)
    {
        this->destroy();
    }

    std::vector<HuffmanNode*> node_list;
    node_list.resize(256, nullptr);

    for (uint32_t index = 0; index < 256; index++)
    {
        HuffmanNode* node = new HuffmanNode;
        node->symbol = index;

        node_list[index] = node;
    }

    uint32_t input_count = 0;

    for (uint32_t index = 0; index < input_lists.size(); index++)
    {
        input_count += input_lists[index].size();
    }

    if (input_count > 0)
    {
        for (const std::span<const uint8_t>& input_list : input_lists)
        {
            for (uint32_t index = 0; index < input_list.size(); index++)
            {
                uint8_t input = input_list[index];

                node_list[input]->probability += 1.0f;
            } 
        }
       
        for (HuffmanNode* node : node_list)
        {
            node->probability /= (float)input_count;
        }
    }

    else
    {
        for (HuffmanNode* node : node_list)
        {
            node->probability = 1.0f / (float)node_list.size();
        }
    }

    this->leave_nodes.insert(this->leave_nodes.end(), node_list.begin(), node_list.end());

    std::sort(node_list.begin(), node_list.end(), [](HuffmanNode* node1, HuffmanNode* node2)
    {
        return node1->probability > node2->probability;
    });

    while (node_list.size() > 1)
    {
        HuffmanNode* node2 = node_list.back();
        node_list.pop_back();
        HuffmanNode* node1 = node_list.back(); 
        node_list.pop_back();

        HuffmanNode* node = new HuffmanNode;
        node->probability = node1->probability + node2->probability;
        node->left = node1;
        node->right = node2;

        for (int32_t index = node_list.size() - 1; index >= 0; index--)
        {
            if (node_list[index]->probability > node->probability)
            {
                node_list.insert(node_list.begin() + index + 1, node);
                node = nullptr;

                break;
            }
        }

        if (node != nullptr)
        {
            node_list.insert(node_list.begin(), node);
        }
    }

    this->root_node = node_list.front();

    if (!this->assign_codes(this->root_node, 0, 0))
    {
        return false;
    }

    std::array<uint8_t, 256> huffman_lengths;
    this->export_code(huffman_lengths);

    if (!this->import_code(huffman_lengths))
    {
        return false;
    }

    return true;
}

void HuffmanCode::destroy()
{
    this->destroy(this->root_node);

    this->root_node = nullptr;
    this->leave_nodes.clear();
}

// Huffman tree encoding taken from https://www.w3.org/Graphics/PNG/RFC-1951
bool HuffmanCode::import_code(const std::array<uint8_t, 256>& huffman_lengths)
{
    if (this->root_node != nullptr)
    {
        this->destroy();
    }

    std::array<uint32_t, 256> length_count;
    length_count.fill(0);

    for (uint32_t index = 0; index < huffman_lengths.size(); index++)
    {
        length_count[huffman_lengths[index]] += 1;
    }

    std::array<uint64_t, 256> base_codes;
    base_codes[0] = 0;

    for (uint32_t index = 1; index < base_codes.size(); index++)
    {
        base_codes[index] = (base_codes[index - 1] + length_count[index - 1]) << 1;
    }

    this->leave_nodes.resize(256, nullptr);
    this->root_node = new HuffmanNode;

    for (uint32_t index = 0; index < this->leave_nodes.size(); index++)
    {
        uint32_t code_length = huffman_lengths[index];
        uint64_t code = base_codes[code_length];
        base_codes[code_length] += 1;
        
        HuffmanNode* node = this->root_node;

        for (int32_t code_bit = code_length - 1; code_bit >= 0; code_bit--)
        {
            if (((code >> code_bit) & 0x01) == 0)
            {
                if (node->left == nullptr)
                {
                    node->left = new HuffmanNode;
                }

                node = node->left;
            }

            else
            {
                if (node->right == nullptr)
                {
                    node->right = new HuffmanNode;
                }

                node = node->right;
            }
        }

        node->symbol = index;
        node->code = code;
        node->code_length = code_length;

        this->leave_nodes[index] = node;
    }

    if(!this->assign_codes(this->root_node, 0, 0))
    {
        return false;
    }

    return true;
}

void HuffmanCode::export_code(std::array<uint8_t, 256>& huffman_lengths) const
{
    for (uint32_t index = 0; index < this->leave_nodes.size(); index++)
    {
        huffman_lengths[index] = this->leave_nodes[index]->code_length;
    }
}

bool HuffmanCode::encode(std::span<const uint8_t> input_list, std::vector<uint8_t>& output_list) const
{
    output_list.clear();
    output_list.reserve(input_list.size());

    uint64_t code_buffer = 0;
    uint32_t code_buffer_size = 0;

    for (uint32_t index = 0; index < input_list.size(); index++)
    {
        uint32_t input = input_list[index];
        HuffmanNode* node = this->leave_nodes[input];

        code_buffer = (code_buffer << node->code_length) | node->code;
        code_buffer_size += node->code_length;

        while (code_buffer_size >= 8)
        {
            uint8_t output = (code_buffer >> (code_buffer_size - 8)) & 0xFF;
            code_buffer_size -= 8;

            output_list.push_back(output);
        }
    }

    if (code_buffer_size > 0)
    {
        uint8_t output = (code_buffer & 0xFF) << (8 - code_buffer_size);

        output_list.push_back(output);
    }

    return true;
}

bool HuffmanCode::decode(std::span<const uint8_t> input_list, std::span<uint8_t> output_list) const
{
    uint32_t input_buffer = 0;
    uint32_t input_buffer_size = 0;
    uint32_t input_offset = 0;

    for (uint32_t index = 0; index < output_list.size(); index++)
    {
        HuffmanNode* current_node = this->root_node;

        while (current_node->left != nullptr && current_node->right != nullptr)
        {
            if (input_buffer_size == 0)
            {
                if (input_offset >= input_list.size())
                {
                    return false;
                }

                input_buffer = input_list[input_offset];
                input_buffer_size = 8;
                input_offset++;
            }

            if ((input_buffer & 0x80) == 0)
            {
                current_node = current_node->left;
            }

            else
            {
                current_node = current_node->right;
            }

            input_buffer = (input_buffer << 1) & 0xFF;
            input_buffer_size--;
        }

        uint32_t output = current_node->symbol;
        output_list[index] = output;
    }

    return true;
}

void HuffmanCode::destroy(HuffmanNode* node)
{
    if (node == nullptr)
    {
        return;
    }

    this->destroy(node->left);
    this->destroy(node->right);

    delete node;
}

bool HuffmanCode::assign_codes(HuffmanNode* node, uint64_t code, uint32_t code_length)
{
    if (node == nullptr)
    {
        return true;
    }

    if (code_length > 64)
    {
        return false;
    }

    node->code = code;
    node->code_length = code_length;
    
    if(!this->assign_codes(node->left, (code << 1) | 0x00, code_length + 1))
    {
        return false;
    }

    if(!this->assign_codes(node->right, (code << 1) | 0x01, code_length + 1))
    {
        return false;
    }

    return true;
}

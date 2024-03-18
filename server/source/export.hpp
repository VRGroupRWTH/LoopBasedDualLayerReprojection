#ifndef HEADER_EXPORT
#define HEADER_EXPORT

#include <glm/glm.hpp>
#include <types.hpp>
#include <vector>
#include <string>
#include <cstdint>

#include "mesh_generator/mesh_generator.hpp"

bool export_color_image(const std::string& file_name, const glm::uvec2& resolution, uint8_t* data, uint32_t size);
bool export_depth_image(const std::string& file_name, const glm::uvec2& resolution, uint8_t* data, uint32_t size);
bool export_mesh(const std::string& file_name, const std::vector<shared::Vertex>& vertices, const std::vector<shared::Index>& indices, const glm::mat4& view_matrix, const glm::mat4& projection_matrix, const glm::uvec2& resolution);
bool export_feature_lines(const std::string& file_name, const std::vector<MeshFeatureLine>& feature_lines);

#endif
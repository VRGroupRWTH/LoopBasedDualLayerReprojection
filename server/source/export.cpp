#include "export.hpp"
#include <spdlog/spdlog.h>
#include <glm/gtx/range.hpp>
#include <fstream>

bool export_color_image(const std::string& file_name, const glm::uvec2& resolution, uint8_t* data, uint32_t size)
{
	if (!file_name.ends_with(".ppm"))
	{
		spdlog::error("Export: File format not supported!");

		return false;
	}

	std::fstream file(file_name, std::ios::out | std::ios::binary);

	if (!file.good())
	{
		spdlog::error("Export: Can't open image file!");

		return false;
	}

	uint32_t required_size = resolution.x * resolution.y * sizeof(glm::u8vec3);

	if (size != required_size)
	{
		spdlog::error("Export: Inalid image size!");

		return false;
	}

	file << "P6" << std::endl;
	file << resolution.x << " " << resolution.y << std::endl;
	file << "255" << std::endl;

	file.write((char*)data, size);

	file.close();

	return true;
}

bool export_depth_image(const std::string& file_name, const glm::uvec2& resolution, uint8_t* data, uint32_t size)
{
	if (!file_name.ends_with(".pfm"))
	{
		spdlog::error("Export: File format not supported!");

		return false;
	}

	std::fstream file(file_name, std::ios::out | std::ios::binary);

	if (!file.good())
	{
		spdlog::error("Export: Can't open image file!");

		return false;
	}

	uint32_t required_size = resolution.x * resolution.y * sizeof(float);

	if (size != required_size)
	{
		spdlog::error("Export: Inalid image size!");

		return false;
	}

	file << "Pf" << std::endl;
	file << resolution.x << " " << resolution.y << std::endl;
	file << "-1.0" << std::endl;

	file.write((char*)data, size);

	file.close();

	return true;
}

bool export_mesh(const std::string& file_name, const std::vector<shared::Vertex>& vertices, const std::vector<shared::Index>& indices, const glm::mat4& view_matrix, const glm::mat4& projection_matrix)
{
	if (!file_name.ends_with(".obj"))
	{
		spdlog::error("Export: File format not supported!");

		return false;
	}

	std::fstream file(file_name, std::ios::out);

	if (!file.good())
	{
		spdlog::error("Export: Can't open image file!");

		return false;
	}

	// Matrix column major
	file << "# view_matrix";

	for (float value : view_matrix)
	{
		file << " " << value;
	}

	file << std::endl;

	// Matrix column major
	file << "# projection_matrix";

	for (float value : projection_matrix)
	{
		file << " " << value;
	}

	file << std::endl;

	for (const shared::Vertex& vertex : vertices)
	{
		file << "v " << vertex.x << " " << vertex.y << " " << vertex.z << std::endl;
	}

	file << "o layer_mesh" << std::endl;

	for (uint32_t index = 0; index < indices.size(); index += 3)
	{
		file << "f " << (indices[index] + 1) << " " << (indices[index + 1] + 1) << " " << (indices[index + 2] + 1) << std::endl;
	}

	file.close();

	return true;
}

bool export_feature_lines(const std::string& file_name, const std::vector<MeshFeatureLine>& feature_lines)
{
	if (!file_name.ends_with(".obj"))
	{
		spdlog::error("Export: File format not supported!");

		return false;
	}

	std::fstream file(file_name, std::ios::out);

	if (!file.good())
	{
		spdlog::error("Export: Can't open image file!");

		return false;
	}

	for (const MeshFeatureLine& feature_line : feature_lines)
	{
		file << "v " << feature_line.start.x << " " << feature_line.start.y << " " << feature_line.id << std::endl;
		file << "v " << feature_line.end.x << " " << feature_line.end.y << " " << feature_line.id << std::endl;
	}

	for (uint32_t index = 0; index < feature_lines.size(); index++)
	{
		file << "l " << (index * 2 + 1) << " " << (index * 2 + 2) << std::endl;
	}

	file.close();

	return true;
}
#ifndef HEADER_MESH_GENERATOR
#define HEADER_MESH_GENERATOR

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>
#include <types.hpp>

enum MeshGeneratorType
{
    MESH_GENERATOR_TYPE_LINE_BASED,
    MESH_GENERATOR_TYPE_LOOP_BASED
};

class MeshGeneratorFrame
{
public:
    MeshGeneratorFrame() = default;
    virtual ~MeshGeneratorFrame() = default;
    
    virtual bool triangulate(std::vector<shared::Vertex>& vertices, std::vector<shared::Index>& indices, shared::ViewMetadata& metadata) = 0;

    virtual GLuint get_depth_buffer() const = 0;
    virtual GLuint get_normal_buffer() const = 0;
    virtual GLuint get_object_id_buffer() const = 0;
};

class MeshGenerator
{
public:
    MeshGenerator() = default;
    virtual ~MeshGenerator() = default;

    virtual bool create(const glm::uvec2& resolution) = 0;
    virtual void destroy() = 0;
    virtual void apply(const shared::MeshSettings& settings) = 0;

    virtual MeshGeneratorFrame* create_frame() = 0;
    virtual void destroy_frame(MeshGeneratorFrame* frame) = 0;
    virtual bool submit_frame(MeshGeneratorFrame* frame) = 0;
    virtual bool map_frame(MeshGeneratorFrame* frame) = 0;
    virtual bool unmap_frame(MeshGeneratorFrame* frame) = 0;
};

MeshGenerator* make_mesh_generator(MeshGeneratorType type);

#endif
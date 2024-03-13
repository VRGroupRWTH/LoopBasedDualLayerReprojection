#ifndef HEADER_SCENE
#define HEADER_SCENE

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>
#include <array>
#include <optional>
#include <string>

#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#include "shader.hpp"

#define SCENE_INDIRECT_MEMORY_LIMIT      1024 //MiB
#define SCENE_INDIRECT_DEFAULT_CELL_SIZE 0.1f
#define SCENE_SKY_SPHERE_RINGS           16
#define SCENE_SKY_SPHERE_SEGMENTS        32
#define SCENE_LIGHT_NEAR_DISTANCE        0.01f

namespace glsl
{
    using namespace glm;
    typedef uint32_t uint;
#include "../shaders/shared_defines.glsl"
}

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec2 texture_coord;
};

struct Texture
{
    std::string file_name;

    uint32_t image_width = 0;
    uint32_t image_height = 0;
    GLuint image_buffer = 0;
};

struct Material
{
    int32_t material_index = -1;

    Texture* base_color_texture = nullptr;
    Texture* material_texture = nullptr;
    Texture* normal_texture = nullptr;
    Texture* emissive_texture = nullptr;

    glm::vec3 base_color = glm::vec3(1.0f);
    glm::vec3 emissive_color = glm::vec3(0.0f);
    float opacity = 1.0f;
    float roughness = 1.0f;
    float metallic = 0.0f;
    bool is_two_sided = false;
    bool is_shadow_caster = true;
};

struct Object
{
    uint32_t id = 0;
    Material* material = nullptr;

    GLuint vertex_buffer;
    uint32_t vertex_count;
};

struct Light
{
    uint32_t type = SCENE_LIGHT_TYPE_DIRECTIONAL;

    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 direction = glm::vec3(-1.0f);
    glm::vec3 color = glm::vec3(1.0f);
    float inner_angle = glm::pi<float>() / 2.0f;
    float outer_angle = glm::pi<float>() / 4.0f;

    glm::mat4 light_projection_matrix;
    std::vector<glm::mat4> light_matrix;

    uint32_t light_array_offset = 0;
    uint32_t light_array_size = 0;
    uint32_t light_layer_index = 0;

    glm::vec2 light_image_plane_size = glm::vec2(1.0f);
    float light_image_plane_near = 0.0f;

    std::vector<GLuint> light_frame_buffers;
    std::vector<GLuint> light_depth_buffers;
};

class Scene
{
private:
    std::vector<Light> lights;
    std::vector<Object> objects;
    std::vector<Material*> materials;
    std::vector<Texture*> textures;
    GLuint vertex_array = 0;

    Material* default_material = nullptr;
    Texture* default_base_color_texture = nullptr;
    Texture* default_material_texture = nullptr;
    Texture* default_normal_texture = nullptr;
    Texture* default_emissive_texture = nullptr;

    GLuint light_buffer = 0;
    GLuint light_depth_array_buffer = 0;
    GLuint light_depth_cube_array_buffer = 0;
    GLuint light_flux_buffer = 0;
    GLuint light_normal_buffer = 0;

    glm::uvec3 indirect_cell_count = glm::uvec3(0);
    glm::vec3 indirect_cell_size = glm::vec3(0.0f);
    glm::vec3 indirect_domain_min = glm::vec3(0.0f);
    glm::vec3 indirect_domain_max = glm::vec3(0.0f);
    uint32_t indirect_iteration_count = 0;

    GLuint indirect_inject_vertex_array = 0;
    GLuint indirect_inject_frame_buffer = 0;

    std::array<GLuint, 3> indirect_red_distribution_buffers;
    std::array<GLuint, 3> indirect_green_distribution_buffers;
    std::array<GLuint, 3> indirect_blue_distribution_buffers;
    std::array<GLuint, 3> indirect_visibility_buffers;
    GLuint indirect_opacity_buffer = 0;

    Shader light_shader = { "Scene Light Shader" };
    Shader indirect_inject_shader = { "Scene Indirect Inject Shader" };
    Shader indirect_visibility_shader = { "Scene Indirect Visibility Shader" };
    Shader indirect_opacity_shader = { "Scene Indirect Opacity Shader" };
    Shader indirect_propagate_shader = { "Scene Indirect Propagate Shader" };

    glm::vec3 scene_min = glm::vec3(0.0f);
    glm::vec3 scene_max = glm::vec3(0.0f);

    glm::vec3 ambient_color = glm::vec3(0.0f);
    float indirect_intensity = 1.0f;
    float exposure = 1.0f;
    float scale = 1.0f;

public:
    Scene() = default;

    bool create(const std::string& scene_file_name, float scale, float exposure, float indirect_intensity, std::optional<std::string> sky_file_name, float sky_intensity = 1.0f);
    void destroy();

    void render(const Shader& shader) const;

private:
    bool create_defaults();
    bool create_materials(const aiScene* scene, const std::string& scene_file_name);
    bool create_objects(const aiScene* scene, float scale);
    bool create_light(const aiScene* scene, float scale);
    bool create_sky(const std::string& sky_file_name, float sky_intensity);
    bool create_material_texture_from_material(const aiMaterial* scene_material, const std::string& scene_file_name, Texture*& texture, bool& use_roughness_factor, bool& use_metallic_factor);
    bool create_texture_from_material(const aiMaterial* scene_material, const std::string& scene_file_name, bool use_srgb, aiTextureType type, Texture*& texture);
    bool create_texture_from_sky_file(const std::string& file_name, Texture*& texture);
    bool create_texture_from_regular_file(const std::string& file_name, bool use_srgb, Texture*& texture);
    bool create_texture_from_high_bitdepth_file(const std::string& file_name, bool use_srgb, Texture*& texture);
    bool create_texture_from_compressed_file(const std::string& file_name, bool use_srgb, Texture*& texture);
    bool create_texture_from_color(const glm::vec4& color, GLenum format, Texture*& texture);

    void compute_indirect_domain();

    bool create_shaders();
    bool create_buffers();
    bool create_color_distribution_buffers(std::array<GLuint, 3>& distribution_buffers);
    void compute_light(const Light& light, uint32_t array_index);
    void compute_indirect();

    void destroy_buffers();
    void destroy_objects();
    void destroy_materials();
    void destroy_textures();
};

#endif

#include "scene.hpp"
#include "shader.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <gli/gli.hpp>
#include <ImfRgbaFile.h>
#include <ImfArray.h>
#include <spdlog/spdlog.h>
#include <memory>

bool Scene::create(const std::string& scene_file_name, float scale, float exposure, float indirect_intensity, std::optional<std::string> sky_file_name, float sky_intensity)
{
    spdlog::info("Scene: Loading scene '{}'", scene_file_name);

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(scene_file_name.c_str(), aiProcess_PreTransformVertices | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_MakeLeftHanded | aiProcess_CalcTangentSpace | aiProcess_FlipUVs | aiProcess_GenSmoothNormals);

    if (scene == nullptr)
    {
        return false;
    }

    if (!this->create_defaults())
    {
        return false;
    }

    if (!this->create_materials(scene, scene_file_name))
    {
        return false;
    }

    if (!this->create_objects(scene, scale))
    {
        return false;
    }

    if (!this->create_light(scene, scale))
    {
        return false;
    }

    if (sky_file_name.has_value())
    {
        if (!this->create_sky(sky_file_name.value(), sky_intensity))
        {
            return false;
        }
    }

    this->compute_indirect_domain();

    if (!this->create_shaders())
    {
        return false;
    }

    if (!this->create_buffers())
    {
        return false;
    }

    this->compute_indirect();

    this->scale = scale;
    this->exposure = exposure;
    this->indirect_intensity = indirect_intensity;

    spdlog::info("Scene: Scene loaded");

    return true;
}

void Scene::destroy()
{
    this->destroy_buffers();
    this->destroy_objects();
    this->destroy_materials();
    this->destroy_textures();
}

void Scene::render(const Shader& shader) const
{
    glBindVertexArray(this->vertex_array);
    
    bool is_two_sided = false;
    glEnable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    shader["scene_ambient_color"] = this->ambient_color;
    shader["scene_exposure"] = this->exposure;

    shader["scene_light_count"] = (uint32_t)this->lights.size();

    shader["scene_indirect_intensity"] = this->indirect_intensity;
    shader["scene_indirect_cell_size"] = this->indirect_cell_size;
    shader["scene_indirect_domain_min"] = this->indirect_domain_min;
    shader["scene_indirect_domain_max"] = this->indirect_domain_max;

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SCENE_LIGHT_BUFFER_BINDING_POINT, this->light_buffer);

    glActiveTexture(GL_TEXTURE0 + SCENE_LIGHT_DEPTH_ARRAY_BUFFER_BINDING_POINT);
    glBindTexture(GL_TEXTURE_2D_ARRAY, this->light_depth_array_buffer);
    glActiveTexture(GL_TEXTURE0 + SCENE_LIGHT_DEPTH_CUBE_ARRAY_BUFFER_BINDING_POINT);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, this->light_depth_cube_array_buffer);

    glActiveTexture(GL_TEXTURE0 + SCENE_INDIRECT_RED_DISTRIBUTION_BUFFER_BINDING_POINT);
    glBindTexture(GL_TEXTURE_3D, this->indirect_red_distribution_buffers[2]);
    glActiveTexture(GL_TEXTURE0 + SCENE_INDIRECT_GREEN_DISTRIBUTION_BUFFER_BINDING_POINT);
    glBindTexture(GL_TEXTURE_3D, this->indirect_green_distribution_buffers[2]);
    glActiveTexture(GL_TEXTURE0 + SCENE_INDIRECT_BLUE_DISTRIBUTION_BUFFER_BINDING_POINT);
    glBindTexture(GL_TEXTURE_3D, this->indirect_blue_distribution_buffers[2]);
    glActiveTexture(GL_TEXTURE0 + SCENE_INDIRECT_OPACITY_BUFFER_BINDING_POINT);
    glBindTexture(GL_TEXTURE_3D, this->indirect_opacity_buffer);

    for(const Object& object : this->objects)
    {
        const Material* material = object.material;

        shader["scene_object_id"] = object.id;
        shader["scene_material_base_color"] = material->base_color;
        shader["scene_material_emissive_color"] = material->emissive_color;
        shader["scene_material_opacity"] = material->opacity;
        shader["scene_material_roughness"] = material->roughness;
        shader["scene_material_metallic"] = material->metallic;

        glActiveTexture(GL_TEXTURE0 + SCENE_MATERIAL_BASE_COLOR_TEXTURE_BINDING_POINT);
        glBindTexture(GL_TEXTURE_2D, material->base_color_texture->image_buffer);
        glActiveTexture(GL_TEXTURE0 + SCENE_MATERIAL_MATERIAL_TEXTURE_BINDING_POINT);
        glBindTexture(GL_TEXTURE_2D, material->material_texture->image_buffer);
        glActiveTexture(GL_TEXTURE0 + SCENE_MATERIAL_NORMAL_TEXTURE_BINDING_POINT);
        glBindTexture(GL_TEXTURE_2D, material->normal_texture->image_buffer);
        glActiveTexture(GL_TEXTURE0 + SCENE_MATERIAL_EMISSIVE_TEXTURE_BINDING_POINT);
        glBindTexture(GL_TEXTURE_2D, material->emissive_texture->image_buffer);

        if (is_two_sided != material->is_two_sided)
        {
            if (material->is_two_sided)
            {
                glDisable(GL_CULL_FACE);
            }

            else
            {
                glEnable(GL_CULL_FACE);
            }

            is_two_sided = material->is_two_sided;
        }

        glBindVertexBuffer(0, object.vertex_buffer, 0, sizeof(Vertex));

        glDrawArrays(GL_TRIANGLES, 0, object.vertex_count);
    }

    glActiveTexture(GL_TEXTURE0 + SCENE_MATERIAL_BASE_COLOR_TEXTURE_BINDING_POINT);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0 + SCENE_MATERIAL_MATERIAL_TEXTURE_BINDING_POINT);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0 + SCENE_MATERIAL_NORMAL_TEXTURE_BINDING_POINT);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0 + SCENE_MATERIAL_EMISSIVE_TEXTURE_BINDING_POINT);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0 + SCENE_LIGHT_DEPTH_ARRAY_BUFFER_BINDING_POINT);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    glActiveTexture(GL_TEXTURE0 + SCENE_LIGHT_DEPTH_CUBE_ARRAY_BUFFER_BINDING_POINT);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);
    glActiveTexture(GL_TEXTURE0 + SCENE_INDIRECT_RED_DISTRIBUTION_BUFFER_BINDING_POINT);
    glBindTexture(GL_TEXTURE_3D, 0);
    glActiveTexture(GL_TEXTURE0 + SCENE_INDIRECT_GREEN_DISTRIBUTION_BUFFER_BINDING_POINT);
    glBindTexture(GL_TEXTURE_3D, 0);
    glActiveTexture(GL_TEXTURE0 + SCENE_INDIRECT_BLUE_DISTRIBUTION_BUFFER_BINDING_POINT);
    glBindTexture(GL_TEXTURE_3D, 0);
    glActiveTexture(GL_TEXTURE0 + SCENE_INDIRECT_OPACITY_BUFFER_BINDING_POINT);
    glBindTexture(GL_TEXTURE_3D, 0);
    glActiveTexture(GL_TEXTURE0);

    glDisable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    glBindVertexArray(0);
}

bool Scene::is_file_supported(const std::string& extension)
{
    std::vector<std::string> supported_list =
    {
        ".fbx",
        ".gltf",
        ".glb",
        ".obj"
    };

    for (const std::string& supported : supported_list)
    {
        if (extension == supported)
        {
            return true;
        }
    }

    return false;
}

bool Scene::create_defaults()
{
    if (!this->create_texture_from_color(glm::vec4(1.0f), GL_SRGB8_ALPHA8, this->default_base_color_texture))
    {
        return false;
    }

    if (!this->create_texture_from_color(glm::vec4(0.0f, 1.0f, 0.0f, 0.0f), GL_RGBA8, this->default_material_texture))
    {
        return false;
    }

    if (!this->create_texture_from_color(glm::vec4(0.5f, 0.5f, 1.0f, 0.0f), GL_RGBA8, this->default_normal_texture))
    {
        return false;
    }

    if (!this->create_texture_from_color(glm::vec4(0.0f), GL_RGBA8, this->default_emissive_texture))
    {
        return false;
    }

    this->default_base_color_texture->file_name = "default_base_color_texture";
    this->default_material_texture->file_name = "default_material_texture";
    this->default_normal_texture->file_name = "default_normal_texture";
    this->default_emissive_texture->file_name = "default_emissive_texture";

    this->default_material = new Material;
    this->default_material->base_color_texture = this->default_base_color_texture;
    this->default_material->material_texture = this->default_material_texture;
    this->default_material->normal_texture = this->default_normal_texture;
    this->default_material->emissive_texture = this->default_emissive_texture;
    this->default_material->base_color = glm::vec3(1.0f);
    this->default_material->emissive_color = glm::vec3(0.0f);
    this->default_material->opacity = 1.0f;
    this->default_material->roughness = 1.0f;
    this->default_material->metallic = 0.0f;
    this->default_material->is_shadow_caster = true;
    this->default_material->is_two_sided = true;

    this->materials.push_back(this->default_material);

    return true;
}

bool Scene::create_materials(const aiScene* scene, const std::string& scene_file_name)
{
    for (uint32_t material_index = 0; material_index < scene->mNumMaterials; material_index++)
    {
        const aiMaterial* scene_material = scene->mMaterials[material_index];

        aiColor3D diffuse_color = aiColor3D(0.0f, 0.0f, 0.0f);
        scene_material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse_color);

        aiColor3D emissive_color = aiColor3D(0.0f, 0.0f, 0.0f);
        scene_material->Get(AI_MATKEY_COLOR_EMISSIVE, emissive_color);

        float opacity = 1.0f;
        scene_material->Get(AI_MATKEY_OPACITY, opacity);

        float shininess = 0.0f;
        scene_material->Get(AI_MATKEY_SHININESS, shininess);

        float reflectivity = 0.0f;
        scene_material->Get(AI_MATKEY_REFLECTIVITY, reflectivity);

        bool two_sided = false;
        scene_material->Get(AI_MATKEY_TWOSIDED, two_sided);

        std::string material_name = scene_material->GetName().C_Str();

        if (material_name.find("DoubleSided") != std::string::npos)
        {
            two_sided = true;
        }

        Material* material = new Material;
        material->material_index = material_index;
        material->base_color = glm::vec3(1.0f);
        material->emissive_color = glm::vec3(1.0f);
        material->opacity = 1.0f;
        material->roughness = 1.0f;
        material->metallic = 1.0f;
        material->is_shadow_caster = true;
        material->is_two_sided = two_sided;

        if (!this->create_texture_from_material(scene_material, scene_file_name, true, aiTextureType_DIFFUSE, material->base_color_texture))
        {
            material->base_color_texture = this->default_base_color_texture;

            material->base_color = glm::make_vec3(&diffuse_color.r);
            material->opacity = glm::clamp(opacity, 0.0f, 1.0f);
        }

        if (!this->create_texture_from_material(scene_material, scene_file_name, false, aiTextureType_SPECULAR, material->material_texture))
        {
            bool use_roughness_factor = true;
            bool use_metallic_factor = true;

            if (!this->create_material_texture_from_material(scene_material, scene_file_name, material->material_texture, use_roughness_factor, use_metallic_factor))
            {
                material->material_texture = this->default_material_texture;

                use_roughness_factor = true;
                use_metallic_factor = true;
            }

            if (use_roughness_factor)
            {
                material->roughness = glm::clamp(1.0f - (glm::sqrt(shininess) / 10.0f), 0.0f, 1.0f);
            }

            if (use_metallic_factor)
            {
                material->metallic = glm::clamp(reflectivity, 0.0f, 1.0f);
            }
        }

        if (!this->create_texture_from_material(scene_material, scene_file_name, false, aiTextureType_NORMALS, material->normal_texture))
        {
            material->normal_texture = this->default_normal_texture;
        }

        if (!this->create_texture_from_material(scene_material, scene_file_name, false, aiTextureType_EMISSIVE, material->emissive_texture))
        {
            material->emissive_texture = this->default_emissive_texture;

            material->emissive_color = glm::make_vec3(&emissive_color.r);
        }

        this->materials.push_back(material);
    }

    return true;
}

bool Scene::create_objects(const aiScene* scene, float scale)
{
    for (uint32_t mesh_index = 0; mesh_index < scene->mNumMeshes; mesh_index++)
    {
        const aiMesh* scene_mesh = scene->mMeshes[mesh_index];

        if ((scene_mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE) == 0)
        {
            continue;
        }

        std::vector<Vertex> vertices;

        for (uint32_t face_index = 0; face_index < scene_mesh->mNumFaces; face_index++)
        {
            const aiFace& face = scene_mesh->mFaces[face_index];

            for (uint32_t vertex_index = 0; vertex_index < 3; vertex_index++)
            {
                uint32_t index = face.mIndices[vertex_index];

                const aiVector3D& position = scene_mesh->mVertices[index];
                const aiVector3D& normal = scene_mesh->mNormals[index];
                const aiVector3D& tangent = scene_mesh->mTangents[index];
                const aiVector3D& bitangent = scene_mesh->mBitangents[index];
                const aiVector3D& texture_coord = scene_mesh->mTextureCoords[0][index];

                Vertex vertex;
                vertex.position = glm::make_vec3(&position.x) * scale;
                vertex.normal = glm::make_vec3(&normal.x);
                vertex.tangent = glm::make_vec3(&tangent.x);
                vertex.texture_coord = glm::make_vec2(&texture_coord.x);

                glm::vec3 vertex_bitangent = glm::make_vec3(&bitangent.x);
                glm::vec3 vertex_normal = glm::cross(vertex.tangent, vertex_bitangent);

                //Convert to right-handed coordinate system
                if (glm::dot(vertex.normal, vertex_normal) < 0.0)
                {
                    vertex.tangent = -vertex.tangent;
                }

                vertices.push_back(vertex);
            }
        }

        if (vertices.empty())
        {
            continue;
        }

        GLuint vertex_buffer = 0;

        glGenBuffers(1, &vertex_buffer);
        glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
        glBufferStorage(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        if (this->objects.empty())
        {
            this->scene_min = vertices.front().position;
            this->scene_max = vertices.front().position;
        }

        for (const Vertex& vertex : vertices)
        {
            this->scene_min = glm::min(this->scene_min, vertex.position);
            this->scene_max = glm::max(this->scene_max, vertex.position);
        }

        Material* material = this->default_material;

        for (Material* loaded_material : this->materials)
        {
            if (loaded_material->material_index == scene_mesh->mMaterialIndex)
            {
                material = loaded_material;

                break;
            }
        }

        Object object;
        object.id = this->objects.size();
        object.material = material;
        object.vertex_buffer = vertex_buffer;
        object.vertex_count = vertices.size();

        this->objects.push_back(object);
    }

    glGenVertexArrays(1, &this->vertex_array);
    glBindVertexArray(this->vertex_array);

    glEnableVertexAttribArray(0);
    glVertexAttribBinding(0, 0);
    glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, position));

    glEnableVertexAttribArray(1);
    glVertexAttribBinding(1, 0);
    glVertexAttribFormat(1, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, normal));

    glEnableVertexAttribArray(2);
    glVertexAttribBinding(2, 0);
    glVertexAttribFormat(2, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, tangent));

    glEnableVertexAttribArray(3);
    glVertexAttribBinding(3, 0);
    glVertexAttribFormat(3, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, texture_coord));

    glBindVertexArray(0);

    return true;
}

bool Scene::create_light(const aiScene* scene, float scale)
{
    for (uint32_t light_index = 0; light_index < scene->mNumLights; light_index++)
    {
        const aiLight* scene_light = scene->mLights[light_index];

        Light light;
        light.position = glm::make_vec3(&scene_light->mPosition.x) * scale;
        light.direction = glm::make_vec3(&scene_light->mDirection.x);
        light.color = glm::make_vec3(&scene_light->mColorDiffuse.r);
        light.inner_angle = scene_light->mAngleInnerCone / 2.0f;
        light.outer_angle = scene_light->mAngleOuterCone / 2.0f;

        float direction_length = glm::length(light.direction);

        if (direction_length > 0.0f)
        {
            light.direction /= direction_length;
        }

        switch (scene_light->mType)
        {
        case aiLightSource_DIRECTIONAL:
            light.type = SCENE_LIGHT_TYPE_DIRECTIONAL;
            break;
        case aiLightSource_SPOT:
            light.type = SCENE_LIGHT_TYPE_SPOT;
            break;
        case aiLightSource_POINT:
            light.type = SCENE_LIGHT_TYPE_POINT;
            break;
        default:
            continue;
        }

        this->lights.push_back(light);
    }

    if (this->lights.empty())
    {
        Light default_light;
        default_light.type = SCENE_LIGHT_TYPE_DIRECTIONAL;
        default_light.direction = glm::normalize(glm::vec3(0.1f, -1.0f, 0.25f));
        default_light.color = glm::vec3(50.0f);

        this->lights.push_back(default_light);
    }

    uint32_t light_array_offset = 0;
    uint32_t light_layer_index = 0;
    
    for (Light& light : this->lights)
    {
        if (light.type == SCENE_LIGHT_TYPE_POINT)
        {
            std::array<glm::vec3, 6> directions =
            {
                glm::vec3(1.0f, 0.0f, 0.0f),
                glm::vec3(-1.0f, 0.0f, 0.0f),
                glm::vec3(0.0f, 1.0f, 0.0f),
                glm::vec3(0.0f, -1.0f, 0.0f),
                glm::vec3(0.0f, 0.0f, 1.0f),
                glm::vec3(0.0f, 0.0f, -1.0f)
            };

            std::array<glm::vec3, 6> up_directions =
            {
                glm::vec3(0.0f, -1.0f, 0.0f),
                glm::vec3(0.0f, -1.0f, 0.0f),
                glm::vec3(0.0f, 0.0f, 1.0f),
                glm::vec3(0.0f, 0.0f, -1.0f),
                glm::vec3(0.0f, -1.0f, 0.0f),
                glm::vec3(0.0f, -1.0f, 0.0f)
            };

            glm::vec3 scene_size = this->scene_max - this->scene_min;
            float scene_diagonal = glm::length(scene_size);

            light.light_projection_matrix = glm::perspective(glm::half_pi<float>(), 1.0f, SCENE_LIGHT_NEAR_DISTANCE, scene_diagonal);
            light.light_matrix.resize(directions.size());
           
            for (uint32_t index = 0; index < directions.size(); index++)
            {
                glm::mat4 view_matrix = glm::lookAt(light.position, light.position + directions[index], up_directions[index]);
                light.light_matrix[index] = light.light_projection_matrix * view_matrix;
            }

            float near_size = 2.0f * SCENE_LIGHT_NEAR_DISTANCE * glm::tan(glm::quarter_pi<float>());

            light.light_image_plane_size = glm::vec2(near_size);
            light.light_image_plane_near = SCENE_LIGHT_NEAR_DISTANCE;

            light.light_array_size = directions.size();
            light.light_array_offset = light_array_offset;
            light.light_layer_index = light_layer_index;

            light_array_offset += light.light_array_size;
            light_layer_index += 1;
        }
    }

    light_layer_index = light_array_offset;

    for (Light& light : this->lights)
    {
        if (light.type == SCENE_LIGHT_TYPE_DIRECTIONAL)
        {
            glm::vec3 direction = light.direction;
            glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

            if (glm::dot(direction, up) > 0.9)
            {
                up = glm::vec3(1.0f, 0.0f, 0.0f);
            }

            glm::mat4 view_matrix = glm::lookAt(glm::vec3(0.0f), direction, up);

            std::array<glm::vec3, 8> box_points =
            {
                glm::vec3(this->scene_min.x, this->scene_min.y, this->scene_min.z),
                glm::vec3(this->scene_min.x, this->scene_min.y, this->scene_max.z),
                glm::vec3(this->scene_min.x, this->scene_max.y, this->scene_min.z),
                glm::vec3(this->scene_min.x, this->scene_max.y, this->scene_max.z),
                glm::vec3(this->scene_max.x, this->scene_min.y, this->scene_min.z),
                glm::vec3(this->scene_max.x, this->scene_min.y, this->scene_max.z),
                glm::vec3(this->scene_max.x, this->scene_max.y, this->scene_min.z),
                glm::vec3(this->scene_max.x, this->scene_max.y, this->scene_max.z)
            };

            for (glm::vec3& point : box_points)
            {
                point = glm::vec3(view_matrix * glm::vec4(point, 1.0));
            }

            glm::vec3 light_min = box_points.front();
            glm::vec3 light_max = box_points.front();

            for (const glm::vec3& point : box_points)
            {
                light_min = glm::min(light_min, point);
                light_max = glm::max(light_max, point);
            }

            light.light_projection_matrix = glm::ortho(light_min.x, light_max.x, light_min.y, light_max.y, -light_max.z, -light_min.z);
            light.light_matrix.push_back(light.light_projection_matrix * view_matrix);
            
            light.light_image_plane_size = glm::vec2(light_max - light_min);
            light.light_image_plane_near = -light_max.z;

            light.light_array_size = 1;
            light.light_array_offset = light_array_offset;
            light.light_layer_index = light_layer_index;

            light_array_offset += light.light_array_size;
            light_layer_index += 1;
        }

        else if (light.type == SCENE_LIGHT_TYPE_SPOT)
        {
            glm::vec3 scene_size = this->scene_max - this->scene_min;
            float scene_diagonal = glm::length(scene_size);

            glm::vec3 position = light.position;
            glm::vec3 direction = light.direction;
            float angle = light.outer_angle;

            glm::vec3 side = glm::vec3(1.0f, 0.0f, 0.0f);

            if (glm::dot(direction, side) > 0.9)
            {
                side = glm::vec3(0.0f, 1.0f, 0.0f);
            }

            glm::mat4 view_matrix = glm::lookAt(position, position + direction, side);
            
            light.light_projection_matrix = glm::perspective(2.0f * angle, 1.0f, SCENE_LIGHT_NEAR_DISTANCE, scene_diagonal);
            light.light_matrix.push_back(light.light_projection_matrix* view_matrix);
            
            float near_size = 2.0f * SCENE_LIGHT_NEAR_DISTANCE * glm::tan(angle);
            
            light.light_image_plane_size = glm::vec2(near_size);
            light.light_image_plane_near = SCENE_LIGHT_NEAR_DISTANCE;

            light.light_array_size = 1;
            light.light_array_offset = light_array_offset;
            light.light_layer_index = light_layer_index;

            light_array_offset += light.light_array_size;
            light_layer_index += 1;
        }
    }

    return true;
}

bool Scene::create_sky(const std::string& sky_file_name, float sky_intensity)
{
    Texture* texture = nullptr;

    if (!this->create_texture_from_sky_file(sky_file_name, texture))
    {
        return false;
    }

    Material* material = new Material;
    material->base_color_texture = this->default_base_color_texture;
    material->material_texture = this->default_material_texture;
    material->normal_texture = this->default_normal_texture;
    material->emissive_texture = texture;
    material->base_color = glm::vec3(1.0f);
    material->emissive_color = glm::vec3(sky_intensity);
    material->opacity = 1.0f;
    material->roughness = 1.0f;
    material->metallic = 0.0f;
    material->is_shadow_caster = false;
    material->is_two_sided = true;

    this->materials.push_back(material);

    std::vector<Vertex> sphere_vertices;
    glm::vec3 sphere_center = (this->scene_max + this->scene_min) / 2.0f;
    float sphere_radius = glm::distance(sphere_center, this->scene_max);

    for (uint32_t ring = 0; ring < SCENE_SKY_SPHERE_RINGS; ring++)
    {
        float coord_v = (float)ring / (float)(SCENE_SKY_SPHERE_RINGS - 1);
        float latitude = glm::pi<float>() * coord_v;

        for (uint32_t segment = 0; segment < SCENE_SKY_SPHERE_SEGMENTS; segment++)
        {
            float coord_u = (float)segment / (float)(SCENE_SKY_SPHERE_SEGMENTS - 1);
            float longitude = glm::two_pi<float>() * coord_u;

            glm::vec3 position = sphere_center;
            position.x += sphere_radius * glm::cos(longitude) * glm::sin(latitude);
            position.y -= sphere_radius * glm::cos(latitude);
            position.z += sphere_radius * glm::sin(longitude) * glm::sin(latitude);

            glm::vec2 texture_coord = glm::vec2(coord_u, 1.0 - coord_v);
            glm::vec3 normal = glm::normalize(sphere_center - position);
            glm::vec3 tangent = glm::vec3(1.0f, 0.0f, 0.0f);

            if (glm::abs(glm::dot(normal, glm::vec3(0.0f, 1.0f, 0.0f))) < 0.99f)
            {
                tangent = glm::normalize(glm::cross(normal, glm::vec3(0.0f, 1.0f, 0.0f)));
            }

            Vertex vertex;
            vertex.position = position;
            vertex.normal = normal;
            vertex.tangent = tangent;
            vertex.texture_coord = texture_coord;

            sphere_vertices.push_back(vertex);
        }
    }

    std::vector<Vertex> vertices;

    for (uint32_t ring = 0; ring < (SCENE_SKY_SPHERE_RINGS - 1); ring++)
    {
        uint32_t lower_ring_offset = ring * SCENE_SKY_SPHERE_SEGMENTS;
        uint32_t upper_ring_offset = lower_ring_offset + SCENE_SKY_SPHERE_SEGMENTS;

        for (uint32_t segment = 0; segment < SCENE_SKY_SPHERE_SEGMENTS; segment++)
        {
            uint32_t current_index = segment;
            uint32_t next_index = (segment + 1) % SCENE_SKY_SPHERE_SEGMENTS;

            vertices.push_back(sphere_vertices[lower_ring_offset + current_index]);
            vertices.push_back(sphere_vertices[lower_ring_offset + next_index]);
            vertices.push_back(sphere_vertices[upper_ring_offset + next_index]);

            vertices.push_back(sphere_vertices[upper_ring_offset + next_index]);
            vertices.push_back(sphere_vertices[upper_ring_offset + current_index]);
            vertices.push_back(sphere_vertices[lower_ring_offset + current_index]);
        }
    }

    GLuint vertex_buffer = 0;
    glGenBuffers(1, &vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferStorage(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    Object object;
    object.id = this->objects.size();
    object.material = material;
    object.vertex_buffer = vertex_buffer;
    object.vertex_count = vertices.size();

    this->objects.push_back(object);

    return true;
}

bool Scene::create_material_texture_from_material(const aiMaterial* scene_material, const std::string& scene_file_name, Texture*& texture, bool& use_roughness_factor, bool& use_metallic_factor)
{
    Texture* roughness_texture = nullptr;
    Texture* metallic_texture = nullptr;

    if (!this->create_texture_from_material(scene_material, scene_file_name, false, aiTextureType_SHININESS, roughness_texture))
    {
        roughness_texture = nullptr;
    }

    if (!this->create_texture_from_material(scene_material, scene_file_name, false, aiTextureType_METALNESS, metallic_texture))
    {
        metallic_texture = nullptr;
    }

    if (roughness_texture != nullptr && metallic_texture != nullptr)
    {
        glm::ivec2 roughness_resolution = glm::ivec2(roughness_texture->image_width, roughness_texture->image_height);
        glm::ivec2 metallic_resolution = glm::ivec2(metallic_texture->image_width, metallic_texture->image_height);

        if (roughness_resolution.x != metallic_resolution.x || roughness_resolution.y != metallic_resolution.y)
        {
            glBindTexture(GL_TEXTURE_2D, roughness_texture->image_buffer);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ZERO);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_ONE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ZERO);
            glBindTexture(GL_TEXTURE_2D, 0);

            texture = roughness_texture;

            use_roughness_factor = false;
            use_metallic_factor = true;

            spdlog::warn("Scene: Can't combine textures: {}, {}", roughness_texture->file_name , metallic_texture->file_name);

            return true;
        }
        
        std::string file_name = roughness_texture->file_name + ", " + metallic_texture->file_name;

        for (Texture* loaded_texture : this->textures)
        {
            if (loaded_texture->file_name == file_name)
            {
                texture = loaded_texture;

                return true;
            }
        }

        glm::ivec2 image_resolution = roughness_resolution;
        uint32_t image_data_size = image_resolution.x * image_resolution.y;

        std::vector<uint8_t> roughness_data(image_data_size);
        std::vector<uint8_t> metallic_data(image_data_size);
        std::vector<glm::u8vec4> combined_data(image_data_size);

        glPixelStorei(GL_PACK_ALIGNMENT, 1);

        glBindTexture(GL_TEXTURE_2D, roughness_texture->image_buffer);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, roughness_data.data());
        glBindTexture(GL_TEXTURE_2D, 0);

        glBindTexture(GL_TEXTURE_2D, metallic_texture->image_buffer);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, metallic_data.data());
        glBindTexture(GL_TEXTURE_2D, 0);

        glPixelStorei(GL_PACK_ALIGNMENT, 4);

        for (uint32_t index = 0; index < image_data_size; index++)
        {
            combined_data[index].x = 0;
            combined_data[index].y = roughness_data[index];
            combined_data[index].z = metallic_data[index];
            combined_data[index].w = 0;
        }

        Texture* combined_texture = new Texture;
        combined_texture->file_name = file_name;
        combined_texture->image_width = image_resolution.x;
        combined_texture->image_height = image_resolution.y;

        glm::ivec2 image_level_resolution = image_resolution;
        uint32_t image_levels = 1;

        while (image_level_resolution.x > 1 || image_level_resolution.y > 1)
        {
            image_level_resolution = glm::max(image_level_resolution / 2, glm::ivec2(1));
            image_levels++;
        }

        glGenTextures(1, &combined_texture->image_buffer);
        glBindTexture(GL_TEXTURE_2D, combined_texture->image_buffer);

        glTexStorage2D(GL_TEXTURE_2D, image_levels, GL_RGBA8, image_resolution.x, image_resolution.y);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image_resolution.x, image_resolution.y, GL_RGBA, GL_UNSIGNED_BYTE, combined_data.data());

        float max_anisotrophic_filtering = 1.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_anisotrophic_filtering);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, max_anisotrophic_filtering);

        glGenerateMipmap(GL_TEXTURE_2D);

        glBindTexture(GL_TEXTURE_2D, 0);

        texture = combined_texture;

        use_roughness_factor = false;
        use_metallic_factor = false;
    }

    else if (roughness_texture != nullptr)
    {
        glBindTexture(GL_TEXTURE_2D, roughness_texture->image_buffer);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ZERO);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_ONE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ZERO);
        glBindTexture(GL_TEXTURE_2D, 0);

        texture = roughness_texture;

        use_roughness_factor = false;
        use_metallic_factor = true;
    }

    else if (metallic_texture != nullptr)
    {
        glBindTexture(GL_TEXTURE_2D, metallic_texture->image_buffer);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ZERO);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_ONE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ZERO);
        glBindTexture(GL_TEXTURE_2D, 0);

        texture = metallic_texture;

        use_roughness_factor = true;
        use_metallic_factor = false;
    }

    else
    {
        return false;
    }

    return true;
}

bool Scene::create_texture_from_material(const aiMaterial* scene_material, const std::string& scene_file_name, bool use_srgb, aiTextureType type, Texture*& texture)
{
    aiString texture_file_name;

    if (scene_material->GetTexture(type, 0, &texture_file_name) != aiReturn_SUCCESS)
    {
        return false;
    }

    if (texture_file_name.length <= 0)
    {
        return false;
    }

    std::string file_name = scene_file_name;
    std::size_t offset = file_name.find_last_of("/\\");

    if (offset == std::string::npos)
    {
        return false;
    }

    file_name = file_name.substr(0, offset + 1) + texture_file_name.C_Str();
    std::replace(file_name.begin(), file_name.end(), '\\', '/');

    for (Texture* loaded_texture : this->textures)
    {
        if (loaded_texture->file_name == file_name)
        {
            texture = loaded_texture;

            return true;
        }
    }

    spdlog::info("Scene: Loading texture {}", file_name);

    std::string file_extension = file_name.substr(file_name.find_last_of('.'));

    if (file_extension == ".dds")
    {
        return this->create_texture_from_compressed_file(file_name, use_srgb, texture);
    }

    else if (file_extension == ".exr")
    {
        return this->create_texture_from_high_bitdepth_file(file_name, use_srgb, texture);
    }

    else
    {
        return this->create_texture_from_regular_file(file_name, use_srgb, texture);
    }

    return false;
}

bool Scene::create_texture_from_sky_file(const std::string& file_name, Texture*& texture)
{
    glm::ivec2 image_resolution = glm::ivec2(0);
    float* image_pointer = (float*)stbi_loadf(file_name.c_str(), &image_resolution.x, &image_resolution.y, nullptr, STBI_rgb_alpha);

    if (image_pointer == nullptr)
    {
        spdlog::error("Can't load texture: {}", file_name);

        return false;
    }

    texture = new Texture;
    texture->file_name = file_name;
    texture->image_width = image_resolution.x;
    texture->image_height = image_resolution.y;
    this->textures.push_back(texture);

    glm::ivec2 image_level_resolution = image_resolution;
    uint32_t image_levels = 1;

    while (image_level_resolution.x > 1 || image_level_resolution.y > 1)
    {
        image_level_resolution = glm::max(image_level_resolution / 2, glm::ivec2(1));
        image_levels++;
    }

    glGenTextures(1, &texture->image_buffer);
    glBindTexture(GL_TEXTURE_2D, texture->image_buffer);

    glTexStorage2D(GL_TEXTURE_2D, image_levels, GL_RGBA32F, image_resolution.x, image_resolution.y);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image_resolution.x, image_resolution.y, GL_RGBA, GL_FLOAT, image_pointer);

    float max_anisotrophic_filtering = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_anisotrophic_filtering);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, max_anisotrophic_filtering);

    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(image_pointer);

    return true;
}

bool Scene::create_texture_from_regular_file(const std::string& file_name, bool use_srgb, Texture*& texture)
{
    glm::ivec2 image_resolution = glm::ivec2(0);
    uint8_t* image_pointer = (uint8_t*)stbi_load(file_name.c_str(), &image_resolution.x, &image_resolution.y, nullptr, STBI_rgb_alpha);

    if (image_pointer == nullptr)
    {
        spdlog::warn("Can't load texture: {}", file_name);

        return false;
    }

    GLenum format = GL_RGBA8;

    if (use_srgb)
    {
        format = GL_SRGB8_ALPHA8;
    }

    texture = new Texture;
    texture->file_name = file_name;
    texture->image_width = image_resolution.x;
    texture->image_height = image_resolution.y;
    this->textures.push_back(texture);

    glm::ivec2 image_level_resolution = image_resolution;
    uint32_t image_levels = 1;

    while (image_level_resolution.x > 1 || image_level_resolution.y > 1)
    {
        image_level_resolution = glm::max(image_level_resolution / 2, glm::ivec2(1));
        image_levels++;
    }

    glGenTextures(1, &texture->image_buffer);
    glBindTexture(GL_TEXTURE_2D, texture->image_buffer);

    glTexStorage2D(GL_TEXTURE_2D, image_levels, format, image_resolution.x, image_resolution.y);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image_resolution.x, image_resolution.y, GL_RGBA, GL_UNSIGNED_BYTE, image_pointer);

    float max_anisotrophic_filtering = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_anisotrophic_filtering);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, max_anisotrophic_filtering);

    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(image_pointer);

    return true;
}

bool Scene::create_texture_from_high_bitdepth_file(const std::string& file_name, bool use_srgb, Texture*& texture)
{
    std::unique_ptr<Imf::RgbaInputFile> image_file;

    try
    {
        image_file = std::make_unique<Imf::RgbaInputFile>(file_name.c_str());
    }

    catch (const std::exception& exception)
    {
        spdlog::warn("Can't load texture: {}", exception.what());

        return false;
    }

    if (!image_file->isComplete())
    {
        return false;
    }

    Imath::Box2i image_box = image_file->dataWindow();

    glm::uvec2 image_resolution = glm::uvec2(0);
    image_resolution.x = image_box.max.x - image_box.min.x + 1;
    image_resolution.y = image_box.max.y - image_box.min.y + 1;

    Imf::Array2D<Imf::Rgba> image_data;
    image_data.resizeErase(image_resolution.x, image_resolution.y);

    image_file->setFrameBuffer(&image_data[0][0] - image_box.min.x - image_box.min.y * image_resolution.x, 1, image_resolution.x);
    image_file->readPixels(image_box.min.y, image_box.max.y);

    GLenum format = GL_RGBA8;

    if (use_srgb)
    {
        format = GL_SRGB8_ALPHA8;
    }
    
    texture = new Texture;
    texture->file_name = file_name;
    texture->image_width = image_resolution.x;
    texture->image_height = image_resolution.y;
    this->textures.push_back(texture);

    glm::ivec2 image_level_resolution = image_resolution;
    uint32_t image_levels = 1;

    while (image_level_resolution.x > 1 || image_level_resolution.y > 1)
    {
        image_level_resolution = glm::max(image_level_resolution / 2, glm::ivec2(1));
        image_levels++;
    }

    glGenTextures(1, &texture->image_buffer);
    glBindTexture(GL_TEXTURE_2D, texture->image_buffer);

    glTexStorage2D(GL_TEXTURE_2D, image_levels, format, image_resolution.x, image_resolution.y);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image_resolution.x, image_resolution.y, GL_RGBA, GL_HALF_FLOAT, &image_data[0][0]);

    float max_anisotrophic_filtering = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_anisotrophic_filtering);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, max_anisotrophic_filtering);

    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

bool Scene::create_texture_from_compressed_file(const std::string& file_name, bool use_srgb, Texture*& texture)
{
    gli::texture image = gli::load(file_name.c_str());

    if (image.empty())
    {
        spdlog::warn("Can't load texture: {}", file_name);

        return false;
    }

    GLenum format = GL_RGBA8;

    switch (image.format())
    {
    case gli::FORMAT_RGBA_DXT1_SRGB_BLOCK8:
    case gli::FORMAT_RGBA_DXT1_UNORM_BLOCK8:
        if (use_srgb)
        {
            format = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
        }

        else
        {
            format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
        }
        break;
    case gli::FORMAT_RGBA_DXT5_SRGB_BLOCK16:
    case gli::FORMAT_RGBA_DXT5_UNORM_BLOCK16:
        if (use_srgb)
        {
            format = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
        }

        else
        {
            format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        }
        break;
    case gli::FORMAT_RG_ATI2N_UNORM_BLOCK16:
        format = GL_COMPRESSED_RG_RGTC2;
        break;
    default:
        return false;
    }

    texture = new Texture;
    texture->file_name = file_name;
    texture->image_width = image.extent().x;
    texture->image_height = image.extent().y;
    this->textures.push_back(texture);

    glGenTextures(1, &texture->image_buffer);
    glBindTexture(GL_TEXTURE_2D, texture->image_buffer);

    for (uint32_t level = 0; level < image.levels(); level++)
    {
        glCompressedTexImage2D(GL_TEXTURE_2D, level, format, image.extent(level).x, image.extent(level).y, 0, image.size(level), image.data(0, 0, level));
    }

    float max_anisotrophic_filtering = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_anisotrophic_filtering);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, max_anisotrophic_filtering);

    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

bool Scene::create_texture_from_color(const glm::vec4& color, GLenum format, Texture*& texture)
{
    texture = new Texture;
    texture->image_width = 1;
    texture->image_height = 1;
    this->textures.push_back(texture);

    glGenTextures(1, &texture->image_buffer);
    glBindTexture(GL_TEXTURE_2D, texture->image_buffer);

    glTexStorage2D(GL_TEXTURE_2D, 1, format, 1, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, &color);

    float max_anisotrophic_filtering = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_anisotrophic_filtering);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, max_anisotrophic_filtering);

    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

void Scene::compute_indirect_domain()
{
    glm::vec3 scene_center = (this->scene_min + this->scene_max) / 2.0f;
    glm::vec3 scene_size = this->scene_max - this->scene_min;

    this->indirect_cell_size = glm::vec3(SCENE_INDIRECT_DEFAULT_CELL_SIZE);
    this->indirect_cell_count = glm::uvec3(glm::max(glm::ceil(scene_size / this->indirect_cell_size), glm::vec3(1.0f)));

    uint32_t indirect_pixel_count = this->indirect_cell_count.x * this->indirect_cell_count.y * this->indirect_cell_count.z;
    uint32_t memory_size = (indirect_pixel_count * (9 * sizeof(glm::vec4) + 3 * sizeof(uint32_t) + sizeof(glm::u8vec4))) / (1024 * 1024); // In MiB

    if (memory_size > SCENE_INDIRECT_MEMORY_LIMIT)
    {
        float factor_scene = (scene_size.x * scene_size.y * scene_size.z) * (9 * sizeof(glm::vec4) + 3 * sizeof(uint32_t) + sizeof(glm::u8vec4));
        float factor_limit = SCENE_INDIRECT_MEMORY_LIMIT * 1024 * 1024;

        this->indirect_cell_size = glm::vec3(std::cbrt(factor_scene / factor_limit));
        this->indirect_cell_count = glm::uvec3(glm::max(glm::ceil(scene_size / this->indirect_cell_size), glm::vec3(1.0f)));

        spdlog::warn("Indirect memory limit reached!");
    }

    this->indirect_domain_min = scene_center - 0.5f * this->indirect_cell_size * glm::vec3(this->indirect_cell_count);
    this->indirect_domain_max = scene_center + 0.5f * this->indirect_cell_size * glm::vec3(this->indirect_cell_count);
    this->indirect_iteration_count = 2 * glm::max(this->indirect_cell_count.x, glm::max(this->indirect_cell_count.y, this->indirect_cell_count.z));
}

bool Scene::create_shaders()
{
    ShaderDefines defines;
    defines.set_define_from_file("#include \"shared_defines.glsl\"", SHADER_DIRECTORY"shared_defines.glsl");
    defines.set_define_from_file("#include \"shared_math_library.glsl\"", SHADER_DIRECTORY"shared_math_library.glsl");
    defines.set_define_from_file("#include \"shared_indirect_library.glsl\"", SHADER_DIRECTORY"shared_indirect_library.glsl");

    if (!this->light_shader.load_shader(SHADER_DIRECTORY"scene_light_shader.vert", SHADER_TYPE_VERTEX, defines))
    {
        return false;
    }

    if (!this->light_shader.load_shader(SHADER_DIRECTORY"scene_light_shader.frag", SHADER_TYPE_FRAGMENT, defines))
    {
        return false;
    }

    if (!this->light_shader.link_program())
    {
        return false;
    }

    if (!this->indirect_inject_shader.load_shader(SHADER_DIRECTORY"scene_indirect_inject_shader.vert", SHADER_TYPE_VERTEX, defines))
    {
        return false;
    }

    if (!this->indirect_inject_shader.load_shader(SHADER_DIRECTORY"scene_indirect_inject_shader.geom", SHADER_TYPE_GEOMETRY, defines))
    {
        return false;
    }

    if (!this->indirect_inject_shader.load_shader(SHADER_DIRECTORY"scene_indirect_inject_shader.frag", SHADER_TYPE_FRAGMENT, defines))
    {
        return false;
    }

    if (!this->indirect_inject_shader.link_program())
    {
        return false;
    }

    if (!this->indirect_visibility_shader.load_shader(SHADER_DIRECTORY"scene_indirect_visibility_shader.vert", SHADER_TYPE_VERTEX, defines))
    {
        return false;
    }

    if (!this->indirect_visibility_shader.load_shader(SHADER_DIRECTORY"scene_indirect_visibility_shader.geom", SHADER_TYPE_GEOMETRY, defines))
    {
        return false;
    }

    if (!this->indirect_visibility_shader.load_shader(SHADER_DIRECTORY"scene_indirect_visibility_shader.frag", SHADER_TYPE_FRAGMENT, defines))
    {
        return false;
    }

    if (!this->indirect_visibility_shader.link_program())
    {
        return false;
    }

    if (!this->indirect_opacity_shader.load_shader(SHADER_DIRECTORY"scene_indirect_opacity_shader.comp", SHADER_TYPE_COMPUTE, defines))
    {
        return false;
    }

    if (!this->indirect_opacity_shader.link_program())
    {
        return false;
    }

    if (!this->indirect_propagate_shader.load_shader(SHADER_DIRECTORY"scene_indirect_propagate_shader.comp", SHADER_TYPE_COMPUTE, defines))
    {
        return false;
    }

    if (!this->indirect_propagate_shader.link_program())
    {
        return false;
    }

    return true;
}

bool Scene::create_buffers()
{
    uint32_t light_array_size = 0;
    uint32_t light_cube_array_size = 0;

    for (const Light& light : this->lights)
    {
        if (light.type == SCENE_LIGHT_TYPE_POINT)
        {
            light_cube_array_size += light.light_array_size;
        }

        light_array_size += light.light_array_size;
    }

    glm::vec3 border_color = glm::vec3(1.0f);

    glGenTextures(1, &this->light_depth_array_buffer);
    glBindTexture(GL_TEXTURE_2D_ARRAY, this->light_depth_array_buffer);

    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_DEPTH_COMPONENT16, SCENE_LIGHT_BUFFER_RESOLUTION, SCENE_LIGHT_BUFFER_RESOLUTION, light_array_size);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LESS);
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, glm::value_ptr(border_color));

    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    if (light_cube_array_size > 0)
    {
        glGenTextures(1, &this->light_depth_cube_array_buffer);
        glTextureView(this->light_depth_cube_array_buffer, GL_TEXTURE_CUBE_MAP_ARRAY, this->light_depth_array_buffer, GL_DEPTH_COMPONENT16, 0, 1, 0, light_cube_array_size);

        glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, this->light_depth_cube_array_buffer);

        glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LESS);
        glTexParameterfv(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_BORDER_COLOR, glm::value_ptr(border_color));

        glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);
    }

    else
    {
        this->light_depth_cube_array_buffer = 0;
    }

    glGenTextures(1, &this->light_normal_buffer);
    glBindTexture(GL_TEXTURE_2D, this->light_normal_buffer);

    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RG16, SCENE_LIGHT_BUFFER_RESOLUTION, SCENE_LIGHT_BUFFER_RESOLUTION);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, 0);

    glGenTextures(1, &this->light_flux_buffer);
    glBindTexture(GL_TEXTURE_2D, this->light_flux_buffer);

    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, SCENE_LIGHT_BUFFER_RESOLUTION, SCENE_LIGHT_BUFFER_RESOLUTION);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, 0);

    std::vector<glsl::Light> light_buffer_data;

    for (uint32_t light_index = 0; light_index < this->lights.size(); light_index++)
    {
        Light& light = this->lights[light_index];

        for (uint32_t array_index = 0; array_index < light.light_array_size; array_index++)
        {
            GLuint light_depth_buffer = 0;

            glGenTextures(1, &light_depth_buffer);
            glTextureView(light_depth_buffer, GL_TEXTURE_2D, this->light_depth_array_buffer, GL_DEPTH_COMPONENT16, 0, 1, light.light_array_offset + array_index, 1);

            glBindTexture(GL_TEXTURE_2D, light_depth_buffer);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

            glBindTexture(GL_TEXTURE_2D, 0);

            GLuint light_frame_buffer = 0;

            glGenFramebuffers(1, &light_frame_buffer);
            glBindFramebuffer(GL_FRAMEBUFFER, light_frame_buffer);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, light_depth_buffer, 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->light_normal_buffer, 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, this->light_flux_buffer, 0);

            std::vector<GLenum> draw_buffers =
            {
                GL_COLOR_ATTACHMENT0,
                GL_COLOR_ATTACHMENT1
            };

            glDrawBuffers(draw_buffers.size(), draw_buffers.data());

            GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

            if (status != GL_FRAMEBUFFER_COMPLETE)
            {
                return false;
            }

            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            light.light_depth_buffers.push_back(light_depth_buffer);
            light.light_frame_buffers.push_back(light_frame_buffer);
        }

        glsl::Light light_data;
        light_data.position = light.position;
        light_data.type = light.type;
        light_data.direction = light.direction;
        light_data.inner_angle = light.inner_angle;
        light_data.color = light.color;
        light_data.outer_angle = light.outer_angle;
        light_data.padding = glm::uvec3(0);
        light_data.light_layer_index = light.light_layer_index;
        light_data.light_projection_matrix = light.light_projection_matrix;
        light_data.light_matrix = light.light_matrix[0];

        light_buffer_data.push_back(light_data);
    }

    glGenBuffers(1, &this->light_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->light_buffer);

    glBufferStorage(GL_SHADER_STORAGE_BUFFER, sizeof(glsl::Light) * light_buffer_data.size(), light_buffer_data.data(), 0);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    if (!this->create_color_distribution_buffers(this->indirect_red_distribution_buffers))
    {
        return false;
    }

    if (!this->create_color_distribution_buffers(this->indirect_green_distribution_buffers))
    {
        return false;
    }

    if (!this->create_color_distribution_buffers(this->indirect_blue_distribution_buffers))
    {
        return false;
    }

    for (uint32_t index = 0; index < this->indirect_visibility_buffers.size(); index++)
    {
        GLuint visibility_buffer = 0;

        glGenTextures(1, &visibility_buffer);
        glBindTexture(GL_TEXTURE_3D, visibility_buffer);

        glTexStorage3D(GL_TEXTURE_3D, 1, GL_R32UI, this->indirect_cell_count.x, this->indirect_cell_count.y, this->indirect_cell_count.z);

        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        glBindTexture(GL_TEXTURE_3D, 0);

        this->indirect_visibility_buffers[index] = visibility_buffer;
    }

    glGenTextures(1, &this->indirect_opacity_buffer);
    glBindTexture(GL_TEXTURE_3D, indirect_opacity_buffer);

    glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA8, this->indirect_cell_count.x, this->indirect_cell_count.y, this->indirect_cell_count.z);

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_3D, GL_TEXTURE_BORDER_COLOR, glm::value_ptr(border_color));

    glBindTexture(GL_TEXTURE_3D, 0);

    glGenVertexArrays(1, &this->indirect_inject_vertex_array);

    glGenFramebuffers(1, &this->indirect_inject_frame_buffer);
    glBindFramebuffer(GL_FRAMEBUFFER, this->indirect_inject_frame_buffer);

    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, this->indirect_red_distribution_buffers[0], 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, this->indirect_green_distribution_buffers[0], 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, this->indirect_blue_distribution_buffers[0], 0);

    std::vector<GLenum> draw_buffers =
    {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2
    };

    glDrawBuffers(draw_buffers.size(), draw_buffers.data());

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
}

bool Scene::create_color_distribution_buffers(std::array<GLuint, 3>& distribution_buffers)
{
    glm::vec4 border_color = glm::vec4(0.0);

    for (uint32_t index = 0; index < distribution_buffers.size(); index++)
    {
        GLuint distribution_buffer = 0;

        glGenTextures(1, &distribution_buffer);
        glBindTexture(GL_TEXTURE_3D, distribution_buffer);

        glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA16F, this->indirect_cell_count.x, this->indirect_cell_count.y, this->indirect_cell_count.z);

        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameterfv(GL_TEXTURE_3D, GL_TEXTURE_BORDER_COLOR, glm::value_ptr(border_color));

        glBindTexture(GL_TEXTURE_3D, 0);

        distribution_buffers[index] = distribution_buffer;
    }

    return true;
}

void Scene::compute_light(const Light& light, uint32_t array_index)
{
    glBindFramebuffer(GL_FRAMEBUFFER, light.light_frame_buffers[array_index]);
    glBindVertexArray(this->vertex_array);

    bool is_two_sided = false;
    
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CW);

    glViewport(0, 0, SCENE_LIGHT_BUFFER_RESOLUTION, SCENE_LIGHT_BUFFER_RESOLUTION);

    glClearDepth(1.0f);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    this->light_shader.use_shader();
    this->light_shader["scene_light_matrix"] = light.light_matrix[array_index];
    this->light_shader["scene_light_type"] = light.type;
    this->light_shader["scene_light_position"] = light.position;
    this->light_shader["scene_light_direction"] = light.direction;
    this->light_shader["scene_light_color"] = light.color;
    this->light_shader["scene_light_image_plane_size"] = light.light_image_plane_size;
    this->light_shader["scene_light_image_plane_near"] = light.light_image_plane_near;
    this->light_shader["scene_light_inner_angle"] = light.inner_angle;
    this->light_shader["scene_light_outer_angle"] = light.outer_angle;

    for (const Object& object : this->objects)
    {
        const Material* material = object.material;

        if (!material->is_shadow_caster)
        {
            continue;
        }

        this->light_shader["scene_material_base_color"] = material->base_color;
        this->light_shader["scene_material_opacity"] = material->opacity;

        glActiveTexture(GL_TEXTURE0 + SCENE_MATERIAL_BASE_COLOR_TEXTURE_BINDING_POINT);
        glBindTexture(GL_TEXTURE_2D, material->base_color_texture->image_buffer);

        if (is_two_sided != material->is_two_sided)
        {
            if (material->is_two_sided)
            {
                glDisable(GL_CULL_FACE);
            }

            else
            {
                glEnable(GL_CULL_FACE);
            }

            is_two_sided = material->is_two_sided;
        }

        glBindVertexBuffer(0, object.vertex_buffer, 0, sizeof(Vertex));

        glDrawArrays(GL_TRIANGLES, 0, object.vertex_count);
    }

    this->light_shader.use_default();

    glActiveTexture(GL_TEXTURE0 + SCENE_MATERIAL_BASE_COLOR_TEXTURE_BINDING_POINT);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);

    glFrontFace(GL_CCW);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Scene::compute_indirect()
{
    glBindFramebuffer(GL_FRAMEBUFFER, this->indirect_inject_frame_buffer);

    glViewport(0, 0, this->indirect_cell_count.x, this->indirect_cell_count.y);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    for (const Light& light : this->lights)
    {
        for (uint32_t array_index = 0; array_index < light.light_array_size; array_index++)
        {
            glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT);

            this->compute_light(light, array_index);

            glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

            glBindFramebuffer(GL_FRAMEBUFFER, this->indirect_inject_frame_buffer);
            glBindVertexArray(this->indirect_inject_vertex_array);

            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);

            glEnable(GL_BLEND);
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_ONE, GL_ONE);

            glViewport(0, 0, this->indirect_cell_count.x, this->indirect_cell_count.y);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, light.light_depth_buffers[array_index]);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, this->light_normal_buffer);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, this->light_flux_buffer);

            this->indirect_inject_shader.use_shader();
            this->indirect_inject_shader["scene_light_matrix_inv"] = glm::inverse(light.light_matrix[array_index]);
            this->indirect_inject_shader["scene_indirect_cell_size"] = this->indirect_cell_size;
            this->indirect_inject_shader["scene_indirect_cell_count"] = this->indirect_cell_count;
            this->indirect_inject_shader["scene_indirect_domain_min"] = this->indirect_domain_min;
            this->indirect_inject_shader["scene_indirect_domain_max"] = this->indirect_domain_max;

            uint32_t point_count = SCENE_LIGHT_BUFFER_RESOLUTION * SCENE_LIGHT_BUFFER_RESOLUTION;
            glDrawArrays(GL_POINTS, 0, point_count);

            this->indirect_inject_shader.use_default();

            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, 0);

            glDisable(GL_BLEND);

            glBindVertexArray(0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindVertexArray(this->vertex_array);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glDepthMask(GL_FALSE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    glViewportIndexedf(0, 0, 0, this->indirect_cell_count.y * 3, this->indirect_cell_count.z * 3);
    glViewportIndexedf(1, 0, 0, this->indirect_cell_count.x * 3, this->indirect_cell_count.z * 3);
    glViewportIndexedf(2, 0, 0, this->indirect_cell_count.x * 3, this->indirect_cell_count.y * 3);

    glBindImageTexture(0, this->indirect_visibility_buffers[0], 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
    glBindImageTexture(1, this->indirect_visibility_buffers[1], 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
    glBindImageTexture(2, this->indirect_visibility_buffers[2], 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);

    this->indirect_visibility_shader.use_shader();
    this->indirect_visibility_shader["scene_indirect_cell_count"] = this->indirect_cell_count;
    this->indirect_visibility_shader["scene_indirect_domain_min"] = this->indirect_domain_min;
    this->indirect_visibility_shader["scene_indirect_domain_max"] = this->indirect_domain_max;

    for (const Object& object : this->objects)
    {
        glBindVertexBuffer(0, object.vertex_buffer, 0, sizeof(Vertex));

        glDrawArrays(GL_TRIANGLES, 0, object.vertex_count);
    }

    this->indirect_visibility_shader.use_default();

    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, this->indirect_visibility_buffers[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, this->indirect_visibility_buffers[1]);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_3D, this->indirect_visibility_buffers[2]);

    glBindImageTexture(0, this->indirect_opacity_buffer, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);

    this->indirect_opacity_shader.use_shader();
    this->indirect_opacity_shader["scene_indirect_cell_count"] = this->indirect_cell_count;

    glm::uvec3 work_group_size = glm::uvec3(SCENE_INDIRECT_OPACITY_WORK_GROUP_SIZE_X, SCENE_INDIRECT_OPACITY_WORK_GROUP_SIZE_Y, SCENE_INDIRECT_OPACITY_WORK_GROUP_SIZE_Z);
    glm::uvec3 work_group_count = (this->indirect_cell_count + work_group_size - glm::uvec3(1)) / work_group_size;

    glDispatchCompute(work_group_count.x, work_group_count.y, work_group_count.z);

    this->indirect_opacity_shader.use_default();
    
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_3D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, 0);

    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);

    glCopyImageSubData(this->indirect_red_distribution_buffers[0], GL_TEXTURE_3D, 0, 0, 0, 0, this->indirect_red_distribution_buffers[2], GL_TEXTURE_3D, 0, 0, 0, 0, this->indirect_cell_count.x, this->indirect_cell_count.y, this->indirect_cell_count.z);
    glCopyImageSubData(this->indirect_green_distribution_buffers[0], GL_TEXTURE_3D, 0, 0, 0, 0, this->indirect_green_distribution_buffers[2], GL_TEXTURE_3D, 0, 0, 0, 0, this->indirect_cell_count.x, this->indirect_cell_count.y, this->indirect_cell_count.z);
    glCopyImageSubData(this->indirect_blue_distribution_buffers[0], GL_TEXTURE_3D, 0, 0, 0, 0, this->indirect_blue_distribution_buffers[2], GL_TEXTURE_3D, 0, 0, 0, 0, this->indirect_cell_count.x, this->indirect_cell_count.y, this->indirect_cell_count.z);

    this->indirect_propagate_shader.use_shader();
    this->indirect_propagate_shader["scene_indirect_cell_count"] = this->indirect_cell_count;

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_3D, this->indirect_visibility_buffers[0]);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_3D, this->indirect_visibility_buffers[1]);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_3D, this->indirect_visibility_buffers[2]);

    for (uint32_t index = 0; index < this->indirect_iteration_count; index++)
    {
        glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        uint32_t read_index = index % 2;
        uint32_t write_index = (index + 1) % 2;

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_3D, this->indirect_red_distribution_buffers[read_index]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_3D, this->indirect_green_distribution_buffers[read_index]);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_3D, this->indirect_blue_distribution_buffers[read_index]);
        
        glBindImageTexture(0, this->indirect_red_distribution_buffers[write_index], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        glBindImageTexture(1, this->indirect_green_distribution_buffers[write_index], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        glBindImageTexture(2, this->indirect_blue_distribution_buffers[write_index], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

        glBindImageTexture(3, this->indirect_red_distribution_buffers[2], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);
        glBindImageTexture(4, this->indirect_green_distribution_buffers[2], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);
        glBindImageTexture(5, this->indirect_blue_distribution_buffers[2], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);

        work_group_size = glm::uvec3(SCENE_INDIRECT_PROPAGATE_WORK_GROUP_SIZE_X, SCENE_INDIRECT_PROPAGATE_WORK_GROUP_SIZE_Y, SCENE_INDIRECT_PROPAGATE_WORK_GROUP_SIZE_Z);
        work_group_count = (this->indirect_cell_count + work_group_size - glm::uvec3(1)) / work_group_size;

        glDispatchCompute(work_group_count.x, work_group_count.y, work_group_count.z);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, 0);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_3D, 0);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_3D, 0);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_3D, 0);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_3D, 0);
    glActiveTexture(GL_TEXTURE0);

    this->indirect_propagate_shader.use_default();

    //Delete textures that are not used anymore
    for (uint32_t index = 0; index < 2; index++)
    {
        glDeleteTextures(1, &this->indirect_red_distribution_buffers[index]);
        glDeleteTextures(1, &this->indirect_green_distribution_buffers[index]);
        glDeleteTextures(1, &this->indirect_blue_distribution_buffers[index]);

        this->indirect_red_distribution_buffers[index] = 0;
        this->indirect_green_distribution_buffers[index] = 0;
        this->indirect_blue_distribution_buffers[index] = 0;
    }

    glDeleteTextures(3, this->indirect_visibility_buffers.data());
    glDeleteTextures(1, &this->light_normal_buffer);
    glDeleteTextures(1, &this->light_flux_buffer);

    this->indirect_visibility_buffers.fill(0);
    this->light_normal_buffer = 0;
    this->light_flux_buffer = 0;
}

void Scene::destroy_buffers()
{
    for (Light& light : this->lights)
    {
        glDeleteFramebuffers(light.light_frame_buffers.size(), light.light_frame_buffers.data());
        glDeleteTextures(light.light_depth_buffers.size(), light.light_depth_buffers.data());

        light.light_frame_buffers.clear();
        light.light_depth_buffers.clear();
    }

    glDeleteFramebuffers(1, &this->indirect_inject_vertex_array);
    glDeleteVertexArrays(1, &this->indirect_inject_vertex_array);

    glDeleteTextures(1, &this->light_depth_cube_array_buffer);
    glDeleteTextures(1, &this->light_depth_array_buffer);
    glDeleteTextures(1, &this->indirect_red_distribution_buffers[2]);
    glDeleteTextures(1, &this->indirect_green_distribution_buffers[2]);
    glDeleteTextures(1, &this->indirect_blue_distribution_buffers[2]);
    glDeleteTextures(1, &this->indirect_opacity_buffer);

    glDeleteBuffers(1, &this->light_buffer);
}

void Scene::destroy_objects()
{
    glDeleteVertexArrays(1, &this->vertex_array);

    for (const Object& object : this->objects)
    {
        glDeleteBuffers(1, &object.vertex_buffer);
    }

    this->objects.clear();
}

void Scene::destroy_materials()
{
    for (Material* material : this->materials)
    {
        delete material;
    }

    this->materials.clear();
}

void Scene::destroy_textures()
{
    for (Texture* texture : this->textures)
    {
        glDeleteTextures(1, &texture->image_buffer);

        delete texture;
    }

    this->textures.clear();
}

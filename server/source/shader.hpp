#ifndef HEADER_SHADER
#define HEADER_SHADER

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GL/glew.h>
#include <vector>
#include <array>
#include <span>
#include <string>
#include <cstdint>

class Shader;
struct UniformEntry;
struct SubroutineUniformEntry;

enum ShaderType
{
    SHADER_TYPE_VERTEX = GL_VERTEX_SHADER,
    SHADER_TYPE_GEOMETRY = GL_GEOMETRY_SHADER,
    SHADER_TYPE_FRAGMENT = GL_FRAGMENT_SHADER,
    SHADER_TYPE_COMPUTE = GL_COMPUTE_SHADER
};

struct ShaderDefineEntry
{
    std::string name;
    std::string value;
};

class ShaderDefines
{
private:
    std::vector<ShaderDefineEntry> defines;

public:
    ShaderDefines() = default;

    void apply_defines(std::string& code) const;

    void set_define(const char* name, const char* value);
    void set_define(const char* name, uint32_t value);
    void set_define(const char* name, int32_t value);
    void set_define(const char* name, float value);
    void set_define(const char* name, double value);
    bool set_define_from_file(const char* name, const char* file_name);
};

class ShaderSubroutine
{
private:
    std::string name;
    std::vector<std::string> indices;

public:
    ShaderSubroutine(const char* name, uint32_t index_count);

    void set_index(uint32_t index, const char* index_name);

    const std::vector<std::string>& get_indices() const;
    const std::string& get_name() const;
};

class ShaderUniform
{
private:
    const UniformEntry* entry;
    const Shader* shader;

public:
    ShaderUniform(const UniformEntry* entry, const Shader* shader);

    void operator=(bool value) const;
    void operator=(uint32_t value) const;
    void operator=(int32_t value) const;
    void operator=(float value) const;
    void operator=(double value) const;

    void operator=(const glm::vec2& value) const;
    void operator=(const glm::vec3& value) const;
    void operator=(const glm::uvec3& value) const;
    void operator=(const glm::vec4& value) const;
    void operator=(const glm::mat4& value) const;

    void operator=(std::span<int32_t> value) const;
    void operator=(std::span<glm::mat3> value) const;

    uint32_t get_location() const;
};

class Shader
{
private:
    std::string shader_name;
    std::vector<UniformEntry*> uniforms;
    std::array<std::vector<SubroutineUniformEntry*>, 6> subroutines;
    std::array<uint32_t, 6> active_shaders;
    GLuint program = 0;

public:
    Shader(const char* shaderName);
    ~Shader();

    bool load_shader(const char* file_name, ShaderType type);
    bool load_shader(const char* file_name, ShaderType type, const ShaderDefines& defines);
    bool link_program(bool make_separate = false, bool show_binary = false);

    void clear_program();

    bool set_shader_storage_buffer(const char* buffer_name, uint32_t binding_point) const;
    bool set_uniform_buffer(const char* buffer_name, uint32_t binding_point) const;
    bool set_subroutines(ShaderType shader_type, uint32_t subroutine_count, ShaderSubroutine* subroutine_list) const;

    ShaderUniform operator[](const char* uniform_name) const;

    const std::string& get_shader_name() const;
    GLuint get_program() const;

    void use_shader() const;
    void use_default() const;

    std::string get_program_binary() const;

private:
    void extract_uniforms();
    void extract_subroutines();
};

#endif

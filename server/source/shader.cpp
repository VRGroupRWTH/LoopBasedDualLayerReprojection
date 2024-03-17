#include "shader.hpp"
#include <fstream>
#include <string>

#include <spdlog/spdlog.h>

struct UniformEntry
{
    std::string name;
    uint32_t type;
    uint32_t location;
    uint32_t array_size;
};

struct SubroutineIndexEntry
{
    std::string name;
    uint32_t index;
};

struct SubroutineUniformEntry
{
    std::string uniform_name;
    uint32_t array_size;
    std::vector<SubroutineIndexEntry> indices;
};

bool is_uniform_sampler(uint32_t type)
{
    switch (type)
    {
        //Default
    case GL_SAMPLER_1D:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_3D:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_1D_SHADOW:
    case GL_SAMPLER_2D_SHADOW:
    case GL_SAMPLER_1D_ARRAY:
    case GL_SAMPLER_2D_ARRAY:
    case GL_SAMPLER_1D_ARRAY_SHADOW:
    case GL_SAMPLER_2D_ARRAY_SHADOW:
    case GL_SAMPLER_2D_MULTISAMPLE:
    case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case GL_SAMPLER_CUBE_SHADOW:
    case GL_SAMPLER_BUFFER:
    case GL_SAMPLER_2D_RECT:
    case GL_SAMPLER_2D_RECT_SHADOW:
        //Int
    case GL_INT_SAMPLER_1D:
    case GL_INT_SAMPLER_2D:
    case GL_INT_SAMPLER_3D:
    case GL_INT_SAMPLER_CUBE:
    case GL_INT_SAMPLER_1D_ARRAY:
    case GL_INT_SAMPLER_2D_ARRAY:
    case GL_INT_SAMPLER_2D_MULTISAMPLE:
    case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case GL_INT_SAMPLER_BUFFER:
    case GL_INT_SAMPLER_2D_RECT:
        //Unsigned Int
    case GL_UNSIGNED_INT_SAMPLER_1D:
    case GL_UNSIGNED_INT_SAMPLER_2D:
    case GL_UNSIGNED_INT_SAMPLER_3D:
    case GL_UNSIGNED_INT_SAMPLER_CUBE:
    case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
    case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_BUFFER:
    case GL_UNSIGNED_INT_SAMPLER_2D_RECT:
        //Image Texture
    case GL_IMAGE_1D:
    case GL_IMAGE_2D:
    case GL_IMAGE_3D:
    case GL_IMAGE_2D_RECT:
    case GL_IMAGE_CUBE:
    case GL_IMAGE_BUFFER:
    case GL_IMAGE_1D_ARRAY:
    case GL_IMAGE_2D_ARRAY:
    case GL_IMAGE_CUBE_MAP_ARRAY:
    case GL_IMAGE_2D_MULTISAMPLE:
    case GL_IMAGE_2D_MULTISAMPLE_ARRAY:
        //Int Image Texture
    case GL_INT_IMAGE_1D:
    case GL_INT_IMAGE_2D:
    case GL_INT_IMAGE_3D:
    case GL_INT_IMAGE_2D_RECT:
    case GL_INT_IMAGE_CUBE:
    case GL_INT_IMAGE_BUFFER:
    case GL_INT_IMAGE_1D_ARRAY:
    case GL_INT_IMAGE_2D_ARRAY:
    case GL_INT_IMAGE_CUBE_MAP_ARRAY:
    case GL_INT_IMAGE_2D_MULTISAMPLE:
    case GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
        //Unsigned Int Image Texture
    case GL_UNSIGNED_INT_IMAGE_1D:
    case GL_UNSIGNED_INT_IMAGE_2D:
    case GL_UNSIGNED_INT_IMAGE_3D:
    case GL_UNSIGNED_INT_IMAGE_2D_RECT:
    case GL_UNSIGNED_INT_IMAGE_CUBE:
    case GL_UNSIGNED_INT_IMAGE_BUFFER:
    case GL_UNSIGNED_INT_IMAGE_1D_ARRAY:
    case GL_UNSIGNED_INT_IMAGE_2D_ARRAY:
    case GL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY:
    case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE:
    case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
        return true;
    }

    return false;
}

uint32_t get_shader_index(ShaderType type)
{
    switch (type)
    {
    case GL_VERTEX_SHADER:
        return 0;
    case GL_TESS_CONTROL_SHADER:
        return 1;
    case GL_TESS_EVALUATION_SHADER:
        return 2;
    case GL_GEOMETRY_SHADER:
        return 3;
    case GL_FRAGMENT_SHADER:
        return 4;
    case GL_COMPUTE_SHADER:
        return 5;
    }

    return 0;
}

bool show_shader_log(GLuint shader, const std::string& shader_name)
{
    GLint status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    if (status == GL_FALSE)
    {
        GLint log_lenght = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_lenght);

        std::string log_string(log_lenght, '\0');
        glGetShaderInfoLog(shader, log_lenght, nullptr, log_string.data());

        spdlog::error("Shader: Error during compilation of shader '{}'!", shader_name);
        spdlog::error(log_string);

        return false;
    }

    return true;
}

bool show_program_log(GLuint program, const std::string& shader_name)
{
    GLint status = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &status);

    if (status == GL_FALSE)
    {
        GLint log_lenght = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_lenght);

        std::string log_string(log_lenght, '\0');
        glGetProgramInfoLog(program, log_lenght, nullptr, log_string.data());

        spdlog::error("Shader: Error during linking of shader '{}'!", shader_name);
        spdlog::error(log_string);

        return false;
    }

    return true;
}

void ShaderDefines::apply_defines(std::string& code) const
{
    for(const ShaderDefineEntry& define : this->defines)
    {
        while (true)
        {
            std::size_t offset = code.find(define.name);

            if (offset == std::string::npos)
            {
                break;
            }

            code.replace(offset, define.name.size(), define.value);
        }
    }
}

void ShaderDefines::set_define(const char* name, const char* value)
{
    ShaderDefineEntry define;
    define.name = name;
    define.value = value;

    this->defines.push_back(define);
}

void ShaderDefines::set_define(const char* name, uint32_t value)
{
    ShaderDefineEntry define;
    define.name = name;
    define.value = std::to_string(value);

    this->defines.push_back(define);
}

void ShaderDefines::set_define(const char* name, int32_t value)
{
    ShaderDefineEntry define;
    define.name = name;
    define.value = std::to_string(value);

    this->defines.push_back(define);
}

void ShaderDefines::set_define(const char* name, float value)
{
    ShaderDefineEntry define;
    define.name = name;
    define.value = std::to_string(value);

    this->defines.push_back(define);
}

void ShaderDefines::set_define(const char* name, double value)
{
    ShaderDefineEntry define;
    define.name = name;
    define.value = std::to_string(value);

    this->defines.push_back(define);
}

bool ShaderDefines::set_define_from_file(const char* name, const char* file_name)
{
    std::fstream file;
    file.open(file_name, std::ios::in | std::ios::binary);

    if (!file.good())
    {
        return false;
    }

    file.seekg(0, std::ios::end);
    uint32_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string file_content(file_size, '\0');
    file.read(file_content.data(), file_size);
    file.close();

    ShaderDefineEntry define;
    define.name = name;
    define.value = file_content;

    this->defines.push_back(define);

    return true;
}

ShaderSubroutine::ShaderSubroutine(const char* name, uint32_t index_count) : name(name)
{
    this->indices.resize(index_count);
}

void ShaderSubroutine::set_index(uint32_t index, const char* index_name)
{
    this->indices[index] = index_name;
}

const std::vector<std::string>& ShaderSubroutine::get_indices(void) const
{
    return this->indices;
}

const std::string& ShaderSubroutine::get_name(void) const
{
    return this->name;
}

ShaderUniform::ShaderUniform(const UniformEntry* entry, const Shader* shader) : entry(entry), shader(shader)
{

}

void ShaderUniform::operator=(bool value) const
{
    if (this->entry == nullptr)
    {
        return;
    }

    if (this->entry->type != GL_BOOL)
    {
        spdlog::warn("Shader: Wrong uniform type attached to uniform '{}' of shader '{}'!", this->entry->name, this->shader->get_shader_name());

        return;
    }

    glUniform1ui(this->entry->location, value ? 1 : 0);
}

void ShaderUniform::operator=(uint32_t value) const
{
    if(this->entry == nullptr)
    {
        return;
    }

    if(this->entry->type != GL_UNSIGNED_INT)
    {
        spdlog::warn("Shader: Wrong uniform type attached to uniform '{}' of shader '{}'!", this->entry->name, this->shader->get_shader_name());

        return;
    }

    glUniform1ui(this->entry->location, value);
}

void ShaderUniform::operator=(int32_t value) const
{
    if(this->entry == nullptr)
    {
        return;
    }

    if(this->entry->type != GL_INT && !is_uniform_sampler(this->entry->type))
    {
        spdlog::warn("Shader: Wrong uniform type attached to uniform '{}' of shader '{}'!", this->entry->name, this->shader->get_shader_name());

        return;
    }

    glUniform1i(this->entry->location, value);
}

void ShaderUniform::operator=(float value) const
{
    if(this->entry == nullptr)
    {
        return;
    }

    if(this->entry->type != GL_FLOAT)
    {
        spdlog::warn("Shader: Wrong uniform type attached to uniform '{}' of shader '{}'!", this->entry->name, this->shader->get_shader_name());

        return;
    }

    glUniform1f(this->entry->location, value);
}

void ShaderUniform::operator=(double value) const
{
    if(this->entry == nullptr)
    {
        return;
    }

    if(this->entry->type != GL_DOUBLE)
    {
        spdlog::warn("Shader: Wrong uniform type attached to uniform '{}' of shader '{}'!", this->entry->name, this->shader->get_shader_name());

        return;
    }

    glUniform1d(this->entry->location, value);
}

void ShaderUniform::operator=(const std::vector<int32_t>& value) const
{
    if (this->entry == nullptr)
    {
        return;
    }

    if (this->entry->type != GL_INT && !is_uniform_sampler(this->entry->type))
    {
        spdlog::warn("Shader: Wrong uniform type attached to uniform '{}' of shader '{}'!", this->entry->name, this->shader->get_shader_name());

        return;
    }

    if (value.size() > this->entry->array_size)
    {
        spdlog::warn("Shader: Wrong uniform array size attached to uniform '{}' of shader '{}'!", this->entry->name, this->shader->get_shader_name());

        return;
    }

    glUniform1iv(this->entry->location, value.size(), value.data());
}

void ShaderUniform::operator=(const glm::vec2& value) const
{
    if (this->entry == nullptr)
    {
        return;
    }

    if (this->entry->type != GL_FLOAT_VEC2)
    {
        spdlog::warn("Shader: Wrong uniform type attached to uniform '{}' of shader '{}'!", this->entry->name, this->shader->get_shader_name());

        return;
    }

    glUniform2f(this->entry->location, value.x, value.y);
}

void ShaderUniform::operator=(const glm::vec3& value) const
{
    if (this->entry == nullptr)
    {
        return;
    }

    if (this->entry->type != GL_FLOAT_VEC3)
    {
        spdlog::warn("Shader: Wrong uniform type attached to uniform '{}' of shader '{}'!", this->entry->name, this->shader->get_shader_name());

        return;
    }

    glUniform3f(this->entry->location, value.x, value.y, value.z);
}

void ShaderUniform::operator=(const glm::uvec3& value) const
{
    if (this->entry == nullptr)
    {
        return;
    }

    if (this->entry->type != GL_UNSIGNED_INT_VEC3)
    {
        spdlog::warn("Shader: Wrong uniform type attached to uniform '{}' of shader '{}'!", this->entry->name, this->shader->get_shader_name());

        return;
    }

    glUniform3ui(this->entry->location, value.x, value.y, value.z);
}

void ShaderUniform::operator=(const glm::vec4& value) const
{
    if (this->entry == nullptr)
    {
        return;
    }

    if (this->entry->type != GL_FLOAT_VEC4)
    {
        spdlog::warn("Shader: Wrong uniform type attached to uniform '{}' of shader '{}'!", this->entry->name, this->shader->get_shader_name());

        return;
    }

    glUniform4f(this->entry->location, value.x, value.y, value.z, value.w);
}

void ShaderUniform::operator=(const glm::mat4& value) const
{
    if (this->entry == nullptr)
    {
        return;
    }

    if (this->entry->type != GL_FLOAT_MAT4)
    {
        spdlog::warn("Shader: Wrong uniform type attached to uniform '{}' of shader '{}'!", this->entry->name, this->shader->get_shader_name());

        return;
    }

    glUniformMatrix4fv(this->entry->location, 1, GL_FALSE, glm::value_ptr(value));
}

uint32_t ShaderUniform::get_location(void) const
{
    return this->entry->location;
}

Shader::Shader(const char* shader_name) : shader_name(shader_name)
{
    this->active_shaders.fill(0);
}

Shader::~Shader()
{
    this->clear_program();
}

bool Shader::load_shader(const char* file_name, ShaderType type)
{
    std::fstream file;
    file.open(file_name, std::ios::in | std::ios::binary);

    if(!file.good())
    {
        spdlog::error("Shader: Can't find file '{}' for shader '{}'!", file_name, this->shader_name);

        file.close();

        return false;
    }

    file.seekg(0, std::ios::end);
    int32_t file_length = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string file_content(file_length, '\0');
    file.read(file_content.data(), file_length);
    file.close();

    const char* file_content_pointer = file_content.c_str();

    GLint shader = glCreateShader(type);
    glShaderSource(shader, 1, &file_content_pointer, &file_length);
    glCompileShader(shader);

    if(!show_shader_log(shader, this->shader_name))
    {
        return false;
    }

    uint32_t shader_index = get_shader_index(type);
    this->active_shaders[shader_index] = type;

    if(this->program == 0)
    {
        this->program = glCreateProgram();
    }

    glAttachShader(this->program, shader);
    glDeleteShader(shader);

    return true;
}

bool Shader::load_shader(const char* file_name, ShaderType type, const ShaderDefines& defines)
{
    std::fstream file;
    file.open(file_name, std::ios::in | std::ios::binary);

    if (!file.good())
    {
        spdlog::error("Shader: Can't find file '{}' for shader '{}'!", file_name, this->shader_name);

        file.close();

        return false;
    }

    file.seekg(0, std::ios::end);
    int32_t file_length = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string file_content(file_length, '\0');
    file.read(file_content.data(), file_length);
    file.close();

    defines.apply_defines(file_content);
    file_length = file_content.size();

    const char* file_content_pointer = file_content.c_str();

    GLint shader = glCreateShader(type);
    glShaderSource(shader, 1, &file_content_pointer, &file_length);
    glCompileShader(shader);

    if (!show_shader_log(shader, this->shader_name))
    {
        return false;
    }

    uint32_t shader_index = get_shader_index(type);
    this->active_shaders[shader_index] = type;

    if (this->program == 0)
    {
        this->program = glCreateProgram();
    }

    glAttachShader(this->program, shader);
    glDeleteShader(shader);

    return true;
}

bool Shader::link_program(bool make_separate, bool show_binary)
{
    if (this->program == 0)
    {
        return false;
    }

    if (make_separate)
    {
        glProgramParameteri(this->program, GL_PROGRAM_SEPARABLE, GL_TRUE);
    }

    if(show_binary)
    {
        glProgramParameteri(this->program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
    }

    glLinkProgram(this->program);

    if(!show_program_log(this->program, this->shader_name))
    {
        return false;
    }

    this->extract_uniforms();
    this->extract_subroutines();

    return true;
}

void Shader::clear_program()
{
    if(this->program != 0)
    {
        glDeleteProgram(this->program);
    }

    for(UniformEntry* uniform : this->uniforms)
    {
        delete uniform;
    }

    for(uint32_t index = 0; index < this->subroutines.size(); index++)
    {
        for (SubroutineUniformEntry* subroutine : this->subroutines[index])
        {
            delete subroutine;
        }
        
        this->subroutines[index].clear();
    }

    this->program = 0;
    this->uniforms.clear();
}

bool Shader::set_shader_storage_buffer(const char* buffer_name, uint32_t binding_point) const
{
    if (this->program == 0)
    {
        return false;
    }

    uint32_t block_index = glGetProgramResourceIndex(this->program, GL_SHADER_STORAGE_BLOCK, buffer_name);

    if(block_index == GL_INVALID_INDEX)
    {
        spdlog::error("Shader: Can't bind buffer '{}' to binding point '{}' of shader '{}'!", buffer_name, binding_point, this->shader_name);

        return false;
    }

    glShaderStorageBlockBinding(this->program, block_index, binding_point);

    return true;
}

bool Shader::set_uniform_buffer(const char* buffer_name, uint32_t binding_point) const
{
    if (this->program == 0)
    {
        return false;
    }

    uint32_t block_index = glGetUniformBlockIndex(this->program, buffer_name);

    if(block_index == GL_INVALID_INDEX)
    {
        spdlog::error("Shader: Can't bind buffer '{}' to binding point '{}' of shader '{}'!", buffer_name, binding_point, this->shader_name);

        return false;
    }

    glUniformBlockBinding(this->program, block_index, binding_point);

    return true;
}

bool Shader::set_subroutines(ShaderType shader_type, uint32_t subroutine_count, ShaderSubroutine* subroutine_list) const
{
    uint32_t shader_index = get_shader_index(shader_type);

    if(this->subroutines[shader_index].size() != subroutine_count)
    {
        spdlog::error("Shader: Wrong subroutine uniform count for shader '{}'!", this->shader_name);

        return false;
    }

    std::vector<uint32_t> uniform_indices;

    for(const SubroutineUniformEntry* uniform_entry : this->subroutines[shader_index])
    {
        ShaderSubroutine* subroutine = nullptr;

        for(uint32_t index = 0; index < subroutine_count; index++)
        {
            if(uniform_entry->uniform_name == subroutine_list[index].get_name())
            {
                subroutine = &subroutine_list[index];

                break;
            }
        }

        if(subroutine == nullptr)
        {
            spdlog::error("Shader: Can't find subroutine uniform '{}' in shader '{}'", uniform_entry->uniform_name, this->shader_name);

            return false;
        }

        const std::vector<std::string>& indices = subroutine->get_indices();

        if(uniform_entry->array_size != indices.size())
        {
            spdlog::error("Shader: Wrong subroutine index count for shader '{}'", this->shader_name);

            return false;
        }

        for(uint32_t index = 0; index < indices.size(); index++)
        {
            bool found = false;

            for(const SubroutineIndexEntry& index_entry : uniform_entry->indices)
            {
                if(indices[index] == index_entry.name)
                {
                    uniform_indices.push_back(index_entry.index);
                    found = true;

                    break;
                }
            }

            if(!found)
            {
                spdlog::error("Shader: Incompatible subroutine index for shader '{}'", this->shader_name);

                return false;
            }
        }
    }

    glUniformSubroutinesuiv(shader_type, subroutine_count, uniform_indices.data());

    return true;    
}

ShaderUniform Shader::operator[](const char* uniform_name) const
{
    for(const UniformEntry* entry : this->uniforms)
    {
        if (entry->name.compare(uniform_name) == 0)
        {
            return ShaderUniform(entry, this);
        }
    }

    spdlog::warn("Shader: Can't find uniform '{}' in shader '{}'!", uniform_name, this->shader_name);

    return ShaderUniform(nullptr, this);
}

const std::string& Shader::get_shader_name() const
{
    return this->shader_name;
}

uint32_t Shader::get_program() const
{
    return this->program;
}

void Shader::use_shader() const
{
    glUseProgram(this->program);
}

void Shader::use_default() const
{
    glUseProgram(0);
}

std::string Shader::get_program_binary() const
{
    int32_t format_count = 0;
    glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &format_count);

    std::vector<int32_t> format_list(format_count);
    glGetIntegerv(GL_PROGRAM_BINARY_FORMATS, format_list.data());

    int32_t binary_length = 0;
    glGetProgramiv(this->program, GL_PROGRAM_BINARY_LENGTH, &binary_length);

    std::string binary_string(binary_length, '\0');
    glGetProgramBinary(this->program, binary_length, nullptr, (GLenum*)format_list.data(), binary_string.data());

    return binary_string;
}

void Shader::extract_uniforms()
{
    for(UniformEntry* entry : this->uniforms)
    {
        delete entry;
    }

    this->uniforms.clear();
    
    int32_t uniform_count = 0;
    glGetProgramiv(this->program, GL_ACTIVE_UNIFORMS, &uniform_count);

    std::vector<uint32_t> uniform_indices(uniform_count);

    for(uint32_t index = 0; index < uniform_count; index++)
    {
        uniform_indices[index] = index;
    }

    std::vector<int32_t> uniform_types(uniform_count);
    glGetActiveUniformsiv(this->program, uniform_count, uniform_indices.data(), GL_UNIFORM_TYPE, uniform_types.data());

    std::vector<int32_t> uniform_array_size(uniform_count);
    glGetActiveUniformsiv(this->program, uniform_count, uniform_indices.data(), GL_UNIFORM_SIZE, uniform_array_size.data());

    std::vector<int32_t> uniform_in_block(uniform_count);
    glGetActiveUniformsiv(this->program, uniform_count, uniform_indices.data(), GL_UNIFORM_BLOCK_INDEX, uniform_in_block.data());

    for (uint32_t index = 0; index < uniform_count; index++)
    {
        if(uniform_in_block[index] != -1)
        {
            continue;
        }

        char uniform_name[256];
        memset(uniform_name, 0, sizeof(uniform_name));

        glGetActiveUniformName(this->program, index, sizeof(uniform_name) - 1, nullptr, uniform_name);

        uint32_t uniform_location = glGetUniformLocation(this->program, uniform_name);

        UniformEntry* entry = new UniformEntry;
        entry->name = uniform_name;
        entry->location = uniform_location;
        entry->type = uniform_types[index];
        entry->array_size = uniform_array_size[index];

        this->uniforms.push_back(entry);
    }
}

void Shader::extract_subroutines()
{
    for (uint32_t index = 0; index < this->subroutines.size(); index++)
    {
        for (SubroutineUniformEntry* subroutine : this->subroutines[index])
        {
            delete subroutine;
        }

        this->subroutines[index].clear();
    }

    for (uint32_t shader_index = 0; shader_index < this->active_shaders.size(); shader_index++)
    {
        uint32_t shader_type = this->active_shaders[shader_index];

        if(shader_type == 0)
        {
            continue;
        }

        int32_t subroutine_count = 0;
        glGetProgramStageiv(this->program, shader_type, GL_ACTIVE_SUBROUTINE_UNIFORMS, &subroutine_count);

        for(uint32_t subroutine_index = 0; subroutine_index < subroutine_count; subroutine_index++)
        {
            int32_t name_length = 0;
            glGetActiveSubroutineUniformName(this->program, shader_type, subroutine_index, 0, &name_length, nullptr);

            std::string name_string(name_length, '\0');
            glGetActiveSubroutineUniformName(this->program, shader_type, subroutine_index, name_length, &name_length, name_string.data());

            int32_t array_size = 0;
            glGetActiveSubroutineUniformiv(this->program, shader_type, subroutine_index, GL_UNIFORM_SIZE, &array_size);

            int32_t index_count = 0;
            glGetActiveSubroutineUniformiv(this->program, shader_type, subroutine_index, GL_NUM_COMPATIBLE_SUBROUTINES, &index_count);

            std::vector<int32_t> indices;
            glGetActiveSubroutineUniformiv(this->program, shader_type, subroutine_index, GL_COMPATIBLE_SUBROUTINES, indices.data());

            SubroutineUniformEntry* subroutine = new SubroutineUniformEntry;
            subroutine->uniform_name = name_string;
            subroutine->array_size = array_size;

            for(uint32_t index = 0; index < index_count; index++)
            {
                int32_t index_name_length = 0;
                glGetActiveSubroutineName(this->program, shader_type, indices[index], 0, &index_name_length, nullptr);

                std::string index_name_string(index_name_length, '\0');
                glGetActiveSubroutineName(this->program, shader_type, indices[index], index_name_length, &index_name_length, index_name_string.data());

                SubroutineIndexEntry index_entry;
                index_entry.name = index_name_string;
                index_entry.index = index;

                subroutine->indices.push_back(index_entry);
            }

            this->subroutines[shader_index].push_back(subroutine);
        }
    }
}
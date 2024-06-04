#include "line_generator.hpp"

#include <chrono>

bool LineQuadTreeLevel::create(const glm::uvec2& resolution)
{
    uint32_t level_buffser_size = resolution.x * resolution.y * sizeof(uint8_t);

    glGenBuffers(1, &this->level_buffer);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, this->level_buffer);

    glBufferStorage(GL_PIXEL_PACK_BUFFER, level_buffser_size, nullptr, GL_CLIENT_STORAGE_BIT | GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    this->level_pointer = (uint8_t*)glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, level_buffser_size, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    this->resolution = resolution;

    return true;
}

void LineQuadTreeLevel::destroy()
{
    glDeleteBuffers(1, &this->level_buffer);

    this->level_buffer = 0;
    this->level_pointer = nullptr;
}

inline bool LineQuadTreeLevel::set_pixel(const glm::ivec2& coord, uint8_t value)
{
    if (coord.x < 0 || coord.x >= this->resolution.x)
    {
        return false;
    }

    if (coord.y < 0 || coord.y >= this->resolution.y)
    {
        return false;
    }

    uint32_t offset = coord.y * this->resolution.x + coord.x;
    this->level_pointer[offset] = value;

    return true;
}

inline bool LineQuadTreeLevel::get_pixel(const glm::ivec2& coord, uint8_t& value) const
{
    if (coord.x < 0 || coord.x >= this->resolution.x)
    {
        return false;
    }

    if (coord.y < 0 || coord.y >= this->resolution.y)
    {
        return false;
    }

    uint32_t offset = coord.y * this->resolution.x + coord.x;
    value = this->level_pointer[offset];

    return true;
}

const glm::uvec2& LineQuadTreeLevel::get_resolution() const
{
    return this->resolution;
}

GLuint LineQuadTreeLevel::get_level_buffer() const
{
    return this->level_buffer;
}

bool LineQuadTree::create(const glm::uvec2& resolution)
{
    glm::uvec2 level_resolution = resolution;

    while (level_resolution.x > 1 && level_resolution.y > 1)
    {
        LineQuadTreeLevel level;

        if (!level.create(level_resolution))
        {
            return false;
        }

        this->levels.push_back(level);

        level_resolution = (level_resolution + glm::uvec2(1)) / glm::uvec2(2);
    }

    LineQuadTreeLevel level;

    if (!level.create(level_resolution))
    {
        return false;
    }

    this->levels.push_back(level);

    return true;
}

void LineQuadTree::destroy()
{
    for (LineQuadTreeLevel& level : this->levels)
    {
        level.destroy();
    }

    this->levels.clear();
}

bool LineQuadTree::fill(GLuint buffer)
{
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    glBindTexture(GL_TEXTURE_2D, buffer);

    for(uint32_t index = 0; index < this->levels.size(); index++)
    {
        const LineQuadTreeLevel& level = this->levels[index];

        glBindBuffer(GL_PIXEL_PACK_BUFFER, level.get_level_buffer());

        glGetTexImage(GL_TEXTURE_2D, index, GL_RED, GL_UNSIGNED_BYTE, nullptr);  
    }

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    glPixelStorei(GL_PACK_ALIGNMENT, 4);

    return true;
}

bool LineQuadTree::remove(const glm::ivec2& coord)
{
    LineQuadTreeLevel& base_level = this->levels.front();

    if (!base_level.set_pixel(coord, 0))
    {
        return false;
    }

    for (uint32_t index = 1; index < this->levels.size(); index++)
    {
        LineQuadTreeLevel& src_level = this->levels[index - 1];
        LineQuadTreeLevel& dst_level = this->levels[index];

        glm::ivec2 dst_coord = coord >> (int32_t)index;
        uint8_t dst_value = 0;

        for (uint32_t offset_y = 0; offset_y < 2; offset_y++)
        {
            for (uint32_t offset_x = 0; offset_x < 2; offset_x++)
            {
                glm::ivec2 src_coord = dst_coord * 2 + glm::ivec2(offset_x, offset_y);
                uint8_t src_value = 0;

                if (!src_level.get_pixel(src_coord, src_value))
                {
                    continue;
                }

                dst_value = glm::max(dst_value, src_value);
            }
        }

        if (!dst_level.set_pixel(dst_coord, dst_value))
        {
            return false;
        }
    }

    return true;
}

bool LineQuadTree::find_global(glm::ivec2& coord) const
{
    const LineQuadTreeLevel& root_level = this->levels.back();

    glm::ivec2 global_coord = glm::ivec2(0);
    uint8_t global_value = 0;

    if (!root_level.get_pixel(global_coord, global_value))
    {
        return false;
    }

    for (int32_t index = this->levels.size() - 1; index >= 1; index--)
    {
        const LineQuadTreeLevel& level = this->levels[index - 1];

        glm::ivec2 seach_coord = glm::ivec2(0);
        uint8_t search_value = 0;

        for (uint32_t offset_y = 0; offset_y < 2; offset_y++)
        {
            for (uint32_t offset_x = 0; offset_x < 2; offset_x++)
            {
                glm::ivec2 level_coord = global_coord * 2 + glm::ivec2(offset_x, offset_y);
                uint8_t level_value = 0;

                if (!level.get_pixel(level_coord, level_value))
                {
                    continue;
                }

                if (level_value > search_value)
                {
                    seach_coord = level_coord;
                    search_value = level_value;
                }
            }
        }

        if (search_value != global_value)
        {
            return false;
        }

        global_coord = seach_coord;
    }

    if (global_value > 0)
    {
        coord = global_coord;

        return true;
    }

    return false;
}

bool LineQuadTree::find_local(const glm::ivec2& center_coord, std::vector<glm::ivec2>& coords) const
{
    const LineQuadTreeLevel& base_level = this->levels.front();

    std::vector<glm::ivec2> local_coords;

    for (int32_t offset_y = -1; offset_y <= 1; offset_y++)
    {
        for (int32_t offset_x = -1; offset_x <= 1; offset_x++)
        {
            if (offset_x == 0 && offset_y == 0)
            {
                continue;
            }

            glm::ivec2 base_coord = center_coord + glm::ivec2(offset_x, offset_y);
            uint8_t base_value = 0;

            if (!base_level.get_pixel(base_coord, base_value))
            {
                continue;
            }

            if (base_value > 0)
            {
                local_coords.push_back(base_coord);
            }
        }
    }

    if (!local_coords.empty())
    {
        coords = local_coords;

        return true;
    }

    return false;
}

bool LineQuadTree::find_local_max(const glm::ivec2& center_coord, glm::ivec2& coord) const
{
    const LineQuadTreeLevel& base_level = this->levels.front();

    glm::ivec2 local_coord = glm::ivec2(0);
    uint8_t local_value = 0;

    for (int32_t offset_y = -1; offset_y <= 1; offset_y++)
    {
        for (int32_t offset_x = -1; offset_x <= 1; offset_x++)
        {
            if (offset_x == 0 && offset_y == 0)
            {
                continue;
            }

            glm::ivec2 base_coord = center_coord + glm::ivec2(offset_x, offset_y);
            uint8_t base_value = 0;

            if (!base_level.get_pixel(base_coord, base_value))
            {
                continue;
            }

            if (base_value > local_value)
            {
                local_coord = base_coord;
                local_value = base_value;
            }
        }
    }

    if (local_value > 0)
    {
        coord = local_coord;

        return true;
    }

    return false;
}

bool LineQuadTree::get_pixel(const glm::ivec2& coord, uint8_t& value) const
{
    const LineQuadTreeLevel& base_level = this->levels.front();

    return base_level.get_pixel(coord, value);
}

uint32_t LineQuadTree::get_level_count() const
{
    return this->levels.size();
}

bool LineGeneratorFrame::triangulate(std::vector<shared::Vertex>& vertices, std::vector<shared::Index>& indices, shared::ViewMetadata& metadata, std::vector<MeshFeatureLine>& feature_lines, bool export_feature_lines)
{
    metadata.line.time_cpu = 0.0f;
    metadata.line.time_line_trace = 0.0f;
    metadata.line.time_triangulation = 0.0f;
    metadata.line.line_count = 0;

    metadata.line.time_edge_detection = this->time_edge;
    metadata.line.time_quad_tree = this->time_quad_tree;

    std::chrono::high_resolution_clock::time_point cpu_start = std::chrono::high_resolution_clock::now();
    this->triangulation.process(this->resolution, this->depth_max, this->line_length_min, this->depth_copy_pointer, this->quad_tree, vertices, indices, metadata, feature_lines, export_feature_lines);
    std::chrono::high_resolution_clock::time_point cpu_end = std::chrono::high_resolution_clock::now();
    metadata.line.time_cpu = std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(cpu_end - cpu_start).count();

    return true;
}

GLuint LineGeneratorFrame::get_depth_buffer() const
{
    return this->depth_buffer;
}

GLuint LineGeneratorFrame::get_normal_buffer() const
{
    return this->normal_buffer;
}

GLuint LineGeneratorFrame::get_object_id_buffer() const
{
    return this->object_id_buffer;
}

bool LineGenerator::create(const glm::uvec2& resolution)
{
    if (!this->create_buffers(resolution))
    {
        return false;
    }

    if (!this->create_shaders())
    {
        return false;
    }

    this->resolution = resolution;

    return true;
}

void LineGenerator::destroy()
{
    if (this->edge_buffer != 0)
    {
        glDeleteTextures(1, &this->edge_buffer);
    }

    this->edge_buffer = 0;
}

void LineGenerator::apply(const shared::MeshSettings& settings)
{
    this->depth_max = settings.depth_max;
    this->laplace_threshold = settings.line.laplace_threshold;
    this->normal_scale = settings.line.normal_scale;
    this->line_length_min = settings.line.line_length_min;
}

MeshGeneratorFrame* LineGenerator::create_frame()
{
    Timer edge_timer;
    Timer quad_tree_timer;

    if (!edge_timer.create())
    {
        return nullptr;
    }

    if (!quad_tree_timer.create())
    {
        return nullptr;
    }

    LineQuadTree quad_tree;

    if (!quad_tree.create(this->resolution))
    {
        return nullptr;
    }

    GLuint depth_buffer = 0;

    glGenTextures(1, &depth_buffer);
    glBindTexture(GL_TEXTURE_2D, depth_buffer);

    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT32, this->resolution.x, this->resolution.y);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, 0);

    GLuint normal_buffer = 0;

    glGenTextures(1, &normal_buffer);
    glBindTexture(GL_TEXTURE_2D, normal_buffer);

    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RG8, this->resolution.x, this->resolution.y);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, 0);

    GLuint object_id_buffer = 0;

    glGenTextures(1, &object_id_buffer);
    glBindTexture(GL_TEXTURE_2D, object_id_buffer);

    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, this->resolution.x, this->resolution.y);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, 0);

    uint32_t depth_copy_buffer_size = this->resolution.x * this->resolution.y * sizeof(float);

    GLuint depth_copy_buffer = 0;
   
    glGenBuffers(1, &depth_copy_buffer);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, depth_copy_buffer);

    glBufferStorage(GL_PIXEL_PACK_BUFFER, depth_copy_buffer_size, nullptr, GL_CLIENT_STORAGE_BIT | GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    float* depth_copy_pointer = (float*)glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, depth_copy_buffer_size, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    LineGeneratorFrame* line_frame = new LineGeneratorFrame;
    line_frame->quad_tree = quad_tree;
    line_frame->resolution = this->resolution;
    line_frame->depth_buffer = depth_buffer;
    line_frame->normal_buffer = normal_buffer;
    line_frame->object_id_buffer = object_id_buffer;
    line_frame->depth_copy_buffer = depth_copy_buffer;
    line_frame->depth_copy_pointer = depth_copy_pointer;
    line_frame->edge_timer = edge_timer;
    line_frame->quad_tree_timer = quad_tree_timer;

    return line_frame;
}

void LineGenerator::destroy_frame(MeshGeneratorFrame* frame)
{
    LineGeneratorFrame* line_frame = (LineGeneratorFrame*)frame;

    glDeleteTextures(1, &line_frame->depth_buffer);
    glDeleteTextures(1, &line_frame->normal_buffer);
    glDeleteTextures(1, &line_frame->object_id_buffer);
    glDeleteBuffers(1, &line_frame->depth_copy_buffer);

    line_frame->depth_buffer = 0;
    line_frame->normal_buffer = 0;
    line_frame->object_id_buffer = 0;
    line_frame->depth_copy_buffer = 0;
    line_frame->depth_copy_pointer = nullptr;

    glDeleteSync(line_frame->fence);

    line_frame->fence = 0;

    line_frame->edge_timer.destroy();
    line_frame->quad_tree_timer.destroy();
    line_frame->quad_tree.destroy();

    delete line_frame;
}

bool LineGenerator::submit_frame(MeshGeneratorFrame* frame)
{
    LineGeneratorFrame* line_frame = (LineGeneratorFrame*)frame;
    line_frame->depth_max = this->depth_max;
    line_frame->line_length_min = this->line_length_min;

    this->perform_edge_pass(line_frame);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    this->perform_quad_tree_pass(line_frame);

    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);

    glBindTexture(GL_TEXTURE_2D, line_frame->depth_buffer);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, line_frame->depth_copy_buffer);

    glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    line_frame->quad_tree.fill(this->edge_buffer);

    line_frame->fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    return true;
}

bool LineGenerator::map_frame(MeshGeneratorFrame* frame)
{
    LineGeneratorFrame* line_frame = (LineGeneratorFrame*)frame;

    if (line_frame->fence == 0)
    {
        return false;
    }

    GLenum result = glClientWaitSync(line_frame->fence, GL_SYNC_FLUSH_COMMANDS_BIT, 0);

    if (result != GL_ALREADY_SIGNALED && result != GL_CONDITION_SATISFIED)
    {
        return false;
    }

    if (!line_frame->edge_timer.get_time(line_frame->time_edge, TIMER_UNIT_MILLISECONDS))
    {
        return false;
    }

    if (!line_frame->quad_tree_timer.get_time(line_frame->time_quad_tree, TIMER_UNIT_MILLISECONDS))
    {
        return false;
    }

    glDeleteSync(line_frame->fence);
    line_frame->fence = 0;

    return true;
}

bool LineGenerator::unmap_frame(MeshGeneratorFrame* frame)
{
    return true;
}

bool LineGenerator::create_buffers(const glm::uvec2& resolution)
{
    glm::uvec2 level_resolution = resolution;
    this->edge_buffer_levels = 1;

    while (level_resolution.x > 1 || level_resolution.y > 1)
    {
        this->edge_buffer_levels++;

        level_resolution = glm::max(glm::uvec2(1), level_resolution / glm::uvec2(2));
    }

    glGenTextures(1, &this->edge_buffer);
    glBindTexture(GL_TEXTURE_2D, this->edge_buffer);

    glTexStorage2D(GL_TEXTURE_2D, this->edge_buffer_levels, GL_R8, resolution.x, resolution.y);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

bool LineGenerator::create_shaders()
{
    ShaderDefines defines;
    defines.set_define_from_file("#include \"shared_defines.glsl\"", SHADER_DIRECTORY"shared_defines.glsl");
    defines.set_define_from_file("#include \"shared_math_library.glsl\"", SHADER_DIRECTORY"shared_math_library.glsl");

    if (!this->edge_shader.load_shader(SHADER_DIRECTORY"line_generator_edge_shader.comp", SHADER_TYPE_COMPUTE, defines))
    {
        return false;
    }

    if (!this->edge_shader.link_program())
    {
        return false;
    }

    if (!this->quad_tree_shader.load_shader(SHADER_DIRECTORY"line_generator_quad_tree_shader.comp", SHADER_TYPE_COMPUTE, defines))
    {
        return false;
    }

    if (!this->quad_tree_shader.link_program())
    {
        return false;
    }

    return true;
}

void LineGenerator::perform_edge_pass(LineGeneratorFrame* line_frame)
{
    std::string pass_label = "line_generator_edge_pass";
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, pass_label.size(), pass_label.data());

    line_frame->edge_timer.begin();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, line_frame->depth_buffer);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, line_frame->normal_buffer);
    
    glBindImageTexture(0, this->edge_buffer, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8);

    this->edge_shader.use_shader();
    this->edge_shader["depth_max"] = this->depth_max;
    this->edge_shader["laplace_threshold"] = this->laplace_threshold;
    this->edge_shader["normal_scale"] = this->normal_scale;
    
    glm::uvec2 work_group_size = glm::uvec2(LINE_GENERATOR_EDGE_WORK_GROUP_SIZE_X, LINE_GENERATOR_EDGE_WORK_GROUP_SIZE_Y);
    glm::uvec2 work_group_count = (this->resolution + work_group_size - glm::uvec2(1)) / work_group_size;

    glDispatchCompute(work_group_count.x, work_group_count.y, 1);

    this->edge_shader.use_default();

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    line_frame->edge_timer.end();

    glPopDebugGroup();
}

void LineGenerator::perform_quad_tree_pass(LineGeneratorFrame* line_frame)
{
    std::string pass_label = "line_generator_quad_tree_pass";
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, pass_label.size(), pass_label.data());

    line_frame->quad_tree_timer.begin();

    this->quad_tree_shader.use_shader();

    for (uint32_t index = 0; index < this->edge_buffer_levels; index++)
    {
        glBindImageTexture(0, this->edge_buffer, index, GL_FALSE, 0, GL_READ_ONLY, GL_R8);
        glBindImageTexture(1, this->edge_buffer, index + 1, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8);

        glm::uvec2 work_group_size = glm::uvec2(LINE_GENERATOR_QUAD_TREE_WORK_GROUP_SIZE_X, LINE_GENERATOR_QUAD_TREE_WORK_GROUP_SIZE_Y);
        glm::uvec2 work_group_count = (this->resolution + work_group_size - glm::uvec2(1)) / work_group_size;

        glDispatchCompute(work_group_count.x, work_group_count.y, 1);

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    this->quad_tree_shader.use_default();

    line_frame->quad_tree_timer.end();

    glPopDebugGroup();
}
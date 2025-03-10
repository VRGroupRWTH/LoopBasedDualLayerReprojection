#include "quad_generator.hpp"
#include <spdlog/spdlog.h>

bool QuadGeneratorFrame::triangulate(std::vector<shared::Vertex>& vertices, std::vector<shared::Index>& indices, shared::ViewMetadata& metadata, std::vector<MeshFeatureLine>& feature_lines, bool export_feature_lines)
{
    metadata.quad.time_copy = this->time_copy;
    metadata.quad.time_delta = this->time_delta;
    metadata.quad.time_refine = this->time_refine;
    metadata.quad.time_corner = this->time_corner;
    metadata.quad.time_write = this->time_write;

    if (this->count_pointer->vertex_count > QUAD_GENERATOR_MAX_VERTEX_COUNT)
    {
        spdlog::error("QuadGeneratorFrame: Vertex count exceeds buffer limit!");
    }

    vertices.resize(this->count_pointer->vertex_count);
    memcpy(vertices.data(), this->vertex_pointer, vertices.size() * sizeof(shared::Vertex));

    if (this->count_pointer->index_count > QUAD_GENERATOR_MAX_INDEX_COUNT)
    {
        spdlog::error("QuadGeneratorFrame: Index count exceeds buffer limit!");
    }

    indices.resize(this->count_pointer->index_count);
    memcpy(indices.data(), this->index_pointer, indices.size() * sizeof(shared::Index));

    return true;
}

GLuint QuadGeneratorFrame::get_depth_buffer() const
{
    return this->depth_buffer;
}

GLuint QuadGeneratorFrame::get_normal_buffer() const
{
    return this->normal_buffer;
}

GLuint QuadGeneratorFrame::get_object_id_buffer() const
{
    return this->object_id_buffer;
}

bool QuadGenerator::create(const glm::uvec2& resolution)
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

void QuadGenerator::destroy()
{
    this->destroy_buffers();
}

void QuadGenerator::apply(const shared::MeshSettings& settings)
{
    this->depth_max = settings.depth_max;
    this->depth_threshold = settings.quad.depth_threshold;
}

MeshGeneratorFrame* QuadGenerator::create_frame()
{
    Timer copy_timer;
    Timer delta_timer;
    Timer refine_timer;
    Timer corner_timer;
    Timer write_timer;

    if (!copy_timer.create())
    {
        return nullptr;
    }

    if (!delta_timer.create())
    {
        return nullptr;
    }

    if (!refine_timer.create())
    {
        return nullptr;
    }

    if (!corner_timer.create())
    {
        return nullptr;
    }

    if (!write_timer.create())
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

    GLuint count_buffer = 0;

    glGenBuffers(1, &count_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, count_buffer);

    glBufferStorage(GL_SHADER_STORAGE_BUFFER, sizeof(glsl::QuadCount), nullptr, GL_CLIENT_STORAGE_BIT | GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    glsl::QuadCount* count_pointer = (glsl::QuadCount*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(glsl::QuadCount), GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    GLuint vertex_buffer = 0;

    glGenBuffers(1, &vertex_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, vertex_buffer);

    glBufferStorage(GL_SHADER_STORAGE_BUFFER, QUAD_GENERATOR_MAX_VERTEX_COUNT * sizeof(glsl::QuadVertex), nullptr, GL_CLIENT_STORAGE_BIT | GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    glsl::QuadVertex* vertex_pointer = (glsl::QuadVertex*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, QUAD_GENERATOR_MAX_VERTEX_COUNT * sizeof(glsl::QuadVertex), GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    GLuint index_buffer = 0;

    glGenBuffers(1, &index_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, index_buffer);

    glBufferStorage(GL_SHADER_STORAGE_BUFFER, QUAD_GENERATOR_MAX_INDEX_COUNT * sizeof(uint32_t), nullptr, GL_CLIENT_STORAGE_BIT | GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    uint32_t* index_pointer = (uint32_t*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, QUAD_GENERATOR_MAX_INDEX_COUNT * sizeof(uint32_t), GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    QuadGeneratorFrame* quad_frame = new QuadGeneratorFrame;
    quad_frame->depth_buffer = depth_buffer;
    quad_frame->normal_buffer = normal_buffer;
    quad_frame->object_id_buffer = object_id_buffer;
    quad_frame->count_buffer = count_buffer;
    quad_frame->vertex_buffer = vertex_buffer;
    quad_frame->index_buffer = index_buffer;
    quad_frame->count_pointer = count_pointer;
    quad_frame->vertex_pointer = vertex_pointer;
    quad_frame->index_pointer = index_pointer;
    quad_frame->copy_timer = copy_timer;
    quad_frame->delta_timer = delta_timer;
    quad_frame->refine_timer = refine_timer;
    quad_frame->corner_timer = corner_timer;
    quad_frame->write_timer = write_timer;

    return quad_frame;
}

void QuadGenerator::destroy_frame(MeshGeneratorFrame* frame)
{
    QuadGeneratorFrame* quad_frame = (QuadGeneratorFrame*)frame;

    glDeleteTextures(1, &quad_frame->depth_buffer);
    glDeleteTextures(1, &quad_frame->normal_buffer);
    glDeleteTextures(1, &quad_frame->object_id_buffer);

    quad_frame->depth_buffer = 0;
    quad_frame->normal_buffer = 0;
    quad_frame->object_id_buffer = 0;

    glDeleteBuffers(1, &quad_frame->count_buffer);
    glDeleteBuffers(1, &quad_frame->vertex_buffer);
    glDeleteBuffers(1, &quad_frame->index_buffer);

    quad_frame->count_buffer = 0;
    quad_frame->vertex_buffer = 0;
    quad_frame->index_buffer = 0;
    quad_frame->count_pointer = nullptr;
    quad_frame->vertex_pointer = nullptr;
    quad_frame->index_pointer = nullptr;

    glDeleteSync(quad_frame->fence);

    quad_frame->fence = 0;

    quad_frame->copy_timer.destroy();
    quad_frame->delta_timer.destroy();
    quad_frame->refine_timer.destroy();
    quad_frame->corner_timer.destroy();
    quad_frame->write_timer.destroy();

    delete quad_frame;
}

bool QuadGenerator::submit_frame(MeshGeneratorFrame* frame)
{
    QuadGeneratorFrame* quad_frame = (QuadGeneratorFrame*)frame;

    glsl::QuadIndirect indirect;
    indirect.group_count_x = 0;
    indirect.group_count_y = 1;
    indirect.group_count_z = 1;
    indirect.quad_count = 0;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->quad_indirect_buffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(indirect), &indirect);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    uint32_t read_buffer = (this->delta_buffer_levels - 1) % 2;
    indirect.group_count_x = (this->setup_buffer_count + QUAD_GENERATOR_REFINE_WORK_GROUP_SIZE_X - 1) / QUAD_GENERATOR_REFINE_WORK_GROUP_SIZE_X;
    indirect.quad_count = this->setup_buffer_count;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->refine_indirect_buffers[read_buffer]);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(indirect), &indirect);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glBindBuffer(GL_COPY_READ_BUFFER, this->setup_buffer);
    glBindBuffer(GL_COPY_WRITE_BUFFER, this->refine_buffers[read_buffer]);
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, this->setup_buffer_count * sizeof(glsl::Quad));
    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, quad_frame->count_buffer);
    glClearBufferSubData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, 0, sizeof(glsl::QuadCount), GL_RED, GL_UNSIGNED_INT, nullptr);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

    this->perform_copy_pass(quad_frame);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    this->perform_delta_pass(quad_frame);

    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

    this->perform_refine_pass(quad_frame);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    this->perform_corner_pass(quad_frame);

    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

    this->perform_write_pass(quad_frame);

    quad_frame->fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    return true;
}

bool QuadGenerator::map_frame(MeshGeneratorFrame* frame)
{
    QuadGeneratorFrame* quad_frame = (QuadGeneratorFrame*)frame;

    if (quad_frame->fence == 0)
    {
        return false;
    }

    GLenum result = glClientWaitSync(quad_frame->fence, GL_SYNC_FLUSH_COMMANDS_BIT, 0);

    if (result != GL_ALREADY_SIGNALED && result != GL_CONDITION_SATISFIED)
    {
        return false;
    }

    if (!quad_frame->copy_timer.get_time(quad_frame->time_copy, TIMER_UNIT_MILLISECONDS))
    {
        return false;
    }

    if (!quad_frame->delta_timer.get_time(quad_frame->time_delta, TIMER_UNIT_MILLISECONDS))
    {
        return false;
    }

    if (!quad_frame->refine_timer.get_time(quad_frame->time_refine, TIMER_UNIT_MILLISECONDS))
    {
        return false;
    }

    if (!quad_frame->corner_timer.get_time(quad_frame->time_corner, TIMER_UNIT_MILLISECONDS))
    {
        return false;
    }

    if (!quad_frame->write_timer.get_time(quad_frame->time_write, TIMER_UNIT_MILLISECONDS))
    {
        return false;
    }

    glDeleteSync(quad_frame->fence);
    quad_frame->fence = 0;

    return true;
}

bool QuadGenerator::unmap_frame(MeshGeneratorFrame* frame)
{
    return true;
}

bool QuadGenerator::create_buffers(const glm::uvec2& resolution)
{
    glm::uvec2 level_resolution = resolution;
    this->delta_buffer_levels = 1;

    while (level_resolution.x > 32 || level_resolution.y > 32)
    {
        this->delta_buffer_levels++;

        level_resolution = glm::max(glm::uvec2(1), level_resolution / glm::uvec2(2));
    }

    for (uint32_t index = 0; index < 2; index++)
    {
        glGenBuffers(1, &this->refine_indirect_buffers[index]);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->refine_indirect_buffers[index]);

        glBufferStorage(GL_SHADER_STORAGE_BUFFER, sizeof(glsl::QuadIndirect), nullptr, GL_DYNAMIC_STORAGE_BIT);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        glGenBuffers(1, &this->refine_buffers[index]);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->refine_buffers[index]);

        glBufferStorage(GL_SHADER_STORAGE_BUFFER, QUAD_GENERATOR_MAX_QUAD_COUNT * sizeof(glsl::Quad), nullptr, 0);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    glGenBuffers(1, &this->quad_indirect_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->quad_indirect_buffer);

    glBufferStorage(GL_SHADER_STORAGE_BUFFER, sizeof(glsl::QuadIndirect), nullptr, GL_DYNAMIC_STORAGE_BIT);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glGenBuffers(1, &this->quad_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->quad_buffer);

    glBufferStorage(GL_SHADER_STORAGE_BUFFER, QUAD_GENERATOR_MAX_QUAD_COUNT * sizeof(glsl::Quad), nullptr, 0);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    std::vector<glsl::Quad> setup_quads;
    
    for (uint32_t coord_y = 0; coord_y < level_resolution.y; coord_y++)
    {
        for (uint32_t coord_x = 0; coord_x < level_resolution.x; coord_x++)
        {
            glsl::Quad quad;
            quad.coord = glm::ivec2(coord_x, coord_y);
            quad.level = this->delta_buffer_levels - 1;

            setup_quads.push_back(quad);
        }
    }

    this->setup_buffer_count = setup_quads.size();

    glGenBuffers(1, &this->setup_buffer);
    glBindBuffer(GL_COPY_READ_BUFFER, this->setup_buffer);

    glBufferStorage(GL_COPY_READ_BUFFER, setup_quads.size() * sizeof(glsl::Quad), setup_quads.data(), 0);

    glBindBuffer(GL_COPY_READ_BUFFER, 0);

    glGenTextures(1, &this->delta_buffer);
    glBindTexture(GL_TEXTURE_2D, this->delta_buffer);

    glTexStorage2D(GL_TEXTURE_2D, this->delta_buffer_levels, GL_RG32F, resolution.x, resolution.y);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, 0);

    glGenTextures(1, &this->corner_buffer);
    glBindTexture(GL_TEXTURE_2D, this->corner_buffer);

    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, resolution.x + 1, resolution.y + 1);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

bool QuadGenerator::create_shaders()
{
    ShaderDefines defines;
    defines.set_define_from_file("#include \"shared_defines.glsl\"", SHADER_DIRECTORY"shared_defines.glsl");
    defines.set_define_from_file("#include \"shared_math_library.glsl\"", SHADER_DIRECTORY"shared_math_library.glsl");

    if (!this->copy_shader.load_shader(SHADER_DIRECTORY"quad_copy_shader.comp", SHADER_TYPE_COMPUTE, defines))
    {
        return false;
    }

    if (!this->copy_shader.link_program())
    {
        return false;
    }

    if (!this->delta_shader.load_shader(SHADER_DIRECTORY"quad_delta_shader.comp", SHADER_TYPE_COMPUTE, defines))
    {
        return false;
    }

    if (!this->delta_shader.link_program())
    {
        return false;
    }

    if (!this->refine_shader.load_shader(SHADER_DIRECTORY"quad_refine_shader.comp", SHADER_TYPE_COMPUTE, defines))
    {
        return false;
    }

    if (!this->refine_shader.link_program())
    {
        return false;
    }

    if (!this->corner_shader.load_shader(SHADER_DIRECTORY"quad_corner_shader.comp", SHADER_TYPE_COMPUTE, defines))
    {
        return false;
    }

    if (!this->corner_shader.link_program())
    {
        return false;
    }

    if (!this->write_shader.load_shader(SHADER_DIRECTORY"quad_write_shader.comp", SHADER_TYPE_COMPUTE, defines))
    {
        return false;
    }

    if (!this->write_shader.link_program())
    {
        return false;
    }

    return true;
}

void QuadGenerator::destroy_buffers()
{
    glDeleteBuffers(2, this->refine_indirect_buffers.data());
    glDeleteBuffers(2, this->refine_buffers.data());
    glDeleteBuffers(1, &this->quad_indirect_buffer);
    glDeleteBuffers(1, &this->quad_buffer);
    glDeleteBuffers(1, &this->setup_buffer);

    this->refine_indirect_buffers.fill(0);
    this->refine_buffers.fill(0);
    this->quad_indirect_buffer = 0;
    this->quad_buffer = 0;
    this->setup_buffer = 0;

    glDeleteTextures(1, &this->delta_buffer);
    glDeleteTextures(1, &this->corner_buffer);

    this->delta_buffer = 0;
    this->corner_buffer = 0;
}

void QuadGenerator::perform_copy_pass(QuadGeneratorFrame* quad_frame)
{
    std::string pass_label = "quad_copy_pass";
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, pass_label.size(), pass_label.data());

    quad_frame->copy_timer.begin();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, quad_frame->depth_buffer);

    glBindImageTexture(0, this->delta_buffer, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG32F);

    this->copy_shader.use_shader();
    this->copy_shader["depth_max"] = this->depth_max;

    glm::uvec2 work_group_size = glm::uvec2(QUAD_GENERATOR_COPY_WORK_GROUP_SIZE_X, QUAD_GENERATOR_COPY_WORK_GROUP_SIZE_Y);
    glm::uvec2 work_group_count = (this->resolution + work_group_size - glm::uvec2(1)) / work_group_size;

    glDispatchCompute(work_group_count.x, work_group_count.y, 1);

    this->copy_shader.use_default();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    quad_frame->copy_timer.end();

    glPopDebugGroup();
}

void QuadGenerator::perform_delta_pass(QuadGeneratorFrame* quad_frame)
{
    std::string pass_label = "quad_delta_pass";
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, pass_label.size(), pass_label.data());

    quad_frame->delta_timer.begin();

    this->delta_shader.use_shader();

    for (uint32_t index = 1; index < this->delta_buffer_levels; index++)
    {
        glm::uvec2 level_resolution = glm::max(glm::uvec2(1), this->resolution >> index);

        glBindImageTexture(0, this->delta_buffer, index - 1, GL_FALSE, 0, GL_READ_ONLY, GL_RG32F);
        glBindImageTexture(1, this->delta_buffer, index, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG32F);

        glm::uvec2 work_group_size = glm::uvec2(QUAD_GENERATOR_DELTA_WORK_GROUP_SIZE_X, QUAD_GENERATOR_DELTA_WORK_GROUP_SIZE_Y);
        glm::uvec2 work_group_count = (level_resolution + work_group_size - glm::uvec2(1)) / work_group_size;

        glDispatchCompute(work_group_count.x, work_group_count.y, 1);

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    this->delta_shader.use_default();

    quad_frame->delta_timer.end();

    glPopDebugGroup();
}

void QuadGenerator::perform_refine_pass(QuadGeneratorFrame* quad_frame)
{
    std::string pass_label = "quad_refine_pass";
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, pass_label.size(), pass_label.data());

    quad_frame->refine_timer.begin();

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, this->quad_indirect_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, this->quad_buffer);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, this->delta_buffer);

    glBindImageTexture(0, this->corner_buffer, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32UI);

    this->refine_shader.use_shader();
    this->refine_shader["depth_threshold"] = this->depth_threshold;

    for (int32_t index = this->delta_buffer_levels - 1; index >= 0; index--)
    {
        uint32_t read_buffer = index % 2;
        uint32_t write_buffer = (index + 1) % 2;

        glsl::QuadIndirect indirect;
        indirect.group_count_x = 0;
        indirect.group_count_y = 1;
        indirect.group_count_z = 1;
        indirect.quad_count = 0;

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->refine_indirect_buffers[write_buffer]);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(indirect), &indirect);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, this->refine_indirect_buffers[read_buffer]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, this->refine_buffers[read_buffer]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, this->refine_indirect_buffers[write_buffer]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, this->refine_buffers[write_buffer]);

        glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, this->refine_indirect_buffers[read_buffer]);

        glDispatchComputeIndirect(0);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
    }

    this->refine_shader.use_default();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    quad_frame->refine_timer.end();

    glPopDebugGroup();
}

void QuadGenerator::perform_corner_pass(QuadGeneratorFrame* quad_frame)
{
    std::string pass_label = "quad_corner_pass";
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, pass_label.size(), pass_label.data());

    quad_frame->corner_timer.begin();

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, quad_frame->count_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, quad_frame->vertex_buffer);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, quad_frame->depth_buffer);

    glBindImageTexture(0, this->corner_buffer, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);

    this->corner_shader.use_shader();

    glm::uvec2 work_group_size = glm::uvec2(QUAD_GENERATOR_CORNER_WORK_GROUP_SIZE_X, QUAD_GENERATOR_CORNER_WORK_GROUP_SIZE_Y);
    glm::uvec2 work_group_count = ((this->resolution + glm::uvec2(1)) + work_group_size - glm::uvec2(1)) / work_group_size;

    glDispatchCompute(work_group_count.x, work_group_count.y, 1);

    this->corner_shader.use_default();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    quad_frame->corner_timer.end();

    glPopDebugGroup();
}

void QuadGenerator::perform_write_pass(QuadGeneratorFrame* quad_frame)
{
    std::string pass_label = "quad_write_pass";
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, pass_label.size(), pass_label.data());

    quad_frame->write_timer.begin();

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, this->quad_indirect_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, this->quad_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, quad_frame->count_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, quad_frame->index_buffer);

    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, this->quad_indirect_buffer);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, this->corner_buffer);

    this->write_shader.use_shader();

    glDispatchComputeIndirect(0);

    this->write_shader.use_default();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    quad_frame->write_timer.end();

    glPopDebugGroup();
}
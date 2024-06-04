#include "loop_generator.hpp"

#include <chrono>

bool LoopGeneratorFrame::triangulate(std::vector<shared::Vertex>& vertices, std::vector<shared::Index>& indices, shared::ViewMetadata& metadata, std::vector<MeshFeatureLine>& features_lines, bool export_feature_lines)
{
    metadata.loop.time_cpu = 0.0f;
    metadata.loop.time_loop_simplification = 0.0f;
    metadata.loop.time_triangulation = 0.0f;
    metadata.loop.time_loop_info = 0.0f;
    metadata.loop.time_loop_sort = 0.0f;
    metadata.loop.time_sweep_line = 0.0f;
    metadata.loop.time_adjacent_two = 0.0f;
    metadata.loop.time_adjacent_one = 0.0f;
    metadata.loop.time_interval_search = 0.0f;
    metadata.loop.time_interval_update = 0.0f;
    metadata.loop.time_inside_outside = 0.0f;
    metadata.loop.time_contour_split = 0.0f;
    metadata.loop.time_contour = 0.0f;
    metadata.loop.loop_count = 0;
    metadata.loop.segment_count = 0;
    metadata.loop.point_count = 0;

    metadata.loop.time_vector = this->time_vector;
    metadata.loop.time_split = this->time_split;
    metadata.loop.time_base = this->time_base;
    metadata.loop.time_combine = this->time_combine;
    metadata.loop.time_distribute = this->time_distribute;
    metadata.loop.time_discard = this->time_discard;
    metadata.loop.time_write = this->time_write;

    std::chrono::high_resolution_clock::time_point cpu_start = std::chrono::high_resolution_clock::now();
    this->triangulation.process(this->resolution, this->triangle_scale, this->loop_pointer, this->loop_count_pointer, this->loop_segment_pointer, vertices, indices, metadata, features_lines, export_feature_lines);
    std::chrono::high_resolution_clock::time_point cpu_end = std::chrono::high_resolution_clock::now();
    metadata.loop.time_cpu = std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(cpu_end - cpu_start).count();

    return true;
}

GLuint LoopGeneratorFrame::get_depth_buffer() const
{
    return this->depth_buffer;
}

GLuint LoopGeneratorFrame::get_normal_buffer() const
{
    return this->normal_buffer;
}

GLuint LoopGeneratorFrame::get_object_id_buffer() const
{
    return this->object_id_buffer;
}

bool LoopGenerator::create(const glm::uvec2& resolution)
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

void LoopGenerator::destroy()
{
    this->destroy_buffers();
}

void LoopGenerator::apply(const shared::MeshSettings& settings)
{
    this->depth_max = settings.depth_max;
    this->depth_base_threshold = settings.loop.depth_base_threshold;
    this->depth_slope_threshold = settings.loop.depth_slope_threshold;
    this->normal_threshold = settings.loop.normal_threshold;
    this->triangle_scale = settings.loop.triangle_scale;
    this->loop_length_min = settings.loop.loop_length_min;
    this->use_normals = settings.loop.use_normals;
    this->use_object_ids = settings.loop.use_object_ids;
}

MeshGeneratorFrame* LoopGenerator::create_frame()
{
    Timer vector_timer;
    Timer split_timer;
    Timer base_timer;
    Timer combine_timer;
    Timer distribute_timer;
    Timer discard_timer;
    Timer write_timer;

    if (!vector_timer.create())
    {
        return nullptr;
    }

    if (!split_timer.create())
    {
        return nullptr;
    }

    if (!base_timer.create())
    {
        return nullptr;
    }

    if (!combine_timer.create())
    {
        return nullptr;
    }

    if (!distribute_timer.create())
    {
        return nullptr;
    }

    if (!discard_timer.create())
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

    uint32_t loop_buffer_size = sizeof(glsl::Loop) * LOOP_GENERATOR_MAX_LOOP_COUNT;
    uint32_t loop_count_buffer_size = sizeof(glsl::LoopCount);
    uint32_t loop_segment_buffer_size = sizeof(glsl::LoopSegment) * LOOP_GENERATOR_MAX_LOOP_SEGMENT_COUNT;

    GLuint loop_buffer = 0;
    GLuint loop_count_buffer = 0;
    GLuint loop_segment_buffer = 0;

    glGenBuffers(1, &loop_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, loop_buffer);

    glBufferStorage(GL_SHADER_STORAGE_BUFFER, loop_buffer_size, nullptr, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_CLIENT_STORAGE_BIT);
    glsl::Loop* loop_pointer = (glsl::Loop*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, loop_buffer_size, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glGenBuffers(1, &loop_count_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, loop_count_buffer);

    glBufferStorage(GL_SHADER_STORAGE_BUFFER, loop_count_buffer_size, nullptr, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_CLIENT_STORAGE_BIT);
    glsl::LoopCount* loop_count_pointer = (glsl::LoopCount*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, loop_count_buffer_size, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glGenBuffers(1, &loop_segment_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, loop_segment_buffer);

    glBufferStorage(GL_SHADER_STORAGE_BUFFER, loop_segment_buffer_size, nullptr, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_CLIENT_STORAGE_BIT);
    glsl::LoopSegment* loop_segment_pointer = (glsl::LoopSegment*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, loop_segment_buffer_size, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    LoopGeneratorFrame* loop_frame = new LoopGeneratorFrame;
    loop_frame->resolution = this->resolution;
    loop_frame->depth_buffer = depth_buffer;
    loop_frame->normal_buffer = normal_buffer;
    loop_frame->object_id_buffer = object_id_buffer;
    loop_frame->loop_buffer = loop_buffer;
    loop_frame->loop_pointer = loop_pointer;
    loop_frame->loop_count_buffer = loop_count_buffer;
    loop_frame->loop_count_pointer = loop_count_pointer;
    loop_frame->loop_segment_buffer = loop_segment_buffer;
    loop_frame->loop_segment_pointer = loop_segment_pointer;
    loop_frame->vector_timer = vector_timer;
    loop_frame->split_timer = split_timer;
    loop_frame->base_timer = base_timer;
    loop_frame->combine_timer = combine_timer;
    loop_frame->distribute_timer = distribute_timer;
    loop_frame->discard_timer = discard_timer;
    loop_frame->write_timer = write_timer;

    return loop_frame;
}

void LoopGenerator::destroy_frame(MeshGeneratorFrame* frame)
{
    LoopGeneratorFrame* loop_frame = (LoopGeneratorFrame*)frame;

    glDeleteTextures(1, &loop_frame->depth_buffer);
    glDeleteTextures(1, &loop_frame->normal_buffer);
    glDeleteTextures(1, &loop_frame->object_id_buffer);

    loop_frame->depth_buffer = 0;
    loop_frame->normal_buffer = 0;
    loop_frame->object_id_buffer = 0;

    glDeleteBuffers(1, &loop_frame->loop_buffer);
    glDeleteBuffers(1, &loop_frame->loop_count_buffer);
    glDeleteBuffers(1, &loop_frame->loop_segment_buffer);

    loop_frame->loop_buffer = 0;
    loop_frame->loop_count_buffer = 0;
    loop_frame->loop_segment_buffer = 0;

    glDeleteSync(loop_frame->fence);

    loop_frame->fence = 0;

    loop_frame->vector_timer.destroy();
    loop_frame->split_timer.destroy();
    loop_frame->base_timer.destroy();
    loop_frame->combine_timer.destroy();
    loop_frame->distribute_timer.destroy();
    loop_frame->discard_timer.destroy();
    loop_frame->write_timer.destroy();

    delete loop_frame;
}

bool LoopGenerator::submit_frame(MeshGeneratorFrame* frame)
{
    LoopGeneratorFrame* loop_frame = (LoopGeneratorFrame*)frame;
    loop_frame->triangle_scale = this->triangle_scale;
        
    glClearTexImage(this->vector_buffer, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nullptr);
    glClearTexImage(this->closed_buffer, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nullptr);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->loop_count_buffer);
    glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT); //TODO: Better barriers

    this->perform_vector_pass(loop_frame);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    this->perform_split_pass(loop_frame);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    this->perform_base_pass(loop_frame);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    this->perform_combine_pass(loop_frame);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    this->perform_distribute_pass(loop_frame);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    this->perform_discard_pass(loop_frame);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    this->perform_write_pass(loop_frame);
    
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

    uint32_t loop_buffer_size = sizeof(glsl::Loop) * LOOP_GENERATOR_MAX_LOOP_COUNT;
    uint32_t loop_count_buffer_size = sizeof(glsl::LoopCount);
    uint32_t loop_segment_buffer_size = sizeof(glsl::LoopSegment) * LOOP_GENERATOR_MAX_LOOP_SEGMENT_COUNT;

    glBindBuffer(GL_COPY_READ_BUFFER, this->loop_buffer);
    glBindBuffer(GL_COPY_WRITE_BUFFER, loop_frame->loop_buffer);

    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, loop_buffer_size);

    glBindBuffer(GL_COPY_READ_BUFFER, this->loop_count_buffer);
    glBindBuffer(GL_COPY_WRITE_BUFFER, loop_frame->loop_count_buffer);

    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, loop_count_buffer_size);

    glBindBuffer(GL_COPY_READ_BUFFER, this->loop_segment_buffer);
    glBindBuffer(GL_COPY_WRITE_BUFFER, loop_frame->loop_segment_buffer);

    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, loop_segment_buffer_size);

    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);

    loop_frame->fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    return true;
}

bool LoopGenerator::map_frame(MeshGeneratorFrame* frame)
{
    LoopGeneratorFrame* loop_frame = (LoopGeneratorFrame*)frame;

    if (loop_frame->fence == 0)
    {
        return false;
    }

    GLenum result = glClientWaitSync(loop_frame->fence, GL_SYNC_FLUSH_COMMANDS_BIT, 0);

    if (result != GL_ALREADY_SIGNALED && result != GL_CONDITION_SATISFIED)
    {
        return false;
    }

    if (!loop_frame->vector_timer.get_time(loop_frame->time_vector, TIMER_UNIT_MILLISECONDS))
    {
        return false;
    }

    if (!loop_frame->split_timer.get_time(loop_frame->time_split, TIMER_UNIT_MILLISECONDS))
    {
        return false;
    }

    if (!loop_frame->base_timer.get_time(loop_frame->time_base, TIMER_UNIT_MILLISECONDS))
    {
        return false;
    }

    if (!loop_frame->combine_timer.get_time(loop_frame->time_combine, TIMER_UNIT_MILLISECONDS))
    {
        return false;
    }

    if (!loop_frame->distribute_timer.get_time(loop_frame->time_distribute, TIMER_UNIT_MILLISECONDS))
    {
        return false;
    }

    if (!loop_frame->discard_timer.get_time(loop_frame->time_discard, TIMER_UNIT_MILLISECONDS))
    {
        return false;
    }

    if (!loop_frame->write_timer.get_time(loop_frame->time_write, TIMER_UNIT_MILLISECONDS))
    {
        return false;
    }

    glDeleteSync(loop_frame->fence);
    loop_frame->fence = 0;

    return true;
}

bool LoopGenerator::unmap_frame(MeshGeneratorFrame* frame)
{
    return true;
}

bool LoopGenerator::is_frame_empty(MeshGeneratorFrame* frame)
{
    LoopGeneratorFrame* loop_frame = (LoopGeneratorFrame*)frame;

    return (loop_frame->fence == 0);
}

bool LoopGenerator::create_buffers(const glm::uvec2& resolution)
{
    glm::uvec2 level_resolution = (resolution * glm::uvec2(2)) / glm::uvec2(LOOP_GENERATOR_BASE_CELL_SIZE);
    uint32_t level_cell_size = LOOP_GENERATOR_BASE_CELL_SIZE;

    while (true)
    {
        uint32_t cell_buffer_size = (level_cell_size * level_cell_size) / 4 + level_cell_size + 1;

        GLuint loop_range_buffer = 0;
        GLuint loop_ramge_count_buffer = 0;

        glGenBuffers(1, &loop_range_buffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, loop_range_buffer);

        glBufferStorage(GL_SHADER_STORAGE_BUFFER, sizeof(glsl::LoopRange) * level_resolution.x * level_resolution.y * cell_buffer_size, nullptr, 0);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        glGenTextures(1, &loop_ramge_count_buffer);
        glBindTexture(GL_TEXTURE_2D, loop_ramge_count_buffer);

        glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, level_resolution.x, level_resolution.y);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glBindTexture(GL_TEXTURE_2D, 0);

        LoopGeneratorLevel level;
        level.level_resolution = level_resolution;
        level.cell_buffer_size = (level_cell_size * level_cell_size) / 4 + level_cell_size + 1; //Only applies for LOOP_GENERATOR_BASE_CELL_SIZE = 8
        level.loop_range_buffer = loop_range_buffer;
        level.loop_range_count_buffer = loop_ramge_count_buffer;

        this->levels.push_back(level);

        if (level_resolution.x <= 1 && level_resolution.y <= 1)
        {
            break;
        }

        level_resolution = (level_resolution + glm::uvec2(1)) / glm::uvec2(2);
        level_cell_size <<= 1;
    }

    glGenTextures(1, &this->vector_buffer);
    glBindTexture(GL_TEXTURE_2D, this->vector_buffer);

    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8UI, resolution.x * 2, resolution.y * 2);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, 0);

    glGenTextures(1, &this->closed_buffer);
    glBindTexture(GL_TEXTURE_2D, this->closed_buffer);

    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8UI, resolution.x + 1, resolution.y + 1);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, 0);

    uint32_t loop_buffer_size = sizeof(glsl::Loop) * resolution.x * resolution.y;
    uint32_t loop_count_buffer_size = sizeof(glsl::LoopCount);
    uint32_t loop_segment_buffer_size = sizeof(glsl::LoopSegment) * (resolution.x * 2) * (resolution.y * 2);

    glGenBuffers(1, &this->loop_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->loop_buffer);

    glBufferStorage(GL_SHADER_STORAGE_BUFFER, loop_buffer_size, nullptr, 0);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glGenBuffers(1, &this->loop_count_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->loop_count_buffer);

    glBufferStorage(GL_SHADER_STORAGE_BUFFER, loop_count_buffer_size, nullptr, 0);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glGenBuffers(1, &this->loop_segment_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, this->loop_segment_buffer);

    glBufferStorage(GL_SHADER_STORAGE_BUFFER, loop_segment_buffer_size, nullptr, 0);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    return true;
}

bool LoopGenerator::create_shaders()
{
    ShaderDefines defines;
    defines.set_define_from_file("#include \"shared_defines.glsl\"", SHADER_DIRECTORY"shared_defines.glsl");
    defines.set_define_from_file("#include \"shared_math_library.glsl\"", SHADER_DIRECTORY"shared_math_library.glsl");

    if (!this->vector_shader.load_shader(SHADER_DIRECTORY"loop_vector_shader.comp", SHADER_TYPE_COMPUTE, defines))
    {
        return false;
    }

    if (!this->vector_shader.link_program())
    {
        return false;
    }

    if (!this->split_shader.load_shader(SHADER_DIRECTORY"loop_split_shader.comp", SHADER_TYPE_COMPUTE, defines))
    {
        return false;
    }

    if (!this->split_shader.link_program())
    {
        return false;
    }

    if (!this->base_shader.load_shader(SHADER_DIRECTORY"loop_base_shader.comp", SHADER_TYPE_COMPUTE, defines))
    {
        return false;
    }

    if (!this->base_shader.link_program())
    {
        return false;
    }

    if (!this->combine_shader.load_shader(SHADER_DIRECTORY"loop_combine_shader.comp", SHADER_TYPE_COMPUTE, defines))
    {
        return false;
    }

    if (!this->combine_shader.link_program())
    {
        return false;
    }

    if (!this->distribute_shader.load_shader(SHADER_DIRECTORY"loop_distribute_shader.comp", SHADER_TYPE_COMPUTE, defines))
    {
        return false;
    }

    if (!this->distribute_shader.link_program())
    {
        return false;
    }

    if (!this->discard_shader.load_shader(SHADER_DIRECTORY"loop_discard_shader.comp", SHADER_TYPE_COMPUTE, defines))
    {
        return false;
    }

    if (!this->discard_shader.link_program())
    {
        return false;
    }

    if (!this->write_shader.load_shader(SHADER_DIRECTORY"loop_write_shader.comp", SHADER_TYPE_COMPUTE, defines))
    {
        return false;
    }

    if (!this->write_shader.link_program())
    {
        return false;
    }

    return true;
}

void LoopGenerator::destroy_buffers()
{
    for (const LoopGeneratorLevel& level : this->levels)
    {
        glDeleteBuffers(1, &level.loop_range_buffer);
        glDeleteTextures(1, &level.loop_range_count_buffer);
    }

    this->levels.clear();

    glDeleteTextures(1, &this->vector_buffer);
    glDeleteTextures(1, &this->closed_buffer);

    this->vector_buffer = 0;
    this->closed_buffer = 0;

    glDeleteBuffers(1, &this->loop_buffer);
    glDeleteBuffers(1, &this->loop_count_buffer);
    glDeleteBuffers(1, &this->loop_segment_buffer);

    this->loop_buffer = 0;
    this->loop_count_buffer = 0;
    this->loop_segment_buffer = 0;
}

void LoopGenerator::perform_vector_pass(LoopGeneratorFrame* loop_frame)
{
    std::string pass_label = "loop_generator_vector_pass";
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, pass_label.size(), pass_label.data());

    loop_frame->vector_timer.begin();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, loop_frame->depth_buffer);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, loop_frame->normal_buffer);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, loop_frame->object_id_buffer);
        
    glBindImageTexture(0, this->vector_buffer, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8UI);
    glBindImageTexture(1, this->closed_buffer, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8UI);

    this->vector_shader.use_shader();
    this->vector_shader["depth_max"] = this->depth_max;
    this->vector_shader["depth_base_threshold"] = this->depth_base_threshold;
    this->vector_shader["depth_slope_threshold"] = this->depth_slope_threshold;
    this->vector_shader["normal_threshold"] = this->normal_threshold;
    this->vector_shader["use_normals"] = this->use_normals;
    this->vector_shader["use_object_ids"] = this->use_object_ids;

    glm::uvec2 work_group_size = glm::uvec2(LOOP_GENERATOR_VECTOR_WORK_GROUP_SIZE_X, LOOP_GENERATOR_VECTOR_WORK_GROUP_SIZE_Y);
    glm::uvec2 work_group_count = ((this->resolution + glm::uvec2(1)) + work_group_size - glm::uvec2(1)) / work_group_size;

    glDispatchCompute(work_group_count.x, work_group_count.y, 1);

    this->vector_shader.use_default();

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    loop_frame->vector_timer.end();

    glPopDebugGroup();
}

void LoopGenerator::perform_split_pass(LoopGeneratorFrame* loop_frame)
{
    std::string pass_label = "loop_generator_split_pass";
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, pass_label.size(), pass_label.data());

    loop_frame->split_timer.begin();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, this->closed_buffer);
        
    glBindImageTexture(0, this->vector_buffer, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8UI);
        
    this->split_shader.use_shader();

    glm::uvec2 work_group_size = glm::uvec2(LOOP_GENERATOR_SPLIT_WORK_GROUP_SIZE_X, LOOP_GENERATOR_SPLIT_WORK_GROUP_SIZE_Y);
    glm::uvec2 work_group_count = ((this->resolution + glm::uvec2(1)) + work_group_size - glm::uvec2(1)) / work_group_size;

    glDispatchCompute(work_group_count.x, work_group_count.y, 1);

    this->split_shader.use_default();

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);

    loop_frame->split_timer.end();

    glPopDebugGroup();
}

void LoopGenerator::perform_base_pass(LoopGeneratorFrame* loop_frame)
{
    std::string pass_label = "loop_generator_base_pass";
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, pass_label.size(), pass_label.data());

    loop_frame->base_timer.begin();

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, this->levels[0].loop_range_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, this->loop_count_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, this->loop_buffer);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, this->vector_buffer);

    glBindImageTexture(0, this->levels[0].loop_range_count_buffer, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32UI);

    this->base_shader.use_shader();
    this->base_shader["cell_buffer_size"] = this->levels[0].cell_buffer_size;
    this->base_shader["loop_min_length"] = this->loop_length_min;

    glm::uvec2 work_group_size = glm::uvec2(LOOP_GENERATOR_BASE_WORK_GROUP_SIZE_X, LOOP_GENERATOR_BASE_WORK_GROUP_SIZE_Y);
    glm::uvec2 work_group_count = (this->levels[0].level_resolution + work_group_size - glm::uvec2(1)) / work_group_size;

    glDispatchCompute(work_group_count.x, work_group_count.y, 1);

    this->base_shader.use_default();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    loop_frame->base_timer.end();

    glPopDebugGroup();
}

void LoopGenerator::perform_combine_pass(LoopGeneratorFrame* loop_frame)
{
    std::string pass_label = "loop_generator_combine_pass";
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, pass_label.size(), pass_label.data());

    loop_frame->combine_timer.begin();

    std::vector<uint32_t> work_group_sizes = this->compute_combine_work_group_sizes();

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, this->loop_count_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, this->loop_buffer);

    this->combine_shader.use_shader();
    this->combine_shader["loop_min_length"] = this->loop_length_min;

    for (uint32_t index = 0; index < this->levels.size() - 1; index++)
    {
        const LoopGeneratorLevel& src_level = this->levels[index];
        const LoopGeneratorLevel& dst_level = this->levels[index + 1];

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT); //TODO: Better barrier

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, src_level.loop_range_buffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, dst_level.loop_range_buffer);

        glBindImageTexture(0, src_level.loop_range_count_buffer, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32UI);
        glBindImageTexture(1, dst_level.loop_range_count_buffer, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32UI);

        this->combine_shader["cell_size_x"] = (uint32_t)(LOOP_GENERATOR_BASE_CELL_SIZE << index);
        this->combine_shader["cell_size_y"] = (uint32_t)(LOOP_GENERATOR_BASE_CELL_SIZE << index);
        this->combine_shader["src_cell_buffer_size"] = src_level.cell_buffer_size;
        this->combine_shader["dst_cell_buffer_size"] = dst_level.cell_buffer_size;

        glm::uvec2 work_group_count = dst_level.level_resolution;
        uint32_t work_group_size = work_group_sizes[index];

        glDispatchComputeGroupSizeARB(1, work_group_count.x, work_group_count.y, work_group_size, 2, 2);
    }

    this->combine_shader.use_default();

    loop_frame->combine_timer.end();

    glPopDebugGroup();
}

void LoopGenerator::perform_distribute_pass(LoopGeneratorFrame* loop_frame)
{
    std::string pass_label = "loop_generator_distribute_pass";
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, pass_label.size(), pass_label.data());

    loop_frame->distribute_timer.begin();

    std::vector<uint32_t> work_group_sizes = this->compute_distribute_work_group_sizes();

    this->distribute_shader.use_shader();

    for (int32_t index = this->levels.size() - 2; index >= 0; index--)
    {
        const LoopGeneratorLevel& src_level = this->levels[index];
        const LoopGeneratorLevel& dst_level = this->levels[index + 1];

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT); //TODO: Better barrier

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, src_level.loop_range_buffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, dst_level.loop_range_buffer);

        glBindImageTexture(0, src_level.loop_range_count_buffer, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
        glBindImageTexture(1, dst_level.loop_range_count_buffer, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32UI);

        this->distribute_shader["cell_size_x"] = (uint32_t)(LOOP_GENERATOR_BASE_CELL_SIZE << index);
        this->distribute_shader["cell_size_y"] = (uint32_t)(LOOP_GENERATOR_BASE_CELL_SIZE << index);
        this->distribute_shader["src_cell_buffer_size"] = src_level.cell_buffer_size;
        this->distribute_shader["dst_cell_buffer_size"] = dst_level.cell_buffer_size;

        uint32_t work_group_width_height = 2;

        if (index == this->levels.size() - 2)
        {
            work_group_width_height = 1;
        }

        glm::uvec2 work_group_count = (dst_level.level_resolution + glm::uvec2(work_group_width_height) - glm::uvec2(1)) / glm::uvec2(work_group_width_height);
        uint32_t work_group_size = work_group_sizes[index];

        glDispatchComputeGroupSizeARB(1, work_group_count.x, work_group_count.y, work_group_size, work_group_width_height, work_group_width_height);
    }

    this->distribute_shader.use_default();

    loop_frame->distribute_timer.end();

    glPopDebugGroup();
}

void LoopGenerator::perform_discard_pass(LoopGeneratorFrame* loop_frame)
{
    std::string pass_label = "loop_generator_discard_pass";
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, pass_label.size(), pass_label.data());

    loop_frame->discard_timer.begin();

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, this->levels[0].loop_range_buffer);

    glBindImageTexture(0, this->vector_buffer, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R8UI);
    glBindImageTexture(1, this->levels[0].loop_range_count_buffer, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32UI);

    this->discard_shader.use_shader();
    this->discard_shader["cell_buffer_size"] = this->levels[0].cell_buffer_size;

    glm::uvec2 work_group_size = glm::uvec2(LOOP_GENERATOR_DISCARD_WORK_GROUP_SIZE_X, LOOP_GENERATOR_DISCARD_WORK_GROUP_SIZE_Y);
    glm::uvec2 work_group_count = (this->levels[0].level_resolution + work_group_size - glm::uvec2(1)) / work_group_size;

    glDispatchCompute(work_group_count.x, work_group_count.y, 1);

    this->discard_shader.use_default();

    loop_frame->discard_timer.end();

    glPopDebugGroup();
}

void LoopGenerator::perform_write_pass(LoopGeneratorFrame* loop_frame)
{
    std::string pass_label = "loop_generator_write_pass";
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, pass_label.size(), pass_label.data());

    loop_frame->write_timer.begin();

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, this->levels[0].loop_range_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, this->loop_segment_buffer);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, loop_frame->depth_buffer);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, this->vector_buffer);

    glBindImageTexture(0, this->levels[0].loop_range_count_buffer, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32UI);

    this->write_shader.use_shader();
    this->write_shader["depth_max"] = this->depth_max;
    this->write_shader["cell_buffer_size"] = this->levels[0].cell_buffer_size;

    glm::uvec2 work_group_size = glm::uvec2(LOOP_GENERATOR_WRITE_WORK_GROUP_SIZE_X, LOOP_GENERATOR_WRITE_WORK_GROUP_SIZE_Y);
    glm::uvec2 work_group_count = (this->levels[0].level_resolution + work_group_size - glm::uvec2(1)) / work_group_size;

    glDispatchCompute(work_group_count.x, work_group_count.y, 1);

    this->write_shader.use_default();

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    loop_frame->write_timer.end();

    glPopDebugGroup();
}

std::vector<uint32_t> LoopGenerator::compute_combine_work_group_sizes() const
{
    std::vector<uint32_t> work_group_sizes(this->levels.size() - 1, 64);
    work_group_sizes[0] = 24;
    work_group_sizes[1] = 32;
    work_group_sizes[work_group_sizes.size() - 2] = 96;
    work_group_sizes[work_group_sizes.size() - 1] = 128;

    return work_group_sizes;
}

std::vector<uint32_t> LoopGenerator::compute_distribute_work_group_sizes() const
{
    std::vector<uint32_t> work_group_sizes(this->levels.size() - 1, 64);
    work_group_sizes[0] = 24;
    work_group_sizes[1] = 32;
    work_group_sizes[work_group_sizes.size() - 3] = 96;
    work_group_sizes[work_group_sizes.size() - 2] = 128;
    work_group_sizes[work_group_sizes.size() - 1] = 128;

    return work_group_sizes;
}
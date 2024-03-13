#version 430

layout(binding = 0, r32ui) uniform uimage3D scene_indirect_visibility_x_buffer;
layout(binding = 1, r32ui) uniform uimage3D scene_indirect_visibility_y_buffer;
layout(binding = 2, r32ui) uniform uimage3D scene_indirect_visibility_z_buffer;

layout(location = 0) in vec3 in_domain_coord;

uniform uvec3 scene_indirect_cell_count;

void main()
{
    ivec3 base_coord = ivec3(in_domain_coord * vec3(scene_indirect_cell_count * 3));
    ivec3 cell_coord = base_coord / 3;

    ivec3 sample_coord = base_coord % 3;
    uint sample_offset = sample_coord.x * 9 + sample_coord.y * 3 + sample_coord.z;
    uint sample_bit = 1 << sample_offset;

    if (gl_ViewportIndex == 0)
    {
        imageAtomicOr(scene_indirect_visibility_x_buffer, cell_coord, sample_bit);
    }

    else if (gl_ViewportIndex == 1)
    {
        imageAtomicOr(scene_indirect_visibility_y_buffer, cell_coord, sample_bit);
    }

    else
    {
        imageAtomicOr(scene_indirect_visibility_z_buffer, cell_coord, sample_bit);
    }
}
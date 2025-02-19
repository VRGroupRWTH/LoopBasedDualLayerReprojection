#version 430
#extension GL_NV_gpu_shader5 : require

#include "shared_defines.glsl"
#include "shared_math_library.glsl"
#include "shared_indirect_library.glsl"
#include "shared_light_library.glsl"

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec3 in_tangent;
layout(location = 3) in vec2 in_texture_coord;

uniform uint scene_object_id;
uniform vec3 scene_volume_position;

void main()
{
    vec3 lighting = compute_lighting(scene_volume_position, in_position, in_normal, in_tangent, in_texture_coord, false);

    out_color = vec4(lighting, scene_exposure * scene_object_id); // use alpha so that variables dont get optimized out
}
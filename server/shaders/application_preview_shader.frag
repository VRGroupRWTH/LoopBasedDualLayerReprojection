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

#define APPLICATION_PREVIEW_LIGHTING  1
#define APPLICATION_PREVIEW_OBJECT_ID 0

// Color Palett from: https://loading.io/color/feature/Spectral-10/
vec3 color_list[10] = vec3[10]
(
    vec3(0.62, 0.00, 0.26),
    vec3(0.84, 0.24, 0.31),
    vec3(0.96, 0.43, 0.26),
    vec3(0.99, 0.68, 0.38),
    vec3(1.00, 0.88, 0.55),
    vec3(0.90, 0.96, 0.60),
    vec3(0.67, 0.87, 0.64),
    vec3(0.40, 0.76, 0.65),
    vec3(0.20, 0.53, 0.74),
    vec3(0.37, 0.31, 0.64)
);

uniform uint scene_object_id;
uniform vec3 camera_position;

void main()
{
    vec3 lighting = compute_lighting(camera_position, in_position, in_normal, in_tangent, in_texture_coord, true);

#if APPLICATION_PREVIEW_LIGHTING
    out_color = vec4(lighting, scene_object_id); //Put the object_id in the alpha channel so that the uniform is not optimzed out
#elif APPLICATION_PREVIEW_OBJECT_ID
    out_color = vec4(color_list[scene_object_id % 10], lighting.x); //Put the lighting in the alpha channel so that other uniforms are not optimzed out
#else
    out_color = vec4(1.0);
#endif
}
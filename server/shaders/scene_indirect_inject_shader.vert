#version 430
#extension GL_NV_gpu_shader5 : require

#include "shared_defines.glsl"
#include "shared_math_library.glsl"

layout(binding = 0) uniform sampler2D scene_light_depth_buffer;
layout(binding = 1) uniform sampler2D scene_light_normal_buffer;
layout(binding = 2) uniform sampler2D scene_light_flux_buffer;

layout(location = 0) out vec3 out_normal;
layout(location = 1) out vec3 out_flux;

uniform mat4 scene_light_matrix_inv;
uniform vec3 scene_indirect_cell_size;

void main()
{
    ivec2 pixel_coord = ivec2(gl_VertexID / SCENE_LIGHT_BUFFER_RESOLUTION, gl_VertexID % SCENE_LIGHT_BUFFER_RESOLUTION);

    out_normal = decode_normal(texelFetch(scene_light_normal_buffer, pixel_coord, 0).xy);
    out_flux = texelFetch(scene_light_flux_buffer, pixel_coord, 0).xyz;

    vec3 image_coord = vec3(0.0);
    image_coord.xy = vec2(pixel_coord + 0.5) / vec2(SCENE_LIGHT_BUFFER_RESOLUTION);
    image_coord.z = texelFetch(scene_light_depth_buffer, pixel_coord, 0).x;
    image_coord = image_coord * 2.0 - 1.0;

    vec4 position = scene_light_matrix_inv * vec4(image_coord, 1.0);
    position /= position.w;
    position.xyz += 0.5 * scene_indirect_cell_size * out_normal; //NOTE: Shift the virtual point light along the normal in order to avoid self lighting and shadowing

    gl_Position = position;
}
#version 430
#extension GL_NV_gpu_shader5 : require

#include "shared_defines.glsl"
#include "shared_math_library.glsl"
#include "shared_indirect_library.glsl"
#include "shared_light_library.glsl"

layout(location = 0) out vec2 out_normal;
layout(location = 1) out uint out_object_id;
layout(location = 2) out vec4 out_color;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec3 in_tangent;
layout(location = 3) in vec2 in_texture_coord;
layout(location = 4) in vec3 in_view_normal;

layout(binding = 0) uniform sampler2D depth_buffer;
layout(binding = 1) uniform usampler2D object_id_buffer;

uniform uint scene_object_id;

uniform mat4 camera_projection_matrix;
uniform vec3 camera_position;

uniform float layer_depth_base_threshold;
uniform float layer_depth_slope_threshold;
uniform bool layer_use_object_ids;
uniform uint layer;

void main()
{
    if(layer > 0)
    {
        uint previous_object_id = texelFetch(object_id_buffer, ivec2(gl_FragCoord.xy), 0).x;
        float previous_depth = texelFetch(depth_buffer, ivec2(gl_FragCoord.xy), 0).x;
        float previous_linear_depth = linear_depth(camera_projection_matrix, previous_depth);

        uint current_object_id = scene_object_id;
        float current_depth = gl_FragCoord.z;
        float current_linear_depth = linear_depth(camera_projection_matrix, current_depth);

        if(layer_use_object_ids)
        {
            if(previous_object_id == current_object_id)
            {
                discard;
            }
        }

        else
        {
            if(previous_linear_depth > (current_linear_depth - layer_depth_base_threshold - current_linear_depth * layer_depth_slope_threshold))
            {
                discard;
            }
        }
    }

    vec3 lighting = compute_lighting(camera_position, in_position, in_normal, in_tangent, in_texture_coord);

    out_normal = encode_normal(in_view_normal);
    out_color = vec4(lighting, 1.0);
    out_object_id = scene_object_id;
}
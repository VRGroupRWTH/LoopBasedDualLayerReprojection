#version 430

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 3) in vec2 in_texture_coord;

layout(location = 0) out vec3 out_position;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec2 out_texture_coord;

uniform mat4 scene_light_matrix;

void main()
{
    out_position = in_position;
    out_normal = in_normal;
    out_texture_coord = in_texture_coord;

    gl_Position = scene_light_matrix * vec4(in_position, 1.0);
}
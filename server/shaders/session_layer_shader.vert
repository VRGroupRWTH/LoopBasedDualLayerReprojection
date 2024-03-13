#version 430

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec3 in_tangent;
layout(location = 3) in vec2 in_texture_coord;

layout(location = 0) out vec3 out_position;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec3 out_tangent;
layout(location = 3) out vec2 out_texture_coord;
layout(location = 4) out vec3 out_view_position;

uniform mat4 camera_view_projection_matrix;
uniform mat4 camera_view_matrix;

void main()
{
    out_position = in_position;
    out_normal = in_normal;
    out_tangent = in_tangent;
    out_texture_coord = in_texture_coord;
    out_view_position = vec3(camera_view_matrix * vec4(in_position, 1.0));

    gl_Position = camera_view_projection_matrix * vec4(in_position, 1.0);
}
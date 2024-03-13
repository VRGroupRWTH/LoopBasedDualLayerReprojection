#version 430

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) out vec3 out_position;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec3 out_tangent;
layout(location = 3) out vec2 out_texture_coord;
layout(location = 4) out flat vec3 out_view_normal;

layout(location = 0) in vec3 in_position[];
layout(location = 1) in vec3 in_normal[];
layout(location = 2) in vec3 in_tangent[];
layout(location = 3) in vec2 in_texture_coord[];
layout(location = 4) in vec3 in_view_position[];

void main()
{
    vec3 position1 = in_view_position[0];
    vec3 position2 = in_view_position[1];
    vec3 position3 = in_view_position[2];

    vec3 normal = cross(position2 - position1, position3 - position1);
    float nornal_length = length(normal);

    if(nornal_length > 0.0)
    {
        normal /= nornal_length;
    }

    out_position = in_position[0];
    out_normal = in_normal[0];
    out_tangent = in_tangent[0];
    out_texture_coord = in_texture_coord[0];
    out_view_normal = normal;
    gl_Position = gl_in[0].gl_Position;
    EmitVertex();

    out_position = in_position[1];
    out_normal = in_normal[1];
    out_tangent = in_tangent[1];
    out_texture_coord = in_texture_coord[1];
    out_view_normal = normal;
    gl_Position = gl_in[1].gl_Position;
    EmitVertex();

    out_position = in_position[2];
    out_normal = in_normal[2];
    out_tangent = in_tangent[2];
    out_texture_coord = in_texture_coord[2];
    out_view_normal = normal;
    gl_Position = gl_in[2].gl_Position;
    EmitVertex();

    EndPrimitive();
}
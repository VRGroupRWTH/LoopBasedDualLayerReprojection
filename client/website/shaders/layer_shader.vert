#version 300 es 
precision highp float;

layout(location = 0) in uvec2 in_position;
layout(location = 1) in float in_depth;

out vec2 coord;

uniform uvec2 geometry_resolution;
uniform mat4 layer_matrix;

void main()
{
    coord = vec2(in_position) / vec2(geometry_resolution);

    gl_Position = layer_matrix * vec4(vec3(coord, in_depth) * 2.0 - 1.0, 1.0);
} 
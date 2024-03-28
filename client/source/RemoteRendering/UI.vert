#version 300 es

in vec2 in_xy;
out vec2 texcoord;

uniform mat4 transform;

void main()
{
    gl_Position = transform * vec4(in_xy, 0.0f, 1.0);
    texcoord = (in_xy * vec2(1.0, -1.0)) + 0.5;
}

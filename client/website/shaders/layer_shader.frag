#version 300 es 
precision mediump float;

out vec4 out_color;

in vec2 coord;

uniform sampler2D image_frame;

void main()
{
    out_color = texture(image_frame, coord);
} 
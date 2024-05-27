#version 300 es 
precision mediump float;
precision mediump int;

out vec4 out_color;
in vec2 coord;

uniform sampler2D image_frame;
uniform uvec2 view_image_offset;
uniform uvec2 view_image_size;

void main()
{
    uvec2 image_size = uvec2(textureSize(image_frame, 0).xy);
    vec2 image_coord = (coord * vec2(view_image_size) + vec2(view_image_offset)) / vec2(image_size);

    out_color = texture(image_frame, image_coord);
} 
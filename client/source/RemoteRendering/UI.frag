#version 300 es

precision mediump float;

in vec2 texcoord;

uniform vec4 icon_color;
uniform sampler2D icon_texture;

out vec4 color;

void main()
{
    vec4 texture_color = texture(icon_texture, texcoord);
    color = vec4(mix(texture_color.rgb, icon_color.rgb, icon_color.a), texture_color.a);
}

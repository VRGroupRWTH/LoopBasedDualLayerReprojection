#version 300 es

in vec2 in_xy;
in float in_z;

out vec2 texcoord_y;
out vec2 texcoord_uv;
out vec4 pos;

uniform vec2 resolution;
uniform vec2 y_texture_size;
uniform vec2 uv_texture_size;
uniform vec2 offset;
uniform mat4 matrix;

vec2 texel_position(vec2 pos, vec2 resolution) {
    return pos * (resolution - 1.0) / resolution;
}

vec2 tex_coord(vec2 pos, vec2 texel_offset, vec2 resolution, vec2 texture_size) {
    vec2 p = texel_position(pos, resolution) + texel_offset + 0.5;
    return p / texture_size;
}

void main()
{
    gl_Position = matrix * vec4(in_xy, in_z, 1.0);
    texcoord_y = tex_coord(in_xy, offset, resolution, y_texture_size);

    vec2 uv_factor = uv_texture_size / y_texture_size;
    texcoord_uv = tex_coord(floor(in_xy * uv_factor), offset * uv_factor, resolution * uv_factor, uv_texture_size);
    // z = (exp(pow(in_z, 1.0)) - 1.0) / (exp(1.0) - 1.0);
    //z = 1.0;
    // pos = gl_Position;
    pos = vec4(0.0, 0.0, in_z, 1.0);
}

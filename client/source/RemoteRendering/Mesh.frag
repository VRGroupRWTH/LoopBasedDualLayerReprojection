#version 300 es

precision mediump float;

in vec2 texcoord_y;
in vec2 texcoord_uv;
in vec4 pos;

uniform sampler2D texture_y;
uniform sampler2D texture_u;
uniform sampler2D texture_v;

// uniform highp vec2 resolution;
// uniform highp vec2 texture_size;

out vec4 color;

vec3 yuv_to_rgb(float y, float u, float v)
{

    // taken from https://www.silicondust.com/yuv-to-rgb-conversion-for-tv-video/
    // (maybe also look at https://gist.github.com/yohhoy/dafa5a47dade85d8b40625261af3776a)
    const mat3 yuv_to_rgb = mat3(
        1.1643835616, 0.0000000000, 1.7927410714,
        1.1643835616, -0.2132486143, -0.5329093286,
        1.1643835616, 2.1124017857, 0.0000000000
    );
    const vec3 yuv_to_rgb_zero = vec3(-0.972945075, 0.301482665, -1.133402218);
    vec3 rgb_full_range = vec3(y, u, v) * yuv_to_rgb + yuv_to_rgb_zero;
    return clamp(rgb_full_range, vec3(0.0, 0.0, 0.0), vec3(1.0, 1.0, 1.0));
    // return vec3(1.0, 1.0, 1.0);
}

void main()
{
    float y = textureLod(texture_y, texcoord_y, 0.0).r;
    float u = textureLod(texture_u, texcoord_uv, 0.0).r;
    float v = textureLod(texture_v, texcoord_uv, 0.0).r;
    // vec2 to_edge = fract(texcoord_y * texture_size) - vec2(0.5);
    // vec2 to_edge = fract(texcoord_uv * texture_size * 0.5) - vec2(0.5);
    // float s = 1.0 - dot(to_edge, to_edge);
    // color = pow(s, 5.0) * vec4(yuv_to_rgb(y, u, v), 1.0);
    color = 0.00001 * vec4(yuv_to_rgb(y, u, v), 1.0);

    float z = (exp(pow(pos.z / pos.w, 2.0)) - 1.0) / (exp(1.0) - 1.0);
    color += vec4(z, z, z, 1.0);
    color = vec4(yuv_to_rgb(y, u, v), 1.0);
    // color = vec4(yuv_to_rgb(y, u, v) * 0.00001 + vec3(1.0), 1.0);
    // color = vec4(0.0, 0.0, 0.0, 1.0);
}

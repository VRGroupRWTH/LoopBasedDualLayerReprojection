#version 430

#include "shared_math_library.glsl"
#include "shared_indirect_library.glsl"

layout(location = 0) out vec4 out_red_distribution;
layout(location = 1) out vec4 out_green_distribution;
layout(location = 2) out vec4 out_blue_distribution;

layout(location = 0) in vec3 in_normal;
layout(location = 1) in vec3 in_flux;

void main()
{
    vec3 normal = normalize(in_normal);
    vec4 distribution = spherical_harmonic_cosine_lobe(normal);

    //NOTE: Where in_flux is in watt
    out_red_distribution = in_flux.x * distribution;   //NOTE: Additive blending
    out_green_distribution = in_flux.y * distribution; //NOTE: Additive blending
    out_blue_distribution = in_flux.z * distribution;  //NOTE: Additive blending
}
#version 430
#extension GL_NV_gpu_shader5 : require

#include "shared_defines.glsl"
#include "shared_math_library.glsl"

layout(location = 0) out vec2 out_normal;
layout(location = 1) out vec4 out_flux;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_texture_coord;

layout(binding = SCENE_MATERIAL_BASE_COLOR_TEXTURE_BINDING_POINT) uniform sampler2D scene_material_base_color_texture;

uniform vec3 scene_material_base_color;
uniform float scene_material_opacity;

uniform uint scene_light_type;
uniform vec3 scene_light_position;
uniform vec3 scene_light_direction;
uniform vec3 scene_light_color;
uniform vec2 scene_light_image_plane_size;
uniform float scene_light_image_plane_near;
uniform float scene_light_inner_angle;
uniform float scene_light_outer_angle;

vec3 compute_pixel_flux()
{
    if(scene_light_type == SCENE_LIGHT_TYPE_DIRECTIONAL)
    {
        vec2 pixel_size = scene_light_image_plane_size / vec2(SCENE_LIGHT_BUFFER_RESOLUTION);
        float pixel_area = pixel_size.x * pixel_size.y; // where pixel_size is in meters

        return scene_light_color * pixel_area; // where scene_light_color is in watt/meter^2
    }

    else if(scene_light_type == SCENE_LIGHT_TYPE_SPOT)
    {
        vec2 pixel_size = scene_light_image_plane_size / vec2(SCENE_LIGHT_BUFFER_RESOLUTION);
        float pixel_area = pixel_size.x * pixel_size.y; // where pixel_size is in meters

        vec3 light_direction = normalize(in_position - scene_light_position);
        float light_attenuation = (4.0 * PI) * (scene_light_image_plane_near * scene_light_image_plane_near); // where scene_light_image_plane_near is in meters
        float cone_attenuation = 1.0 - smoothstep(scene_light_inner_angle, scene_light_outer_angle, acos(dot(light_direction, scene_light_direction))); // assume that scene_light_direction is normalized

        vec3 light_radiance = (scene_light_color * cone_attenuation) / light_attenuation; // where scene_light_color is in watt and light_radiance is in watt / (meter^2 * str)

        // Assume that every point within the area of the pixel receives only light from the one direction.
        // Besides that assume that every point reveives an the same amount of light.
        // In this case the light distribution at a point is a delta distribution which turns radiance into irradiance.
        // By multipling the irradiance by the area of the pixel, the total flux that the pixel receives can be calculated.
        return light_radiance * pixel_area;
    }

    else if(scene_light_type == SCENE_LIGHT_TYPE_POINT)
    {
        vec2 pixel_size = scene_light_image_plane_size / vec2(SCENE_LIGHT_BUFFER_RESOLUTION);
        float pixel_area = pixel_size.x * pixel_size.y; // where pixel_size is in meters

        float light_attenuation = (4.0 * PI) * (scene_light_image_plane_near * scene_light_image_plane_near); // where scene_light_image_plane_near is in meters
        vec3 light_radiance = scene_light_color / light_attenuation; // where scene_light_color is in watt and light_radiance is in watt / (meter^2 * str)

        // Assume that every point within the area of the pixel receives only light from the one direction.
        // Besides that assume that every point reveives an the same amount of light.
        // In this case the light distribution at a point is a delta distribution which turns radiance into irradiance.
        // By multipling the irradiance by the area of the pixel, the total flux that the pixel receives can be calculated.
        return light_radiance * pixel_area;
    }

    return vec3(0.0);
}

void main()
{
    vec4 base_color_opacity = texture(scene_material_base_color_texture, in_texture_coord);
    base_color_opacity *= vec4(scene_material_base_color, scene_material_opacity);

    if(base_color_opacity.w < EPSILON)
    {
        discard;
    }

    vec3 pixel_flux = compute_pixel_flux();

    out_normal = encode_normal(in_normal);
    out_flux = vec4(pixel_flux * base_color_opacity.xyz, 1.0); // where out_flux is in watt
}
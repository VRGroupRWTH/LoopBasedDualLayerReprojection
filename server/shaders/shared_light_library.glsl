#ifndef SHADER_SHARED_LIGHT_LIBRARY
#define SHADER_SHARED_LIGHT_LIBRARY

layout(binding = SCENE_MATERIAL_BASE_COLOR_TEXTURE_BINDING_POINT) uniform sampler2D scene_material_base_color_texture;
layout(binding = SCENE_MATERIAL_MATERIAL_TEXTURE_BINDING_POINT) uniform sampler2D scene_material_material_texture;
layout(binding = SCENE_MATERIAL_NORMAL_TEXTURE_BINDING_POINT) uniform sampler2D scene_material_normal_texture;
layout(binding = SCENE_MATERIAL_EMISSIVE_TEXTURE_BINDING_POINT) uniform sampler2D scene_material_emissive_texture;

layout(binding = SCENE_LIGHT_DEPTH_ARRAY_BUFFER_BINDING_POINT) uniform sampler2DArrayShadow scene_light_depth_array_buffer;
layout(binding = SCENE_LIGHT_DEPTH_CUBE_ARRAY_BUFFER_BINDING_POINT) uniform samplerCubeArrayShadow scene_light_depth_cube_array_buffer;

layout(binding = SCENE_INDIRECT_RED_DISTRIBUTION_BUFFER_BINDING_POINT) uniform sampler3D scene_indirect_red_distribution_buffer;
layout(binding = SCENE_INDIRECT_GREEN_DISTRIBUTION_BUFFER_BINDING_POINT) uniform sampler3D scene_indirect_green_distribution_buffer;
layout(binding = SCENE_INDIRECT_BLUE_DISTRIBUTION_BUFFER_BINDING_POINT) uniform sampler3D scene_indirect_blue_distribution_buffer;
layout(binding = SCENE_INDIRECT_OPACITY_BUFFER_BINDING_POINT) uniform sampler3D scene_indirect_opacity_buffer;

layout(binding = SCENE_LIGHT_BUFFER_BINDING_POINT, std430) readonly buffer LightBuffer
{
    Light lights[];
};

uniform vec3 scene_material_base_color;
uniform vec3 scene_material_emissive_color;
uniform float scene_material_opacity;
uniform float scene_material_roughness;
uniform float scene_material_metallic;

uniform uint scene_light_count;

uniform float scene_indirect_intensity;
uniform vec3 scene_indirect_cell_size;
uniform vec3 scene_indirect_domain_min;
uniform vec3 scene_indirect_domain_max;

uniform vec3 scene_ambient_color;
uniform float scene_exposure;

#define LIGHT_SHADOW_BIAS_DIRECTIONAL 0.008
#define LIGHT_SHADOW_BIAS_SPOT        0.00008
#define LIGHT_SHADOW_BIAS_POINT       0.00008

//-- Material Functions ---------------------------------------------------------------------------

struct Material
{
    vec3 base_color;
    float opacity;

    float occlusion;
    float roughness;
    float metallic;

    vec3 emissive;
};

Material load_material(vec2 surface_texture_coord)
{
    vec4 base_color = texture(scene_material_base_color_texture, surface_texture_coord);
    vec3 material_factor = texture(scene_material_material_texture, surface_texture_coord).xyz;
    vec3 emissive = texture(scene_material_emissive_texture, surface_texture_coord).xyz;

    Material material;
    material.base_color = base_color.xyz * scene_material_base_color;
    material.opacity = base_color.w * scene_material_opacity;
    material.occlusion = material_factor.x;
    material.roughness = material_factor.y * scene_material_roughness;
    material.metallic = material_factor.z * scene_material_metallic;
    material.emissive = emissive * scene_material_emissive_color;

    return material;
}

vec3 load_normal(vec2 surface_texture_coord, vec3 surface_normal, vec3 surface_tangent)
{
    vec3 normal = normalize(surface_normal);
    vec3 tangent = normalize(surface_tangent);
    vec3 bitangent = cross(normal, tangent);
    
    vec3 image_normal = texture(scene_material_normal_texture, surface_texture_coord).xyz * 2.0 - 1.0;
    image_normal.z = 1.0 - length(image_normal.xy);

	vec3 shading_normal = normalize(mat3(tangent, bitangent, normal) * image_normal);

	if(!gl_FrontFacing)
	{
		shading_normal = -shading_normal;
	}

    return shading_normal;
}

//-- Shadow Functions ------------------------------------------------------------------------------

float get_shadow(uint light, vec3 surface_position)
{
    uint light_type = lights[light].type;

    if(light_type != SCENE_LIGHT_TYPE_POINT)
    {
        float light_bias = 0.0;

        if(light_type == SCENE_LIGHT_TYPE_DIRECTIONAL)
        {
            light_bias = LIGHT_SHADOW_BIAS_DIRECTIONAL;
        }

        else if(light_type == SCENE_LIGHT_TYPE_SPOT)
        {
            light_bias = LIGHT_SHADOW_BIAS_SPOT;
        }

        vec4 shadow_position = lights[light].light_matrix * vec4(surface_position, 1.0);
        shadow_position /= shadow_position.w;
        shadow_position.xyz = (shadow_position.xyz + 1.0) / 2.0;

        return texture(scene_light_depth_array_buffer, vec4(shadow_position.xy, lights[light].light_layer_index, shadow_position.z - light_bias)).x;
    }

    else
    {
        vec3 light_direction = surface_position - lights[light].position;
		vec3 light_direction_abs = abs(normalize(light_direction));
		float light_direction_primary = max(light_direction_abs.x, max(light_direction_abs.y, light_direction_abs.z));

		float shadow_distance_linear = 0.0;

		if(light_direction_abs.x == light_direction_primary)
		{
			shadow_distance_linear = -abs(light_direction.x);
		}

		else if(light_direction_abs.y == light_direction_primary)
		{
			shadow_distance_linear = -abs(light_direction.y);
		}

		else
		{
			shadow_distance_linear = -abs(light_direction.z);
		}

        float shadow_distance = unorm_depth(lights[light].light_projection_matrix, shadow_distance_linear);

        return texture(scene_light_depth_cube_array_buffer, vec4(light_direction, lights[light].light_layer_index), shadow_distance - LIGHT_SHADOW_BIAS_POINT).x;
    }

    return 0.0;
}

//-- Light Functions -------------------------------------------------------------------------------

//This function computes the incomming radiance for a light-source.
//The final unit can be seen as incomming radiance (watts/(meter^2 * str)), eventough the final unit is in watts/meter^2.
//This is due to the fact, that the light source only contributes like a dirca impulse during the integration of the BRDF.
vec3 get_light_radiance(uint light, vec3 surface_position)
{
    uint light_type = lights[light].type;

    if(light_type == SCENE_LIGHT_TYPE_DIRECTIONAL)
    {
        vec3 light_color = lights[light].color; //where light_color is in watts

        return light_color;
    }

    else if(light_type == SCENE_LIGHT_TYPE_SPOT)
    {
        vec3 light_direction = (lights[light].position - surface_position); //where light_direction is in meters
        vec3 light_color = lights[light].color; //where light_color is in watts
        float light_attenuation = (4.0 * PI) * dot(light_direction, light_direction);
        float cone_attenuation = 1.0 - smoothstep(lights[light].inner_angle, lights[light].outer_angle, acos(dot(-normalize(light_direction), lights[light].direction))); //assume that direction is normalized

        return (light_color * cone_attenuation) / light_attenuation;
    }

    else if(light_type == SCENE_LIGHT_TYPE_POINT)
    {
        vec3 light_direction = (lights[light].position - surface_position); //where light_direction is in meters
        vec3 light_color = lights[light].color; //where light_color is in watts
        float light_attenuation = (4.0 * PI) * dot(light_direction, light_direction);

        return light_color / light_attenuation;
    }

    return vec3(0.0);
}

vec3 get_light_direction(uint light, vec3 surface_position)
{
    uint light_type = lights[light].type;

    if(light_type == SCENE_LIGHT_TYPE_DIRECTIONAL)
    {
        return -lights[light].direction; //assume that direction is normalized
    }

    else if(light_type == SCENE_LIGHT_TYPE_SPOT)
    {
        return normalize(lights[light].position - surface_position);
    }

    else if(light_type == SCENE_LIGHT_TYPE_POINT)
    {
        return normalize(lights[light].position - surface_position);
    }

    return vec3(0.0);
}

//-- BRDF Functions -------------------------------------------------------------------------------
//The following BRDF function is based on the GLTF-Specification.
//https://github.com/KhronosGroup/glTF/tree/main/specification/2.0

#define DIELECTRIC_F_ZERO 0.04 //index of refraction of 1.5

float compute_facet_distribution(float alpha, float n_dot_h)
{
    float alpha_square = alpha * alpha;
    float factor = n_dot_h * n_dot_h * (alpha_square - 1.0) + 1.0;

    return (alpha_square * heaviside(n_dot_h)) / (PI * factor * factor);
}

float compute_visibility(float alpha, float h_dot_l, float h_dot_v, float n_dot_l, float n_dot_v)
{
    float alpha_square = alpha * alpha;
    float factor1 = abs(n_dot_l) + sqrt(alpha_square + (1.0 - alpha_square) * n_dot_l * n_dot_l);
    float factor2 = abs(n_dot_v) + sqrt(alpha_square + (1.0 - alpha_square) * n_dot_v * n_dot_v);

    return (heaviside(h_dot_l) * heaviside(h_dot_v)) / (factor1 * factor2);
}

vec3 compute_fresnel(vec3 f_zero, float h_dot_v)
{
    return f_zero + (vec3(1.0) - f_zero) * pow(1.0 - abs(h_dot_v), 5.0);
}

float compute_brdf_specular(float alpha, float h_dot_l, float h_dot_v, float n_dot_l, float n_dot_v, float n_dot_h)
{
    float visibility = compute_visibility(alpha, h_dot_l, h_dot_v, n_dot_l, n_dot_v);
    float distribution = compute_facet_distribution(alpha, n_dot_h);

    return visibility * distribution;
}

vec3 compute_brdf_diffuse(vec3 diffuse_color)
{
    return (1.0 / PI) * diffuse_color;
}

vec3 compute_brdf(vec3 light_direction, vec3 normal_direction, vec3 view_direction, Material material)
{
    float n_dot_l = dot(normal_direction, light_direction);

    if (n_dot_l <= 0.0)
    {
        return vec3(0.0);
    }

    vec3 half_direction = normalize(light_direction + view_direction);

    float h_dot_l = dot(half_direction, light_direction);
    float h_dot_v = dot(half_direction, view_direction);
    float n_dot_v = dot(normal_direction, view_direction);
    float n_dot_h = dot(normal_direction, half_direction);

    vec3 diffuse_color = mix(material.base_color, vec3(0.0), material.metallic);
    vec3 f_zero = mix(vec3(DIELECTRIC_F_ZERO), material.base_color, material.metallic);

    float alpha = material.roughness * material.roughness;
    vec3 fersnel = compute_fresnel(f_zero, h_dot_v);

    vec3 diffuse = (1.0 - fersnel) * compute_brdf_diffuse(diffuse_color);
    vec3 specular = fersnel * compute_brdf_specular(alpha, h_dot_l, h_dot_v, n_dot_l, n_dot_v, n_dot_h);

    return diffuse + specular;
}


//-- Indirect Functions ----------------------------------------------------------------------------

vec3 get_indirect_diffuse_radiance(vec3 surface_position, vec3 normal_direction)
{
    vec3 cell_coord = (surface_position - scene_indirect_domain_min) / (scene_indirect_domain_max - scene_indirect_domain_min);

    vec4 red_distribution = texture(scene_indirect_red_distribution_buffer, cell_coord);
    vec4 green_distribution = texture(scene_indirect_green_distribution_buffer, cell_coord);
    vec4 blue_distribution = texture(scene_indirect_blue_distribution_buffer, cell_coord);

    //Where center_distance_square is in meter^2
    const float center_distance_square = pow(length(scene_indirect_cell_size) / 4.0, 2); //Assume that the average distance to a cell center is 25% of the cell diagonal.

    vec3 indirect_radiance = vec3(0.0); //Where indirect light is in watts / (meter^2 * str)
    indirect_radiance.x = max(0.0, spherical_harmonic_evaluate(-normal_direction, red_distribution)) / center_distance_square;
    indirect_radiance.y = max(0.0, spherical_harmonic_evaluate(-normal_direction, green_distribution)) / center_distance_square;
    indirect_radiance.z = max(0.0, spherical_harmonic_evaluate(-normal_direction, blue_distribution)) / center_distance_square;

    return indirect_radiance;
}

vec3 get_indirect_glossy_radiance(vec3 surface_position, vec3 reflect_direction)
{
    const float sample_offset = length(scene_indirect_cell_size) * 2; //Offset ray to prevent self illumination
    const float sample_step = 0.2;
    const float sample_length = 4.0;

    vec3 indirect_radiance = vec3(0.0); //Where indirect light is in watts / (meter^2 * str)
    float optical_depth = 0.0;

    for(float sample_distance = 0.0; sample_distance < sample_offset; sample_distance += sample_step)
    {
        vec3 sample_position = surface_position + sample_distance * reflect_direction;
        vec3 cell_coord = (sample_position - scene_indirect_domain_min) / (scene_indirect_domain_max - scene_indirect_domain_min);

        vec3 opacity_distribution = texture(scene_indirect_opacity_buffer, cell_coord).xyz;
        float opacity = dot(opacity_distribution, abs(reflect_direction));

        optical_depth += opacity * sample_step;
    }

    for(float sample_distance = sample_offset; sample_distance < sample_length; sample_distance += sample_step)
    {
        vec3 sample_position = surface_position + sample_distance * reflect_direction;
        vec3 cell_coord = (sample_position - scene_indirect_domain_min) / (scene_indirect_domain_max - scene_indirect_domain_min);

        vec4 red_distribution = texture(scene_indirect_red_distribution_buffer, cell_coord);
        vec4 green_distribution = texture(scene_indirect_green_distribution_buffer, cell_coord);
        vec4 blue_distribution = texture(scene_indirect_blue_distribution_buffer, cell_coord);
        vec3 opacity_distribution = texture(scene_indirect_opacity_buffer, cell_coord).xyz;
        float opacity = dot(opacity_distribution, abs(reflect_direction));

        //Where center_distance_square is in meter^2
        float distance_square = pow(sample_distance, 2);
        float transmittance = exp(-optical_depth);

        indirect_radiance.x += transmittance * max(0.0, spherical_harmonic_evaluate(-reflect_direction, red_distribution)) / distance_square;
        indirect_radiance.y += transmittance * max(0.0, spherical_harmonic_evaluate(-reflect_direction, green_distribution)) / distance_square;
        indirect_radiance.z += transmittance * max(0.0, spherical_harmonic_evaluate(-reflect_direction, blue_distribution)) / distance_square;

        optical_depth += opacity * sample_step;
    }

    return indirect_radiance;
}

//-- Tone-Mapping Functions ------------------------------------------------------------------------
// Exposure based tone mapping
//https://learnopengl.com/Advanced-Lighting/HDR

vec3 compute_tone_mapping(vec3 lighting, float exposure)
{
    return vec3(1.0) - exp(-lighting * exposure);
}

//-- Public Function -------------------------------------------------------------------------------

vec3 compute_lighting(vec3 view_position, vec3 surface_position, vec3 surface_normal, vec3 surface_tangent, vec2 surface_texture_coord)
{
    Material material = load_material(surface_texture_coord);

    if(material.opacity <= 0.5)
    {
        discard;
    }

    vec3 lighting = vec3(0.0); 

    if(dot(material.emissive, vec3(1.0)) < EPSILON) //apply direct lighting only if the material is not emissive
    {
        vec3 normal_direction = load_normal(surface_texture_coord, surface_normal, surface_tangent);
        vec3 view_direction = normalize(view_position - surface_position);
        vec3 reflect_direction = normalize(reflect(-view_direction, normal_direction));

        lighting += material.base_color * scene_ambient_color; //where scene_ambient_color is in watts/meter^2 *str

        for(uint light = 0; light < scene_light_count; light++)
        {
            vec3 light_direction = get_light_direction(light, surface_position);

            lighting += compute_brdf(light_direction, normal_direction, view_direction, material) * get_light_radiance(light, surface_position) * get_shadow(light, surface_position) * dot(normal_direction, light_direction);
        }

        //NOTE: The weights for the indirect illumination are not physically based
        float indirect_diffuse_strength = 1.0;
        float indirect_glossy_strength = 1.0 - smoothstep(0.0, 0.8, material.roughness);

        material.roughness = 1.0; //NOTE: Adjust roughness, since light propagation volumes can only deliver an approximation for the indirect lighting
        lighting += scene_indirect_intensity * compute_brdf(normal_direction, normal_direction, view_direction, material) * get_indirect_diffuse_radiance(surface_position, normal_direction) * indirect_diffuse_strength * dot(normal_direction, normal_direction);
        material.roughness = 0.5; //NOTE: Adjust roughness, since light propagation volumes can only deliver an approximation for the indirect lighting
        lighting += scene_indirect_intensity * compute_brdf(reflect_direction, normal_direction, view_direction, material) * get_indirect_glossy_radiance(surface_position, reflect_direction) * indirect_glossy_strength * dot(normal_direction, reflect_direction);

        lighting *= (1.0 - material.occlusion);
    }

    else
    {
        lighting += material.emissive;
    }

    return compute_tone_mapping(lighting, scene_exposure);
}

#endif
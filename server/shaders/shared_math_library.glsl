#ifndef SHADER_SHARED_MATH_LIBRARY
#define SHADER_SHARED_MATH_LIBRARY

#define PI 3.14159265358
#define EPSILON 0.00001

bool is_outside(ivec2 coord, ivec2 image_size)
{
    return any(lessThan(coord, ivec2(0))) || any(greaterThanEqual(coord, image_size));
}

bool is_outside_volume(ivec3 coord, ivec3 image_size)
{
    return any(lessThan(coord, ivec3(0))) || any(greaterThanEqual(coord, image_size));
}

bool is_outside_cell(ivec2 coord, ivec2 cell_min, ivec2 cell_max)
{
    return any(lessThan(coord, cell_min)) || any(greaterThanEqual(coord, cell_max));
}

float heaviside(float value)
{
    if (value <= 0.0)
    {
        return 0.0;
    }

    return 1.0;
}

vec2 quadrant(vec2 vector)
{
    vec2 result;
    result.x = (vector.x >= 0.0) ? 1.0 : -1.0;
    result.y = (vector.y >= 0.0) ? 1.0 : -1.0;

    return result;
}

// Encoding and Decoding of normals presented in the paiper
// "On floating-point normal vectors". This method is also known
//  as oct32 and is compared with other techniques in the paiper
// "A Survey of Efficient Representationsfor Independent Unit Vectors".
vec2 encode_normal(vec3 normal)
{
    vec2 projection = normal.xy * (1.0 / (abs(normal.x) + abs(normal.y) + abs(normal.z)));
    vec2 coord = (normal.z <= 0.0) ? ((1.0 - abs(projection.yx)) * quadrant(projection)) : projection;
    
    return (coord + 1.0) / 2.0;
}

vec3 decode_normal(vec2 vector)
{
    vec2 coord = vector * 2.0 - 1.0;
    vec3 normal = vec3(coord.xy, 1.0 - abs(coord.x) - abs(coord.y));
    normal.xy = (normal.z < 0.0) ? ((1.0 - abs(normal.yx)) * quadrant(normal.xy)) : normal.xy;
    
    return normalize(normal);
}

uint encode_direction(ivec2 direction)
{
    ivec2 rotated_direction = ivec2(0);
    rotated_direction.x = direction.x + direction.y;
    rotated_direction.y = direction.y - direction.x;
    
    uvec2 unit_direction = uvec2(rotated_direction + 1) / 2;
    uint encoded_direction = unit_direction.x | (unit_direction.y << 1);

    return encoded_direction;
}

//Assumes that the encoded direction is stored in the first two bits
ivec2 decode_direction(uint encoded_direction)
{
    uvec2 unit_direction = uvec2(0);
    unit_direction.x = encoded_direction & 0x1;
    unit_direction.y = (encoded_direction >> 1) & 0x01;

    ivec2 rotated_direction = ivec2(unit_direction) * 2 - 1;
    ivec2 direction = rotated_direction + ivec2(-rotated_direction.y, rotated_direction.x);
    direction /= 2;

    return direction;
}

float linear_depth(mat4 projection_matrix, float unorm_depth)
{
    float snorm_depth = unorm_depth * 2.0 - 1.0;

    return projection_matrix[3][2] / (snorm_depth * projection_matrix[2][3] - projection_matrix[2][2]);
}

float unorm_depth(mat4 projection_matrix, float linear_depth)
{
    float snorm_depth = (projection_matrix[3][2] / (linear_depth * projection_matrix[2][3])) + (projection_matrix[2][2] / projection_matrix[2][3]);

    return (snorm_depth + 1.0) / 2.0;
}

#endif
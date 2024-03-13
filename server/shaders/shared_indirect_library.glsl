#ifndef SHADER_SHARED_INDIRECT_LIBRARY
#define SHADER_SHARED_INDIRECT_LIBRARY

//NOTE: Standard factors for real valued spherical harmonics making them orhonormal.
#define SPHERICAL_HARMONIC_BAND0_FACTOR (0.5 * sqrt(1.0 / PI))
#define SPHERICAL_HARMONIC_BAND1_FACTOR sqrt(3.0 / (4.0 * PI))

//NOTE: Integration of the spherical harmoinc base functions using the standard factors
#define SPHERICAL_HARMONIC_BAND0_INTEGRATED (2.0 * sqrt(PI))
#define SPHERICAL_HARMONIC_BAND1_INTEGRATED 0.0

//NOTE: The given factors where computed for the function I(theta, phi) = max(cos(theta), 0) / PI.
//      This function was choosen, since the integral over it is one.
//      Therefore the function describes a distribution.
#define SPHERICAL_HARMONIC_COSINE_LOBE_BAND0_FACTOR (0.5 * sqrt(1.0 / PI))
#define SPHERICAL_HARMONIC_COSINE_LOBE_BAND1_FACTOR sqrt(1.0 / (3.0 * PI))

//NOTE: Assume direction is allready normalize
float spherical_harmonic_evaluate(vec3 direction, vec4 spherical_harmonic)
{
    return dot(spherical_harmonic, vec4(SPHERICAL_HARMONIC_BAND0_FACTOR,
                                        SPHERICAL_HARMONIC_BAND1_FACTOR * direction.y,
                                        SPHERICAL_HARMONIC_BAND1_FACTOR * direction.z,
                                        SPHERICAL_HARMONIC_BAND1_FACTOR * direction.x));
}

//NOTE: Assume direction is allready normalize
//      The compute spherical harmonic represents the function I(theta, phi) = max(cos(theta), 0) / PI,
//      when it is rotated towards "direction".
vec4 spherical_harmonic_cosine_lobe(vec3 direction)
{
    return vec4(SPHERICAL_HARMONIC_COSINE_LOBE_BAND0_FACTOR,
                SPHERICAL_HARMONIC_COSINE_LOBE_BAND1_FACTOR * direction.y,
                SPHERICAL_HARMONIC_COSINE_LOBE_BAND1_FACTOR * direction.z,
                SPHERICAL_HARMONIC_COSINE_LOBE_BAND1_FACTOR * direction.x);
}

//NOTE: Integrate the given spherical haromic
float spherical_harmonic_integrate(vec4 spherical_harmonic)
{
    return dot(spherical_harmonic, vec4(SPHERICAL_HARMONIC_BAND0_INTEGRATED,
                                        SPHERICAL_HARMONIC_BAND1_INTEGRATED,
                                        SPHERICAL_HARMONIC_BAND1_INTEGRATED,
                                        SPHERICAL_HARMONIC_BAND1_INTEGRATED));
}

#endif
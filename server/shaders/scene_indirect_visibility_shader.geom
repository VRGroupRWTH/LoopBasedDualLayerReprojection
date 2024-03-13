#version 430

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) out vec3 out_domain_coord;

uniform vec3 scene_indirect_domain_min;
uniform vec3 scene_indirect_domain_max;

void main()
{
    vec3 position1 = gl_in[0].gl_Position.xyz;
    vec3 position2 = gl_in[1].gl_Position.xyz;
    vec3 position3 = gl_in[2].gl_Position.xyz;

    vec3 domain_coord1 = (position1 - scene_indirect_domain_min) / (scene_indirect_domain_max - scene_indirect_domain_min);
    vec3 domain_coord2 = (position2 - scene_indirect_domain_min) / (scene_indirect_domain_max - scene_indirect_domain_min);
    vec3 domain_coord3 = (position3 - scene_indirect_domain_min) / (scene_indirect_domain_max - scene_indirect_domain_min);

    vec2 screen_coord1 = vec2(0.0);
    vec2 screen_coord2 = vec2(0.0);
    vec2 screen_coord3 = vec2(0.0);

    vec3 normal_abs = abs(cross(position2 - position1, position3 - position1));
    float normal_max = max(normal_abs.x, max(normal_abs.y, normal_abs.z));

    if (normal_abs.x == normal_max)
    {
        screen_coord1 = domain_coord1.yz;
        screen_coord2 = domain_coord2.yz;
        screen_coord3 = domain_coord3.yz;

        gl_ViewportIndex = 0;
    }

    else if (normal_abs.y == normal_max)
    {
        screen_coord1 = domain_coord1.xz;
        screen_coord2 = domain_coord2.xz;
        screen_coord3 = domain_coord3.xz;

        gl_ViewportIndex = 1;
    }

    else
    {
        screen_coord1 = domain_coord1.xy;
        screen_coord2 = domain_coord2.xy;
        screen_coord3 = domain_coord3.xy;

        gl_ViewportIndex = 2;
    }

    out_domain_coord = domain_coord1;
    gl_Position = vec4(screen_coord1 * 2.0 - 1.0, 0.0, 1.0);
    EmitVertex();

    out_domain_coord = domain_coord2;
    gl_Position = vec4(screen_coord2 * 2.0 - 1.0, 0.0, 1.0);
    EmitVertex();

    out_domain_coord = domain_coord3;
    gl_Position = vec4(screen_coord3 * 2.0 - 1.0, 0.0, 1.0);
    EmitVertex();

    EndPrimitive();
}
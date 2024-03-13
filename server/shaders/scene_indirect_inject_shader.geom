#version 430

layout(points) in;
layout(points, max_vertices = 1) out;

layout(location = 0) in vec3 in_normal[];
layout(location = 1) in vec3 in_flux[];

layout(location = 0) out vec3 out_normal;
layout(location = 1) out vec3 out_flux;

uniform vec3 scene_indirect_domain_min;
uniform vec3 scene_indirect_domain_max;
uniform uvec3 scene_indirect_cell_count;

void main()
{
    vec3 cell_coord = gl_in[0].gl_Position.xyz;
    cell_coord = (cell_coord - scene_indirect_domain_min) / (scene_indirect_domain_max - scene_indirect_domain_min);
    cell_coord.xy = cell_coord.xy * 2.0 - 1.0;
    cell_coord.z = cell_coord.z * scene_indirect_cell_count.z;

    out_normal = in_normal[0];
    out_flux = in_flux[0];
    gl_Position = vec4(cell_coord.xy, 0.0, 1.0);
    gl_Layer = int(cell_coord.z);

    EmitVertex();
    EndPrimitive();
}
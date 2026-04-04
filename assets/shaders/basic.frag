#version 460 core

in vec3 vertex_color;
in float fog_distance;

layout (location = 3) uniform vec3 u_fog_color;
layout (location = 4) uniform float u_fog_start;
layout (location = 5) uniform float u_fog_end;

out vec4 out_color;

void main()
{
    float fog_range = max(u_fog_end - u_fog_start, 0.001);
    float fog_factor = clamp((fog_distance - u_fog_start) / fog_range, 0.0, 1.0);
    vec3 color = mix(vertex_color, u_fog_color, fog_factor);
    out_color = vec4(color, 1.0);
}

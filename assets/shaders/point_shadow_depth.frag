#version 460 core

in vec3 shadow_world_position;
layout (location = 2) uniform vec3 u_light_position;
layout (location = 3) uniform float u_light_radius;

void main()
{
    gl_FragDepth = length(shadow_world_position - u_light_position) / u_light_radius;
}

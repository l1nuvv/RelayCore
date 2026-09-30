#version 460 core

layout (location = 0) in vec3 in_position;
layout (location = 0) uniform mat4 u_light_mvp;
layout (location = 1) uniform mat4 u_model;

out vec3 shadow_world_position;

void main()
{
    vec4 world = u_model * vec4(in_position, 1.0);
    shadow_world_position = world.xyz;
    gl_Position = u_light_mvp * vec4(in_position, 1.0);
}

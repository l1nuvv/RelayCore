#version 460 core

layout (location = 0) in vec3 in_position;

layout (location = 0) uniform mat4 u_light_mvp;

void main()
{
    gl_Position = u_light_mvp * vec4(in_position, 1.0);
}

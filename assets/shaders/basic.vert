#version 460 core

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_color;

layout (location = 0) uniform mat4 u_mvp;

out vec3 vertex_color;

void main()
{
    vertex_color = in_color;
    gl_Position = u_mvp * vec4(in_position, 1.0);
}

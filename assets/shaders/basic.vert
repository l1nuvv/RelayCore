#version 460 core

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_color;

layout (location = 0) uniform mat4 u_mvp;
layout (location = 1) uniform mat4 u_model;
layout (location = 2) uniform vec3 u_camera_position;

out vec3 vertex_color;
out float fog_distance;

void main()
{
    vec3 world_position = (u_model * vec4(in_position, 1.0)).xyz;
    vertex_color = in_color;
    fog_distance = distance(world_position, u_camera_position);
    gl_Position = u_mvp * vec4(in_position, 1.0);
}

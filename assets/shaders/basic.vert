#version 460 core

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_color;
layout (location = 2) in vec3 in_normal;
layout (location = 3) in float in_material_kind;

layout (location = 0) uniform mat4 u_mvp;
layout (location = 1) uniform mat4 u_model;
layout (location = 2) uniform vec3 u_camera_position;

out vec3 vertex_color;
out vec3 world_position;
out vec3 world_normal;
flat out float material_kind;
out float fog_distance;

void main()
{
    world_position = (u_model * vec4(in_position, 1.0)).xyz;
    world_normal = normalize(mat3(transpose(inverse(u_model))) * in_normal);
    vertex_color = in_color;
    material_kind = in_material_kind;
    fog_distance = distance(world_position, u_camera_position);
    gl_Position = u_mvp * vec4(in_position, 1.0);
}

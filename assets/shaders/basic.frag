#version 460 core

in vec3 vertex_color;
in vec3 world_position;
in vec3 world_normal;
flat in float material_kind;
in float fog_distance;

layout (location = 3) uniform vec3 u_fog_color;
layout (location = 4) uniform float u_fog_start;
layout (location = 5) uniform float u_fog_end;
layout (location = 6) uniform int u_local_light_count;
layout (location = 7) uniform vec3 u_local_light_positions[8];
layout (location = 15) uniform vec3 u_local_light_colors[8];
layout (location = 23) uniform float u_local_light_radii[8];
layout (location = 31) uniform float u_scene_time;
layout (location = 32) uniform sampler2D u_blockout_atlas;

out vec4 out_color;

float hash12(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec2 surface_uv(vec3 position, vec3 normal)
{
    vec3 abs_normal = abs(normal);
    if (abs_normal.y >= abs_normal.x && abs_normal.y >= abs_normal.z)
    {
        return position.xz;
    }
    if (abs_normal.x >= abs_normal.z)
    {
        return position.zy;
    }
    return position.xy;
}

float panel_seam_mask(vec2 uv, float scale, float width)
{
    vec2 tiled = uv / max(scale, 0.001);
    vec2 cell = fract(tiled);
    float edge_distance = min(min(cell.x, 1.0 - cell.x), min(cell.y, 1.0 - cell.y));
    float feather = max(fwidth(edge_distance) * 1.5, 0.003);
    return 1.0 - smoothstep(width, width + feather, edge_distance);
}

float soft_noise(vec2 uv, float scale)
{
    vec2 cell = floor(uv / scale);
    return hash12(cell);
}

float soft_panel_volume(vec2 uv, float scale)
{
    vec2 cell = abs(fract((uv / max(scale, 0.001)) + 0.5) - 0.5);
    float profile = max(cell.x, cell.y) * 2.0;
    return 1.0 - smoothstep(0.28, 0.92, profile);
}

float atlas_world_scale(int material_id)
{
    if (material_id == 5 || material_id == 10 || material_id == 11 || material_id == 12)
    {
        return 1.35;
    }
    if (material_id == 6)
    {
        return 1.15;
    }
    if (material_id == 7 || material_id == 8 || material_id == 9)
    {
        return 1.6;
    }
    if (material_id == 16 || material_id == 17 || material_id == 18 || material_id == 19)
    {
        return 0.75;
    }
    if (material_id == 3)
    {
        return 1.8;
    }
    return 1.0;
}

vec2 blockout_atlas_uv(int material_id, vec2 uv)
{
    const vec2 atlas_grid = vec2(8.0, 4.0);
    const vec2 tile_size = 1.0 / atlas_grid;
    vec2 tile_origin = vec2(float(material_id % 8), float(material_id / 8)) * tile_size;
    vec2 tiled_uv = fract(uv / max(atlas_world_scale(material_id), 0.001));
    vec2 gutter = tile_size / 16.0;
    return tile_origin + gutter + (tiled_uv * (tile_size - (gutter * 2.0)));
}

vec3 sample_blockout_atlas(int material_id, vec2 uv)
{
    return texture(u_blockout_atlas, blockout_atlas_uv(material_id, uv)).rgb;
}

vec3 apply_material_surface(vec3 base_color, vec2 uv, vec3 normal, int material_id, out vec3 emissive)
{
    emissive = vec3(0.0);

    float grime = 0.96 + (soft_noise(uv + vec2(19.0, 7.0), 3.6) - 0.5) * 0.08;
    float broad_panels = panel_seam_mask(uv, 1.8, 0.035);
    float micro_panels = panel_seam_mask(uv + vec2(0.35, 0.2), 0.6, 0.012);
    float vent_lines = panel_seam_mask(uv + vec2(0.17, 0.0), 0.22, 0.02);
    float stripe_phase = fract((uv.x + uv.y) * 0.15);
    float diagonal_wear = smoothstep(0.38, 0.62, stripe_phase);
    float upward_bias = clamp(normal.y, 0.0, 1.0);
    float downward_bias = clamp(-normal.y, 0.0, 1.0);
    float broad_volume = soft_panel_volume(uv + vec2(0.11, 0.07), 3.2);

    vec3 color = base_color * grime;
    color *= mix(0.95, 1.035, broad_volume);

    if (material_id == 5)
    {
        float tile = panel_seam_mask(uv, 1.2, 0.03);
        float strip = panel_seam_mask(uv + vec2(0.0, 0.07), 0.42, 0.018);
        color *= mix(0.84, 1.08, diagonal_wear);
        color *= 1.0 - (tile * 0.18) - (strip * 0.05);
    }
    else if (material_id == 6)
    {
        float grid = panel_seam_mask(uv, 1.35, 0.024);
        color *= 1.04 - (grid * 0.14);
        color *= mix(0.92, 1.02, soft_noise(uv + vec2(2.0, 5.0), 2.8));
    }
    else if (material_id == 7 || material_id == 8 || material_id == 9 || material_id == 10)
    {
        float seam_strength = material_id == 8 ? 0.20 : 0.13;
        float fill_shift = mix(0.94, 1.06, soft_noise(uv + vec2(3.0, 11.0), 1.8));
        color *= fill_shift;
        color *= 1.0 - (broad_panels * seam_strength) - (micro_panels * 0.06);
        color *= mix(0.94, 1.04, diagonal_wear);

        if (material_id == 8)
        {
            float inset = panel_seam_mask(uv + vec2(0.18, 0.11), 0.9, 0.026);
            color *= 1.0 - (inset * 0.08);
        }

        if (material_id == 9)
        {
            float service_band = panel_seam_mask(uv + vec2(0.0, 0.16), 0.52, 0.02);
            color *= 1.0 + (service_band * 0.05);
        }
    }
    else if (material_id == 11 || material_id == 12)
    {
        float bevel = panel_seam_mask(uv, 1.5, 0.04);
        color *= 1.0 - (bevel * 0.14);
        color *= mix(0.95, 1.05, soft_noise(uv + vec2(13.0, 1.0), 2.4));
    }
    else if (material_id == 16 || material_id == 17)
    {
        color *= mix(0.88, 1.10, vent_lines);
        color *= mix(0.94, 1.04, soft_noise(uv + vec2(7.0, 2.0), 0.8));
    }
    else if (material_id == 18)
    {
        float segment = panel_seam_mask(uv, 0.42, 0.04);
        color *= mix(0.90, 1.08, segment);
    }
    else if (material_id == 19)
    {
        float pulse = 0.82 + (sin(u_scene_time * 2.4 + uv.x * 4.0) * 0.06);
        float strip = panel_seam_mask(uv, 0.34, 0.07);
        color *= 1.12 + (strip * 0.10);
        emissive = base_color * (0.24 + (strip * 0.12)) * pulse;
    }
    else if (material_id == 3)
    {
        float glass_band = panel_seam_mask(uv + vec2(0.0, 0.1), 0.75, 0.018);
        float fresnel = pow(1.0 - clamp(abs(dot(normalize(world_normal), vec3(0.0, 0.0, 1.0))), 0.0, 1.0), 2.0);
        color *= 1.02 + (glass_band * 0.08);
        emissive += base_color * fresnel * 0.04;
    }
    else
    {
        color *= 1.0 - (micro_panels * 0.03);
    }

    color *= mix(1.0, 1.04, upward_bias * 0.5);
    color *= mix(1.0, 0.94, downward_bias * 0.7);
    return color;
}

void main()
{
    vec3 normal = normalize(world_normal);
    vec2 uv = surface_uv(world_position, normal);
    int material_id = int(material_kind + 0.5);
    vec3 atlas_sample = sample_blockout_atlas(material_id, uv);
    vec3 atlas_base = mix(vertex_color, vertex_color * (0.62 + (atlas_sample * 0.92)), 0.58);
    vec3 emissive;
    vec3 lit_color = apply_material_surface(atlas_base, uv, normal, material_id, emissive);

    for (int light_index = 0; light_index < u_local_light_count; ++light_index)
    {
        float radius = max(u_local_light_radii[light_index], 0.001);
        float distance_to_light = distance(world_position, u_local_light_positions[light_index]);
        float attenuation = clamp(1.0 - (distance_to_light / radius), 0.0, 1.0);
        attenuation *= attenuation;
        float edge_falloff = mix(0.92, 1.08, clamp(dot(normal, normalize(u_local_light_positions[light_index] - world_position)) * 0.5 + 0.5, 0.0, 1.0));
        lit_color += u_local_light_colors[light_index] * attenuation * edge_falloff;
    }

    lit_color += emissive;

    float fog_range = max(u_fog_end - u_fog_start, 0.001);
    float fog_factor = clamp((fog_distance - u_fog_start) / fog_range, 0.0, 1.0);
    vec3 color = mix(clamp(lit_color, 0.0, 1.0), u_fog_color, fog_factor);
    out_color = vec4(color, 1.0);
}

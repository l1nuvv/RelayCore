#version 460 core

in vec3 vertex_color;
in vec3 world_position;
in vec3 world_normal;
flat in float material_kind;
in float fog_distance;

layout (location = 2) uniform vec3 u_camera_position;
layout (location = 3) uniform vec3 u_fog_color;
layout (location = 4) uniform float u_fog_start;
layout (location = 5) uniform float u_fog_end;
layout (location = 6) uniform int u_local_light_count;
layout (location = 7) uniform vec3 u_local_light_positions[8];
layout (location = 15) uniform vec3 u_local_light_colors[8];
layout (location = 23) uniform float u_local_light_radii[8];
layout (location = 31) uniform float u_scene_time;
layout (location = 32) uniform sampler2D u_blockout_atlas;
layout (location = 33) uniform float u_viewport_width;
layout (location = 34) uniform float u_viewport_height;
layout (location = 35) uniform float u_postfx_vignette_strength;
layout (location = 36) uniform float u_postfx_grain_strength;
layout (location = 37) uniform float u_postfx_scanline_strength;
layout (location = 38) uniform float u_postfx_contrast;
layout (location = 39) uniform float u_postfx_saturation;
layout (location = 40) uniform int u_shadow_enabled;
layout (location = 41) uniform mat4 u_shadow_light_view_projection;
layout (location = 45) uniform vec3 u_shadow_light_direction;
layout (location = 46) uniform vec3 u_shadow_light_color;
layout (location = 47) uniform float u_shadow_light_intensity;
layout (location = 48) uniform sampler2D u_shadow_near_map;
layout (location = 49) uniform float u_shadow_strength;
layout (location = 50) uniform float u_shadow_near_bias;
layout (location = 51) uniform float u_shadow_slope_bias_scale;
layout (location = 52) uniform float u_shadow_near_normal_offset;
layout (location = 53) uniform int u_shadow_far_enabled;
layout (location = 54) uniform sampler2D u_shadow_far_map;
layout (location = 55) uniform float u_shadow_cascade_split_distance;
layout (location = 56) uniform mat4 u_shadow_far_light_view_projection;
layout (location = 60) uniform float u_shadow_cascade_blend_range;
layout (location = 61) uniform float u_shadow_near_pcf_radius;
layout (location = 62) uniform float u_shadow_far_pcf_radius;
layout (location = 63) uniform float u_shadow_contact_strength;
layout (location = 64) uniform float u_indirect_bounce_strength;
layout (location = 65) uniform int u_shadow_debug_cascades;
layout (location = 66) uniform float u_shadow_far_bias;
layout (location = 67) uniform float u_shadow_far_normal_offset;

out vec4 out_color;

float hash12(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec2 rotate_vec2(vec2 value, float angle)
{
    float sine = sin(angle);
    float cosine = cos(angle);
    return vec2(
        value.x * cosine - value.y * sine,
        value.x * sine + value.y * cosine
    );
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

float sample_shadow_map(
    sampler2D shadow_map,
    mat4 shadow_light_view_projection,
    vec3 normal,
    float base_bias,
    float normal_offset,
    float pcf_radius,
    int kernel_half_width,
    float jitter_strength
)
{
    if (u_shadow_enabled == 0)
    {
        return 1.0;
    }

    vec3 shadow_world_position = world_position + (normal * normal_offset);
    vec4 light_clip = shadow_light_view_projection * vec4(shadow_world_position, 1.0);
    vec3 shadow_coord = light_clip.xyz / max(light_clip.w, 0.0001);
    shadow_coord = shadow_coord * 0.5 + 0.5;

    if (shadow_coord.x <= 0.0 || shadow_coord.x >= 1.0
        || shadow_coord.y <= 0.0 || shadow_coord.y >= 1.0
        || shadow_coord.z <= 0.0 || shadow_coord.z >= 1.0)
    {
        return 1.0;
    }

    vec3 light_vector = normalize(-u_shadow_light_direction);
    float ndotl = clamp(dot(normal, light_vector), 0.0, 1.0);
    float slope_bias = (1.0 - ndotl) * u_shadow_slope_bias_scale;
    float bias = max(base_bias + slope_bias, base_bias * 0.25);
    vec2 texel_size = 1.0 / vec2(textureSize(shadow_map, 0));
    float filter_radius = max(pcf_radius, 0.0001);
    vec2 jitter_cell = floor(shadow_world_position.xz * 1.75);
    float jitter_angle = hash12(jitter_cell) * 6.2831853;
    float visibility = 0.0;
    float sample_weight = 0.0;

    for (int y = -2; y <= 2; ++y)
    {
        for (int x = -2; x <= 2; ++x)
        {
            if (abs(x) > kernel_half_width || abs(y) > kernel_half_width)
            {
                continue;
            }

            float x_weight = float((kernel_half_width + 1) - abs(x));
            float y_weight = float((kernel_half_width + 1) - abs(y));
            float tap_weight = x_weight * y_weight;
            vec2 tap_offset = vec2(x, y);
            tap_offset = rotate_vec2(tap_offset, jitter_angle * max(jitter_strength, 0.0));
            float closest_depth = texture(
                shadow_map,
                shadow_coord.xy + (tap_offset * texel_size * filter_radius)
            ).r;
            visibility += (shadow_coord.z - bias <= closest_depth ? 1.0 : (1.0 - u_shadow_strength)) * tap_weight;
            sample_weight += tap_weight;
        }
    }

    return visibility / max(sample_weight, 1.0);
}

float sample_shadow_visibility(vec3 normal)
{
    float near_visibility = sample_shadow_map(
        u_shadow_near_map,
        u_shadow_light_view_projection,
        normal,
        u_shadow_near_bias,
        u_shadow_near_normal_offset,
        u_shadow_near_pcf_radius,
        1,
        0.0
    );
    if (u_shadow_far_enabled == 0)
    {
        return near_visibility;
    }

    float far_visibility = sample_shadow_map(
        u_shadow_far_map,
        u_shadow_far_light_view_projection,
        normal,
        u_shadow_far_bias,
        u_shadow_far_normal_offset,
        u_shadow_far_pcf_radius,
        2,
        1.0
    );
    float blend_range = max(u_shadow_cascade_blend_range, 0.0001);
    float blend_start = u_shadow_cascade_split_distance - (blend_range * 0.5);
    float blend_end = u_shadow_cascade_split_distance + (blend_range * 0.5);
    float cascade_mix = smoothstep(blend_start, blend_end, fog_distance);
    return mix(near_visibility, far_visibility, cascade_mix);
}

float shadow_cascade_mix()
{
    if (u_shadow_far_enabled == 0)
    {
        return 0.0;
    }

    float blend_range = max(u_shadow_cascade_blend_range, 0.0001);
    float blend_start = u_shadow_cascade_split_distance - (blend_range * 0.5);
    float blend_end = u_shadow_cascade_split_distance + (blend_range * 0.5);
    return smoothstep(blend_start, blend_end, fog_distance);
}

vec3 safe_normalize_vec3(vec3 value, vec3 fallback)
{
    float length_squared = dot(value, value);
    if (length_squared <= 0.000001)
    {
        return fallback;
    }

    return value * inversesqrt(length_squared);
}

vec3 apply_material_surface(vec3 base_color, vec2 uv, vec3 normal, int material_id, out vec3 emissive)
{
    emissive = vec3(0.0);

    if (material_id == 3)
    {
        vec3 view_direction = safe_normalize_vec3(u_camera_position - world_position, vec3(0.0, 0.0, 1.0));
        float glass_band = panel_seam_mask(uv + vec2(0.0, 0.1), 0.75, 0.018);
        float fresnel = pow(1.0 - clamp(abs(dot(normal, view_direction)), 0.0, 1.0), 3.0);
        vec3 glass_tint = mix(vec3(0.12, 0.18, 0.22), base_color, 0.65);
        vec3 glass_color = mix(glass_tint, glass_tint * vec3(0.72, 0.88, 1.04), 0.45);
        glass_color *= 0.92 + (glass_band * 0.04);
        glass_color *= 0.96 + (fresnel * 0.08);
        emissive += glass_tint * (0.03 + (fresnel * 0.10));
        return glass_color;
    }

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
    if (material_id == 3)
    {
        atlas_base = vertex_color;
    }
    vec3 emissive;
    vec3 material_color = apply_material_surface(atlas_base, uv, normal, material_id, emissive);
    float sky_ambient = clamp(normal.y * 0.5 + 0.5, 0.0, 1.0);
    float broad_shadow = panel_seam_mask(uv + vec2(0.13, 0.09), 1.9, 0.032);
    float micro_shadow = panel_seam_mask(uv + vec2(0.41, 0.17), 0.55, 0.012);
    float underside_shadow = clamp(-normal.y, 0.0, 1.0);
    float cavity_shadow = (broad_shadow * 0.12) + (micro_shadow * 0.05) + (underside_shadow * 0.10);
    float ambient_strength = clamp(mix(0.42, 0.74, sky_ambient) - cavity_shadow, 0.20, 1.00);
    vec3 cool_ambient = vec3(0.06, 0.09, 0.13) * (0.36 + (sky_ambient * 0.34));
    vec3 warm_bounce = vec3(0.10, 0.07, 0.05) * (0.06 + (underside_shadow * 0.20));
    float bounce_strength = max(u_indirect_bounce_strength, 0.0);
    vec3 bounce_tint = mix(vec3(0.06, 0.06, 0.07), material_color, 0.55);
    vec3 material_bounce =
        bounce_tint * (0.04 + (underside_shadow * 0.18) + ((1.0 - sky_ambient) * 0.06)) * bounce_strength;
    vec3 lit_color = material_color * ambient_strength;
    lit_color += material_color * (cool_ambient + warm_bounce);
    lit_color += material_bounce;
    vec3 directional_light = normalize(-u_shadow_light_direction);
    float directional_diffuse = clamp(dot(normal, directional_light), 0.0, 1.0);
    float directional_shadow = sample_shadow_visibility(normal);
    float upward_receiver = clamp(normal.y, 0.0, 1.0);
    float contact_shadow = (1.0 - directional_shadow) * directional_diffuse * upward_receiver;
    lit_color *= 1.0 - (contact_shadow * u_shadow_contact_strength);
    lit_color += material_color
                 * u_shadow_light_color
                 * directional_diffuse
                 * directional_shadow
                 * u_shadow_light_intensity;

    for (int light_index = 0; light_index < u_local_light_count; ++light_index)
    {
        float radius = max(u_local_light_radii[light_index], 0.001);
        vec3 light_vector = u_local_light_positions[light_index] - world_position;
        float distance_to_light = length(light_vector);
        vec3 light_direction = distance_to_light > 0.0001 ? (light_vector / distance_to_light) : vec3(0.0, 1.0, 0.0);
        float attenuation = clamp(1.0 - (distance_to_light / radius), 0.0, 1.0);
        attenuation *= attenuation;
        float wrapped_diffuse = clamp(dot(normal, light_direction) * 0.65 + 0.35, 0.0, 1.0);
        lit_color += u_local_light_colors[light_index] * attenuation * wrapped_diffuse * 1.12;
    }

    lit_color += emissive;

    float fog_range = max(u_fog_end - u_fog_start, 0.001);
    float fog_factor = clamp((fog_distance - u_fog_start) / fog_range, 0.0, 1.0);
    vec3 color = mix(clamp(lit_color, 0.0, 1.0), u_fog_color, fog_factor);
    if (u_shadow_debug_cascades != 0)
    {
        float cascade_mix = shadow_cascade_mix();
        vec3 cascade_tint = mix(vec3(0.18, 0.42, 0.95), vec3(0.95, 0.56, 0.14), cascade_mix);
        color = mix(color, color * cascade_tint, 0.38);
    }

    vec2 viewport_size = vec2(max(u_viewport_width, 1.0), max(u_viewport_height, 1.0));
    vec2 screen_uv = gl_FragCoord.xy / viewport_size;
    float screen_radius = distance(screen_uv, vec2(0.5));
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float vignette = 1.0 - (smoothstep(0.28, 0.84, screen_radius) * u_postfx_vignette_strength);
    vec2 grain_uv = floor(gl_FragCoord.xy * 0.5) + vec2(floor(u_scene_time * 24.0));
    float grain = (hash12(grain_uv) - 0.5) * u_postfx_grain_strength * (0.08 + ((1.0 - luminance) * 0.16));
    float scanline_wave = sin((screen_uv.y * viewport_size.y * 0.92) + (u_scene_time * 10.0)) * 0.5 + 0.5;
    float scanline = 1.0 - ((0.04 + (scanline_wave * 0.08)) * u_postfx_scanline_strength);

    color = mix(vec3(luminance), color, u_postfx_saturation);
    color = ((color - 0.5) * u_postfx_contrast) + 0.5;
    color *= mix(vec3(0.97, 0.99, 1.01), vec3(1.01, 1.00, 0.98), smoothstep(0.18, 0.78, luminance));
    color = (color * vignette * scanline) + vec3(grain);
    color = clamp(color, 0.0, 1.0);
    float alpha = 1.0;
    if (material_id == 3)
    {
        vec3 view_direction = safe_normalize_vec3(u_camera_position - world_position, vec3(0.0, 0.0, 1.0));
        float fresnel = pow(1.0 - clamp(abs(dot(normal, view_direction)), 0.0, 1.0), 2.6);
        alpha = clamp(0.16 + (fresnel * 0.34) + (fog_factor * 0.08), 0.14, 0.52);
    }

    out_color = vec4(color, alpha);
}

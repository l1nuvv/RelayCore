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

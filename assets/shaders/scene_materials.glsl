vec3 apply_material_surface(vec3 base_color, vec2 uv, vec3 normal, int material_id, out vec3 emissive)
{
    emissive = vec3(0.0);
    int material_profile = rc_material_profile(material_id);
    if (material_profile == RC_MATERIAL_PROFILE_DEFAULT)
    {
        return base_color;
    }

    if (material_profile == RC_MATERIAL_PROFILE_GLASS)
    {
        vec3 view_direction = safe_normalize_vec3(u_camera_position - world_position, vec3(0.0, 0.0, 1.0));
        float glass_band = panel_seam_mask(uv + vec2(0.0, 0.1), 0.75, 0.018);
        float fresnel = pow(1.0 - clamp(abs(dot(normal, view_direction)), 0.0, 1.0), 3.0);
        vec3 glass_tint = mix(rc_glass_visual_tint(), base_color, 0.65);
        vec3 glass_color = mix(glass_tint, glass_tint * vec3(0.72, 0.88, 1.04), 0.45);
        glass_color *= 0.92 + (glass_band * 0.04);
        glass_color *= 0.96 + (fresnel * 0.08);
        emissive += glass_tint * (rc_material_emissive_strength(material_id) + (fresnel * 0.10));
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

    if (material_profile == RC_MATERIAL_PROFILE_FLOOR_GRID)
    {
        float tile = panel_seam_mask(uv, 1.2, 0.03);
        float strip = panel_seam_mask(uv + vec2(0.0, 0.07), 0.42, 0.018);
        color *= mix(0.84, 1.08, diagonal_wear);
        color *= 1.0 - (tile * 0.18) - (strip * 0.05);
    }
    else if (material_profile == RC_MATERIAL_PROFILE_CEILING_GRID)
    {
        float grid = panel_seam_mask(uv, 1.35, 0.024);
        color *= 1.04 - (grid * 0.14);
        color *= mix(0.92, 1.02, soft_noise(uv + vec2(2.0, 5.0), 2.8));
    }
    else if (material_profile == RC_MATERIAL_PROFILE_PANEL
             || material_profile == RC_MATERIAL_PROFILE_PANEL_INSET
             || material_profile == RC_MATERIAL_PROFILE_PANEL_SERVICE)
    {
        float seam_strength = material_profile == RC_MATERIAL_PROFILE_PANEL_INSET ? 0.20 : 0.13;
        float fill_shift = mix(0.94, 1.06, soft_noise(uv + vec2(3.0, 11.0), 1.8));
        color *= fill_shift;
        color *= 1.0 - (broad_panels * seam_strength) - (micro_panels * 0.06);
        color *= mix(0.94, 1.04, diagonal_wear);

        if (material_profile == RC_MATERIAL_PROFILE_PANEL_INSET)
        {
            float inset = panel_seam_mask(uv + vec2(0.18, 0.11), 0.9, 0.026);
            color *= 1.0 - (inset * 0.08);
        }

        if (material_profile == RC_MATERIAL_PROFILE_PANEL_SERVICE)
        {
            float service_band = panel_seam_mask(uv + vec2(0.0, 0.16), 0.52, 0.02);
            color *= 1.0 + (service_band * 0.05);
        }
    }
    else if (material_profile == RC_MATERIAL_PROFILE_PLATFORM_TOP)
    {
        float bevel = panel_seam_mask(uv, 1.5, 0.04);
        color *= 1.0 - (bevel * 0.14);
        color *= mix(0.95, 1.05, soft_noise(uv + vec2(13.0, 1.0), 2.4));
    }
    else if (material_profile == RC_MATERIAL_PROFILE_STRUCTURAL)
    {
        color *= mix(0.88, 1.10, vent_lines);
        color *= mix(0.94, 1.04, soft_noise(uv + vec2(7.0, 2.0), 0.8));
    }
    else if (material_profile == RC_MATERIAL_PROFILE_DIVIDER)
    {
        float segment = panel_seam_mask(uv, 0.42, 0.04);
        color *= mix(0.90, 1.08, segment);
    }
    else if (material_profile == RC_MATERIAL_PROFILE_LIGHT_STRIP)
    {
        float pulse_speed = max(rc_material_emissive_pulse_speed(material_id), 0.001);
        float pulse = 0.82 + (sin(u_scene_time * pulse_speed + uv.x * 4.0) * 0.06);
        float strip = panel_seam_mask(uv, 0.34, 0.07);
        color *= 1.12 + (strip * 0.10);
        emissive = base_color * (rc_material_emissive_strength(material_id) + (strip * 0.12)) * pulse;
    }
    else
    {
        color *= 1.0 - (micro_panels * 0.03);
    }

    color *= mix(1.0, 1.04, upward_bias * 0.5);
    color *= mix(1.0, 0.94, downward_bias * 0.7);
    return color;
}

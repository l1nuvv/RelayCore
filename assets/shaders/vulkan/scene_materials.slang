float3 apply_material_surface(float3 base_color, float2 uv, float3 normal, float3 world_position, int material_id, out float3 emissive)
{
    emissive = float3(0.0);
    int material_profile = int(materials.atlas[material_id].z);
    if (material_profile == 0)
    {
        return base_color;
    }

    if (material_profile == 1)
    {
        float3 view_direction = safe_normalize_vec3(float3(draw.cameraX, draw.cameraY, draw.cameraZ) - world_position, float3(0.0, 0.0, 1.0));
        float glass_band = panel_seam_mask(uv + float2(0.0, 0.1), 0.75, 0.018);
        float fresnel = pow(1.0 - clamp(abs(dot(normal, view_direction)), 0.0, 1.0), 3.0);
        float3 glass_tint = lerp(materials.glassTint.rgb, base_color, 0.65);
        float3 glass_color = lerp(glass_tint, glass_tint * float3(0.72, 0.88, 1.04), 0.45);
        glass_color *= 0.92 + (glass_band * 0.04);
        glass_color *= 0.96 + (fresnel * 0.08);
        emissive += glass_tint * (materials.response[material_id].z + (fresnel * 0.10));
        return glass_color;
    }

    float grime = 0.96 + (soft_noise(uv + float2(19.0, 7.0), 3.6) - 0.5) * 0.08;
    float broad_panels = panel_seam_mask(uv, 1.8, 0.035);
    float micro_panels = panel_seam_mask(uv + float2(0.35, 0.2), 0.6, 0.012);
    float vent_lines = panel_seam_mask(uv + float2(0.17, 0.0), 0.22, 0.02);
    float stripe_phase = frac((uv.x + uv.y) * 0.15);
    float diagonal_wear = smoothstep(0.38, 0.62, stripe_phase);
    float upward_bias = clamp(normal.y, 0.0, 1.0);
    float downward_bias = clamp(-normal.y, 0.0, 1.0);
    float broad_volume = soft_panel_volume(uv + float2(0.11, 0.07), 3.2);

    float3 color = base_color * grime;
    color *= lerp(0.95, 1.035, broad_volume);

    if (material_profile == 2)
    {
        float tile = panel_seam_mask(uv, 1.2, 0.03);
        float strip = panel_seam_mask(uv + float2(0.0, 0.07), 0.42, 0.018);
        color *= lerp(0.84, 1.08, diagonal_wear);
        color *= 1.0 - (tile * 0.18) - (strip * 0.05);
    }
    else if (material_profile == 3)
    {
        float grid = panel_seam_mask(uv, 1.35, 0.024);
        color *= 1.04 - (grid * 0.14);
        color *= lerp(0.92, 1.02, soft_noise(uv + float2(2.0, 5.0), 2.8));
    }
    else if (material_profile == 4
             || material_profile == 5
             || material_profile == 6)
    {
        float seam_strength = material_profile == 5 ? 0.20 : 0.13;
        float fill_shift = lerp(0.94, 1.06, soft_noise(uv + float2(3.0, 11.0), 1.8));
        color *= fill_shift;
        color *= 1.0 - (broad_panels * seam_strength) - (micro_panels * 0.06);
        color *= lerp(0.94, 1.04, diagonal_wear);

        if (material_profile == 5)
        {
            float inset = panel_seam_mask(uv + float2(0.18, 0.11), 0.9, 0.026);
            color *= 1.0 - (inset * 0.08);
        }

        if (material_profile == 6)
        {
            float service_band = panel_seam_mask(uv + float2(0.0, 0.16), 0.52, 0.02);
            color *= 1.0 + (service_band * 0.05);
        }
    }
    else if (material_profile == 7)
    {
        float bevel = panel_seam_mask(uv, 1.5, 0.04);
        color *= 1.0 - (bevel * 0.14);
        color *= lerp(0.95, 1.05, soft_noise(uv + float2(13.0, 1.0), 2.4));
    }
    else if (material_profile == 8)
    {
        color *= lerp(0.88, 1.10, vent_lines);
        color *= lerp(0.94, 1.04, soft_noise(uv + float2(7.0, 2.0), 0.8));
    }
    else if (material_profile == 9)
    {
        float segment = panel_seam_mask(uv, 0.42, 0.04);
        color *= lerp(0.90, 1.08, segment);
    }
    else if (material_profile == 10)
    {
        float pulse_speed = max(materials.response[material_id].w, 0.001);
        float pulse = 0.82 + (sin(frame.viewportTimeDebug.z * pulse_speed + uv.x * 4.0) * 0.06);
        float strip = panel_seam_mask(uv, 0.34, 0.07);
        color *= 1.12 + (strip * 0.10);
        emissive = base_color * (materials.response[material_id].z + (strip * 0.12)) * pulse;
    }
    else
    {
        color *= 1.0 - (micro_panels * 0.03);
    }

    color *= lerp(1.0, 1.04, upward_bias * 0.5);
    color *= lerp(1.0, 0.94, downward_bias * 0.7);
    return color;
}

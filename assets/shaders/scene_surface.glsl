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

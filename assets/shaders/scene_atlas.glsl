float atlas_world_scale(int material_id)
{
    return rc_material_atlas_world_scale(material_id);
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
    // Derivatives of the unwrapped coordinates prevent blurry bands at tile boundaries.
    vec2 scale = vec2(0.875 / 8.0, 0.875 / 4.0) / max(atlas_world_scale(material_id), 0.001);
    return textureGrad(u_blockout_atlas, blockout_atlas_uv(material_id, uv), dFdx(uv) * scale, dFdy(uv) * scale).rgb;
}

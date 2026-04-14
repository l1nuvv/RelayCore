#ifndef RELAYCORE_SCENE_APPLY_POSTFX
#define RELAYCORE_SCENE_APPLY_POSTFX 1
#endif

#ifndef RELAYCORE_SCENE_FOG_SCALE
#define RELAYCORE_SCENE_FOG_SCALE 1.0
#endif

#ifndef RELAYCORE_SCENE_COLOR_LIFT
#define RELAYCORE_SCENE_COLOR_LIFT 0.0
#endif

#ifndef RELAYCORE_SCENE_FORCE_ALPHA
#define RELAYCORE_SCENE_FORCE_ALPHA -1.0
#endif

#ifndef RELAYCORE_SCENE_EMISSIVE_BOOST
#define RELAYCORE_SCENE_EMISSIVE_BOOST 0.0
#endif

float scene_fog_factor(float base_fog_factor)
{
    return clamp(base_fog_factor * RELAYCORE_SCENE_FOG_SCALE, 0.0, 1.0);
}

vec3 scene_apply_postfx(vec3 color, float luminance, vec2 screen_uv, vec2 viewport_size)
{
    if (RELAYCORE_SCENE_APPLY_POSTFX == 0)
    {
        return clamp(color + vec3(RELAYCORE_SCENE_COLOR_LIFT), 0.0, 1.0);
    }

    float screen_radius = distance(screen_uv, vec2(0.5));
    float vignette = 1.0 - (smoothstep(0.28, 0.84, screen_radius) * u_postfx_vignette_strength);
    vec2 grain_uv = floor(gl_FragCoord.xy * 0.5) + vec2(floor(u_scene_time * 24.0));
    float grain = (hash12(grain_uv) - 0.5) * u_postfx_grain_strength * (0.08 + ((1.0 - luminance) * 0.16));
    float scanline_wave = sin((screen_uv.y * viewport_size.y * 0.92) + (u_scene_time * 10.0)) * 0.5 + 0.5;
    float scanline = 1.0 - ((0.04 + (scanline_wave * 0.08)) * u_postfx_scanline_strength);

    vec3 postfx_color = mix(vec3(luminance), color, u_postfx_saturation);
    postfx_color = ((postfx_color - 0.5) * u_postfx_contrast) + 0.5;
    postfx_color *= mix(vec3(0.97, 0.99, 1.01), vec3(1.01, 1.00, 0.98), smoothstep(0.18, 0.78, luminance));
    postfx_color = (postfx_color * vignette * scanline) + vec3(grain);
    return clamp(postfx_color + vec3(RELAYCORE_SCENE_COLOR_LIFT), 0.0, 1.0);
}

float scene_alpha(float computed_alpha)
{
    if (RELAYCORE_SCENE_FORCE_ALPHA >= 0.0)
    {
        return RELAYCORE_SCENE_FORCE_ALPHA;
    }

    return clamp(computed_alpha, 0.0, 1.0);
}

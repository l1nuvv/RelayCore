#include "scene_uniforms.glsl"
#include "scene_math.glsl"
#include "scene_surface.glsl"
#include "scene_atlas.glsl"
#include "scene_shadows.glsl"
#include "scene_materials.glsl"
#include "scene_postfx.glsl"

void main()
{
    vec3 normal = normalize(world_normal);
    vec2 uv = surface_uv(world_position, normal);
    int material_id = int(material_kind + 0.5);
    vec3 atlas_sample = sample_blockout_atlas(material_id, uv);
    vec3 atlas_base = mix(vertex_color, vertex_color * (0.62 + (atlas_sample * 0.92)), rc_material_atlas_mix(material_id));

    vec3 emissive;
    vec3 material_color = apply_material_surface(atlas_base, uv, normal, material_id, emissive);
    emissive *= 1.0 + RELAYCORE_SCENE_EMISSIVE_BOOST;
    float sky_ambient = clamp(normal.y * 0.5 + 0.5, 0.0, 1.0);
    float broad_shadow = panel_seam_mask(uv + vec2(0.13, 0.09), 1.9, 0.032);
    float micro_shadow = panel_seam_mask(uv + vec2(0.41, 0.17), 0.55, 0.012);
    float underside_shadow = clamp(-normal.y, 0.0, 1.0);
    float cavity_shadow = (broad_shadow * rc_cavity_broad_strength())
                          + (micro_shadow * rc_cavity_micro_strength())
                          + (underside_shadow * rc_cavity_underside_strength());
    float ambient_strength = clamp(
        (mix(rc_ambient_floor(), rc_ambient_zenith(), sky_ambient) - (cavity_shadow * rc_cavity_ambient_darken()))
            * rc_material_ambient_bias(material_id),
        rc_ambient_min(),
        rc_ambient_max()
    );
    vec3 cool_ambient = rc_cool_ambient_color() * (rc_cool_ambient_floor() + (sky_ambient * rc_cool_ambient_zenith()));
    vec3 warm_bounce = rc_warm_bounce_color() * (rc_warm_bounce_floor() + (underside_shadow * rc_warm_bounce_underside()));
    float bounce_strength = max(u_indirect_bounce_strength, 0.0);
    vec3 bounce_tint = mix(rc_bounce_neutral_color(), material_color, rc_material_bounce_mix(material_id));
    vec3 material_bounce =
        bounce_tint
        * (rc_bounce_floor() + (underside_shadow * rc_bounce_underside()) + ((1.0 - sky_ambient) * rc_bounce_darkness()))
        * bounce_strength;
    vec3 lit_color = material_color * ambient_strength;
    lit_color += material_color * (cool_ambient + warm_bounce) * 1.04;
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
        attenuation = attenuation * attenuation * (3.0 - (2.0 * attenuation));
        float wrapped_diffuse = clamp(
            dot(normal, light_direction) * rc_local_light_wrap() + rc_local_light_fill(),
            0.0,
            1.0
        );
        lit_color += u_local_light_colors[light_index] * attenuation * wrapped_diffuse * rc_local_light_gain();
    }

    lit_color += emissive;

    float fog_range = max(u_fog_end - u_fog_start, 0.001);
    float fog_factor = clamp((fog_distance - u_fog_start) / fog_range, 0.0, 1.0);
    vec3 color = mix(clamp(lit_color, 0.0, 1.0), u_fog_color, scene_fog_factor(fog_factor));
    if (u_shadow_debug_cascades != 0)
    {
        float cascade_mix = shadow_cascade_mix();
        vec3 cascade_tint = mix(rc_cascade_near_tint(), rc_cascade_far_tint(), cascade_mix);
        color = mix(color, color * cascade_tint, 0.38);
    }

    vec2 viewport_size = vec2(max(u_viewport_width, 1.0), max(u_viewport_height, 1.0));
    vec2 screen_uv = gl_FragCoord.xy / viewport_size;
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = scene_apply_postfx(color, luminance, screen_uv, viewport_size);

    float alpha = 1.0;
    if (rc_material_profile(material_id) == RC_MATERIAL_PROFILE_GLASS)
    {
        vec3 view_direction = safe_normalize_vec3(u_camera_position - world_position, vec3(0.0, 0.0, 1.0));
        float fresnel = pow(1.0 - clamp(abs(dot(normal, view_direction)), 0.0, 1.0), 2.6);
        alpha = clamp(0.16 + (fresnel * 0.34) + (fog_factor * 0.08), 0.14, 0.52);
    }

    out_color = vec4(color, scene_alpha(alpha));
}

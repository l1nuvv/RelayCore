#include "relaycore/core/log.h"
#include "relaycore/render/renderer.h"
#include "relaygame/demo_game_debug_commands_internal.h"

namespace RelayGame::detail
{
    auto handle_render_authoring_commands(
        DemoGameRuntimeState state,
        RelayCore::EngineServices& services,
        const IterationCommandRequests& requests
    ) -> bool
    {
        RelayCore::Renderer& renderer = services.renderer();
        if (!requests.overlay.dump_render_authoring_contract)
        {
            return false;
        }

        const auto& authoring_config = renderer.authoring_config();
        RelayCore::log::info("Render authoring contract: {}", authoring_config.source_path.string());
        for (const RelayCore::SurfaceMaterial material : RelayCore::all_surface_materials())
        {
            const auto& authored_material = RelayCore::render::material_authoring(authoring_config, material);
            RelayCore::log::info(
                "Material {} | profile={} | transparent={} | bevel={} | atlas_scale={:.2f} | atlas_mix={:.2f} | "
                "ambient_bias={:.2f} | bounce_mix={:.2f} | emissive={:.2f} | pulse={:.2f}",
                authored_material.canonical_name,
                RelayCore::render::material_render_profile_name(authored_material.render_profile),
                authored_material.transparent ? "yes" : "no",
                authored_material.bevel_enabled ? "yes" : "no",
                authored_material.atlas_world_scale,
                authored_material.atlas_mix,
                authored_material.ambient_bias,
                authored_material.bounce_mix,
                authored_material.emissive_strength,
                authored_material.emissive_pulse_speed
            );
        }

        const auto& lighting = authoring_config.lighting;
        RelayCore::log::info(
            "Lighting ambient | min={:.2f} max={:.2f} floor={:.2f} zenith={:.2f} | cool floor={:.2f} zenith={:.2f}",
            lighting.ambient_min,
            lighting.ambient_max,
            lighting.ambient_floor,
            lighting.ambient_zenith,
            lighting.cool_ambient_floor,
            lighting.cool_ambient_zenith
        );
        RelayCore::log::info(
            "Lighting bounce | warm floor={:.2f} underside={:.2f} | neutral floor={:.2f} underside={:.2f} | "
            "darkness={:.2f}",
            lighting.warm_bounce_floor,
            lighting.warm_bounce_underside,
            lighting.bounce_floor,
            lighting.bounce_underside,
            lighting.bounce_darkness
        );
        RelayCore::log::info(
            "Lighting cavity/local | broad={:.2f} micro={:.2f} underside={:.2f} darken={:.2f} | wrap={:.2f} "
            "fill={:.2f} gain={:.2f}",
            lighting.cavity_broad_strength,
            lighting.cavity_micro_strength,
            lighting.cavity_underside_strength,
            lighting.cavity_ambient_darken,
            lighting.local_light_wrap,
            lighting.local_light_fill,
            lighting.local_light_gain
        );

        const auto& render_light = authoring_config.render_light;
        RelayCore::log::info(
            "Render light contract | default_color=[{:.2f}, {:.2f}, {:.2f}] | default_intensity={:.2f} | "
            "default_radius={:.2f} | intensity=[{:.2f}, {:.2f}] | radius=[{:.2f}, {:.2f}]",
            render_light.default_color.x,
            render_light.default_color.y,
            render_light.default_color.z,
            render_light.default_intensity,
            render_light.default_radius,
            render_light.min_intensity,
            render_light.max_intensity,
            render_light.min_radius,
            render_light.max_radius
        );

        publish_and_log_command_result(
            state,
            services,
            true,
            "Dumped render authoring contract: " + authoring_config.source_path.string()
        );
        return true;
    }
} // namespace RelayGame::detail

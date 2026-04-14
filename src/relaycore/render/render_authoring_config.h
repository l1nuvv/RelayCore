#pragma once

#include "relaycore/world/surface_material_utils.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <glm/vec3.hpp>
#include <string>

namespace RelayCore::render
{
    enum class MaterialRenderProfile : std::uint8_t {
        Default = 0,
        Glass,
        FloorGrid,
        CeilingGrid,
        Panel,
        PanelInset,
        PanelService,
        PlatformTop,
        Structural,
        Divider,
        LightStrip,
    };

    struct MaterialAuthoringConfig {
        RelayCore::SurfaceMaterial material = RelayCore::SurfaceMaterial::Concrete;
        std::string canonical_name{};
        glm::vec3 base_color{1.0F, 1.0F, 1.0F};
        MaterialRenderProfile render_profile = MaterialRenderProfile::Default;
        bool transparent = false;
        bool bevel_enabled = false;
        float atlas_world_scale = 1.0F;
        float atlas_mix = 0.58F;
        float ambient_bias = 1.0F;
        float bounce_mix = 0.55F;
        float emissive_strength = 0.0F;
        float emissive_pulse_speed = 0.0F;
    };

    struct LightingAuthoringConfig {
        glm::vec3 cool_ambient_color{0.08F, 0.11F, 0.15F};
        float cool_ambient_floor = 0.40F;
        float cool_ambient_zenith = 0.36F;
        glm::vec3 warm_bounce_color{0.11F, 0.08F, 0.06F};
        float warm_bounce_floor = 0.08F;
        float warm_bounce_underside = 0.18F;
        glm::vec3 bounce_neutral_color{0.06F, 0.06F, 0.07F};
        float bounce_floor = 0.04F;
        float bounce_underside = 0.18F;
        float bounce_darkness = 0.06F;
        float ambient_floor = 0.54F;
        float ambient_zenith = 0.82F;
        float ambient_min = 0.32F;
        float ambient_max = 1.02F;
        float cavity_broad_strength = 0.12F;
        float cavity_micro_strength = 0.05F;
        float cavity_underside_strength = 0.10F;
        float cavity_ambient_darken = 0.78F;
        float local_light_wrap = 0.55F;
        float local_light_fill = 0.45F;
        float local_light_gain = 1.28F;
    };

    struct RenderLightAuthoringConfig {
        glm::vec3 default_color{0.18F, 0.22F, 0.30F};
        float default_intensity = 1.0F;
        float default_radius = 3.5F;
        float min_radius = 0.5F;
        float max_radius = 48.0F;
        float min_intensity = 0.0F;
        float max_intensity = 8.0F;
    };

    struct RenderAuthoringConfig {
        std::filesystem::path source_path{};
        std::array<MaterialAuthoringConfig, RelayCore::surface_material_count()> materials{};
        LightingAuthoringConfig lighting{};
        RenderLightAuthoringConfig render_light{};
    };

    [[nodiscard]] auto default_render_authoring_config() -> RenderAuthoringConfig;

    [[nodiscard]] auto load_render_authoring_config(const std::filesystem::path& config_path) -> RenderAuthoringConfig;

    [[nodiscard]] auto load_default_render_authoring_config() -> RenderAuthoringConfig;

    [[nodiscard]] auto material_authoring(const RenderAuthoringConfig& config, RelayCore::SurfaceMaterial material)
        -> const MaterialAuthoringConfig&;

    [[nodiscard]] auto material_render_profile_name(MaterialRenderProfile profile) noexcept -> std::string_view;
} // namespace RelayCore::render

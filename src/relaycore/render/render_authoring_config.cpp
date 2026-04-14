#include "relaycore/render/render_authoring_config.h"

#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"

#include <array>
#include <glm/common.hpp>
#include <glm/vec3.hpp>
#include <string_view>
#include <toml++/toml.hpp>

namespace RelayCore::render
{
    namespace
    {
        template<typename T>
        void
        assign_if_present(const toml::node_view<const toml::node>& view, const std::string_view key, T& destination)
        {
            if (const auto value = view[key].template value<T>())
            {
                destination = *value;
            }
        }

        [[nodiscard]] auto
        read_vec3(const toml::node_view<const toml::node>& view, const std::string_view key, const glm::vec3& fallback)
            -> glm::vec3
        {
            const toml::node_view<const toml::node> node = view[key];
            if (const toml::array* array = node.as_array(); array != nullptr && array->size() >= 3)
            {
                const auto x = (*array)[0].value<double>();
                const auto y = (*array)[1].value<double>();
                const auto z = (*array)[2].value<double>();
                if (x && y && z)
                {
                    return {
                        static_cast<float>(*x),
                        static_cast<float>(*y),
                        static_cast<float>(*z),
                    };
                }
            }

            return fallback;
        }

        [[nodiscard]] auto parse_profile_name(const std::string_view value) -> MaterialRenderProfile
        {
            if (value == "default")
                return MaterialRenderProfile::Default;
            if (value == "glass")
                return MaterialRenderProfile::Glass;
            if (value == "floor_grid")
                return MaterialRenderProfile::FloorGrid;
            if (value == "ceiling_grid")
                return MaterialRenderProfile::CeilingGrid;
            if (value == "panel")
                return MaterialRenderProfile::Panel;
            if (value == "panel_inset")
                return MaterialRenderProfile::PanelInset;
            if (value == "panel_service")
                return MaterialRenderProfile::PanelService;
            if (value == "platform_top")
                return MaterialRenderProfile::PlatformTop;
            if (value == "structural")
                return MaterialRenderProfile::Structural;
            if (value == "divider")
                return MaterialRenderProfile::Divider;
            if (value == "light_strip")
                return MaterialRenderProfile::LightStrip;

            throw std::runtime_error("Unknown material render profile: " + std::string(value));
        }

        void
        validate_unit_color(const glm::vec3& color, const std::string_view owner, const std::string_view field_name)
        {
            if (color.x < 0.0F || color.y < 0.0F || color.z < 0.0F || color.x > 1.0F || color.y > 1.0F
                || color.z > 1.0F)
            {
                throw std::runtime_error(
                    std::string(owner) + " requires " + std::string(field_name) + " components in [0, 1]"
                );
            }
        }

        void validate_material_entry(const MaterialAuthoringConfig& material)
        {
            const std::string owner = "Render authoring material '" + material.canonical_name + "'";
            if (material.canonical_name.empty())
            {
                throw std::runtime_error("Render authoring material entry is missing canonical_name");
            }
            validate_unit_color(material.base_color, owner, "base_color");
            if (material.atlas_world_scale <= 0.0F)
            {
                throw std::runtime_error(owner + " requires atlas_world_scale > 0");
            }
            if (material.atlas_mix < 0.0F || material.atlas_mix > 1.0F)
            {
                throw std::runtime_error(owner + " requires atlas_mix in [0, 1]");
            }
            if (material.ambient_bias < 0.5F || material.ambient_bias > 1.5F)
            {
                throw std::runtime_error(owner + " requires ambient_bias in [0.5, 1.5]");
            }
            if (material.bounce_mix < 0.0F || material.bounce_mix > 1.0F)
            {
                throw std::runtime_error(owner + " requires bounce_mix in [0, 1]");
            }
            if (material.emissive_strength < 0.0F || material.emissive_pulse_speed < 0.0F)
            {
                throw std::runtime_error(owner + " requires non-negative emissive values");
            }
            if (material.transparent && material.render_profile != MaterialRenderProfile::Glass)
            {
                throw std::runtime_error(owner + " may only use transparent=true with profile=glass");
            }
            if (material.emissive_pulse_speed > 0.0F && material.render_profile != MaterialRenderProfile::LightStrip)
            {
                throw std::runtime_error(owner + " only allows emissive_pulse_speed for profile=light_strip");
            }
            if (material.render_profile != MaterialRenderProfile::Glass
                && material.render_profile != MaterialRenderProfile::LightStrip && material.emissive_strength > 0.05F)
            {
                throw std::runtime_error(
                    owner + " only allows subtle emissive_strength <= 0.05 outside glass/light_strip profiles"
                );
            }

            switch (material.render_profile)
            {
                case MaterialRenderProfile::Glass:
                    if (!material.transparent)
                    {
                        throw std::runtime_error(owner + " requires transparent=true for profile=glass");
                    }
                    if (material.bevel_enabled)
                    {
                        throw std::runtime_error(owner + " disallows bevel_enabled for profile=glass");
                    }
                    if (material.atlas_mix > 0.20F)
                    {
                        throw std::runtime_error(owner + " requires atlas_mix <= 0.20 for profile=glass");
                    }
                    if (material.bounce_mix > 0.35F)
                    {
                        throw std::runtime_error(owner + " requires bounce_mix <= 0.35 for profile=glass");
                    }
                    if (material.emissive_strength > 0.05F)
                    {
                        throw std::runtime_error(owner + " requires emissive_strength <= 0.05 for profile=glass");
                    }
                    break;
                case MaterialRenderProfile::FloorGrid:
                case MaterialRenderProfile::CeilingGrid:
                case MaterialRenderProfile::Panel:
                case MaterialRenderProfile::PanelInset:
                case MaterialRenderProfile::PanelService:
                case MaterialRenderProfile::PlatformTop:
                    if (material.transparent)
                    {
                        throw std::runtime_error(owner + " requires opaque authoring for this render profile");
                    }
                    if (!material.bevel_enabled)
                    {
                        throw std::runtime_error(owner + " requires bevel_enabled=true for this render profile");
                    }
                    break;
                case MaterialRenderProfile::Structural:
                case MaterialRenderProfile::Divider:
                    if (material.transparent)
                    {
                        throw std::runtime_error(owner + " requires opaque authoring for this render profile");
                    }
                    if (material.bevel_enabled)
                    {
                        throw std::runtime_error(owner + " disallows bevel_enabled for this render profile");
                    }
                    break;
                case MaterialRenderProfile::LightStrip:
                    if (material.transparent)
                    {
                        throw std::runtime_error(owner + " requires opaque authoring for profile=light_strip");
                    }
                    if (material.bevel_enabled)
                    {
                        throw std::runtime_error(owner + " disallows bevel_enabled for profile=light_strip");
                    }
                    if (material.emissive_strength <= 0.0F)
                    {
                        throw std::runtime_error(owner + " requires emissive_strength > 0 for profile=light_strip");
                    }
                    if (material.emissive_pulse_speed <= 0.0F)
                    {
                        throw std::runtime_error(owner + " requires emissive_pulse_speed > 0 for profile=light_strip");
                    }
                    if (material.ambient_bias < 1.0F)
                    {
                        throw std::runtime_error(owner + " requires ambient_bias >= 1.0 for profile=light_strip");
                    }
                    break;
                case MaterialRenderProfile::Default:
                    break;
            }
        }

        [[nodiscard]] auto make_material(
            const RelayCore::SurfaceMaterial material,
            const glm::vec3& base_color,
            const MaterialRenderProfile profile,
            const bool transparent,
            const bool bevel_enabled,
            const float atlas_world_scale,
            const float atlas_mix = 0.58F,
            const float ambient_bias = 1.0F,
            const float bounce_mix = 0.55F,
            const float emissive_strength = 0.0F,
            const float emissive_pulse_speed = 0.0F
        ) -> MaterialAuthoringConfig
        {
            return MaterialAuthoringConfig{
                .material = material,
                .canonical_name = std::string{RelayCore::canonical_surface_material_name(material)},
                .base_color = base_color,
                .render_profile = profile,
                .transparent = transparent,
                .bevel_enabled = bevel_enabled,
                .atlas_world_scale = atlas_world_scale,
                .atlas_mix = atlas_mix,
                .ambient_bias = ambient_bias,
                .bounce_mix = bounce_mix,
                .emissive_strength = emissive_strength,
                .emissive_pulse_speed = emissive_pulse_speed,
            };
        }

        void validate_render_light_contract(const RenderLightAuthoringConfig& config)
        {
            validate_unit_color(config.default_color, "Render light authoring contract", "default_color");
            if (config.default_radius <= 0.0F || config.min_radius <= 0.0F || config.max_radius < config.min_radius)
            {
                throw std::runtime_error("Render light authoring contract requires positive radius bounds");
            }
            if (config.default_intensity < 0.0F || config.min_intensity < 0.0F
                || config.max_intensity < config.min_intensity)
            {
                throw std::runtime_error("Render light authoring contract requires non-negative intensity bounds");
            }
            if (config.default_radius < config.min_radius || config.default_radius > config.max_radius)
            {
                throw std::runtime_error(
                    "Render light authoring contract requires default_radius inside [min_radius, max_radius]"
                );
            }
            if (config.default_intensity < config.min_intensity || config.default_intensity > config.max_intensity)
            {
                throw std::runtime_error(
                    "Render light authoring contract requires default_intensity inside [min_intensity, max_intensity]"
                );
            }
        }

        void validate_lighting_contract(const LightingAuthoringConfig& config)
        {
            validate_unit_color(config.cool_ambient_color, "Lighting authoring contract", "cool_ambient_color");
            validate_unit_color(config.warm_bounce_color, "Lighting authoring contract", "warm_bounce_color");
            validate_unit_color(config.bounce_neutral_color, "Lighting authoring contract", "bounce_neutral_color");
            if (config.ambient_min < 0.0F || config.ambient_max < config.ambient_min)
            {
                throw std::runtime_error("Lighting authoring contract requires ambient_min <= ambient_max");
            }
            if (config.ambient_floor < config.ambient_min || config.ambient_floor > config.ambient_max
                || config.ambient_zenith < config.ambient_min || config.ambient_zenith > config.ambient_max)
            {
                throw std::runtime_error(
                    "Lighting authoring contract requires ambient_floor and ambient_zenith inside [ambient_min, "
                    "ambient_max]"
                );
            }
            if (config.ambient_floor > config.ambient_zenith)
            {
                throw std::runtime_error("Lighting authoring contract requires ambient_floor <= ambient_zenith");
            }
            if (config.warm_bounce_floor > config.warm_bounce_underside
                || config.bounce_floor > config.bounce_underside)
            {
                throw std::runtime_error(
                    "Lighting authoring contract requires underside bounce to be at least as strong as floor bounce"
                );
            }
            if (config.local_light_wrap < 0.0F || config.local_light_wrap > 1.0F || config.local_light_fill < 0.0F
                || config.local_light_fill > 1.0F)
            {
                throw std::runtime_error("Lighting authoring contract requires local_light_wrap/fill in [0, 1]");
            }
            if (config.local_light_gain <= 0.0F || config.local_light_gain > 4.0F)
            {
                throw std::runtime_error("Lighting authoring contract requires local_light_gain in (0, 4]");
            }
            if (config.bounce_darkness < 0.0F || config.bounce_darkness > 1.0F || config.cavity_ambient_darken < 0.0F
                || config.cavity_ambient_darken > 1.0F)
            {
                throw std::runtime_error(
                    "Lighting authoring contract requires bounce_darkness and cavity_ambient_darken in [0, 1]"
                );
            }
            if (config.cavity_broad_strength < 0.0F || config.cavity_broad_strength > 1.0F
                || config.cavity_micro_strength < 0.0F || config.cavity_micro_strength > 1.0F
                || config.cavity_underside_strength < 0.0F || config.cavity_underside_strength > 1.0F)
            {
                throw std::runtime_error("Lighting authoring contract requires cavity strengths in [0, 1]");
            }
        }
    } // namespace

    auto default_render_authoring_config() -> RenderAuthoringConfig
    {
        RenderAuthoringConfig config{};
        config.source_path = core::asset_path("config/render_authoring.toml");
        config.materials = {{
            make_material(
                RelayCore::SurfaceMaterial::Concrete,
                {0.62F, 0.64F, 0.67F},
                MaterialRenderProfile::Default,
                false,
                false,
                1.0F
            ),
            make_material(
                RelayCore::SurfaceMaterial::Metal,
                {0.60F, 0.67F, 0.74F},
                MaterialRenderProfile::Default,
                false,
                false,
                1.0F
            ),
            make_material(
                RelayCore::SurfaceMaterial::Tile,
                {0.73F, 0.76F, 0.78F},
                MaterialRenderProfile::Default,
                false,
                false,
                1.0F
            ),
            make_material(
                RelayCore::SurfaceMaterial::Glass,
                {0.45F, 0.72F, 0.84F},
                MaterialRenderProfile::Glass,
                true,
                false,
                1.8F,
                0.0F,
                0.92F,
                0.15F,
                0.03F,
                0.0F
            ),
            make_material(
                RelayCore::SurfaceMaterial::Hazard,
                {0.92F, 0.78F, 0.18F},
                MaterialRenderProfile::Default,
                false,
                false,
                1.0F
            ),
            make_material(
                RelayCore::SurfaceMaterial::Floor,
                {0.29F, 0.30F, 0.32F},
                MaterialRenderProfile::FloorGrid,
                false,
                true,
                1.35F
            ),
            make_material(
                RelayCore::SurfaceMaterial::Ceiling,
                {0.18F, 0.20F, 0.22F},
                MaterialRenderProfile::CeilingGrid,
                false,
                true,
                1.15F
            ),
            make_material(
                RelayCore::SurfaceMaterial::Wall,
                {0.43F, 0.46F, 0.50F},
                MaterialRenderProfile::Panel,
                false,
                true,
                1.6F
            ),
            make_material(
                RelayCore::SurfaceMaterial::WallPanel,
                {0.58F, 0.61F, 0.66F},
                MaterialRenderProfile::PanelInset,
                false,
                true,
                1.6F
            ),
            make_material(
                RelayCore::SurfaceMaterial::Corridor,
                {0.28F, 0.34F, 0.40F},
                MaterialRenderProfile::PanelService,
                false,
                true,
                1.6F
            ),
            make_material(
                RelayCore::SurfaceMaterial::Platform,
                {0.37F, 0.38F, 0.43F},
                MaterialRenderProfile::Panel,
                false,
                true,
                1.35F
            ),
            make_material(
                RelayCore::SurfaceMaterial::PlatformTop,
                {0.72F, 0.63F, 0.24F},
                MaterialRenderProfile::PlatformTop,
                false,
                true,
                1.35F
            ),
            make_material(
                RelayCore::SurfaceMaterial::PlatformTop2,
                {0.82F, 0.43F, 0.18F},
                MaterialRenderProfile::PlatformTop,
                false,
                true,
                1.35F
            ),
            make_material(
                RelayCore::SurfaceMaterial::Pillar,
                {0.48F, 0.55F, 0.60F},
                MaterialRenderProfile::Default,
                false,
                false,
                1.0F
            ),
            make_material(
                RelayCore::SurfaceMaterial::Cover,
                {0.25F, 0.38F, 0.31F},
                MaterialRenderProfile::Default,
                false,
                false,
                1.0F
            ),
            make_material(
                RelayCore::SurfaceMaterial::Block,
                {0.46F, 0.39F, 0.31F},
                MaterialRenderProfile::Default,
                false,
                false,
                1.0F
            ),
            make_material(
                RelayCore::SurfaceMaterial::Beam,
                {0.64F, 0.39F, 0.16F},
                MaterialRenderProfile::Structural,
                false,
                false,
                0.75F
            ),
            make_material(
                RelayCore::SurfaceMaterial::Trim,
                {0.26F, 0.29F, 0.33F},
                MaterialRenderProfile::Structural,
                false,
                false,
                0.75F
            ),
            make_material(
                RelayCore::SurfaceMaterial::Divider,
                {0.24F, 0.63F, 0.72F},
                MaterialRenderProfile::Divider,
                false,
                false,
                0.75F
            ),
            make_material(
                RelayCore::SurfaceMaterial::LightStrip,
                {0.74F, 0.82F, 0.66F},
                MaterialRenderProfile::LightStrip,
                false,
                false,
                0.75F,
                0.58F,
                1.04F,
                0.65F,
                0.24F,
                2.4F
            ),
            make_material(
                RelayCore::SurfaceMaterial::Crate,
                {0.48F, 0.33F, 0.18F},
                MaterialRenderProfile::Default,
                false,
                false,
                1.0F
            ),
            make_material(
                RelayCore::SurfaceMaterial::Post,
                {0.73F, 0.75F, 0.79F},
                MaterialRenderProfile::Default,
                false,
                false,
                1.0F
            ),
        }};
        for (const RelayCore::SurfaceMaterial material : RelayCore::all_surface_materials())
        {
            validate_material_entry(config.materials[static_cast<std::size_t>(material)]);
        }
        validate_lighting_contract(config.lighting);
        validate_render_light_contract(config.render_light);

        return config;
    }

    auto load_render_authoring_config(const std::filesystem::path& config_path) -> RenderAuthoringConfig
    {
        RenderAuthoringConfig config = default_render_authoring_config();
        config.source_path = config_path;

        if (!std::filesystem::exists(config_path))
        {
            throw std::runtime_error("Render authoring config not found: " + config_path.string());
        }

        try
        {
            const toml::table document = toml::parse_file(config_path.string());

            const auto lighting = document["lighting"];
            config.lighting.cool_ambient_color =
                read_vec3(lighting, "cool_ambient_color", config.lighting.cool_ambient_color);
            assign_if_present(lighting, "cool_ambient_floor", config.lighting.cool_ambient_floor);
            assign_if_present(lighting, "cool_ambient_zenith", config.lighting.cool_ambient_zenith);
            config.lighting.warm_bounce_color =
                read_vec3(lighting, "warm_bounce_color", config.lighting.warm_bounce_color);
            assign_if_present(lighting, "warm_bounce_floor", config.lighting.warm_bounce_floor);
            assign_if_present(lighting, "warm_bounce_underside", config.lighting.warm_bounce_underside);
            config.lighting.bounce_neutral_color =
                read_vec3(lighting, "bounce_neutral_color", config.lighting.bounce_neutral_color);
            assign_if_present(lighting, "bounce_floor", config.lighting.bounce_floor);
            assign_if_present(lighting, "bounce_underside", config.lighting.bounce_underside);
            assign_if_present(lighting, "bounce_darkness", config.lighting.bounce_darkness);
            assign_if_present(lighting, "ambient_floor", config.lighting.ambient_floor);
            assign_if_present(lighting, "ambient_zenith", config.lighting.ambient_zenith);
            assign_if_present(lighting, "ambient_min", config.lighting.ambient_min);
            assign_if_present(lighting, "ambient_max", config.lighting.ambient_max);
            assign_if_present(lighting, "cavity_broad_strength", config.lighting.cavity_broad_strength);
            assign_if_present(lighting, "cavity_micro_strength", config.lighting.cavity_micro_strength);
            assign_if_present(lighting, "cavity_underside_strength", config.lighting.cavity_underside_strength);
            assign_if_present(lighting, "cavity_ambient_darken", config.lighting.cavity_ambient_darken);
            assign_if_present(lighting, "local_light_wrap", config.lighting.local_light_wrap);
            assign_if_present(lighting, "local_light_fill", config.lighting.local_light_fill);
            assign_if_present(lighting, "local_light_gain", config.lighting.local_light_gain);

            const auto render_light = document["render_light"];
            config.render_light.default_color =
                read_vec3(render_light, "default_color", config.render_light.default_color);
            assign_if_present(render_light, "default_intensity", config.render_light.default_intensity);
            assign_if_present(render_light, "default_radius", config.render_light.default_radius);
            assign_if_present(render_light, "min_radius", config.render_light.min_radius);
            assign_if_present(render_light, "max_radius", config.render_light.max_radius);
            assign_if_present(render_light, "min_intensity", config.render_light.min_intensity);
            assign_if_present(render_light, "max_intensity", config.render_light.max_intensity);

            std::array<bool, RelayCore::surface_material_count()> seen_materials{};
            if (const toml::array* materials = document["materials"].as_array(); materials != nullptr)
            {
                for (const toml::node& entry_node : *materials)
                {
                    const toml::table* entry = entry_node.as_table();
                    if (entry == nullptr)
                    {
                        throw std::runtime_error("Render authoring 'materials' entries must be tables");
                    }

                    const toml::node_view<const toml::node> material_entry{*entry};
                    const std::optional<std::string> name = material_entry["name"].value<std::string>();
                    if (!name.has_value() || name->empty())
                    {
                        throw std::runtime_error("Render authoring material entry requires a non-empty name");
                    }

                    const std::optional<RelayCore::SurfaceMaterial> material =
                        RelayCore::try_parse_surface_material_name(*name);
                    if (!material.has_value())
                    {
                        throw std::runtime_error("Unknown render authoring material name: " + *name);
                    }

                    const std::size_t material_index = static_cast<std::size_t>(*material);
                    if (seen_materials[material_index])
                    {
                        throw std::runtime_error("Duplicate render authoring material entry: " + *name);
                    }
                    seen_materials[material_index] = true;

                    MaterialAuthoringConfig& authored_material = config.materials[material_index];
                    authored_material.material = *material;
                    authored_material.canonical_name =
                        std::string{RelayCore::canonical_surface_material_name(*material)};
                    authored_material.base_color =
                        read_vec3(material_entry, "base_color", authored_material.base_color);
                    assign_if_present(material_entry, "transparent", authored_material.transparent);
                    assign_if_present(material_entry, "bevel_enabled", authored_material.bevel_enabled);
                    assign_if_present(material_entry, "atlas_world_scale", authored_material.atlas_world_scale);
                    assign_if_present(material_entry, "atlas_mix", authored_material.atlas_mix);
                    assign_if_present(material_entry, "ambient_bias", authored_material.ambient_bias);
                    assign_if_present(material_entry, "bounce_mix", authored_material.bounce_mix);
                    assign_if_present(material_entry, "emissive_strength", authored_material.emissive_strength);
                    assign_if_present(material_entry, "emissive_pulse_speed", authored_material.emissive_pulse_speed);

                    if (const std::optional<std::string> profile = material_entry["profile"].value<std::string>();
                        profile.has_value())
                    {
                        authored_material.render_profile = parse_profile_name(*profile);
                    }
                }
            }

            for (const RelayCore::SurfaceMaterial material : RelayCore::all_surface_materials())
            {
                validate_material_entry(config.materials[static_cast<std::size_t>(material)]);
            }
            validate_lighting_contract(config.lighting);
            validate_render_light_contract(config.render_light);

            log::info("Loaded render authoring config: {}", config_path.string());
        } catch (const toml::parse_error& error)
        {
            log::error(
                "Failed to parse render authoring config {} at line {}, column {}: {}",
                config_path.string(),
                error.source().begin.line,
                error.source().begin.column,
                error.description()
            );
            throw;
        }

        return config;
    }

    auto load_default_render_authoring_config() -> RenderAuthoringConfig
    {
        return load_render_authoring_config(core::asset_path("config/render_authoring.toml"));
    }

    auto material_authoring(const RenderAuthoringConfig& config, const RelayCore::SurfaceMaterial material)
        -> const MaterialAuthoringConfig&
    {
        return config.materials[static_cast<std::size_t>(material)];
    }

    auto material_render_profile_name(const MaterialRenderProfile profile) noexcept -> std::string_view
    {
        switch (profile)
        {
            case MaterialRenderProfile::Default:
                return "default";
            case MaterialRenderProfile::Glass:
                return "glass";
            case MaterialRenderProfile::FloorGrid:
                return "floor_grid";
            case MaterialRenderProfile::CeilingGrid:
                return "ceiling_grid";
            case MaterialRenderProfile::Panel:
                return "panel";
            case MaterialRenderProfile::PanelInset:
                return "panel_inset";
            case MaterialRenderProfile::PanelService:
                return "panel_service";
            case MaterialRenderProfile::PlatformTop:
                return "platform_top";
            case MaterialRenderProfile::Structural:
                return "structural";
            case MaterialRenderProfile::Divider:
                return "divider";
            case MaterialRenderProfile::LightStrip:
                return "light_strip";
        }

        return "unknown";
    }
} // namespace RelayCore::render

#include "relaycore/render/pipeline_authoring_overlay.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <toml++/toml.hpp>
#include <unordered_set>
#include <utility>

namespace RelayCore::render
{
    namespace
    {
        [[noreturn]] void
        invalid_overlay(const std::filesystem::path& path, const std::string_view owner, const std::string_view reason)
        {
            throw std::runtime_error(
                "Invalid pipeline authoring overlay '" + path.string() + "': " + std::string{owner} + " "
                + std::string{reason}
            );
        }

        [[nodiscard]] auto normalized_level_key(const std::filesystem::path& path) -> std::string
        {
            return path.lexically_normal().generic_string();
        }

        void ensure_optional_table(
            const toml::node_view<const toml::node>& node,
            const std::filesystem::path& path,
            const std::string_view owner
        )
        {
            if (node && !node.is_table())
            {
                invalid_overlay(path, owner, "must be a TOML table");
            }
        }

        [[nodiscard]] auto read_optional_float(
            const toml::node_view<const toml::node>& table,
            const std::filesystem::path& path,
            const std::string_view owner,
            const std::string_view key
        ) -> std::optional<float>
        {
            const toml::node_view<const toml::node> node = table[key];
            if (!node)
            {
                return std::nullopt;
            }

            const std::optional<double> value = node.value<double>();
            if (!value.has_value() || !std::isfinite(*value)
                || *value < -static_cast<double>(std::numeric_limits<float>::max())
                || *value > static_cast<double>(std::numeric_limits<float>::max()))
            {
                invalid_overlay(path, std::string{owner} + "." + std::string{key}, "must be a finite float");
            }

            return static_cast<float>(*value);
        }

        [[nodiscard]] auto read_optional_bool(
            const toml::node_view<const toml::node>& table,
            const std::filesystem::path& path,
            const std::string_view owner,
            const std::string_view key
        ) -> std::optional<bool>
        {
            const toml::node_view<const toml::node> node = table[key];
            if (!node)
            {
                return std::nullopt;
            }

            const std::optional<bool> value = node.value<bool>();
            if (!value.has_value())
            {
                invalid_overlay(path, std::string{owner} + "." + std::string{key}, "must be a boolean");
            }

            return *value;
        }

        [[nodiscard]] auto read_required_string(
            const toml::node_view<const toml::node>& table,
            const std::filesystem::path& path,
            const std::string_view owner,
            const std::string_view key
        ) -> std::string
        {
            const std::optional<std::string> value = table[key].value<std::string>();
            if (!value.has_value() || value->empty())
            {
                invalid_overlay(path, std::string{owner} + "." + std::string{key}, "requires a non-empty string");
            }

            return *value;
        }

        [[nodiscard]] auto read_optional_vec3(
            const toml::node_view<const toml::node>& table,
            const std::filesystem::path& path,
            const std::string_view owner,
            const std::string_view key
        ) -> std::optional<glm::vec3>
        {
            const toml::node_view<const toml::node> node = table[key];
            if (!node)
            {
                return std::nullopt;
            }

            const toml::array* values = node.as_array();
            if (values == nullptr || values->size() != 3)
            {
                invalid_overlay(
                    path,
                    std::string{owner} + "." + std::string{key},
                    "must be an array with exactly three numeric elements"
                );
            }

            std::array<float, 3> result{};
            for (std::size_t index = 0; index < result.size(); ++index)
            {
                const std::optional<double> component = (*values)[index].value<double>();
                if (!component.has_value() || !std::isfinite(*component)
                    || *component < -static_cast<double>(std::numeric_limits<float>::max())
                    || *component > static_cast<double>(std::numeric_limits<float>::max()))
                {
                    invalid_overlay(
                        path,
                        std::string{owner} + "." + std::string{key},
                        "must contain finite float components"
                    );
                }
                result[index] = static_cast<float>(*component);
            }

            return glm::vec3{result[0], result[1], result[2]};
        }

        void
        validate_unit_color(const glm::vec3& color, const std::filesystem::path& path, const std::string_view owner)
        {
            if (color.x < 0.0F || color.y < 0.0F || color.z < 0.0F || color.x > 1.0F || color.y > 1.0F
                || color.z > 1.0F)
            {
                invalid_overlay(path, owner, "requires components in [0, 1]");
            }
        }

        void validate_non_negative_vec3(
            const glm::vec3& value,
            const std::filesystem::path& path,
            const std::string_view owner
        )
        {
            if (value.x < 0.0F || value.y < 0.0F || value.z < 0.0F)
            {
                invalid_overlay(path, owner, "requires non-negative components");
            }
        }

        void validate_renderer_overrides(
            const RendererAuthoringOverrides& overrides,
            const RendererConfig& base,
            const std::filesystem::path& path
        )
        {
            RendererConfig candidate = base;
            apply_renderer_authoring_overrides(candidate, overrides);

            if (candidate.indirect_bounce_strength < 0.0F || candidate.indirect_bounce_strength > 4.0F)
            {
                invalid_overlay(path, "global.indirect_bounce_strength", "must be in [0, 4]");
            }
            validate_unit_color(candidate.fog_color, path, "global.fog_color");
            if (candidate.fog_start < 0.0F)
            {
                invalid_overlay(path, "global.fog_start", "must be non-negative");
            }
            if (candidate.fog_end <= candidate.fog_start)
            {
                invalid_overlay(path, "global.fog_end", "must be greater than fog_start");
            }
            validate_unit_color(candidate.shadow_light_color, path, "global.shadow_light_color");
            if (candidate.shadow_light_intensity < 0.0F || candidate.shadow_light_intensity > 8.0F)
            {
                invalid_overlay(path, "global.shadow_light_intensity", "must be in [0, 8]");
            }
            if (candidate.shadow_strength < 0.0F || candidate.shadow_strength > 1.0F)
            {
                invalid_overlay(path, "global.shadow_strength", "must be in [0, 1]");
            }
            if (candidate.shadow_contact_strength < 0.0F || candidate.shadow_contact_strength > 1.0F)
            {
                invalid_overlay(path, "global.shadow_contact_strength", "must be in [0, 1]");
            }
        }

        void validate_material_overrides(
            const MaterialAuthoringOverrides& overrides,
            const std::filesystem::path& path,
            const std::string_view owner
        )
        {
            if (overrides.empty())
            {
                invalid_overlay(path, owner, "must override at least one field");
            }
            if (overrides.base_color.has_value())
            {
                validate_unit_color(*overrides.base_color, path, std::string{owner} + ".base_color");
            }
            if (overrides.ambient_bias.has_value()
                && (*overrides.ambient_bias < 0.5F || *overrides.ambient_bias > 1.5F))
            {
                invalid_overlay(path, std::string{owner} + ".ambient_bias", "must be in [0.5, 1.5]");
            }
            if (overrides.bounce_mix.has_value() && (*overrides.bounce_mix < 0.0F || *overrides.bounce_mix > 1.0F))
            {
                invalid_overlay(path, std::string{owner} + ".bounce_mix", "must be in [0, 1]");
            }
        }

        void validate_solid_overrides(
            const SolidVisualOverrides& overrides,
            const std::filesystem::path& path,
            const std::string_view owner
        )
        {
            if (overrides.empty())
            {
                invalid_overlay(path, owner, "must override at least one field");
            }
            if (overrides.tint.has_value())
            {
                validate_non_negative_vec3(*overrides.tint, path, std::string{owner} + ".tint");
            }
            if (overrides.brightness.has_value() && *overrides.brightness < 0.0F)
            {
                invalid_overlay(path, std::string{owner} + ".brightness", "must be non-negative");
            }
        }

        void validate_render_light_overrides(
            const RenderLightVisualOverrides& overrides,
            const std::filesystem::path& path,
            const std::string_view owner
        )
        {
            if (overrides.empty())
            {
                invalid_overlay(path, owner, "must override at least one field");
            }
            if (overrides.color.has_value())
            {
                validate_unit_color(*overrides.color, path, std::string{owner} + ".color");
            }
            if (overrides.radius.has_value() && *overrides.radius <= 0.0F)
            {
                invalid_overlay(path, std::string{owner} + ".radius", "must be greater than zero");
            }
            if (overrides.intensity.has_value() && *overrides.intensity < 0.0F)
            {
                invalid_overlay(path, std::string{owner} + ".intensity", "must be non-negative");
            }
        }

        [[nodiscard]] auto
        parse_renderer_overrides(const toml::node_view<const toml::node>& table, const std::filesystem::path& path)
            -> RendererAuthoringOverrides
        {
            RendererAuthoringOverrides overrides{};
            overrides.indirect_bounce_strength = read_optional_float(table, path, "global", "indirect_bounce_strength");
            overrides.fog_enabled = read_optional_bool(table, path, "global", "fog_enabled");
            overrides.fog_color = read_optional_vec3(table, path, "global", "fog_color");
            overrides.fog_start = read_optional_float(table, path, "global", "fog_start");
            overrides.fog_end = read_optional_float(table, path, "global", "fog_end");
            overrides.shadow_light_color = read_optional_vec3(table, path, "global", "shadow_light_color");
            overrides.shadow_light_intensity = read_optional_float(table, path, "global", "shadow_light_intensity");
            overrides.shadow_strength = read_optional_float(table, path, "global", "shadow_strength");
            overrides.shadow_contact_strength = read_optional_float(table, path, "global", "shadow_contact_strength");
            return overrides;
        }

        [[nodiscard]] auto parse_material_overrides(
            const toml::table& table,
            const std::filesystem::path& path,
            const std::size_t entry_index
        ) -> MaterialAuthoringOverrides
        {
            const std::string owner = "materials[" + std::to_string(entry_index) + "]";
            const toml::node_view<const toml::node> view{table};
            const std::string name = read_required_string(view, path, owner, "name");
            const std::optional<RelayCore::SurfaceMaterial> material = RelayCore::try_parse_surface_material_name(name);
            if (!material.has_value())
            {
                invalid_overlay(path, owner + ".name", "references unknown material '" + name + "'");
            }

            MaterialAuthoringOverrides overrides{};
            overrides.material = *material;
            overrides.canonical_name = std::string{RelayCore::canonical_surface_material_name(*material)};
            overrides.base_color = read_optional_vec3(view, path, owner, "base_color");
            overrides.ambient_bias = read_optional_float(view, path, owner, "ambient_bias");
            overrides.bounce_mix = read_optional_float(view, path, owner, "bounce_mix");
            validate_material_overrides(overrides, path, owner);
            return overrides;
        }

        [[nodiscard]] auto parse_solid_overrides(
            const toml::table& table,
            const std::filesystem::path& path,
            const std::string_view level_owner,
            const std::size_t entry_index
        ) -> SolidVisualOverrides
        {
            const std::string owner = std::string{level_owner} + ".solids[" + std::to_string(entry_index) + "]";
            const toml::node_view<const toml::node> view{table};

            SolidVisualOverrides overrides{};
            overrides.authoring_id = read_required_string(view, path, owner, "id");
            overrides.tint = read_optional_vec3(view, path, owner, "tint");
            overrides.brightness = read_optional_float(view, path, owner, "brightness");
            validate_solid_overrides(overrides, path, owner);
            return overrides;
        }

        [[nodiscard]] auto parse_render_light_overrides(
            const toml::table& table,
            const std::filesystem::path& path,
            const std::string_view level_owner,
            const std::size_t entry_index
        ) -> RenderLightVisualOverrides
        {
            const std::string owner = std::string{level_owner} + ".render_lights[" + std::to_string(entry_index) + "]";
            const toml::node_view<const toml::node> view{table};

            RenderLightVisualOverrides overrides{};
            overrides.authoring_id = read_required_string(view, path, owner, "id");
            overrides.color = read_optional_vec3(view, path, owner, "color");
            overrides.radius = read_optional_float(view, path, owner, "radius");
            overrides.intensity = read_optional_float(view, path, owner, "intensity");
            validate_render_light_overrides(overrides, path, owner);
            return overrides;
        }

        [[nodiscard]] auto parse_level_overrides(
            const toml::table& table,
            const std::filesystem::path& path,
            const std::size_t level_index
        ) -> LevelVisualOverrides
        {
            const std::string owner = "levels[" + std::to_string(level_index) + "]";
            const toml::node_view<const toml::node> view{table};

            LevelVisualOverrides overrides{};
            overrides.level_path = read_required_string(view, path, owner, "path");
            if (overrides.level_path.is_absolute())
            {
                invalid_overlay(path, owner + ".path", "must be a relative asset path");
            }
            overrides.level_path = overrides.level_path.lexically_normal();
            if (overrides.level_path.empty() || overrides.level_path == ".")
            {
                invalid_overlay(path, owner + ".path", "must identify a level asset");
            }

            const toml::node_view<const toml::node> solid_node = view["solids"];
            if (solid_node && !solid_node.is_array())
            {
                invalid_overlay(path, owner + ".solids", "must be an array of tables");
            }
            if (const toml::array* solids = solid_node.as_array(); solids != nullptr)
            {
                std::unordered_set<std::string> seen_ids{};
                std::size_t index = 0;
                for (const toml::node& node : *solids)
                {
                    const toml::table* entry = node.as_table();
                    if (entry == nullptr)
                    {
                        invalid_overlay(path, owner + ".solids", "must contain only tables");
                    }

                    SolidVisualOverrides parsed = parse_solid_overrides(*entry, path, owner, index);
                    if (!seen_ids.insert(parsed.authoring_id).second)
                    {
                        invalid_overlay(path, owner + ".solids", "contains duplicate id '" + parsed.authoring_id + "'");
                    }
                    overrides.solids.push_back(std::move(parsed));
                    ++index;
                }
            }

            const toml::node_view<const toml::node> light_node = view["render_lights"];
            if (light_node && !light_node.is_array())
            {
                invalid_overlay(path, owner + ".render_lights", "must be an array of tables");
            }
            if (const toml::array* lights = light_node.as_array(); lights != nullptr)
            {
                std::unordered_set<std::string> seen_ids{};
                std::size_t index = 0;
                for (const toml::node& node : *lights)
                {
                    const toml::table* entry = node.as_table();
                    if (entry == nullptr)
                    {
                        invalid_overlay(path, owner + ".render_lights", "must contain only tables");
                    }

                    RenderLightVisualOverrides parsed = parse_render_light_overrides(*entry, path, owner, index);
                    if (!seen_ids.insert(parsed.authoring_id).second)
                    {
                        invalid_overlay(
                            path,
                            owner + ".render_lights",
                            "contains duplicate id '" + parsed.authoring_id + "'"
                        );
                    }
                    overrides.render_lights.push_back(std::move(parsed));
                    ++index;
                }
            }

            if (overrides.empty())
            {
                invalid_overlay(path, owner, "must contain at least one solid or render-light override");
            }
            return overrides;
        }
    } // namespace

    auto RendererAuthoringOverrides::empty() const noexcept -> bool
    {
        return !indirect_bounce_strength.has_value() && !fog_enabled.has_value() && !fog_color.has_value()
               && !fog_start.has_value() && !fog_end.has_value() && !shadow_light_color.has_value()
               && !shadow_light_intensity.has_value() && !shadow_strength.has_value()
               && !shadow_contact_strength.has_value();
    }

    auto MaterialAuthoringOverrides::empty() const noexcept -> bool
    {
        return !base_color.has_value() && !ambient_bias.has_value() && !bounce_mix.has_value();
    }

    auto SolidVisualOverrides::empty() const noexcept -> bool
    {
        return !tint.has_value() && !brightness.has_value();
    }

    auto RenderLightVisualOverrides::empty() const noexcept -> bool
    {
        return !color.has_value() && !radius.has_value() && !intensity.has_value();
    }

    auto LevelVisualOverrides::empty() const noexcept -> bool
    {
        return solids.empty() && render_lights.empty();
    }

    auto PipelineAuthoringOverlay::empty() const noexcept -> bool
    {
        return renderer.empty() && materials.empty() && levels.empty();
    }

    auto load_pipeline_authoring_overlay(const std::filesystem::path& overlay_path) -> PipelineAuthoringOverlay
    {
        if (!std::filesystem::exists(overlay_path))
        {
            throw std::runtime_error("Pipeline authoring overlay not found: " + overlay_path.string());
        }

        try
        {
            const toml::table document = toml::parse_file(overlay_path.string());

            PipelineAuthoringOverlay overlay{};
            overlay.source_path = overlay_path;

            const toml::node_view<const toml::node> global = document["global"];
            ensure_optional_table(global, overlay_path, "global");
            if (global)
            {
                overlay.renderer = parse_renderer_overrides(global, overlay_path);
                validate_renderer_overrides(overlay.renderer, RendererConfig{}, overlay_path);
            }

            const toml::node_view<const toml::node> materials_node = document["materials"];
            if (materials_node && !materials_node.is_array())
            {
                invalid_overlay(overlay_path, "materials", "must be an array of tables");
            }
            if (const toml::array* materials = materials_node.as_array(); materials != nullptr)
            {
                std::array<bool, RelayCore::surface_material_count()> seen_materials{};
                std::size_t index = 0;
                for (const toml::node& node : *materials)
                {
                    const toml::table* entry = node.as_table();
                    if (entry == nullptr)
                    {
                        invalid_overlay(overlay_path, "materials", "must contain only tables");
                    }

                    MaterialAuthoringOverrides parsed = parse_material_overrides(*entry, overlay_path, index);
                    const std::size_t material_index = static_cast<std::size_t>(parsed.material);
                    if (seen_materials[material_index])
                    {
                        invalid_overlay(
                            overlay_path,
                            "materials",
                            "contains duplicate material '" + parsed.canonical_name + "'"
                        );
                    }
                    seen_materials[material_index] = true;
                    overlay.materials.push_back(std::move(parsed));
                    ++index;
                }
            }

            const toml::node_view<const toml::node> levels_node = document["levels"];
            if (levels_node && !levels_node.is_array())
            {
                invalid_overlay(overlay_path, "levels", "must be an array of tables");
            }
            if (const toml::array* levels = levels_node.as_array(); levels != nullptr)
            {
                std::unordered_set<std::string> seen_levels{};
                std::size_t index = 0;
                for (const toml::node& node : *levels)
                {
                    const toml::table* entry = node.as_table();
                    if (entry == nullptr)
                    {
                        invalid_overlay(overlay_path, "levels", "must contain only tables");
                    }

                    LevelVisualOverrides parsed = parse_level_overrides(*entry, overlay_path, index);
                    const std::string key = normalized_level_key(parsed.level_path);
                    if (!seen_levels.insert(key).second)
                    {
                        invalid_overlay(overlay_path, "levels", "contains duplicate path '" + key + "'");
                    }
                    overlay.levels.push_back(std::move(parsed));
                    ++index;
                }
            }

            if (overlay.empty())
            {
                invalid_overlay(overlay_path, "document", "must contain at least one override");
            }
            return overlay;
        } catch (const toml::parse_error&)
        {
            throw;
        }
    }

    void apply_renderer_authoring_overrides(RendererConfig& config, const RendererAuthoringOverrides& overrides)
    {
        if (overrides.indirect_bounce_strength.has_value())
            config.indirect_bounce_strength = *overrides.indirect_bounce_strength;
        if (overrides.fog_enabled.has_value())
            config.fog_enabled = *overrides.fog_enabled;
        if (overrides.fog_color.has_value())
            config.fog_color = *overrides.fog_color;
        if (overrides.fog_start.has_value())
            config.fog_start = *overrides.fog_start;
        if (overrides.fog_end.has_value())
            config.fog_end = *overrides.fog_end;
        if (overrides.shadow_light_color.has_value())
            config.shadow_light_color = *overrides.shadow_light_color;
        if (overrides.shadow_light_intensity.has_value())
            config.shadow_light_intensity = *overrides.shadow_light_intensity;
        if (overrides.shadow_strength.has_value())
            config.shadow_strength = *overrides.shadow_strength;
        if (overrides.shadow_contact_strength.has_value())
            config.shadow_contact_strength = *overrides.shadow_contact_strength;
    }

    void apply_material_authoring_overrides(
        RenderAuthoringConfig& config,
        const std::vector<MaterialAuthoringOverrides>& overrides
    )
    {
        for (const MaterialAuthoringOverrides& entry : overrides)
        {
            MaterialAuthoringConfig& material = config.materials[static_cast<std::size_t>(entry.material)];
            if (entry.base_color.has_value())
                material.base_color = *entry.base_color;
            if (entry.ambient_bias.has_value())
                material.ambient_bias = *entry.ambient_bias;
            if (entry.bounce_mix.has_value())
                material.bounce_mix = *entry.bounce_mix;
        }
    }

    void validate_level_authoring_overlay_targets(
        const LevelVisualOverrides& overrides,
        const LevelAuthoringIdentityIndex& identities
    )
    {
        const std::string level = normalized_level_key(overrides.level_path);
        for (const SolidVisualOverrides& solid : overrides.solids)
        {
            if (!identities.solid_ids.contains(solid.authoring_id))
            {
                throw std::runtime_error(
                    "Pipeline authoring overlay for level '" + level + "' references unknown or stale solid id '"
                    + solid.authoring_id + "'"
                );
            }
        }

        for (const RenderLightVisualOverrides& light : overrides.render_lights)
        {
            if (!identities.render_light_ids.contains(light.authoring_id))
            {
                throw std::runtime_error(
                    "Pipeline authoring overlay for level '" + level + "' references unknown or stale render-light id '"
                    + light.authoring_id + "'"
                );
            }
        }
    }
} // namespace RelayCore::render

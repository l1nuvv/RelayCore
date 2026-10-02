/**
 * @file src/relaycore/core/engine_config.cpp
 * @brief Engine configuration loading and TOML parsing helpers.
 */

#include "relaycore/core/engine_config.h"

#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/core/profiling.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <toml++/toml.hpp>

namespace RelayCore::core
{
    namespace
    {
        [[noreturn]] void invalid_config_value(
            const std::filesystem::path& config_path,
            const std::string_view key,
            const std::string_view reason
        )
        {
            throw std::runtime_error(
                "Invalid engine config '" + config_path.string() + "': key '" + std::string{key} + "' "
                + std::string{reason}
            );
        }

        auto leaf_key(const std::string_view dotted_key) -> std::string_view
        {
            const std::size_t separator = dotted_key.rfind('.');
            return separator == std::string_view::npos ? dotted_key : dotted_key.substr(separator + 1);
        }

        void ensure_optional_table(
            const toml::node_view<const toml::node>& view,
            const std::filesystem::path& config_path,
            const std::string_view key
        )
        {
            if (view && !view.is_table())
            {
                invalid_config_value(config_path, key, "must be a TOML table");
            }
        }

        void assign_if_present(
            const toml::node_view<const toml::node>& view,
            const std::filesystem::path& config_path,
            const std::string_view dotted_key,
            std::string& destination
        )
        {
            const auto node = view[leaf_key(dotted_key)];
            if (!node)
            {
                return;
            }
            if (!node.is_string())
            {
                invalid_config_value(config_path, dotted_key, "must be a TOML string");
            }
            destination = *node.value<std::string>();
        }

        void assign_if_present(
            const toml::node_view<const toml::node>& view,
            const std::filesystem::path& config_path,
            const std::string_view dotted_key,
            bool& destination
        )
        {
            const auto node = view[leaf_key(dotted_key)];
            if (!node)
            {
                return;
            }
            if (!node.is_boolean())
            {
                invalid_config_value(config_path, dotted_key, "must be a TOML boolean");
            }
            destination = *node.value<bool>();
        }

        void assign_if_present(
            const toml::node_view<const toml::node>& view,
            const std::filesystem::path& config_path,
            const std::string_view dotted_key,
            int& destination
        )
        {
            const auto node = view[leaf_key(dotted_key)];
            if (!node)
            {
                return;
            }
            if (!node.is_integer())
            {
                invalid_config_value(config_path, dotted_key, "must be a TOML integer");
            }
            const std::int64_t value = *node.value<std::int64_t>();
            if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
            {
                invalid_config_value(config_path, dotted_key, "is not representable as an integer");
            }
            destination = static_cast<int>(value);
        }

        void assign_if_present(
            const toml::node_view<const toml::node>& view,
            const std::filesystem::path& config_path,
            const std::string_view dotted_key,
            float& destination
        )
        {
            const auto node = view[leaf_key(dotted_key)];
            if (!node)
            {
                return;
            }
            if (!node.is_number())
            {
                invalid_config_value(config_path, dotted_key, "must be a TOML number");
            }
            const double value = *node.value<double>();
            if (!std::isfinite(value) || value < -static_cast<double>(std::numeric_limits<float>::max())
                || value > static_cast<double>(std::numeric_limits<float>::max()))
            {
                invalid_config_value(config_path, dotted_key, "must be finite and representable as a number");
            }
            destination = static_cast<float>(value);
        }

        /**
         * @brief Reads a three-component vector from a TOML array.
         * @param view TOML table or node view that owns the key.
         * @param key Key to read from the node view.
         * @param fallback Value returned when the key is absent.
         * @return Parsed vector, or the fallback value when the key is absent.
         */
        auto read_vec3(
            const toml::node_view<const toml::node>& view,
            const std::filesystem::path& config_path,
            const std::string_view dotted_key,
            const glm::vec3& fallback
        ) -> glm::vec3
        {
            RELAYCORE_PROFILE_FUNCTION();
            const toml::node_view<const toml::node> node = view[leaf_key(dotted_key)];
            if (!node)
            {
                return fallback;
            }
            const toml::array* array = node.as_array();
            if (array == nullptr)
            {
                invalid_config_value(config_path, dotted_key, "must be a TOML array of three numbers");
            }
            if (array->size() != 3)
            {
                invalid_config_value(config_path, dotted_key, "must contain exactly three numbers");
            }
            std::array<float, 3> values{};
            for (std::size_t index = 0; index < values.size(); ++index)
            {
                const toml::node& element = (*array)[index];
                if (!element.is_number())
                {
                    invalid_config_value(config_path, dotted_key, "must contain only numbers");
                }
                const double value = *element.value<double>();
                if (!std::isfinite(value) || value < -static_cast<double>(std::numeric_limits<float>::max())
                    || value > static_cast<double>(std::numeric_limits<float>::max()))
                {
                    invalid_config_value(config_path, dotted_key, "must contain finite representable numbers");
                }
                values[index] = static_cast<float>(value);
            }
            return {values[0], values[1], values[2]};
        }
    } // namespace

    auto load_engine_config(const std::filesystem::path& config_path) -> EngineConfig
    {
        RELAYCORE_PROFILE_FUNCTION();
        EngineConfig config{};
        config.source_path = config_path;

        if (!std::filesystem::exists(config_path))
        {
            log::warn("Config file not found: {}. Using defaults.", config_path.string());
            return config;
        }

        try
        {
            const toml::table document = toml::parse_file(config_path.string());

            const auto window = document["window"];
            ensure_optional_table(window, config_path, "window");
            assign_if_present(window, config_path, "window.title", config.window.title);
            assign_if_present(window, config_path, "window.width", config.window.width);
            assign_if_present(window, config_path, "window.height", config.window.height);
            assign_if_present(window, config_path, "window.high_dpi", config.window.high_dpi);
            assign_if_present(window, config_path, "window.vsync", config.window.vsync);

            const auto render = document["render"];
            ensure_optional_table(render, config_path, "render");
#define RELAYCORE_ASSIGN_RENDER(key, field) assign_if_present(render, config_path, "render." key, config.renderer.field)
            RELAYCORE_ASSIGN_RENDER("clear_r", clear_r);
            RELAYCORE_ASSIGN_RENDER("clear_g", clear_g);
            RELAYCORE_ASSIGN_RENDER("clear_b", clear_b);
            RELAYCORE_ASSIGN_RENDER("indirect_bounce_strength", indirect_bounce_strength);
            RELAYCORE_ASSIGN_RENDER("fog_enabled", fog_enabled);
            RELAYCORE_ASSIGN_RENDER("fog_start", fog_start);
            RELAYCORE_ASSIGN_RENDER("fog_end", fog_end);
            RELAYCORE_ASSIGN_RENDER("postfx_vignette_strength", postfx_vignette_strength);
            RELAYCORE_ASSIGN_RENDER("postfx_grain_strength", postfx_grain_strength);
            RELAYCORE_ASSIGN_RENDER("postfx_scanline_strength", postfx_scanline_strength);
            RELAYCORE_ASSIGN_RENDER("postfx_contrast", postfx_contrast);
            RELAYCORE_ASSIGN_RENDER("postfx_saturation", postfx_saturation);
            RELAYCORE_ASSIGN_RENDER("hdr_enabled", hdr_enabled);
            RELAYCORE_ASSIGN_RENDER("exposure_ev", exposure_ev);
            if (config.renderer.exposure_ev < -10 || config.renderer.exposure_ev > 10)
                invalid_config_value(config_path, "render.exposure_ev", "must be in [-10, 10] stops");
            RELAYCORE_ASSIGN_RENDER("shadow_enabled", shadow_enabled);
            RELAYCORE_ASSIGN_RENDER("shadow_map_resolution", shadow_map_resolution);
            RELAYCORE_ASSIGN_RENDER("shadow_light_intensity", shadow_light_intensity);
            RELAYCORE_ASSIGN_RENDER("shadow_strength", shadow_strength);
            RELAYCORE_ASSIGN_RENDER("shadow_contact_strength", shadow_contact_strength);
            RELAYCORE_ASSIGN_RENDER("shadow_bias", shadow_bias_near);
            RELAYCORE_ASSIGN_RENDER("shadow_bias", shadow_bias_far);
            RELAYCORE_ASSIGN_RENDER("shadow_bias_near", shadow_bias_near);
            RELAYCORE_ASSIGN_RENDER("shadow_bias_far", shadow_bias_far);
            RELAYCORE_ASSIGN_RENDER("shadow_slope_bias_scale", shadow_slope_bias_scale);
            RELAYCORE_ASSIGN_RENDER("shadow_normal_offset", shadow_normal_offset_near);
            RELAYCORE_ASSIGN_RENDER("shadow_normal_offset", shadow_normal_offset_far);
            RELAYCORE_ASSIGN_RENDER("shadow_normal_offset_near", shadow_normal_offset_near);
            RELAYCORE_ASSIGN_RENDER("shadow_normal_offset_far", shadow_normal_offset_far);
            RELAYCORE_ASSIGN_RENDER("shadow_pcf_radius_near", shadow_pcf_radius_near);
            RELAYCORE_ASSIGN_RENDER("shadow_pcf_radius_far", shadow_pcf_radius_far);
            RELAYCORE_ASSIGN_RENDER("shadow_cascade_split_distance", shadow_cascade_split_distance);
            RELAYCORE_ASSIGN_RENDER("shadow_cascade_blend_range", shadow_cascade_blend_range);
            RELAYCORE_ASSIGN_RENDER("shadow_debug_cascades", shadow_debug_cascades);
            RELAYCORE_ASSIGN_RENDER("shadow_focus_distance", shadow_focus_distance);
            RELAYCORE_ASSIGN_RENDER("shadow_projection_radius", shadow_projection_radius);
            RELAYCORE_ASSIGN_RENDER("shadow_projection_depth", shadow_projection_depth);
            RELAYCORE_ASSIGN_RENDER("shadow_far_focus_distance", shadow_far_focus_distance);
            RELAYCORE_ASSIGN_RENDER("shadow_far_projection_radius", shadow_far_projection_radius);
            RELAYCORE_ASSIGN_RENDER("shadow_far_projection_depth", shadow_far_projection_depth);
#undef RELAYCORE_ASSIGN_RENDER
            config.renderer.fog_color = read_vec3(render, config_path, "render.fog_color", config.renderer.fog_color);
            config.renderer.shadow_light_direction =
                read_vec3(render, config_path, "render.shadow_light_direction", config.renderer.shadow_light_direction);
            config.renderer.shadow_light_color =
                read_vec3(render, config_path, "render.shadow_light_color", config.renderer.shadow_light_color);
            assign_if_present(render, config_path, "render.near_plane", config.renderer.camera.near_plane);
            assign_if_present(render, config_path, "render.far_plane", config.renderer.camera.far_plane);
            float fov_degrees = glm::degrees(config.renderer.camera.vertical_fov_radians);
            assign_if_present(render, config_path, "render.fov_degrees", fov_degrees);
            config.renderer.camera.vertical_fov_radians = glm::radians(fov_degrees);
            config.renderer.camera.position =
                read_vec3(render, config_path, "render.camera_position", config.renderer.camera.position);
            config.renderer.camera.target =
                read_vec3(render, config_path, "render.camera_target", config.renderer.camera.target);

            const auto imgui = document["imgui"];
            ensure_optional_table(imgui, config_path, "imgui");
            assign_if_present(imgui, config_path, "imgui.enabled", config.renderer.imgui.enabled);
            assign_if_present(imgui, config_path, "imgui.show_demo_window", config.renderer.imgui.show_demo_window);

            const auto input = document["input"];
            ensure_optional_table(input, config_path, "input");
            assign_if_present(input, config_path, "input.move_speed", config.input.move_speed);
            assign_if_present(input, config_path, "input.sprint_multiplier", config.input.sprint_multiplier);
            assign_if_present(
                input,
                config_path,
                "input.mouse_sensitivity_radians",
                config.input.mouse_sensitivity_radians
            );
            assign_if_present(input, config_path, "input.capture_mouse_on_start", config.input.capture_mouse_on_start);

            const auto audio = document["audio"];
            ensure_optional_table(audio, config_path, "audio");
            assign_if_present(audio, config_path, "audio.enabled", config.audio.enabled);
            assign_if_present(audio, config_path, "audio.master_volume", config.audio.master_volume);
            assign_if_present(audio, config_path, "audio.startup_sound", config.audio.startup_sound);

            const auto game = document["game"];
            ensure_optional_table(game, config_path, "game");
            assign_if_present(game, config_path, "game.level_path", config.gameplay.level_path);
            assign_if_present(game, config_path, "game.spawn_classname", config.gameplay.spawn_classname);

            const auto controls = game["controls"];
            ensure_optional_table(controls, config_path, "game.controls");
#define RELAYCORE_ASSIGN_CONTROL(key, field)                                                                           \
    assign_if_present(controls, config_path, "game.controls." key, config.gameplay.controls.field)
            RELAYCORE_ASSIGN_CONTROL("move_forward", move_forward);
            RELAYCORE_ASSIGN_CONTROL("move_backward", move_backward);
            RELAYCORE_ASSIGN_CONTROL("move_right", move_right);
            RELAYCORE_ASSIGN_CONTROL("move_left", move_left);
            RELAYCORE_ASSIGN_CONTROL("jump", jump);
            RELAYCORE_ASSIGN_CONTROL("sprint", sprint);
            RELAYCORE_ASSIGN_CONTROL("sprint_alt", sprint_alt);
            RELAYCORE_ASSIGN_CONTROL("slow", slow);
            RELAYCORE_ASSIGN_CONTROL("slow_alt", slow_alt);
            RELAYCORE_ASSIGN_CONTROL("noclip_down", noclip_down);
            RELAYCORE_ASSIGN_CONTROL("noclip_down_alt", noclip_down_alt);
            RELAYCORE_ASSIGN_CONTROL("primary_attack", primary_attack);
            RELAYCORE_ASSIGN_CONTROL("use", use);
            RELAYCORE_ASSIGN_CONTROL("toggle_capture", toggle_capture);
            RELAYCORE_ASSIGN_CONTROL("toggle_vsync", toggle_vsync);
            RELAYCORE_ASSIGN_CONTROL("toggle_noclip", toggle_noclip);
            RELAYCORE_ASSIGN_CONTROL("toggle_inspector", toggle_inspector);
            RELAYCORE_ASSIGN_CONTROL("pin_inspector", pin_inspector);
            RELAYCORE_ASSIGN_CONTROL("reload_level", reload_level);
            RELAYCORE_ASSIGN_CONTROL("open_inspector_source", open_inspector_source);
            RELAYCORE_ASSIGN_CONTROL("save_runtime_snapshot", save_runtime_snapshot);
            RELAYCORE_ASSIGN_CONTROL("restore_runtime_snapshot", restore_runtime_snapshot);
            RELAYCORE_ASSIGN_CONTROL("quit", quit);
#undef RELAYCORE_ASSIGN_CONTROL

            const auto gameplay_audio = game["audio"];
            ensure_optional_table(gameplay_audio, config_path, "game.audio");
#define RELAYCORE_ASSIGN_GAME_AUDIO(key, field)                                                                        \
    assign_if_present(gameplay_audio, config_path, "game.audio." key, config.gameplay.audio.field)
            RELAYCORE_ASSIGN_GAME_AUDIO("ambient_loop", ambient_loop);
            RELAYCORE_ASSIGN_GAME_AUDIO("panel_press", panel_press);
            RELAYCORE_ASSIGN_GAME_AUDIO("pickup", pickup);
            RELAYCORE_ASSIGN_GAME_AUDIO("door_open", door_open);
            RELAYCORE_ASSIGN_GAME_AUDIO("door_close", door_close);
            RELAYCORE_ASSIGN_GAME_AUDIO("access_denied", access_denied);
            RELAYCORE_ASSIGN_GAME_AUDIO("mission_complete", mission_complete);
            RELAYCORE_ASSIGN_GAME_AUDIO("weapon_fire", weapon_fire);
            RELAYCORE_ASSIGN_GAME_AUDIO("weapon_hit", weapon_hit);
            RELAYCORE_ASSIGN_GAME_AUDIO("threat_windup", threat_windup);
            RELAYCORE_ASSIGN_GAME_AUDIO("rover_attack", rover_attack);
            RELAYCORE_ASSIGN_GAME_AUDIO("turret_fire", turret_fire);
            RELAYCORE_ASSIGN_GAME_AUDIO("turret_destroyed", turret_destroyed);
            RELAYCORE_ASSIGN_GAME_AUDIO("player_hurt", player_hurt);
            RELAYCORE_ASSIGN_GAME_AUDIO("player_respawn", player_respawn);
#undef RELAYCORE_ASSIGN_GAME_AUDIO

            if (config.window.width <= 0)
            {
                invalid_config_value(config_path, "window.width", "must be greater than zero");
            }
            if (config.window.height <= 0)
            {
                invalid_config_value(config_path, "window.height", "must be greater than zero");
            }
            if (fov_degrees <= 0.0F || fov_degrees >= 180.0F)
            {
                invalid_config_value(config_path, "render.fov_degrees", "must be greater than zero and less than 180");
            }
            if (config.renderer.camera.near_plane <= 0.0F)
            {
                invalid_config_value(config_path, "render.near_plane", "must be greater than zero");
            }
            if (config.renderer.camera.far_plane <= config.renderer.camera.near_plane)
            {
                invalid_config_value(config_path, "render.far_plane", "must be greater than 'render.near_plane'");
            }
            if (config.renderer.camera.position.x == config.renderer.camera.target.x
                && config.renderer.camera.position.y == config.renderer.camera.target.y
                && config.renderer.camera.position.z == config.renderer.camera.target.z)
            {
                invalid_config_value(config_path, "render.camera_position", "must differ from 'render.camera_target'");
            }
            if (glm::length(config.renderer.shadow_light_direction) <= 0.0001F)
            {
                invalid_config_value(config_path, "render.shadow_light_direction", "must not be degenerate");
            }
            if (config.renderer.shadow_map_resolution < 256)
            {
                invalid_config_value(config_path, "render.shadow_map_resolution", "must be at least 256");
            }
            if (config.renderer.shadow_projection_radius < 1.0F)
            {
                invalid_config_value(config_path, "render.shadow_projection_radius", "must be at least 1");
            }
            if (config.renderer.shadow_projection_depth < config.renderer.shadow_projection_radius + 1.0F)
            {
                invalid_config_value(
                    config_path,
                    "render.shadow_projection_depth",
                    "must be at least 'render.shadow_projection_radius + 1'"
                );
            }
            if (config.renderer.shadow_far_projection_radius < 1.0F)
            {
                invalid_config_value(config_path, "render.shadow_far_projection_radius", "must be at least 1");
            }
            if (config.renderer.shadow_far_projection_depth < config.renderer.shadow_far_projection_radius + 1.0F)
            {
                invalid_config_value(
                    config_path,
                    "render.shadow_far_projection_depth",
                    "must be at least 'render.shadow_far_projection_radius + 1'"
                );
            }

            log::info("Loaded engine config: {}", config_path.string());
        } catch (const toml::parse_error& error)
        {
            log::error(
                "Failed to parse config {} at line {}, column {}: {}",
                config_path.string(),
                error.source().begin.line,
                error.source().begin.column,
                error.description()
            );
            throw;
        }

        return config;
    }

    auto load_default_engine_config() -> EngineConfig
    {
        RELAYCORE_PROFILE_FUNCTION();
        return load_engine_config(asset_path("config/engine.toml"));
    }
} // namespace RelayCore::core

/**
 * @file src/relaycore/core/engine_config.cpp
 * @brief Engine configuration loading and TOML parsing helpers.
 */

#include "relaycore/core/engine_config.h"

#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"

#include <array>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <string_view>
#include <toml++/toml.hpp>

namespace RelayCore::core
{
    namespace
    {
        /**
         * @brief Assigns a TOML value to a destination if the key exists and matches the type.
         * @tparam T Value type to read.
         * @param view TOML table or node view that owns the key.
         * @param key Key to read from the node view.
         * @param destination Destination updated on success.
         */
        template<typename T>
        void
        assign_if_present(const toml::node_view<const toml::node>& view, const std::string_view key, T& destination)
        {
            if (const auto value = view[key].template value<T>())
            {
                destination = *value;
            }
        }

        /**
         * @brief Reads a three-component vector from a TOML array.
         * @param view TOML table or node view that owns the key.
         * @param key Key to read from the node view.
         * @param fallback Value returned when parsing fails.
         * @return Parsed vector or the fallback value.
         */
        auto
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
                    return glm::vec3{static_cast<float>(*x), static_cast<float>(*y), static_cast<float>(*z)};
                }
            }

            return fallback;
        }
    } // namespace

    auto load_engine_config(const std::filesystem::path& config_path) -> EngineConfig
    {
        EngineConfig config{};

        if (!std::filesystem::exists(config_path))
        {
            log::warn("Config file not found: {}. Using defaults.", config_path.string());
            return config;
        }

        try
        {
            const toml::table document = toml::parse_file(config_path.string());

            const auto window = document["window"];
            assign_if_present(window, "title", config.window.title);
            assign_if_present(window, "width", config.window.width);
            assign_if_present(window, "height", config.window.height);
            assign_if_present(window, "high_dpi", config.window.high_dpi);
            assign_if_present(window, "vsync", config.window.vsync);

            const auto render = document["render"];
            assign_if_present(render, "clear_r", config.renderer.clear_r);
            assign_if_present(render, "clear_g", config.renderer.clear_g);
            assign_if_present(render, "clear_b", config.renderer.clear_b);
            assign_if_present(render, "indirect_bounce_strength", config.renderer.indirect_bounce_strength);
            assign_if_present(render, "fog_enabled", config.renderer.fog_enabled);
            assign_if_present(render, "fog_start", config.renderer.fog_start);
            assign_if_present(render, "fog_end", config.renderer.fog_end);
            assign_if_present(render, "postfx_vignette_strength", config.renderer.postfx_vignette_strength);
            assign_if_present(render, "postfx_grain_strength", config.renderer.postfx_grain_strength);
            assign_if_present(render, "postfx_scanline_strength", config.renderer.postfx_scanline_strength);
            assign_if_present(render, "postfx_contrast", config.renderer.postfx_contrast);
            assign_if_present(render, "postfx_saturation", config.renderer.postfx_saturation);
            assign_if_present(render, "shadow_enabled", config.renderer.shadow_enabled);
            assign_if_present(render, "shadow_map_resolution", config.renderer.shadow_map_resolution);
            assign_if_present(render, "shadow_light_intensity", config.renderer.shadow_light_intensity);
            assign_if_present(render, "shadow_strength", config.renderer.shadow_strength);
            assign_if_present(render, "shadow_contact_strength", config.renderer.shadow_contact_strength);
            assign_if_present(render, "shadow_bias", config.renderer.shadow_bias_near);
            assign_if_present(render, "shadow_bias", config.renderer.shadow_bias_far);
            assign_if_present(render, "shadow_bias_near", config.renderer.shadow_bias_near);
            assign_if_present(render, "shadow_bias_far", config.renderer.shadow_bias_far);
            assign_if_present(render, "shadow_slope_bias_scale", config.renderer.shadow_slope_bias_scale);
            assign_if_present(render, "shadow_normal_offset", config.renderer.shadow_normal_offset_near);
            assign_if_present(render, "shadow_normal_offset", config.renderer.shadow_normal_offset_far);
            assign_if_present(render, "shadow_normal_offset_near", config.renderer.shadow_normal_offset_near);
            assign_if_present(render, "shadow_normal_offset_far", config.renderer.shadow_normal_offset_far);
            assign_if_present(render, "shadow_pcf_radius_near", config.renderer.shadow_pcf_radius_near);
            assign_if_present(render, "shadow_pcf_radius_far", config.renderer.shadow_pcf_radius_far);
            assign_if_present(render, "shadow_cascade_split_distance", config.renderer.shadow_cascade_split_distance);
            assign_if_present(render, "shadow_cascade_blend_range", config.renderer.shadow_cascade_blend_range);
            assign_if_present(render, "shadow_debug_cascades", config.renderer.shadow_debug_cascades);
            assign_if_present(render, "shadow_focus_distance", config.renderer.shadow_focus_distance);
            assign_if_present(render, "shadow_projection_radius", config.renderer.shadow_projection_radius);
            assign_if_present(render, "shadow_projection_depth", config.renderer.shadow_projection_depth);
            assign_if_present(render, "shadow_far_focus_distance", config.renderer.shadow_far_focus_distance);
            assign_if_present(render, "shadow_far_projection_radius", config.renderer.shadow_far_projection_radius);
            assign_if_present(render, "shadow_far_projection_depth", config.renderer.shadow_far_projection_depth);
            config.renderer.fog_color = read_vec3(render, "fog_color", config.renderer.fog_color);
            config.renderer.shadow_light_direction =
                read_vec3(render, "shadow_light_direction", config.renderer.shadow_light_direction);
            config.renderer.shadow_light_color =
                read_vec3(render, "shadow_light_color", config.renderer.shadow_light_color);
            assign_if_present(render, "near_plane", config.renderer.camera.near_plane);
            assign_if_present(render, "far_plane", config.renderer.camera.far_plane);
            if (const auto fov_degrees = render["fov_degrees"].value<double>())
            {
                config.renderer.camera.vertical_fov_radians = glm::radians(static_cast<float>(*fov_degrees));
            }
            config.renderer.camera.position = read_vec3(render, "camera_position", config.renderer.camera.position);
            config.renderer.camera.target = read_vec3(render, "camera_target", config.renderer.camera.target);

            const auto imgui = document["imgui"];
            assign_if_present(imgui, "enabled", config.renderer.imgui.enabled);
            assign_if_present(imgui, "show_demo_window", config.renderer.imgui.show_demo_window);

            const auto input = document["input"];
            assign_if_present(input, "move_speed", config.input.move_speed);
            assign_if_present(input, "sprint_multiplier", config.input.sprint_multiplier);
            assign_if_present(input, "mouse_sensitivity_radians", config.input.mouse_sensitivity_radians);
            assign_if_present(input, "capture_mouse_on_start", config.input.capture_mouse_on_start);

            const auto audio = document["audio"];
            assign_if_present(audio, "enabled", config.audio.enabled);
            assign_if_present(audio, "master_volume", config.audio.master_volume);
            assign_if_present(audio, "startup_sound", config.audio.startup_sound);

            const auto game = document["game"];
            assign_if_present(game, "level_path", config.gameplay.level_path);
            assign_if_present(game, "spawn_classname", config.gameplay.spawn_classname);

            const auto controls = game["controls"];
            assign_if_present(controls, "move_forward", config.gameplay.controls.move_forward);
            assign_if_present(controls, "move_backward", config.gameplay.controls.move_backward);
            assign_if_present(controls, "move_right", config.gameplay.controls.move_right);
            assign_if_present(controls, "move_left", config.gameplay.controls.move_left);
            assign_if_present(controls, "jump", config.gameplay.controls.jump);
            assign_if_present(controls, "sprint", config.gameplay.controls.sprint);
            assign_if_present(controls, "sprint_alt", config.gameplay.controls.sprint_alt);
            assign_if_present(controls, "slow", config.gameplay.controls.slow);
            assign_if_present(controls, "slow_alt", config.gameplay.controls.slow_alt);
            assign_if_present(controls, "noclip_down", config.gameplay.controls.noclip_down);
            assign_if_present(controls, "noclip_down_alt", config.gameplay.controls.noclip_down_alt);
            assign_if_present(controls, "use", config.gameplay.controls.use);
            assign_if_present(controls, "toggle_capture", config.gameplay.controls.toggle_capture);
            assign_if_present(controls, "toggle_vsync", config.gameplay.controls.toggle_vsync);
            assign_if_present(controls, "toggle_noclip", config.gameplay.controls.toggle_noclip);
            assign_if_present(controls, "quit", config.gameplay.controls.quit);

            const auto gameplay_audio = game["audio"];
            assign_if_present(gameplay_audio, "ambient_loop", config.gameplay.audio.ambient_loop);
            assign_if_present(gameplay_audio, "panel_press", config.gameplay.audio.panel_press);
            assign_if_present(gameplay_audio, "door_open", config.gameplay.audio.door_open);
            assign_if_present(gameplay_audio, "door_close", config.gameplay.audio.door_close);
            assign_if_present(gameplay_audio, "access_denied", config.gameplay.audio.access_denied);

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
        return load_engine_config(asset_path("config/engine.toml"));
    }
} // namespace RelayCore::core

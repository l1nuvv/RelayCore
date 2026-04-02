/**
 * @file src/relaycore/core/engine_config.cpp
 * @brief Engine configuration loading and TOML parsing helpers.
 */

#include "relaycore/core/engine_config.h"

#include <array>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <string_view>
#include <toml++/toml.hpp>

#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"

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
            if (const auto value = view[key].template value<T>()) { destination = *value; }
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
            assign_if_present(render, "near_plane", config.renderer.camera.near_plane);
            assign_if_present(render, "far_plane", config.renderer.camera.far_plane);
            if (const auto fov_degrees = render["fov_degrees"].value<double>())
            {
                config.renderer.camera.vertical_fov_radians = glm::radians(static_cast<float>(*fov_degrees));
            }
            config.renderer.camera.position = read_vec3(render, "camera_position", config.renderer.camera.position);
            config.renderer.camera.target   = read_vec3(render, "camera_target", config.renderer.camera.target);

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

    auto load_default_engine_config() -> EngineConfig { return load_engine_config(asset_path("config/engine.toml")); }
} // namespace RelayCore::core

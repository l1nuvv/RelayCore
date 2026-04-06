/**
 * @file src/relaycore/core/engine_config.h
 * @brief Engine configuration aggregates and loading helpers.
 */

#pragma once

#include "relaycore/app/fly_camera_controller.h"
#include "relaycore/audio/audio_system.h"
#include "relaycore/debug/imgui_overlay.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/camera.h"

#include <filesystem>
#include <string>

namespace RelayCore::render
{
    /**
     * @brief Render-system settings loaded from configuration.
     */
    struct RendererConfig {
        float clear_r = 0.10F;                        ///< Red clear-channel value.
        float clear_g = 0.11F;                        ///< Green clear-channel value.
        float clear_b = 0.14F;                        ///< Blue clear-channel value.
        bool fog_enabled = true;                      ///< Enables simple distance fog for world readability.
        glm::vec3 fog_color{0.09F, 0.10F, 0.12F};     ///< Fog color blended over distant geometry.
        float fog_start = 5.5F;                       ///< World-space distance where fog begins.
        float fog_end = 18.0F;                        ///< World-space distance where fog reaches full opacity.
        PerspectiveCameraConfig camera{};             ///< Camera startup configuration.
        RelayCore::debug::ImGuiOverlayConfig imgui{}; ///< ImGui overlay configuration.
    };
} // namespace RelayCore::render

namespace RelayCore::core
{
    struct GameplayControlsConfig {
        std::string move_forward{"W"};
        std::string move_backward{"S"};
        std::string move_right{"D"};
        std::string move_left{"A"};
        std::string jump{"Space"};
        std::string sprint{"Left Shift"};
        std::string sprint_alt{"Right Shift"};
        std::string slow{"Left Alt"};
        std::string slow_alt{"Right Alt"};
        std::string noclip_down{"Left Ctrl"};
        std::string noclip_down_alt{"Right Ctrl"};
        std::string use{"E"};
        std::string toggle_capture{"F1"};
        std::string toggle_vsync{"V"};
        std::string toggle_noclip{"F3"};
        std::string quit{"Escape"};
    };

    struct GameplayAudioConfig {
        std::string ambient_loop{};
        std::string panel_press{};
        std::string door_open{};
        std::string door_close{};
    };

    /**
     * @brief Demo/gameplay bootstrap settings loaded from configuration.
     */
    struct GameplayConfig {
        std::string level_path{"levels/test_room.toml"}; ///< Relative asset path or absolute path to the level file.
        std::string spawn_classname{"player_spawn"};     ///< Entity classname used as the player spawn point.
        GameplayControlsConfig controls{};               ///< Gameplay action bindings loaded by the demo runtime.
        GameplayAudioConfig audio{};                     ///< Gameplay-facing world audio cues.
    };

    /**
     * @brief Aggregate configuration consumed by the engine runtime.
     */
    struct EngineConfig {
        WindowConfig window{};             ///< Window-system settings.
        render::RendererConfig renderer{}; ///< Renderer settings.
        FlyCameraControllerConfig input{}; ///< Fly-camera input settings.
        AudioSystemConfig audio{};         ///< Audio-system settings.
        GameplayConfig gameplay{};         ///< Demo/gameplay bootstrap settings.
    };

    /**
     * @brief Loads engine configuration from a TOML file.
     * @param config_path Path to the TOML configuration file.
     * @return Parsed engine configuration.
     */
    [[nodiscard]] auto load_engine_config(const std::filesystem::path& config_path) -> EngineConfig;

    /**
     * @brief Loads the default engine configuration from the assets directory.
     * @return Parsed engine configuration.
     */
    [[nodiscard]] auto load_default_engine_config() -> EngineConfig;
} // namespace RelayCore::core

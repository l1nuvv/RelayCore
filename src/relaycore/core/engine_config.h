/**
 * @file src/relaycore/core/engine_config.h
 * @brief Engine configuration aggregates and loading helpers.
 */

#pragma once

#include <filesystem>

#include "relaycore/app/fly_camera_controller.h"
#include "relaycore/audio/audio_system.h"
#include "relaycore/debug/imgui_overlay.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/camera.h"

namespace RelayCore::render
{
    /**
     * @brief Render-system settings loaded from configuration.
     */
    struct RendererConfig {
        float                                clear_r = 0.10F; ///< Red clear-channel value.
        float                                clear_g = 0.11F; ///< Green clear-channel value.
        float                                clear_b = 0.14F; ///< Blue clear-channel value.
        PerspectiveCameraConfig              camera{}; ///< Camera startup configuration.
        RelayCore::debug::ImGuiOverlayConfig imgui{}; ///< ImGui overlay configuration.
    };
} // namespace RelayCore::render

namespace RelayCore::core
{
    /**
     * @brief Aggregate configuration consumed by the engine runtime.
     */
    struct EngineConfig {
        WindowConfig              window{}; ///< Window-system settings.
        render::RendererConfig    renderer{}; ///< Renderer settings.
        FlyCameraControllerConfig input{}; ///< Fly-camera input settings.
        AudioSystemConfig         audio{}; ///< Audio-system settings.
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

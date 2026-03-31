#pragma once

#include <filesystem>

#include "app/fly_camera_controller.hpp"
#include "audio/audio_system.hpp"
#include "debug/imgui_overlay.hpp"
#include "platform/window.hpp"
#include "render/camera.hpp"

namespace RelayCore::render
{
    struct RendererConfig {
        float                                clear_r = 0.10F;
        float                                clear_g = 0.11F;
        float                                clear_b = 0.14F;
        PerspectiveCameraConfig              camera{};
        RelayCore::debug::ImGuiOverlayConfig imgui{};
    };
} // namespace RelayCore::render

namespace RelayCore::core
{
    struct EngineConfig {
        WindowConfig              window{};
        render::RendererConfig    renderer{};
        FlyCameraControllerConfig input{};
        AudioSystemConfig         audio{};
    };

    [[nodiscard]] auto load_engine_config(const std::filesystem::path& config_path) -> EngineConfig;

    [[nodiscard]] auto load_default_engine_config() -> EngineConfig;
} // namespace RelayCore::core

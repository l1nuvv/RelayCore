/**
 * @file src/relaycore/app/engine_services.cpp
 * @brief EngineServices accessors and control helpers.
 */

#include "relaycore/app/engine_services.h"

#include "relaycore/audio/audio_system.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/camera.h"
#include "relaycore/render/renderer.h"

namespace RelayCore
{
    EngineServices::EngineServices(
        const core::EngineConfig& config,
        Window&                   window,
        Renderer&                 renderer,
        AudioSystem&              audio_system,
        bool&                     is_running
    ) noexcept :
        config_(&config),
        window_(&window),
        renderer_(&renderer),
        audio_system_(&audio_system),
        is_running_(&is_running) {}

    auto EngineServices::config() const noexcept -> const core::EngineConfig& { return *config_; }

    auto EngineServices::window() noexcept -> Window& { return *window_; }

    auto EngineServices::window() const noexcept -> const Window& { return *window_; }

    auto EngineServices::renderer() noexcept -> Renderer& { return *renderer_; }

    auto EngineServices::renderer() const noexcept -> const Renderer& { return *renderer_; }

    auto EngineServices::audio_system() noexcept -> AudioSystem& { return *audio_system_; }

    auto EngineServices::audio_system() const noexcept -> const AudioSystem& { return *audio_system_; }

    auto EngineServices::camera() noexcept -> render::PerspectiveCamera& { return renderer_->camera(); }

    auto EngineServices::camera() const noexcept -> const render::PerspectiveCamera& { return renderer_->camera(); }

    void EngineServices::request_quit() noexcept { *is_running_ = false; }

    auto EngineServices::is_running() const noexcept -> bool { return *is_running_; }
} // namespace RelayCore

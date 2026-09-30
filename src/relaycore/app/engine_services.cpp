/**
 * @file src/relaycore/app/engine_services.cpp
 * @brief EngineServices accessors and control helpers.
 */

#include "relaycore/app/engine_services.h"

#include "relaycore/audio/audio_system.h"
#include "relaycore/core/profiling.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/camera.h"
#include "relaycore/render/renderer.h"

#include <type_traits>
#include <utility>

namespace RelayCore
{
    EngineServices::EngineServices(
        core::EngineConfig& config,
        Window& window,
        Renderer& renderer,
        AudioSystem& audio_system,
        bool& is_running
    ) noexcept :
        config_(&config), window_(&window), renderer_(&renderer), audio_system_(&audio_system), is_running_(&is_running)
    {
        RELAYCORE_PROFILE_FUNCTION();
    }

    auto EngineServices::config() const noexcept -> const core::EngineConfig&
    {
        return *config_;
    }

    void EngineServices::commit_live_reload_config(core::EngineConfig&& config) noexcept
    {
        static_assert(std::is_nothrow_move_assignable_v<render::RendererConfig>);
        static_assert(std::is_nothrow_move_assignable_v<FlyCameraControllerConfig>);
        static_assert(std::is_nothrow_move_assignable_v<core::GameplayConfig>);
        config_->renderer = std::move(config.renderer);
        config_->input = std::move(config.input);
        config_->gameplay = std::move(config.gameplay);
    }

    auto EngineServices::window() noexcept -> Window&
    {
        return *window_;
    }

    auto EngineServices::window() const noexcept -> const Window&
    {
        return *window_;
    }

    auto EngineServices::renderer() noexcept -> Renderer&
    {
        return *renderer_;
    }

    auto EngineServices::renderer() const noexcept -> const Renderer&
    {
        return *renderer_;
    }

    auto EngineServices::audio_system() noexcept -> AudioSystem&
    {
        return *audio_system_;
    }

    auto EngineServices::audio_system() const noexcept -> const AudioSystem&
    {
        return *audio_system_;
    }

    auto EngineServices::camera() noexcept -> render::PerspectiveCamera&
    {
        return renderer_->camera();
    }

    auto EngineServices::camera() const noexcept -> const render::PerspectiveCamera&
    {
        return renderer_->camera();
    }

    void EngineServices::request_quit() noexcept
    {
        *is_running_ = false;
    }

    auto EngineServices::is_running() const noexcept -> bool
    {
        return *is_running_;
    }
} // namespace RelayCore

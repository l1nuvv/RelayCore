#pragma once

#include "relaycore/core/engine_config.hpp"

namespace RelayCore
{
    class AudioSystem;
    class Renderer;
    class Window;
}

namespace RelayCore::render
{
    class PerspectiveCamera;
}

namespace RelayCore
{
    class EngineServices {
    public:
        EngineServices(
            const core::EngineConfig& config,
            Window&                   window,
            Renderer&                 renderer,
            AudioSystem&              audio_system,
            bool&                     is_running
        ) noexcept;

        [[nodiscard]] auto config() const noexcept -> const core::EngineConfig&;

        [[nodiscard]] auto window() noexcept -> Window&;
        [[nodiscard]] auto window() const noexcept -> const Window&;

        [[nodiscard]] auto renderer() noexcept -> Renderer&;
        [[nodiscard]] auto renderer() const noexcept -> const Renderer&;

        [[nodiscard]] auto audio_system() noexcept -> AudioSystem&;
        [[nodiscard]] auto audio_system() const noexcept -> const AudioSystem&;

        [[nodiscard]] auto camera() noexcept -> render::PerspectiveCamera&;
        [[nodiscard]] auto camera() const noexcept -> const render::PerspectiveCamera&;

        void request_quit() noexcept;

        [[nodiscard]] auto is_running() const noexcept -> bool;

    private:
        const core::EngineConfig* config_       = nullptr;
        Window*                   window_       = nullptr;
        Renderer*                 renderer_     = nullptr;
        AudioSystem*              audio_system_ = nullptr;
        bool*                     is_running_   = nullptr;
    };
} // namespace RelayCore

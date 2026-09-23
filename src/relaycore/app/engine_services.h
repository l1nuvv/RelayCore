/**
 * @file src/relaycore/app/engine_services.h
 * @brief Narrow runtime service container passed to the game layer.
 */

#pragma once

#include "relaycore/core/engine_config.h"

namespace RelayCore
{
    class AudioSystem;
    class Renderer;
    class Window;
} // namespace RelayCore

namespace RelayCore::render
{
    class PerspectiveCamera;
}

namespace RelayCore
{
    /**
     * @brief Provides controlled access to engine-owned runtime systems.
     *
     * The game layer receives this facade instead of direct ownership of engine
     * subsystems.
     */
    class EngineServices {
    public:
        /**
         * @brief Builds a service view over live engine subsystems.
         * @param config Loaded engine configuration.
         * @param window Window and OpenGL context owner.
         * @param renderer Rendering subsystem.
         * @param audio_system Audio subsystem.
         * @param is_running Reference to the application running flag.
         */
        EngineServices(
            core::EngineConfig& config,
            Window& window,
            Renderer& renderer,
            AudioSystem& audio_system,
            bool& is_running
        ) noexcept;

        /**
         * @brief Returns the loaded engine configuration.
         * @return Immutable engine configuration.
         */
        [[nodiscard]] auto config() const noexcept -> const core::EngineConfig&;

        /** Applies reloadable settings; window, audio device and config path retain their startup values. */
        void commit_live_reload_config(core::EngineConfig&& config) noexcept;

        /**
         * @brief Returns the mutable window wrapper.
         * @return Window wrapper owned by the application.
         */
        [[nodiscard]] auto window() noexcept -> Window&;

        /**
         * @brief Returns the immutable window wrapper.
         * @return Window wrapper owned by the application.
         */
        [[nodiscard]] auto window() const noexcept -> const Window&;

        /**
         * @brief Returns the mutable renderer.
         * @return Renderer owned by the application.
         */
        [[nodiscard]] auto renderer() noexcept -> Renderer&;

        /**
         * @brief Returns the immutable renderer.
         * @return Renderer owned by the application.
         */
        [[nodiscard]] auto renderer() const noexcept -> const Renderer&;

        /**
         * @brief Returns the mutable audio system.
         * @return Audio system owned by the application.
         */
        [[nodiscard]] auto audio_system() noexcept -> AudioSystem&;

        /**
         * @brief Returns the immutable audio system.
         * @return Audio system owned by the application.
         */
        [[nodiscard]] auto audio_system() const noexcept -> const AudioSystem&;

        /**
         * @brief Returns the mutable camera owned by the renderer.
         * @return Perspective camera currently used for rendering.
         */
        [[nodiscard]] auto camera() noexcept -> render::PerspectiveCamera&;

        /**
         * @brief Returns the immutable camera owned by the renderer.
         * @return Perspective camera currently used for rendering.
         */
        [[nodiscard]] auto camera() const noexcept -> const render::PerspectiveCamera&;

        /**
         * @brief Requests that the application stop its main loop.
         */
        void request_quit() noexcept;

        /**
         * @brief Reports whether the application is still running.
         * @return True while the main loop should continue.
         */
        [[nodiscard]] auto is_running() const noexcept -> bool;

    private:
        core::EngineConfig* config_ = nullptr; ///< Configuration backing the current runtime.
        Window* window_ = nullptr;             ///< Window subsystem view.
        Renderer* renderer_ = nullptr;         ///< Renderer subsystem view.
        AudioSystem* audio_system_ = nullptr;  ///< Audio subsystem view.
        bool* is_running_ = nullptr;           ///< Mutable main-loop running flag.
    };
} // namespace RelayCore

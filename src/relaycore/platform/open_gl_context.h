#pragma once

#include "relaycore/render/color_pipeline_mode.h"

#include <SDL3/SDL.h>
#include <memory>

namespace RelayCore
{
    class OpenGlContext {
    public:
        static void configure_window(render::ColorPipelineMode color_pipeline_mode);

        // The window must outlive this context.
        OpenGlContext(SDL_Window* window, bool vsync, render::ColorPipelineMode color_pipeline_mode);

        OpenGlContext(const OpenGlContext&) = delete;
        auto operator=(const OpenGlContext&) -> OpenGlContext& = delete;
        OpenGlContext(OpenGlContext&&) = delete;
        auto operator=(OpenGlContext&&) -> OpenGlContext& = delete;

        [[nodiscard]] auto handle() const noexcept -> SDL_GLContext;
        [[nodiscard]] auto framebuffer_srgb_capable() const noexcept -> int;
        [[nodiscard]] auto swap_interval() const -> int;
        [[nodiscard]] auto set_swap_interval(int interval) const -> bool;
        [[nodiscard]] auto toggle_vsync() const -> bool;
        void swap_buffers() const;

    private:
        void log_context_info() const;

        SDL_Window* window_ = nullptr;
        std::unique_ptr<SDL_GLContextState, decltype(&SDL_GL_DestroyContext)> context_{nullptr, SDL_GL_DestroyContext};
        int framebuffer_srgb_capable_ = 0;
    };
} // namespace RelayCore

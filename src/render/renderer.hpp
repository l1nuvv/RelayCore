#pragma once

#include "app/frame_timer.hpp"
#include "core/engine_config.hpp"
#include "debug/imgui_overlay.hpp"
#include "render/camera.hpp"
#include "render/mesh.hpp"
#include "render/shader_program.hpp"

namespace RelayCore
{
    class Window;
    class AudioSystem;
}

namespace RelayCore
{
    class Renderer {
    public:
        explicit Renderer(const render::RendererConfig& config = render::RendererConfig{});

        void initialize(const Window& window);

        void process_event(const SDL_Event& event);

        void on_resize(int width, int height) noexcept;

        void render_frame(const FrameInfo& frame, const AudioSystem& audio_system);

        [[nodiscard]] auto camera() noexcept -> render::PerspectiveCamera&;

        [[nodiscard]] auto camera() const noexcept -> const render::PerspectiveCamera&;

    private:
        render::RendererConfig    config_{};
        bool                      initialized_     = false;
        int                       viewport_width_  = 1280;
        int                       viewport_height_ = 720;
        render::PerspectiveCamera camera_{};
        render::ShaderProgram     shader_program_{};
        render::ColorCubeMesh     cube_mesh_{};
        debug::ImGuiOverlay       imgui_overlay_{};
    };
} // namespace RelayCore

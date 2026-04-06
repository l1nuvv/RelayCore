#pragma once

#include "relaycore/app/frame_timer.h"
#include "relaycore/core/engine_config.h"
#include "relaycore/debug/imgui_overlay.h"
#include "relaycore/render/camera.h"
#include "relaycore/render/mesh.h"
#include "relaycore/render/render_diagnostics.h"
#include "relaycore/render/render_queue.h"
#include "relaycore/render/shader_program.h"
#include "relaycore/render/texture2d.h"

#include <SDL3/SDL.h>
#include <vector>

namespace RelayCore
{
    class Window;
    class AudioSystem;
} // namespace RelayCore

namespace RelayCore
{
    class Renderer {
    public:
        explicit Renderer(const render::RendererConfig& config = render::RendererConfig{});

        void initialize(const Window& window);

        void process_event(const SDL_Event& event);

        void on_resize(int width, int height) noexcept;

        void begin_frame();

        void draw_render_queue(std::span<const render::RenderItem> items);

        void set_local_lights(std::span<const render::LocalLight> lights);

        void set_scene_time(float seconds) noexcept;

        void draw_debug_overlay(const FrameInfo& frame, const AudioSystem& audio_system);

        void set_world_diagnostics(const render::WorldDiagnostics& diagnostics) noexcept;

        [[nodiscard]] auto world_diagnostics() const noexcept -> const render::WorldDiagnostics&;

        void set_player_diagnostics(const render::PlayerDiagnostics& diagnostics) noexcept;

        [[nodiscard]] auto player_diagnostics() const noexcept -> const render::PlayerDiagnostics&;

        void set_interaction_diagnostics(const render::InteractionDiagnostics& diagnostics) noexcept;

        [[nodiscard]] auto interaction_diagnostics() const noexcept -> const render::InteractionDiagnostics&;

        [[nodiscard]] auto camera() noexcept -> render::PerspectiveCamera&;

        [[nodiscard]] auto camera() const noexcept -> const render::PerspectiveCamera&;

        [[nodiscard]] auto diagnostics_visible() const noexcept -> bool;

        void set_diagnostics_visible(bool visible) noexcept;

        void toggle_diagnostics_visible() noexcept;

    private:
        render::RendererConfig config_{};
        bool initialized_ = false;
        int viewport_width_ = 1280;
        int viewport_height_ = 720;
        render::PerspectiveCamera camera_{};
        render::ShaderProgram shader_program_{};
        debug::ImGuiOverlay imgui_overlay_{};
        render::WorldDiagnostics world_diagnostics_{};
        render::PlayerDiagnostics player_diagnostics_{};
        render::InteractionDiagnostics interaction_diagnostics_{};
        std::vector<render::LocalLight> local_lights_{};
        render::Texture2D blockout_atlas_{};
        float scene_time_seconds_ = 0.0F;
    };
} // namespace RelayCore

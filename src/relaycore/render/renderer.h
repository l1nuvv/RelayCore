/**
 * @file src/relaycore/render/renderer.h
 * @brief Renderer entry point used by the application and game layer.
 */

#pragma once

#include <glm/mat4x4.hpp>

#include "relaycore/app/frame_timer.h"
#include "relaycore/core/engine_config.h"
#include "relaycore/debug/imgui_overlay.h"
#include "relaycore/render/camera.h"
#include "relaycore/render/mesh.h"
#include "relaycore/render/shader_program.h"

namespace RelayCore
{
    class Window;
    class AudioSystem;
} // namespace RelayCore

namespace RelayCore
{
    /**
     * @brief Owns the current render pipeline and debug overlay.
     */
    class Renderer {
    public:
        /**
         * @brief Creates a renderer with the supplied configuration.
         * @param config Renderer startup configuration.
         */
        explicit Renderer(const render::RendererConfig& config = render::RendererConfig{});

        /**
         * @brief Initializes OpenGL state and render resources.
         * @param window Window that owns the active OpenGL context.
         */
        void initialize(const Window& window);

        /**
         * @brief Forwards an SDL event to renderer-owned subsystems.
         * @param event SDL event received from the main loop.
         */
        void process_event(const SDL_Event& event);

        /**
         * @brief Updates viewport-dependent state after a resize.
         * @param width New framebuffer width.
         * @param height New framebuffer height.
         */
        void on_resize(int width, int height) noexcept;

        /**
         * @brief Begins a new frame and clears the framebuffer.
         */
        void begin_frame();

        /**
         * @brief Draws a color cube using the supplied model transform.
         * @param model_matrix World-space model matrix for the cube.
         */
        void draw_color_cube(const glm::mat4& model_matrix);

        /**
         * @brief Renders the debug overlay for the current frame.
         * @param frame Current frame timing information.
         * @param audio_system Audio system whose state is displayed.
         */
        void draw_debug_overlay(const FrameInfo& frame, const AudioSystem& audio_system);

        /**
         * @brief Returns the mutable render camera.
         * @return Renderer camera.
         */
        [[nodiscard]] auto camera() noexcept -> render::PerspectiveCamera&;

        /**
         * @brief Returns the immutable render camera.
         * @return Renderer camera.
         */
        [[nodiscard]] auto camera() const noexcept -> const render::PerspectiveCamera&;

    private:
        render::RendererConfig    config_{}; ///< Stored renderer configuration.
        bool                      initialized_     = false; ///< True after resource initialization succeeds.
        int                       viewport_width_  = 1280; ///< Current framebuffer width.
        int                       viewport_height_ = 720; ///< Current framebuffer height.
        render::PerspectiveCamera camera_{}; ///< Active render camera.
        render::ShaderProgram     shader_program_{}; ///< Shader program used for debug geometry.
        render::ColorCubeMesh     cube_mesh_{}; ///< Debug cube mesh.
        debug::ImGuiOverlay       imgui_overlay_{}; ///< Dear ImGui debug overlay.
    };
} // namespace RelayCore

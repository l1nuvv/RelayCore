#include "relaycore/render/renderer.h"

#include <glad/gl.h>
#include <glm/mat4x4.hpp>

#include "relaycore/audio/audio_system.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/core/profiling.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/gl_debug.h"
#include "relaycore/render/gl_loader.h"

namespace RelayCore
{
    Renderer::Renderer(const render::RendererConfig& config) :
        config_(config), camera_(config.camera), imgui_overlay_(config.imgui)
    {}

    void Renderer::initialize(const Window& window)
    {
        if (initialized_) { return; }

        render::gl::load();
        render::gl_debug::initialize();

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        const char* vendor       = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        const char* renderer     = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        const char* version      = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        const char* glsl_version = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));

        log::info("GL vendor: {}", vendor != nullptr ? vendor : "unknown");
        log::info("GL renderer: {}", renderer != nullptr ? renderer : "unknown");
        log::info("GL version: {}", version != nullptr ? version : "unknown");
        log::info("GLSL version: {}", glsl_version != nullptr ? glsl_version : "unknown");
        log::debug("Asset root: {}", core::asset_root_path().string());

        shader_program_.load_from_files(core::asset_path("shaders/basic.vert"), core::asset_path("shaders/basic.frag"));
        cube_mesh_.initialize();
        camera_.set_viewport(viewport_width_, viewport_height_);
        imgui_overlay_.initialize(window);

        initialized_ = true;
    }

    void Renderer::process_event(const SDL_Event& event) { imgui_overlay_.process_event(event); }

    void Renderer::on_resize(const int width, const int height) noexcept
    {
        viewport_width_  = width;
        viewport_height_ = height;
        camera_.set_viewport(width, height);
        if (initialized_) { glViewport(0, 0, width, height); }
    }

    void Renderer::begin_frame()
    {
        RELAYCORE_PROFILE_ZONE_N("Renderer::begin_frame");
        {
            RELAYCORE_PROFILE_ZONE_N("glClearColor");
            glClearColor(config_.clear_r, config_.clear_g, config_.clear_b, 1.0F);
        }
        {
            RELAYCORE_PROFILE_ZONE_N("glClear");
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
    }

    void Renderer::draw_color_cube(const glm::mat4& model_matrix)
    {
        RELAYCORE_PROFILE_ZONE_N("Renderer::draw_color_cube");
        shader_program_.bind();
        constexpr GLint kMvpLocation = 0;
        const glm::mat4 mvp          = camera_.view_projection_matrix() * model_matrix;
        shader_program_.set_mat4(kMvpLocation, mvp);
        cube_mesh_.draw();
    }

    void Renderer::draw_color_mesh(const render::StaticColorMesh& mesh, const glm::mat4& model_matrix)
    {
        RELAYCORE_PROFILE_ZONE_N("Renderer::draw_color_mesh");
        shader_program_.bind();
        constexpr GLint kMvpLocation = 0;
        const glm::mat4 mvp          = camera_.view_projection_matrix() * model_matrix;
        shader_program_.set_mat4(kMvpLocation, mvp);
        mesh.draw();
    }

    void Renderer::draw_debug_overlay(const FrameInfo& frame, const AudioSystem& audio_system)
    {
        if (!imgui_overlay_.enabled()) { return; }

        imgui_overlay_.update_diagnostics(frame);

        if (!imgui_overlay_.diagnostics_visible()) { return; }
        {
            RELAYCORE_PROFILE_ZONE_N("imgui.begin_frame");
            imgui_overlay_.begin_frame();
        }
        {
            RELAYCORE_PROFILE_ZONE_N("imgui.draw_diagnostics");
            imgui_overlay_.draw_diagnostics(frame, camera_, audio_system, world_diagnostics_);
        }
        {
            RELAYCORE_PROFILE_ZONE_N("imgui.render");
            imgui_overlay_.render();
        }
    }

    void Renderer::set_world_diagnostics(const render::WorldDiagnostics& diagnostics) noexcept
    {
        world_diagnostics_ = diagnostics;
    }

    auto Renderer::world_diagnostics() const noexcept -> const render::WorldDiagnostics& { return world_diagnostics_; }

    auto Renderer::camera() noexcept -> render::PerspectiveCamera& { return camera_; }

    auto Renderer::camera() const noexcept -> const render::PerspectiveCamera& { return camera_; }

    auto Renderer::diagnostics_visible() const noexcept -> bool { return imgui_overlay_.diagnostics_visible(); }

    void Renderer::set_diagnostics_visible(const bool visible) noexcept
    {
        imgui_overlay_.set_diagnostics_visible(visible);
    }

    void Renderer::toggle_diagnostics_visible() noexcept { imgui_overlay_.toggle_diagnostics_visible(); }
} // namespace RelayCore

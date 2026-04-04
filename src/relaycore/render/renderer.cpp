#include "relaycore/render/renderer.h"

#include "relaycore/audio/audio_system.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/core/profiling.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/gl_debug.h"
#include "relaycore/render/gl_loader.h"

#include <glad/gl.h>
#include <glm/mat4x4.hpp>

namespace RelayCore
{
    Renderer::Renderer(const render::RendererConfig& config) :
        config_(config), camera_(config.camera), imgui_overlay_(config.imgui)
    {}

    void Renderer::initialize(const Window& window)
    {
        if (initialized_)
        {
            return;
        }

        render::gl::load();
        render::gl_debug::initialize();

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        const char* glsl_version = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));

        log::info("GL vendor: {}", vendor != nullptr ? vendor : "unknown");
        log::info("GL renderer: {}", renderer != nullptr ? renderer : "unknown");
        log::info("GL version: {}", version != nullptr ? version : "unknown");
        log::info("GLSL version: {}", glsl_version != nullptr ? glsl_version : "unknown");
        log::debug("Asset root: {}", core::asset_root_path().string());

        shader_program_.load_from_files(core::asset_path("shaders/basic.vert"), core::asset_path("shaders/basic.frag"));
        camera_.set_viewport(viewport_width_, viewport_height_);
        imgui_overlay_.initialize(window);

        initialized_ = true;
    }

    void Renderer::process_event(const SDL_Event& event)
    {
        imgui_overlay_.process_event(event);
    }

    void Renderer::on_resize(const int width, const int height) noexcept
    {
        viewport_width_ = width;
        viewport_height_ = height;
        camera_.set_viewport(width, height);
        if (initialized_)
        {
            glViewport(0, 0, width, height);
        }
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

    void Renderer::draw_render_queue(const std::span<const render::RenderItem> items)
    {
        RELAYCORE_PROFILE_ZONE_N("Renderer::draw_render_queue");
        if (items.empty())
        {
            return;
        }

        shader_program_.bind();
        constexpr GLint kMvpLocation = 0;
        constexpr GLint kModelLocation = 1;
        constexpr GLint kCameraPositionLocation = 2;
        constexpr GLint kFogColorLocation = 3;
        constexpr GLint kFogStartLocation = 4;
        constexpr GLint kFogEndLocation = 5;
        const glm::mat4 view_projection = camera_.view_projection_matrix();
        shader_program_.set_vec3(kCameraPositionLocation, camera_.position());
        shader_program_.set_vec3(kFogColorLocation, config_.fog_color);
        shader_program_.set_float(
            kFogStartLocation,
            config_.fog_enabled ? config_.fog_start : config_.camera.far_plane
        );
        shader_program_.set_float(kFogEndLocation, config_.fog_enabled ? config_.fog_end : config_.camera.far_plane);

        for (const render::RenderItem& item : items)
        {
            if (item.mesh == nullptr)
            {
                continue;
            }
            const glm::mat4 mvp = view_projection * item.model_matrix;
            shader_program_.set_mat4(kMvpLocation, mvp);
            shader_program_.set_mat4(kModelLocation, item.model_matrix);
            item.mesh->draw();
        }
    }

    void Renderer::draw_debug_overlay(const FrameInfo& frame, const AudioSystem& audio_system)
    {
        if (!imgui_overlay_.enabled())
        {
            return;
        }

        imgui_overlay_.update_diagnostics(frame);

        if (!imgui_overlay_.should_render(interaction_diagnostics_))
        {
            return;
        }
        {
            RELAYCORE_PROFILE_ZONE_N("imgui.begin_frame");
            imgui_overlay_.begin_frame();
        }
        if (imgui_overlay_.diagnostics_visible())
        {
            RELAYCORE_PROFILE_ZONE_N("imgui.draw_diagnostics");
            imgui_overlay_.draw_diagnostics(
                frame,
                camera_,
                audio_system,
                world_diagnostics_,
                player_diagnostics_,
                interaction_diagnostics_
            );
        }
        imgui_overlay_.draw_interaction_prompt(interaction_diagnostics_);
        {
            RELAYCORE_PROFILE_ZONE_N("imgui.render");
            imgui_overlay_.render();
        }
    }

    void Renderer::set_world_diagnostics(const render::WorldDiagnostics& diagnostics) noexcept
    {
        world_diagnostics_ = diagnostics;
    }

    auto Renderer::world_diagnostics() const noexcept -> const render::WorldDiagnostics&
    {
        return world_diagnostics_;
    }

    void Renderer::set_player_diagnostics(const render::PlayerDiagnostics& diagnostics) noexcept
    {
        player_diagnostics_ = diagnostics;
    }

    auto Renderer::player_diagnostics() const noexcept -> const render::PlayerDiagnostics&
    {
        return player_diagnostics_;
    }

    void Renderer::set_interaction_diagnostics(const render::InteractionDiagnostics& diagnostics) noexcept
    {
        interaction_diagnostics_ = diagnostics;
    }

    auto Renderer::interaction_diagnostics() const noexcept -> const render::InteractionDiagnostics&
    {
        return interaction_diagnostics_;
    }

    auto Renderer::camera() noexcept -> render::PerspectiveCamera&
    {
        return camera_;
    }

    auto Renderer::camera() const noexcept -> const render::PerspectiveCamera&
    {
        return camera_;
    }

    auto Renderer::diagnostics_visible() const noexcept -> bool
    {
        return imgui_overlay_.diagnostics_visible();
    }

    void Renderer::set_diagnostics_visible(const bool visible) noexcept
    {
        imgui_overlay_.set_diagnostics_visible(visible);
    }

    void Renderer::toggle_diagnostics_visible() noexcept
    {
        imgui_overlay_.toggle_diagnostics_visible();
    }
} // namespace RelayCore

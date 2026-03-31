#include "render/renderer.hpp"

#include <glad/gl.h>
#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "audio/audio_system.hpp"
#include "core/log.hpp"
#include "core/paths.hpp"
#include "core/profiling.hpp"
#include "platform/window.hpp"
#include "render/gl_debug.hpp"
#include "render/gl_loader.hpp"

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

    auto Renderer::camera() noexcept -> render::PerspectiveCamera& { return camera_; }

    auto Renderer::camera() const noexcept -> const render::PerspectiveCamera& { return camera_; }

    void Renderer::render_frame(const FrameInfo& frame, const AudioSystem& audio_system)
    {
        RELAYCORE_PROFILE_ZONE_N("Renderer::render_frame");
        {
            RELAYCORE_PROFILE_ZONE_N("clear");
            glClearColor(config_.clear_r, config_.clear_g, config_.clear_b, 1.0F);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        const glm::mat4 mvp = [&]() {
            RELAYCORE_PROFILE_ZONE_N("update_matrices");

            const float angle_a = static_cast<float>(frame.elapsed_seconds) * 0.85F;
            const float angle_b = static_cast<float>(frame.elapsed_seconds) * 0.45F;

            glm::mat4 model(1.0F);
            model = glm::rotate(model, angle_a, glm::vec3(0.25F, 1.0F, 0.10F));
            model = glm::rotate(model, angle_b, glm::vec3(1.0F, 0.15F, 0.30F));

            return camera_.view_projection_matrix() * model;
        }();
        {
            RELAYCORE_PROFILE_ZONE_N("draw_cube");
            shader_program_.bind();
            constexpr GLint kMvpLocation = 0;
            shader_program_.set_mat4(kMvpLocation, mvp);
            cube_mesh_.draw();
        }
        if (imgui_overlay_.enabled())
        {
            {
                RELAYCORE_PROFILE_ZONE_N("imgui.begin_frame");
                imgui_overlay_.begin_frame();
            }
            {
                RELAYCORE_PROFILE_ZONE_N("imgui.draw_diagnostics");
                imgui_overlay_.draw_diagnostics(frame, camera_, audio_system);
            }
            {
                RELAYCORE_PROFILE_ZONE_N("imgui.render");
                imgui_overlay_.render();
            }
        }
    }
} // namespace RelayCore

/**
 * @file src/relaycore/debug/imgui_overlay.cpp
 * @brief Dear ImGui overlay implementation.
 */

#include "relaycore/debug/imgui_overlay.h"

#include <cmath>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include "relaycore/audio/audio_system.h"
#include "relaycore/core/log.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/camera.h"

namespace RelayCore::debug
{
    ImGuiOverlay::ImGuiOverlay(const ImGuiOverlayConfig& config) :
        config_(config) {}

    ImGuiOverlay::~ImGuiOverlay() noexcept { shutdown(); }

    void ImGuiOverlay::initialize(const Window& window)
    {
        if (initialized_ || !config_.enabled) { return; }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io    = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        ImGui_ImplSDL3_InitForOpenGL(window.handle(), window.gl_context());
        ImGui_ImplOpenGL3_Init("#version 460 core");

        fps_log_accumulator_ = 0.0F;
        initialized_         = true;

        log::info("ImGui overlay initialized");
    }

    void ImGuiOverlay::shutdown() noexcept
    {
        if (!initialized_) { return; }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        fps_log_accumulator_ = 0.0F;
        initialized_         = false;
    }

    void ImGuiOverlay::process_event(const SDL_Event& event)
    {
        if (!initialized_) { return; }
        ImGui_ImplSDL3_ProcessEvent(&event);
    }

    void ImGuiOverlay::begin_frame()
    {
        if (!initialized_) { return; }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiOverlay::draw_diagnostics(
        const FrameInfo&                 frame,
        const render::PerspectiveCamera& camera,
        const AudioSystem&               audio_system)
    {
        if (!initialized_) { return; }

        ImGuiIO& io = ImGui::GetIO();

        const float fps           = io.Framerate;
        const float frame_time_ms =
            fps > 0.0F ? (1000.0F / fps) : static_cast<float>(frame.delta_seconds * 1000.0);

        if (config_.log_fps_to_console)
        {
            fps_log_accumulator_ += io.DeltaTime;

            const float interval =
                config_.fps_log_interval_seconds > 0.1F
                    ? config_.fps_log_interval_seconds
                    : 0.1F;

            if (fps_log_accumulator_ >= interval)
            {
                fps_log_accumulator_ = std::fmod(fps_log_accumulator_, interval);
                log::info("FPS: {:.1f} | Frame time: {:.3f} ms", fps, frame_time_ms);
            }
        }

        ImGui::Begin("RelayCore Diagnostics");
        ImGui::Text("Frame time: %.3f ms", frame_time_ms);
        ImGui::Text("FPS: %.1f", fps);

        const auto& position = camera.position();
        ImGui::SeparatorText("Camera");
        ImGui::Text("Position: %.2f %.2f %.2f", position.x, position.y, position.z);
        ImGui::Text("Yaw/Pitch: %.2f / %.2f", camera.yaw_radians(), camera.pitch_radians());

        ImGui::SeparatorText("Audio");
        ImGui::Text("Initialized: %s", audio_system.is_initialized() ? "yes" : "no");
        ImGui::Text("Master volume: %.2f", audio_system.config().master_volume);
        ImGui::End();

        if (config_.show_demo_window) { ImGui::ShowDemoWindow(&config_.show_demo_window); }
    }

    void ImGuiOverlay::render() const
    {
        if (!initialized_) { return; }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    auto ImGuiOverlay::is_initialized() const noexcept -> bool { return initialized_; }

    auto ImGuiOverlay::enabled() const noexcept -> bool { return config_.enabled; }
} // namespace RelayCore::debug

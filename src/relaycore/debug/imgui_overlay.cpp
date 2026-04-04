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
#include "relaycore/core/profiling.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/camera.h"

namespace RelayCore::debug
{
    ImGuiOverlay::ImGuiOverlay(const ImGuiOverlayConfig& config) : config_(config) {}

    ImGuiOverlay::~ImGuiOverlay() noexcept { shutdown(); }

    void ImGuiOverlay::initialize(const Window& window)
    {
        if (initialized_ || !config_.enabled) { return; }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        ImGui_ImplSDL3_InitForOpenGL(window.handle(), window.gl_context());
        ImGui_ImplOpenGL3_Init("#version 460 core");

        window_                   = &window;
        fps_log_accumulator_      = 0.0F;
        frame_time_sample_count_  = 0;
        frame_time_sample_cursor_ = 0;
        frame_time_samples_.fill(0.0F);
        initialized_ = true;

        log::info("ImGui overlay initialized");
    }

    void ImGuiOverlay::shutdown() noexcept
    {
        if (!initialized_) { return; }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        window_                   = nullptr;
        fps_log_accumulator_      = 0.0F;
        frame_time_sample_count_  = 0;
        frame_time_sample_cursor_ = 0;
        frame_time_samples_.fill(0.0F);
        initialized_ = false;
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

    void ImGuiOverlay::push_frame_time_sample(const float frame_time_ms) noexcept
    {
        frame_time_samples_[frame_time_sample_cursor_] = frame_time_ms;
        frame_time_sample_cursor_                      = (frame_time_sample_cursor_ + 1) % frame_time_samples_.size();

        if (frame_time_sample_count_ < frame_time_samples_.size()) { ++frame_time_sample_count_; }
    }

    auto ImGuiOverlay::average_frame_time_ms() const noexcept -> float
    {
        if (frame_time_sample_count_ == 0) { return 0.0F; }

        float sum = 0.0F;
        for (std::size_t i = 0; i < frame_time_sample_count_; ++i)
        {
            sum += frame_time_samples_[i];
        }

        return sum / static_cast<float>(frame_time_sample_count_);
    }

    auto ImGuiOverlay::vsync_label(const int swap_interval) -> const char*
    {
        switch (swap_interval)
        {
            case -1: return "adaptive";
            case 1: return "on";
            default: return "off";
        }
    }

    void ImGuiOverlay::draw_diagnostics(
        const FrameInfo& frame, const render::PerspectiveCamera& camera, const AudioSystem& audio_system
    )
    {
        (void) frame;

        if (!initialized_ || !diagnostics_visible_) { return; }

        const float refresh_rate_hz = window_ != nullptr ? window_->refresh_rate_hz() : 0.0F;
        const int   swap_interval   = window_ != nullptr ? window_->swap_interval() : 0;

        ImGui::Begin("RelayCore Diagnostics");
        ImGui::Text("Frame time: %.3f ms", frame_time_ms_);
        ImGui::Text("Frame time avg (%zu): %.3f ms", frame_time_sample_count_, avg_frame_time_ms_);
        ImGui::Text("FPS: %.1f", fps_);
        ImGui::Text("FPS avg: %.1f", avg_fps_);

        if (refresh_rate_hz > 0.0F)
        {
            ImGui::Text("Refresh rate: %.1f Hz", refresh_rate_hz);
        } else
        {
            ImGui::TextUnformatted("Refresh rate: unknown");
        }

        ImGui::Text("VSync: %s (%d)", vsync_label(swap_interval), swap_interval);

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
        {
            RELAYCORE_PROFILE_ZONE_N("ImGui::Render");
            ImGui::Render();
        }
        {
            RELAYCORE_PROFILE_ZONE_N("ImGui_ImplOpenGL3_RenderDrawData");
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }
    }

    auto ImGuiOverlay::is_initialized() const noexcept -> bool { return initialized_; }

    auto ImGuiOverlay::enabled() const noexcept -> bool { return config_.enabled; }

    auto ImGuiOverlay::diagnostics_visible() const noexcept -> bool { return diagnostics_visible_; }

    void ImGuiOverlay::set_diagnostics_visible(bool visible) noexcept { diagnostics_visible_ = visible; }

    void ImGuiOverlay::toggle_diagnostics_visible() noexcept { diagnostics_visible_ = !diagnostics_visible_; }

    void ImGuiOverlay::update_diagnostics(const FrameInfo& frame) noexcept
    {
        frame_time_ms_ = static_cast<float>(frame.delta_seconds) * 1000.0;
        push_frame_time_sample(frame_time_ms_);

        avg_frame_time_ms_ = average_frame_time_ms();
        fps_               = frame.delta_seconds > 0.0 ? static_cast<float>(1.0 / frame.delta_seconds) : 0.0;
        avg_fps_           = avg_frame_time_ms_ > 0.0F ? (1000.0F / avg_frame_time_ms_) : 0.0F;

        if (config_.log_fps_to_console)
        {
            fps_log_accumulator_ += static_cast<float>(frame.delta_seconds);

            const float interval = config_.fps_log_interval_seconds > 0.1F ? config_.fps_log_interval_seconds : 0.1F;
            if (fps_log_accumulator_ >= interval)
            {
                fps_log_accumulator_ = std::fmod(fps_log_accumulator_, interval);
                log::info(
                    "FPS: {:.1f} | Frame time: {:.3f} ms | Frame time avg: {:.3f} ms",
                    fps_,
                    frame_time_ms_,
                    avg_frame_time_ms_
                );
            }
        }
    }
} // namespace RelayCore::debug

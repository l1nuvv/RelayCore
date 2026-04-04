/**
 * @file src/relaycore/debug/imgui_overlay.cpp
 * @brief Dear ImGui overlay implementation.
 */

#include "relaycore/debug/imgui_overlay.h"

#include "relaycore/audio/audio_system.h"
#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/camera.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <limits>
#include <string>

namespace RelayCore::debug
{
    namespace
    {
        [[nodiscard]] auto movement_mode_label(std::uint32_t movement_mode) -> const char*
        {
            switch (movement_mode)
            {
                case 0:
                    return "grounded";
                case 1:
                    return "air";
                case 2:
                    return "noclip";
                default:
                    return "unknown";
            }
        }

        [[nodiscard]] auto has_interaction_prompt(const render::InteractionDiagnostics& interaction) -> bool
        {
            return interaction.has_focused_usable && !interaction.focused_label.empty();
        }

        void draw_vec3_row(const char* label, const glm::vec3& value)
        {
            ImGui::Text("%s: %.2f %.2f %.2f", label, value.x, value.y, value.z);
        }
    } // namespace

    ImGuiOverlay::ImGuiOverlay(const ImGuiOverlayConfig& config) :
        config_(config), diagnostics_visible_(config.diagnostics_visible_on_startup)
    {}

    ImGuiOverlay::~ImGuiOverlay() noexcept
    {
        shutdown();
    }

    void ImGuiOverlay::initialize(const Window& window)
    {
        if (initialized_ || !config_.enabled)
        {
            return;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        ImGui_ImplSDL3_InitForOpenGL(window.handle(), window.gl_context());
        ImGui_ImplOpenGL3_Init("#version 460 core");

        window_ = &window;
        fps_log_accumulator_ = 0.0F;
        frame_time_sample_count_ = 0;
        frame_time_sample_cursor_ = 0;
        frame_time_samples_.fill(0.0F);
        initialized_ = true;
        diagnostics_visible_ = config_.diagnostics_visible_on_startup;

        log::info("ImGui overlay initialized");
    }

    void ImGuiOverlay::shutdown() noexcept
    {
        if (!initialized_)
        {
            return;
        }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        window_ = nullptr;
        fps_log_accumulator_ = 0.0F;
        frame_time_sample_count_ = 0;
        frame_time_sample_cursor_ = 0;
        frame_time_samples_.fill(0.0F);
        frame_time_ms_ = 0.0F;
        avg_frame_time_ms_ = 0.0F;
        fps_ = 0.0F;
        avg_fps_ = 0.0F;
        initialized_ = false;
    }

    void ImGuiOverlay::process_event(const SDL_Event& event)
    {
        if (!initialized_)
        {
            return;
        }

        ImGui_ImplSDL3_ProcessEvent(&event);
    }

    void ImGuiOverlay::begin_frame()
    {
        if (!initialized_)
        {
            return;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiOverlay::push_frame_time_sample(const float frame_time_ms) noexcept
    {
        frame_time_samples_[frame_time_sample_cursor_] = frame_time_ms;
        frame_time_sample_cursor_ = (frame_time_sample_cursor_ + 1) % frame_time_samples_.size();

        if (frame_time_sample_count_ < frame_time_samples_.size())
        {
            ++frame_time_sample_count_;
        }
    }

    auto ImGuiOverlay::average_frame_time_ms() const noexcept -> float
    {
        if (frame_time_sample_count_ == 0)
        {
            return 0.0F;
        }

        float sum = 0.0F;
        for (std::size_t i = 0; i < frame_time_sample_count_; ++i)
        {
            sum += frame_time_samples_[i];
        }

        return sum / static_cast<float>(frame_time_sample_count_);
    }

    auto ImGuiOverlay::minimum_frame_time_ms() const noexcept -> float
    {
        if (frame_time_sample_count_ == 0)
        {
            return 0.0F;
        }

        float minimum = std::numeric_limits<float>::max();
        for (std::size_t i = 0; i < frame_time_sample_count_; ++i)
        {
            minimum = std::min(minimum, frame_time_samples_[i]);
        }

        return minimum;
    }

    auto ImGuiOverlay::maximum_frame_time_ms() const noexcept -> float
    {
        if (frame_time_sample_count_ == 0)
        {
            return 0.0F;
        }

        float maximum = 0.0F;
        for (std::size_t i = 0; i < frame_time_sample_count_; ++i)
        {
            maximum = std::max(maximum, frame_time_samples_[i]);
        }

        return maximum;
    }

    auto ImGuiOverlay::vsync_label(const int swap_interval) -> const char*
    {
        switch (swap_interval)
        {
            case -1:
                return "adaptive";
            case 1:
                return "on";
            default:
                return "off";
        }
    }

    void ImGuiOverlay::draw_diagnostics(
        const FrameInfo& frame,
        const render::PerspectiveCamera& camera,
        const AudioSystem& audio_system,
        const render::WorldDiagnostics& world,
        const render::PlayerDiagnostics& player,
        const render::InteractionDiagnostics& interaction
    )
    {
        (void) frame;

        if (!initialized_ || !diagnostics_visible_)
        {
            return;
        }

        const float refresh_rate_hz = window_ != nullptr ? window_->refresh_rate_hz() : 0.0F;
        const int swap_interval = window_ != nullptr ? window_->swap_interval() : 0;
        const float refresh_budget_ms = refresh_rate_hz > 0.0F ? (1000.0F / refresh_rate_hz) : 0.0F;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(
            ImVec2(viewport->WorkPos.x + 12.0F, viewport->WorkPos.y + 12.0F),
            ImGuiCond_FirstUseEver
        );
        ImGui::SetNextWindowSize(ImVec2(420.0F, 0.0F), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.92F);

        constexpr ImGuiWindowFlags kWindowFlags = ImGuiWindowFlags_NoSavedSettings;

        if (!ImGui::Begin("RelayCore Diagnostics", nullptr, kWindowFlags))
        {
            ImGui::End();
            return;
        }

        if (ImGui::CollapsingHeader("Frame", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Frame time: %.3f ms", frame_time_ms_);
            ImGui::Text("Frame avg (%zu): %.3f ms", frame_time_sample_count_, avg_frame_time_ms_);
            ImGui::Text("Frame min/max: %.3f / %.3f ms", minimum_frame_time_ms(), maximum_frame_time_ms());
            ImGui::Text("FPS: %.1f (avg %.1f)", fps_, avg_fps_);

            if (refresh_rate_hz > 0.0F)
            {
                ImGui::Text("Refresh rate: %.1f Hz", refresh_rate_hz);
                ImGui::Text("Refresh budget: %.3f ms", refresh_budget_ms);
            } else
            {
                ImGui::TextUnformatted("Refresh rate: unknown");
            }

            ImGui::Text("VSync: %s (%d)", vsync_label(swap_interval), swap_interval);

            if (config_.show_frame_time_plot && frame_time_sample_count_ > 1)
            {
                const float plot_max = std::max(20.0F, maximum_frame_time_ms() * 1.15F);
                ImGui::PlotLines(
                    "##frame_time_plot",
                    frame_time_samples_.data(),
                    static_cast<int>(frame_time_sample_count_),
                    static_cast<int>(
                        frame_time_sample_count_ == frame_time_samples_.size() ? frame_time_sample_cursor_ : 0
                    ),
                    nullptr,
                    0.0F,
                    plot_max,
                    ImVec2(-1.0F, 64.0F)
                );
            }

            ImGui::Checkbox("Show frame plot", &config_.show_frame_time_plot);
            ImGui::SameLine();
            ImGui::Checkbox("Log FPS", &config_.log_fps_to_console);
            ImGui::Checkbox("ImGui demo window", &config_.show_demo_window);
        }

        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const auto& position = camera.position();
            ImGui::Text("Position: %.2f %.2f %.2f", position.x, position.y, position.z);
            ImGui::Text("Yaw/Pitch: %.2f / %.2f", camera.yaw_radians(), camera.pitch_radians());
        }

        if (ImGui::CollapsingHeader("World", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Solids: %u visible / %u total", world.visible_solids, world.total_solids);
            ImGui::Text("Draw calls: %u", world.draw_calls);
            ImGui::Text("Triangles: %u", world.triangles);
        }

        if (ImGui::CollapsingHeader("Player", ImGuiTreeNodeFlags_DefaultOpen))
        {
            draw_vec3_row("Position", player.position);
            draw_vec3_row("Velocity", player.velocity);
            ImGui::Text("Speed: %.2f", player.speed);
            ImGui::Text("Grounded: %s", player.grounded ? "yes" : "no");
            ImGui::Text("Mode: %s", movement_mode_label(player.movement_mode));
            ImGui::Text("Collision: %s", player.collision_enabled ? "on" : "off");
        }

        if (ImGui::CollapsingHeader("Interaction", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const char* focused_label =
                interaction.focused_label.empty() ? "<none>" : interaction.focused_label.c_str();
            const char* last_target = interaction.last_target.empty() ? "<none>" : interaction.last_target.c_str();

            ImGui::Text("Focused usable: %s", interaction.has_focused_usable ? "yes" : "no");
            ImGui::Text("Focused label: %s", focused_label);
            ImGui::Text("Focus distance: %.2f", interaction.focused_distance);
            ImGui::Text("Last target: %s", last_target);
            ImGui::Text("Doors: %u active / %u total", interaction.active_doors, interaction.total_doors);
            ImGui::Text("Triggers: %u fired / %u total", interaction.fired_triggers, interaction.total_triggers);
            ImGui::Text("Use pressed: %s", interaction.use_pressed ? "yes" : "no");
            ImGui::Checkbox("Show interaction prompt", &config_.show_interaction_prompt);
        }

        if (ImGui::CollapsingHeader("Audio", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Initialized: %s", audio_system.is_initialized() ? "yes" : "no");
            ImGui::Text("Master volume: %.2f", audio_system.config().master_volume);
        }

        ImGui::End();

        if (config_.show_demo_window)
        {
            ImGui::ShowDemoWindow(&config_.show_demo_window);
        }
    }

    void ImGuiOverlay::draw_interaction_prompt(const render::InteractionDiagnostics& interaction) const
    {
        if (!initialized_ || !config_.show_interaction_prompt || !has_interaction_prompt(interaction))
        {
            return;
        }

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const std::string binding_label = interaction.use_binding_label.empty() ? "E" : interaction.use_binding_label;
        std::string prompt_label = "[" + binding_label + "] " + interaction.focused_label;

        if (interaction.focused_distance > 0.0F)
        {
            char distance_label[32]{};
            std::snprintf(distance_label, sizeof(distance_label), " (%.1fm)", interaction.focused_distance);
            prompt_label += distance_label;
        }

        ImGui::SetNextWindowPos(
            ImVec2(
                viewport->WorkPos.x + (viewport->WorkSize.x * 0.5F),
                viewport->WorkPos.y + viewport->WorkSize.y - 56.0F
            ),
            ImGuiCond_Always,
            ImVec2(0.5F, 1.0F)
        );
        ImGui::SetNextWindowBgAlpha(0.82F);

        constexpr ImGuiWindowFlags kPromptFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs
                                                  | ImGuiWindowFlags_AlwaysAutoResize
                                                  | ImGuiWindowFlags_NoSavedSettings;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0F, 8.0F));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06F, 0.07F, 0.08F, 0.82F));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96F, 0.93F, 0.72F, 1.0F));
        ImGui::Begin("RelayCore Interaction Prompt", nullptr, kPromptFlags);
        ImGui::TextUnformatted(prompt_label.c_str());
        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }

    void ImGuiOverlay::render() const
    {
        if (!initialized_)
        {
            return;
        }

        {
            RELAYCORE_PROFILE_ZONE_N("ImGui::Render");
            ImGui::Render();
        }
        {
            RELAYCORE_PROFILE_ZONE_N("ImGui_ImplOpenGL3_RenderDrawData");
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }
    }

    auto ImGuiOverlay::is_initialized() const noexcept -> bool
    {
        return initialized_;
    }

    auto ImGuiOverlay::enabled() const noexcept -> bool
    {
        return config_.enabled;
    }

    auto ImGuiOverlay::diagnostics_visible() const noexcept -> bool
    {
        return diagnostics_visible_;
    }

    void ImGuiOverlay::set_diagnostics_visible(const bool visible) noexcept
    {
        diagnostics_visible_ = visible;
    }

    void ImGuiOverlay::toggle_diagnostics_visible() noexcept
    {
        diagnostics_visible_ = !diagnostics_visible_;
    }

    auto ImGuiOverlay::should_render(const render::InteractionDiagnostics& interaction) const noexcept -> bool
    {
        return initialized_
               && (diagnostics_visible_ || (config_.show_interaction_prompt && has_interaction_prompt(interaction))
                   || config_.show_demo_window);
    }

    void ImGuiOverlay::update_diagnostics(const FrameInfo& frame) noexcept
    {
        frame_time_ms_ = static_cast<float>(frame.delta_seconds) * 1000.0F;
        push_frame_time_sample(frame_time_ms_);

        avg_frame_time_ms_ = average_frame_time_ms();
        fps_ = frame.delta_seconds > 0.0 ? static_cast<float>(1.0 / frame.delta_seconds) : 0.0F;
        avg_fps_ = avg_frame_time_ms_ > 0.0F ? (1000.0F / avg_frame_time_ms_) : 0.0F;

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

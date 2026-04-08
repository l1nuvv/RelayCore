/**
 * @file src/relaycore/debug/imgui_overlay.cpp
 * @brief Dear ImGui overlay implementation.
 */

#include "relaycore/debug/imgui_overlay.h"

#include "relaycore/audio/audio_system.h"
#include "relaycore/core/engine_config.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/core/profiling.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/camera.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <string>

namespace RelayCore::debug
{
    namespace
    {
        [[nodiscard]] auto movement_mode_label(const std::uint32_t movement_mode) -> const char*
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

        void configure_imgui_font(ImGuiIO& io)
        {
            io.Fonts->Clear();

            const ImWchar* glyph_ranges = io.Fonts->GetGlyphRangesCyrillic();
            ImFontConfig font_config{};
            font_config.OversampleH = 2;
            font_config.OversampleV = 2;
            font_config.PixelSnapH = false;

            const std::filesystem::path asset_font = core::asset_path("fonts/RobotoMono-Regular.ttf");
            const std::filesystem::path segoe_ui_font = "C:/Windows/Fonts/segoeui.ttf";
            const std::filesystem::path arial_font = "C:/Windows/Fonts/arial.ttf";

            for (const std::filesystem::path& font_path : {asset_font, segoe_ui_font, arial_font})
            {
                if (!std::filesystem::exists(font_path))
                {
                    continue;
                }

                if (io.Fonts->AddFontFromFileTTF(font_path.string().c_str(), 19.0F, &font_config, glyph_ranges)
                    != nullptr)
                {
                    log::info("ImGui font loaded from {}", font_path.string());
                    return;
                }
            }

            io.Fonts->AddFontDefault();
            log::warn("ImGui font fallback to default font; Cyrillic glyphs may be unavailable");
        }

        void draw_shadow_controls(render::RendererConfig& renderer_config)
        {
            if (!ImGui::CollapsingHeader("Shadow Tuning", ImGuiTreeNodeFlags_DefaultOpen))
            {
                return;
            }

            ImGui::Checkbox("Enable shadows", &renderer_config.shadow_enabled);
            ImGui::SliderFloat("Indirect bounce", &renderer_config.indirect_bounce_strength, 0.0F, 0.30F, "%.3f");
            ImGui::SliderFloat("Shadow strength", &renderer_config.shadow_strength, 0.0F, 1.0F, "%.3f");
            ImGui::SliderFloat("Shadow contact", &renderer_config.shadow_contact_strength, 0.0F, 0.30F, "%.3f");
            ImGui::SliderFloat("Near shadow bias", &renderer_config.shadow_bias_near, 0.0001F, 0.0100F, "%.4f");
            ImGui::SliderFloat("Shadow slope bias", &renderer_config.shadow_slope_bias_scale, 0.0F, 0.0200F, "%.4f");
            ImGui::SliderFloat("Near normal offset", &renderer_config.shadow_normal_offset_near, 0.0F, 0.080F, "%.3f");
            ImGui::SliderFloat("Shadow near PCF", &renderer_config.shadow_pcf_radius_near, 0.5F, 3.0F, "%.2f");
            ImGui::SliderFloat("Shadow far PCF", &renderer_config.shadow_pcf_radius_far, 0.5F, 4.0F, "%.2f");
            ImGui::SliderFloat(
                "Shadow split distance",
                &renderer_config.shadow_cascade_split_distance,
                2.0F,
                24.0F,
                "%.2f"
            );
            ImGui::SliderFloat("Shadow blend range", &renderer_config.shadow_cascade_blend_range, 0.0F, 6.0F, "%.2f");
            ImGui::Checkbox("Debug cascade tint", &renderer_config.shadow_debug_cascades);
            ImGui::SliderFloat("Shadow focus distance", &renderer_config.shadow_focus_distance, 0.0F, 24.0F, "%.2f");
            ImGui::SliderFloat("Shadow radius", &renderer_config.shadow_projection_radius, 2.0F, 28.0F, "%.2f");
            ImGui::SliderFloat("Shadow depth", &renderer_config.shadow_projection_depth, 4.0F, 48.0F, "%.2f");
            ImGui::SeparatorText("Far Cascade");
            ImGui::SliderFloat("Far shadow bias", &renderer_config.shadow_bias_far, 0.0001F, 0.0160F, "%.4f");
            ImGui::SliderFloat("Far normal offset", &renderer_config.shadow_normal_offset_far, 0.0F, 0.120F, "%.3f");
            ImGui::SliderFloat("Far focus distance", &renderer_config.shadow_far_focus_distance, 6.0F, 40.0F, "%.2f");
            ImGui::SliderFloat("Far radius", &renderer_config.shadow_far_projection_radius, 6.0F, 48.0F, "%.2f");
            ImGui::SliderFloat("Far depth", &renderer_config.shadow_far_projection_depth, 8.0F, 80.0F, "%.2f");
            ImGui::SliderFloat("Shadow light intensity", &renderer_config.shadow_light_intensity, 0.0F, 2.0F, "%.2f");
            ImGui::SliderFloat3(
                "Shadow light direction",
                &renderer_config.shadow_light_direction.x,
                -1.0F,
                1.0F,
                "%.2f"
            );
            ImGui::ColorEdit3("Shadow light color", &renderer_config.shadow_light_color.x);
        }

        void draw_shadow_preview(const render::WorldDiagnostics& world)
        {
            if (!world.shadow_enabled || world.shadow_preview_texture_id == 0)
            {
                return;
            }
            if (!ImGui::CollapsingHeader("Shadow Map Preview", ImGuiTreeNodeFlags_DefaultOpen))
            {
                return;
            }

            constexpr float preview_size = 192.0F;
            const ImTextureID near_texture_id =
                static_cast<ImTextureID>(static_cast<std::intptr_t>(world.shadow_preview_texture_id));
            ImGui::Text("Near");
            ImGui::Image(near_texture_id, ImVec2(preview_size, preview_size), ImVec2(0.0F, 1.0F), ImVec2(1.0F, 0.0F));

            if (world.shadow_far_preview_texture_id != 0)
            {
                const ImTextureID far_texture_id =
                    static_cast<ImTextureID>(static_cast<std::intptr_t>(world.shadow_far_preview_texture_id));
                ImGui::SameLine();
                ImGui::BeginGroup();
                ImGui::Text("Far");
                ImGui::Image(
                    far_texture_id,
                    ImVec2(preview_size, preview_size),
                    ImVec2(0.0F, 1.0F),
                    ImVec2(1.0F, 0.0F)
                );
                ImGui::EndGroup();
            }
        }

    } // namespace

    ImGuiOverlay::ImGuiOverlay(const ImGuiOverlayConfig& config) : config_(config) {}

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
        configure_imgui_font(io);

        ImGui::StyleColorsDark();

        ImGui_ImplSDL3_InitForOpenGL(window.handle(), window.gl_context());
        ImGui_ImplOpenGL3_Init("#version 460 core");

        window_ = &window;
        fps_log_accumulator_ = 0.0F;
        frame_time_sample_count_ = 0;
        frame_time_sample_cursor_ = 0;
        frame_time_samples_.fill(0.0F);
        initialized_ = true;

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
        render::RendererConfig& renderer_config,
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

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 12.0F, viewport->WorkPos.y + 12.0F), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.92F);
        constexpr ImGuiWindowFlags kWindowFlags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;

        ImGui::Begin("RelayCore Diagnostics", nullptr, kWindowFlags);
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

        ImGui::SeparatorText("World");
        ImGui::Text("Solids: %u visible / %u total", world.visible_solids, world.total_solids);
        ImGui::Text("Draw calls: %u", world.draw_calls);
        ImGui::Text("Triangles: %u", world.triangles);
        ImGui::Text("Indirect bounce: %.3f", world.indirect_bounce_strength);
        ImGui::Text("Shadows: %s", world.shadow_enabled ? "on" : "off");
        if (world.shadow_enabled)
        {
            ImGui::Text("Shadow map: %u", world.shadow_map_resolution);
            ImGui::Text("Shadow cascades: %u", world.shadow_cascade_count);
            ImGui::Text("Shadow split: %.2f", world.shadow_cascade_split_distance);
            ImGui::Text("Shadow blend: %.2f", world.shadow_cascade_blend_range);
            ImGui::Text("Shadow contact: %.3f", world.shadow_contact_strength);
            ImGui::Text("Shadow near bias: %.4f", world.shadow_bias_near);
            ImGui::Text("Shadow far bias: %.4f", world.shadow_bias_far);
            ImGui::Text("Shadow near offset: %.3f", world.shadow_normal_offset_near);
            ImGui::Text("Shadow far offset: %.3f", world.shadow_normal_offset_far);
            ImGui::Text("Shadow near PCF: %.2f", world.shadow_pcf_radius_near);
            ImGui::Text("Shadow far PCF: %.2f", world.shadow_pcf_radius_far);
            ImGui::Text("Shadow radius: %.2f", world.shadow_projection_radius);
            ImGui::Text("Shadow far radius: %.2f", world.shadow_far_projection_radius);
            ImGui::Text("Shadow focus: %.2f", world.shadow_focus_distance);
            ImGui::Text("Shadow far focus: %.2f", world.shadow_far_focus_distance);
            ImGui::Text("Shadow texel: %.4f wu", world.shadow_texel_world_size);
            ImGui::Text("Shadow far texel: %.4f wu", world.shadow_far_texel_world_size);
        }
        draw_shadow_controls(renderer_config);
        draw_shadow_preview(world);

        ImGui::SeparatorText("Player");
        ImGui::Text("Position: %.2f %.2f %.2f", player.position.x, player.position.y, player.position.z);
        ImGui::Text("Velocity: %.2f %.2f %.2f", player.velocity.x, player.velocity.y, player.velocity.z);
        ImGui::Text("Speed: %.2f", player.speed);
        ImGui::Text("Grounded: %s", player.grounded ? "yes" : "no");
        ImGui::Text("Mode: %s", movement_mode_label(player.movement_mode));
        ImGui::Text("Collision: %s", player.collision_enabled ? "on" : "off");

        ImGui::SeparatorText("Interaction");
        ImGui::Text("Focused usable: %s", interaction.has_focused_usable ? "yes" : "no");
        ImGui::Text("Focused label: %s", interaction.focused_label.c_str());
        ImGui::Text("Focus distance: %.2f", interaction.focused_distance);
        ImGui::Text("Last target: %s", interaction.last_target.c_str());
        ImGui::Text("Doors: %u active / %u total", interaction.active_doors, interaction.total_doors);
        ImGui::Text("Triggers: %u fired / %u total", interaction.fired_triggers, interaction.total_triggers);
        ImGui::Text("Use pressed: %s", interaction.use_pressed ? "yes" : "no");
        ImGui::Text("Inventory: %s", interaction.has_inventory ? interaction.inventory_summary.c_str() : "(empty)");
        ImGui::Text("Objective: %s", interaction.has_objective ? interaction.objective_title.c_str() : "(none)");

        ImGui::SeparatorText("Audio");
        ImGui::Text("Initialized: %s", audio_system.is_initialized() ? "yes" : "no");
        ImGui::Text("Master volume: %.2f", audio_system.config().master_volume);
        ImGui::End();

        if (config_.show_demo_window)
        {
            ImGui::ShowDemoWindow(&config_.show_demo_window);
        }
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

    void ImGuiOverlay::set_diagnostics_visible(bool visible) noexcept
    {
        diagnostics_visible_ = visible;
    }

    void ImGuiOverlay::toggle_diagnostics_visible() noexcept
    {
        diagnostics_visible_ = !diagnostics_visible_;
    }

    auto ImGuiOverlay::should_render() const noexcept -> bool
    {
        return initialized_ && (diagnostics_visible_ || config_.show_demo_window);
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

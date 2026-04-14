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
#include "relaycore/debug/inspector_source_reference.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/camera.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <string>
#include <string_view>

namespace RelayCore::debug
{
    namespace
    {
        using InspectorSourceFilter = render::DiagnosticsSourceFilter;

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

        [[nodiscard]] auto bool_label(const bool value) -> const char*
        {
            return value ? "yes" : "no";
        }

        [[nodiscard]] auto value_or_none(const std::string& value) -> const char*
        {
            return value.empty() ? "(none)" : value.c_str();
        }

        [[nodiscard]] auto ratio_or_zero(const std::uint32_t value, const std::uint32_t total) -> float
        {
            if (total == 0)
            {
                return 0.0F;
            }

            return static_cast<float>(value) / static_cast<float>(total);
        }

        [[nodiscard]] auto hud_event_color(const render::HudEventStyle style) -> ImVec4
        {
            switch (style)
            {
                case render::HudEventStyle::Success:
                    return ImVec4(0.45F, 0.90F, 0.55F, 1.0F);
                case render::HudEventStyle::Warning:
                    return ImVec4(1.0F, 0.78F, 0.28F, 1.0F);
                default:
                    return ImVec4(0.80F, 0.84F, 0.90F, 1.0F);
            }
        }

        [[nodiscard]] auto inspector_source_bucket_label(const render::InspectorDiagnostics& inspector) -> const char*
        {
            if (!inspector.has_authored_source)
            {
                return "untracked";
            }
            if (inspector.authored_source_kind == "generated"
                || inspector.authored_source_kind == "generated_from_entity")
            {
                return "generated";
            }
            if (inspector.has_authored_placement_location)
            {
                return "instanced";
            }
            return "direct";
        }

        [[nodiscard]] auto inspector_source_filter_label(const InspectorSourceFilter filter) -> const char*
        {
            switch (filter)
            {
                case InspectorSourceFilter::Direct:
                    return "Direct";
                case InspectorSourceFilter::Instanced:
                    return "Instanced";
                case InspectorSourceFilter::Generated:
                    return "Generated";
                case InspectorSourceFilter::All:
                default:
                    return "All";
            }
        }

        [[nodiscard]] auto source_filter_from_index(const int filter_index) -> InspectorSourceFilter
        {
            switch (std::clamp(filter_index, 0, 3))
            {
                case 1:
                    return InspectorSourceFilter::Direct;
                case 2:
                    return InspectorSourceFilter::Instanced;
                case 3:
                    return InspectorSourceFilter::Generated;
                case 0:
                default:
                    return InspectorSourceFilter::All;
            }
        }

        [[nodiscard]] auto inspector_matches_source_filter(
            const render::InspectorDiagnostics& inspector,
            const InspectorSourceFilter filter
        ) -> bool
        {
            if (filter == InspectorSourceFilter::All)
            {
                return true;
            }

            const std::string_view bucket = inspector_source_bucket_label(inspector);
            switch (filter)
            {
                case InspectorSourceFilter::Direct:
                    return bucket == "direct";
                case InspectorSourceFilter::Instanced:
                    return bucket == "instanced";
                case InspectorSourceFilter::Generated:
                    return bucket == "generated";
                case InspectorSourceFilter::All:
                default:
                    return true;
            }
        }

        [[nodiscard]] auto warning_source_bucket_label(const render::AuthoringWarningDiagnostics& warning) -> const
            char*
        {
            return warning.source_bucket.empty() ? "direct" : warning.source_bucket.c_str();
        }

        [[nodiscard]] auto
        source_bucket_matches_filter(const std::string_view bucket, const InspectorSourceFilter filter) -> bool
        {
            if (filter == InspectorSourceFilter::All)
            {
                return true;
            }

            switch (filter)
            {
                case InspectorSourceFilter::Direct:
                    return bucket == "direct";
                case InspectorSourceFilter::Instanced:
                    return bucket == "instanced";
                case InspectorSourceFilter::Generated:
                    return bucket == "generated";
                case InspectorSourceFilter::All:
                default:
                    return true;
            }
        }

        [[nodiscard]] auto warning_matches_source_filter(
            const render::AuthoringWarningDiagnostics& warning,
            const InspectorSourceFilter filter
        ) -> bool
        {
            return source_bucket_matches_filter(warning_source_bucket_label(warning), filter);
        }

        void draw_text_value(const char* label, const std::string& value)
        {
            ImGui::Text("%s: %s", label, value_or_none(value));
        }

        void draw_text_wrapped_value(const char* label, const std::string& value)
        {
            ImGui::TextWrapped("%s: %s", label, value_or_none(value));
        }

        void draw_optional_text_value(const char* label, const std::string& value)
        {
            if (!value.empty())
            {
                ImGui::Text("%s: %s", label, value.c_str());
            }
        }

        void draw_optional_text_wrapped_value(const char* label, const std::string& value)
        {
            if (!value.empty())
            {
                ImGui::TextWrapped("%s: %s", label, value.c_str());
            }
        }

        void
        draw_optional_source_location(const char* label, const std::string& file_path, const int line, const int column)
        {
            if (file_path.empty() || line <= 0 || column <= 0)
            {
                return;
            }

            ImGui::TextWrapped("%s: %s", label, file_path.c_str());
            ImGui::Text("%s loc: %d:%d", label, line, column);
        }

        void copy_text_to_clipboard(const std::string& text, const char* context_label)
        {
            if (text.empty())
            {
                RelayCore::log::info("{} copy ignored: no text available", context_label);
                return;
            }

            if (!SDL_SetClipboardText(text.c_str()))
            {
                RelayCore::log::warn("{} copy failed: {}", context_label, SDL_GetError());
                return;
            }

            RelayCore::log::info("{} copied: {}", context_label, text);
        }

        void draw_source_open_button(
            const char* button_label,
            const RelayCore::debug::SourceLocation& location,
            const char* context_label
        )
        {
            if (!location.valid())
            {
                return;
            }

            if (!ImGui::Button(button_label))
            {
                return;
            }

            const RelayCore::debug::SourceOpenResult result = RelayCore::debug::open_source_location(location);
            const std::string source_ref = RelayCore::debug::build_source_reference(location);
            if (result.launched)
            {
                RelayCore::log::info(
                    "{} opened via {}: {}",
                    context_label,
                    result.strategy.empty() ? std::string{"editor launcher"} : result.strategy,
                    source_ref
                );
                return;
            }

            RelayCore::log::warn(
                "{} open failed for {}: {}",
                context_label,
                source_ref.empty() ? std::string{"<unknown source>"} : source_ref,
                result.error.empty() ? std::string{"unknown error"} : result.error
            );
        }

        [[nodiscard]] auto begin_section(const char* label, const ImVec4& accent, const bool default_open = false)
            -> bool
        {
            const ImVec4 header{accent.x * 0.45F, accent.y * 0.45F, accent.z * 0.45F, 0.78F};
            const ImVec4 hovered{accent.x * 0.62F, accent.y * 0.62F, accent.z * 0.62F, 0.88F};
            const ImVec4 active{accent.x * 0.78F, accent.y * 0.78F, accent.z * 0.78F, 0.96F};

            ImGui::PushStyleColor(ImGuiCol_Header, header);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, hovered);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, active);
            const bool open = ImGui::CollapsingHeader(
                label,
                (default_open ? ImGuiTreeNodeFlags_DefaultOpen : 0) | ImGuiTreeNodeFlags_SpanAvailWidth
            );
            ImGui::PopStyleColor(3);
            return open;
        }

        [[nodiscard]] auto begin_metric_table(const char* id) -> bool
        {
            return ImGui::BeginTable(
                id,
                2,
                ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_PadOuterX
                    | ImGuiTableFlags_RowBg
            );
        }

        void begin_metric_row(const char* label)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("%s", label);
            ImGui::TableSetColumnIndex(1);
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

        void draw_shadow_controls_contents(render::RendererConfig& renderer_config)
        {
            ImGui::Checkbox("Enable shadows", &renderer_config.shadow_enabled);
            if (ImGui::TreeNodeEx("Lighting", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::SliderFloat("Indirect bounce", &renderer_config.indirect_bounce_strength, 0.0F, 0.30F, "%.3f");
                ImGui::SliderFloat("Shadow strength", &renderer_config.shadow_strength, 0.0F, 1.0F, "%.3f");
                ImGui::SliderFloat("Shadow contact", &renderer_config.shadow_contact_strength, 0.0F, 0.30F, "%.3f");
                ImGui::SliderFloat(
                    "Shadow light intensity",
                    &renderer_config.shadow_light_intensity,
                    0.0F,
                    2.0F,
                    "%.2f"
                );
                ImGui::ColorEdit3("Shadow light color", &renderer_config.shadow_light_color.x);
                ImGui::TreePop();
            }
            if (ImGui::TreeNodeEx("Near Cascade", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::SliderFloat("Near shadow bias", &renderer_config.shadow_bias_near, 0.0001F, 0.0100F, "%.4f");
                ImGui::SliderFloat(
                    "Shadow slope bias",
                    &renderer_config.shadow_slope_bias_scale,
                    0.0F,
                    0.0200F,
                    "%.4f"
                );
                ImGui::SliderFloat(
                    "Near normal offset",
                    &renderer_config.shadow_normal_offset_near,
                    0.0F,
                    0.080F,
                    "%.3f"
                );
                ImGui::SliderFloat("Shadow near PCF", &renderer_config.shadow_pcf_radius_near, 0.5F, 3.0F, "%.2f");
                ImGui::SliderFloat(
                    "Shadow split distance",
                    &renderer_config.shadow_cascade_split_distance,
                    2.0F,
                    24.0F,
                    "%.2f"
                );
                ImGui::SliderFloat(
                    "Shadow blend range",
                    &renderer_config.shadow_cascade_blend_range,
                    0.0F,
                    6.0F,
                    "%.2f"
                );
                ImGui::Checkbox("Debug cascade tint", &renderer_config.shadow_debug_cascades);
                ImGui::SliderFloat(
                    "Shadow focus distance",
                    &renderer_config.shadow_focus_distance,
                    0.0F,
                    24.0F,
                    "%.2f"
                );
                ImGui::SliderFloat("Shadow radius", &renderer_config.shadow_projection_radius, 2.0F, 28.0F, "%.2f");
                ImGui::SliderFloat("Shadow depth", &renderer_config.shadow_projection_depth, 4.0F, 48.0F, "%.2f");
                ImGui::TreePop();
            }
            if (ImGui::TreeNodeEx("Far Cascade"))
            {
                ImGui::SliderFloat("Shadow far PCF", &renderer_config.shadow_pcf_radius_far, 0.5F, 4.0F, "%.2f");
                ImGui::SliderFloat("Far shadow bias", &renderer_config.shadow_bias_far, 0.0001F, 0.0160F, "%.4f");
                ImGui::SliderFloat(
                    "Far normal offset",
                    &renderer_config.shadow_normal_offset_far,
                    0.0F,
                    0.120F,
                    "%.3f"
                );
                ImGui::SliderFloat(
                    "Far focus distance",
                    &renderer_config.shadow_far_focus_distance,
                    6.0F,
                    40.0F,
                    "%.2f"
                );
                ImGui::SliderFloat("Far radius", &renderer_config.shadow_far_projection_radius, 6.0F, 48.0F, "%.2f");
                ImGui::SliderFloat("Far depth", &renderer_config.shadow_far_projection_depth, 8.0F, 80.0F, "%.2f");
                ImGui::TreePop();
            }
            if (ImGui::TreeNodeEx("Light Direction"))
            {
                ImGui::SliderFloat3(
                    "Shadow light direction",
                    &renderer_config.shadow_light_direction.x,
                    -1.0F,
                    1.0F,
                    "%.2f"
                );
                ImGui::TreePop();
            }
        }

        void draw_shadow_preview_contents(const render::WorldDiagnostics& world)
        {
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

    void ImGuiOverlay::apply_config(const ImGuiOverlayConfig& config) noexcept
    {
        config_ = config;
        if (!config_.enabled)
        {
            diagnostics_visible_ = false;
            advanced_diagnostics_visible_ = false;
        }
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
        const render::RuntimeDiagnostics& runtime,
        const render::PlayerDiagnostics& player,
        const render::InteractionDiagnostics& interaction,
        const render::InspectorDiagnostics& inspector,
        const render::AudioDiagnostics& audio,
        const render::IterationDiagnostics& iteration
    )
    {
        RELAYCORE_PROFILE_ZONE_N("ImGuiOverlay::draw_diagnostics");
        (void) frame;

        if (!initialized_ || !diagnostics_visible_)
        {
            return;
        }

        const float refresh_rate_hz = window_ != nullptr ? window_->refresh_rate_hz() : 0.0F;
        const int swap_interval = window_ != nullptr ? window_->swap_interval() : 0;
        const float target_frame_ms = refresh_rate_hz > 0.0F ? 1000.0F / refresh_rate_hz : 16.667F;
        const bool frame_over_budget = avg_frame_time_ms_ > target_frame_ms * 1.05F;
        const bool sim_over_budget = runtime.fixed_steps > runtime.fixed_step_budget
                                     || runtime.simulation_backlog_ms > std::max(runtime.fixed_step_ms, 0.001F);
        const bool blocked_focus =
            interaction.has_focused_usable
            && (!interaction.focused_enabled || interaction.focused_locked || !interaction.focused_requirement_met);
        const bool has_attention = frame_over_budget || sim_over_budget || blocked_focus || !audio.initialized
                                   || !player.alive || player.health < 35.0F;

        std::array<float, kFrameTimeSampleCount> ordered_frame_times{};
        float frame_plot_max = std::max(target_frame_ms * 2.0F, 20.0F);
        if (frame_time_sample_count_ > 0)
        {
            const std::size_t start =
                frame_time_sample_count_ == frame_time_samples_.size() ? frame_time_sample_cursor_ : 0;
            for (std::size_t i = 0; i < frame_time_sample_count_; ++i)
            {
                const float sample = frame_time_samples_[(start + i) % frame_time_samples_.size()];
                ordered_frame_times[i] = sample;
                frame_plot_max = std::max(frame_plot_max, sample);
            }
        }

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 12.0F, viewport->WorkPos.y + 12.0F), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(540.0F, 560.0F), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.92F);
        constexpr ImGuiWindowFlags kWindowFlags = ImGuiWindowFlags_NoSavedSettings;

        ImGui::Begin("RelayCore Diagnostics", nullptr, kWindowFlags);
        (void) audio_system;

        if (begin_section("Summary", ImVec4(0.25F, 0.48F, 0.82F, 1.0F), true))
        {
            ImGui::Text("Frame %.2f ms | avg %.2f ms | FPS %.1f", frame_time_ms_, avg_frame_time_ms_, fps_);
            ImGui::PlotLines(
                "##frame_times",
                ordered_frame_times.data(),
                static_cast<int>(frame_time_sample_count_),
                0,
                nullptr,
                0.0F,
                frame_plot_max,
                ImVec2(-FLT_MIN, 72.0F)
            );

            if (refresh_rate_hz > 0.0F)
            {
                ImGui::Text(
                    "Budget %.2f ms @ %.1f Hz | VSync %s",
                    target_frame_ms,
                    refresh_rate_hz,
                    vsync_label(swap_interval)
                );
            } else
            {
                ImGui::Text("Budget %.2f ms | Refresh unknown | VSync %s", target_frame_ms, vsync_label(swap_interval));
            }

            if (ImGui::BeginTable("summary_cards", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextColumn();
                ImGui::BeginChild("summary_runtime_card", ImVec2(0.0F, 132.0F), true);
                ImGui::TextDisabled("Runtime");
                if (begin_metric_table("summary_runtime_table"))
                {
                    begin_metric_row("Level");
                    ImGui::Text("%s", value_or_none(runtime.has_level ? runtime.level_name : std::string{}));
                    begin_metric_row("Fixed sim");
                    ImGui::Text(
                        "%u / %u @ %.2f ms",
                        runtime.fixed_steps,
                        runtime.fixed_step_budget,
                        runtime.fixed_step_ms
                    );
                    begin_metric_row("Backlog");
                    ImGui::Text("%.3f ms", runtime.simulation_backlog_ms);
                    begin_metric_row("Player");
                    ImGui::Text(
                        "%s | hp %.0f/%.0f",
                        movement_mode_label(player.movement_mode),
                        player.health,
                        player.max_health
                    );
                    ImGui::EndTable();
                }
                ImGui::EndChild();

                ImGui::TableNextColumn();
                ImGui::BeginChild("summary_goal_card", ImVec2(0.0F, 132.0F), true);
                ImGui::TextDisabled("Session");
                if (interaction.has_objective)
                {
                    ImGui::Text("Objective %u / %u", interaction.objective_current, interaction.objective_total);
                    ImGui::ProgressBar(
                        std::clamp(
                            ratio_or_zero(interaction.objective_current, interaction.objective_total),
                            0.0F,
                            1.0F
                        ),
                        ImVec2(-FLT_MIN, 0.0F)
                    );
                    ImGui::TextWrapped("%s", interaction.objective_title.c_str());
                } else
                {
                    ImGui::TextUnformatted("Objective: (none)");
                }
                ImGui::Separator();
                ImGui::Text(
                    "Focus: %s",
                    interaction.has_focused_usable ? value_or_none(interaction.focused_label) : "(none)"
                );
                ImGui::Text(
                    "Ambient: %s | One-shots %u",
                    audio.has_active_loop ? value_or_none(audio.active_source_kind) : "(none)",
                    audio.one_shot_count
                );
                if (iteration.has_last_reload_result)
                {
                    ImGui::TextColored(
                        iteration.last_reload_succeeded ? ImVec4(0.45F, 0.90F, 0.55F, 1.0F)
                                                        : ImVec4(1.0F, 0.55F, 0.35F, 1.0F),
                        "Reload: %s",
                        value_or_none(iteration.last_reload_summary)
                    );
                } else
                {
                    ImGui::Text("Reload: idle");
                }
                ImGui::Text("Authoring warnings: %u", static_cast<unsigned>(iteration.authoring_warnings.size()));
                ImGui::Text(
                    "Saved snapshot: %s",
                    iteration.has_saved_runtime_snapshot ? value_or_none(iteration.saved_snapshot_summary) : "(none)"
                );
                ImGui::TextDisabled(
                    "F2 overlay | F6 tools | F4 highlight | F5 pin | F7 reload | F8 open source | F9 save snapshot | "
                    "F10 restore snapshot"
                );
                ImGui::EndChild();
                ImGui::EndTable();
            }

            if (has_attention)
            {
                ImGui::SeparatorText("Attention");
                if (frame_over_budget)
                {
                    ImGui::TextColored(
                        ImVec4(1.0F, 0.78F, 0.28F, 1.0F),
                        "Frame average over budget: %.2f / %.2f ms",
                        avg_frame_time_ms_,
                        target_frame_ms
                    );
                }
                if (sim_over_budget)
                {
                    ImGui::TextColored(
                        ImVec4(1.0F, 0.78F, 0.28F, 1.0F),
                        "Simulation backlog: %.3f ms with %u / %u fixed steps",
                        runtime.simulation_backlog_ms,
                        runtime.fixed_steps,
                        runtime.fixed_step_budget
                    );
                }
                if (blocked_focus)
                {
                    ImGui::TextColored(
                        ImVec4(1.0F, 0.55F, 0.35F, 1.0F),
                        "Focused usable is blocked: enabled=%s locked=%s requirement=%s",
                        bool_label(interaction.focused_enabled),
                        bool_label(interaction.focused_locked),
                        bool_label(interaction.focused_requirement_met)
                    );
                }
                if (!audio.initialized)
                {
                    ImGui::TextColored(ImVec4(1.0F, 0.55F, 0.35F, 1.0F), "Audio system is not initialized");
                }
                if (!player.alive)
                {
                    ImGui::TextColored(
                        ImVec4(1.0F, 0.55F, 0.35F, 1.0F),
                        "Player down: respawn in %.2f s",
                        player.respawn_remaining_seconds
                    );
                } else if (player.health < 35.0F)
                {
                    ImGui::TextColored(
                        ImVec4(1.0F, 0.78F, 0.28F, 1.0F),
                        "Low health: %.0f / %.0f",
                        player.health,
                        player.max_health
                    );
                }
                if (iteration.has_last_reload_result && !iteration.last_reload_succeeded)
                {
                    ImGui::TextColored(
                        ImVec4(1.0F, 0.55F, 0.35F, 1.0F),
                        "Reload failed: %s",
                        value_or_none(iteration.last_reload_summary)
                    );
                }
            }
        }

        if (advanced_diagnostics_visible_ && begin_section("World", ImVec4(0.20F, 0.55F, 0.75F, 1.0F), true))
        {
            if (begin_metric_table("world_metrics"))
            {
                begin_metric_row("Solids");
                ImGui::Text("%u / %u visible", world.visible_solids, world.total_solids);
                begin_metric_row("Chunks");
                ImGui::Text(
                    "%u visible | %u culled | %.0f%% visible",
                    world.visible_chunks,
                    world.culled_chunks,
                    ratio_or_zero(world.visible_chunks, world.total_chunks) * 100.0F
                );
                begin_metric_row("Chunk split");
                ImGui::Text(
                    "%u/%u opaque | %u/%u transparent",
                    world.opaque_visible_chunks,
                    world.opaque_total_chunks,
                    world.transparent_visible_chunks,
                    world.transparent_total_chunks
                );
                begin_metric_row("Lights");
                ImGui::Text(
                    "%u active / %u total / %u budget",
                    world.local_light_count,
                    world.total_render_lights,
                    world.active_local_light_budget
                );
                begin_metric_row("Draw calls");
                ImGui::Text(
                    "%u total | %u o / %u ov / %u t",
                    world.draw_calls,
                    world.opaque_draw_calls,
                    world.overlay_draw_calls,
                    world.transparent_draw_calls
                );
                begin_metric_row("Triangles");
                ImGui::Text(
                    "%u total | %u o / %u ov / %u t",
                    world.triangles,
                    world.opaque_triangles,
                    world.overlay_triangles,
                    world.transparent_triangles
                );
                begin_metric_row("Shadows");
                ImGui::Text(
                    "%s | cascades %u | map %u",
                    bool_label(world.shadow_enabled),
                    world.shadow_cascade_count,
                    world.shadow_map_resolution
                );
                ImGui::EndTable();
            }

            if (ImGui::TreeNodeEx("Scene Breakdown"))
            {
                if (begin_metric_table("world_scene_breakdown"))
                {
                    begin_metric_row("Doors");
                    ImGui::Text("%u draws", world.door_draw_calls);
                    begin_metric_row("Usables");
                    ImGui::Text("%u draws", world.usable_draw_calls);
                    begin_metric_row("Highlights");
                    ImGui::Text("%u draws", world.highlight_draw_calls);
                    begin_metric_row("Feedback");
                    ImGui::Text("%u draws", world.feedback_draw_calls);
                    begin_metric_row("Inspector");
                    ImGui::Text("%u boxes / %u markers", world.inspector_draw_calls, world.inspector_marker_draw_calls);
                    ImGui::EndTable();
                }
                ImGui::TreePop();
            }
            if (world.shadow_enabled && ImGui::TreeNodeEx("Shadow Details"))
            {
                if (begin_metric_table("world_shadow_details"))
                {
                    begin_metric_row("Bounce");
                    ImGui::Text("%.3f", world.indirect_bounce_strength);
                    begin_metric_row("Split / blend");
                    ImGui::Text("%.2f / %.2f", world.shadow_cascade_split_distance, world.shadow_cascade_blend_range);
                    begin_metric_row("Contact");
                    ImGui::Text("%.3f", world.shadow_contact_strength);
                    begin_metric_row("Near bias / offset");
                    ImGui::Text("%.4f / %.3f", world.shadow_bias_near, world.shadow_normal_offset_near);
                    begin_metric_row("Far bias / offset");
                    ImGui::Text("%.4f / %.3f", world.shadow_bias_far, world.shadow_normal_offset_far);
                    begin_metric_row("Near PCF / texel");
                    ImGui::Text("%.2f / %.4f", world.shadow_pcf_radius_near, world.shadow_texel_world_size);
                    begin_metric_row("Far PCF / texel");
                    ImGui::Text("%.2f / %.4f", world.shadow_pcf_radius_far, world.shadow_far_texel_world_size);
                    begin_metric_row("Near radius / focus");
                    ImGui::Text("%.2f / %.2f", world.shadow_projection_radius, world.shadow_focus_distance);
                    begin_metric_row("Far radius / focus");
                    ImGui::Text("%.2f / %.2f", world.shadow_far_projection_radius, world.shadow_far_focus_distance);
                    ImGui::EndTable();
                }
                ImGui::TreePop();
            }
        }

        if (advanced_diagnostics_visible_ && begin_section("Runtime", ImVec4(0.32F, 0.44F, 0.58F, 1.0F)))
        {
            if (begin_metric_table("runtime_metrics"))
            {
                begin_metric_row("Level path");
                ImGui::TextWrapped("%s", value_or_none(runtime.has_level ? runtime.level_path : std::string{}));
                begin_metric_row("Stress map");
                ImGui::Text("%s", bool_label(runtime.stress_level));
                begin_metric_row("Entities");
                ImGui::Text("%u", runtime.level_entity_count);
                begin_metric_row("Simulation");
                ImGui::Text(
                    "tick %llu | overlap %u | fixed %.2f ms",
                    static_cast<unsigned long long>(runtime.simulation_tick),
                    runtime.active_trigger_overlaps,
                    runtime.fixed_step_ms
                );
                ImGui::EndTable();
            }
        }

        if (advanced_diagnostics_visible_ && begin_section("Iteration", ImVec4(0.46F, 0.36F, 0.24F, 1.0F), true))
        {
            if (begin_metric_table("iteration_metrics"))
            {
                begin_metric_row("Last reload");
                ImGui::Text(
                    "%s",
                    iteration.has_last_reload_result ? value_or_none(iteration.last_reload_summary) : "idle"
                );
                begin_metric_row("Result");
                ImGui::Text(
                    "%s",
                    iteration.has_last_reload_result ? (iteration.last_reload_succeeded ? "ok" : "failed")
                                                     : "not attempted"
                );
                begin_metric_row("Last tool");
                ImGui::Text(
                    "%s",
                    iteration.has_last_command_result ? value_or_none(iteration.last_command_summary) : "idle"
                );
                begin_metric_row("Warnings");
                ImGui::Text("%u", static_cast<unsigned>(iteration.authoring_warnings.size()));
                ImGui::EndTable();
            }

            if (iteration.has_last_reload_result && !iteration.last_reload_error.empty())
            {
                ImGui::SeparatorText("Last Reload Error");
                ImGui::TextWrapped("%s", iteration.last_reload_error.c_str());
            }

            if (iteration.has_last_command_result)
            {
                ImGui::SeparatorText("Last Tool Result");
                ImGui::TextColored(
                    iteration.last_command_succeeded ? ImVec4(0.45F, 0.90F, 0.55F, 1.0F)
                                                     : ImVec4(1.0F, 0.55F, 0.35F, 1.0F),
                    "%s",
                    value_or_none(iteration.last_command_summary)
                );
                if (!iteration.last_command_error.empty())
                {
                    ImGui::TextWrapped("%s", iteration.last_command_error.c_str());
                }
            }

            ImGui::SeparatorText("Snapshot");
            if (ImGui::Button("Save Snapshot"))
            {
                save_runtime_snapshot_requested_ = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Restore Snapshot"))
            {
                restore_runtime_snapshot_requested_ = true;
            }
            if (iteration.has_last_snapshot_result)
            {
                ImGui::TextColored(
                    iteration.last_snapshot_succeeded ? ImVec4(0.45F, 0.90F, 0.55F, 1.0F)
                                                      : ImVec4(1.0F, 0.55F, 0.35F, 1.0F),
                    "%s",
                    value_or_none(iteration.last_snapshot_summary)
                );
                if (!iteration.last_snapshot_error.empty())
                {
                    ImGui::TextWrapped("%s", iteration.last_snapshot_error.c_str());
                }
            }

            ImGui::SeparatorText("Material/Lighting Contract");
            ImGui::TextUnformatted(
                "Dump the current material vocabulary and light contract from render_authoring.toml."
            );
            if (ImGui::Button("Dump Contract to Log"))
            {
                dump_render_authoring_contract_requested_ = true;
            }

            ImGui::SeparatorText("Authoring Warnings");
            if (iteration.authoring_warnings.empty())
            {
                ImGui::TextUnformatted("No authored warnings for the current map.");
            } else
            {
                int warning_filter_index = static_cast<int>(authoring_warning_source_filter_);
                ImGui::SetNextItemWidth(160.0F);
                if (ImGui::Combo("Warning filter", &warning_filter_index, "All\0Direct\0Instanced\0Generated\0"))
                {
                    authoring_warning_source_filter_ = source_filter_from_index(warning_filter_index);
                }

                const auto warning_filter = authoring_warning_source_filter_;
                if (ImGui::Button("Open First##warning"))
                {
                    open_first_authoring_warning_requested_ = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Dump Filtered##warning"))
                {
                    dump_authoring_warnings_requested_ = true;
                }
                for (std::size_t warning_index = 0; warning_index < iteration.authoring_warnings.size();
                     ++warning_index)
                {
                    const render::AuthoringWarningDiagnostics& warning = iteration.authoring_warnings[warning_index];
                    if (!warning_matches_source_filter(warning, warning_filter))
                    {
                        continue;
                    }

                    ImGui::PushID(static_cast<int>(warning_index));
                    ImGui::BeginChild("authoring_warning_item", ImVec2(0.0F, 92.0F), true);
                    ImGui::TextWrapped("%s", warning.message.c_str());
                    ImGui::TextDisabled("Bucket: %s", warning_source_bucket_label(warning));
                    if (!warning.primary_label.empty())
                    {
                        ImGui::TextWrapped("A: %s", warning.primary_label.c_str());
                    }
                    if (warning.primary_line > 0 && warning.primary_column > 0)
                    {
                        ImGui::TextWrapped(
                            "A src: %s:%d:%d",
                            warning.primary_file_path.c_str(),
                            warning.primary_line,
                            warning.primary_column
                        );
                    }
                    if (!warning.secondary_label.empty())
                    {
                        ImGui::TextWrapped("B: %s", warning.secondary_label.c_str());
                    }
                    if (warning.secondary_line > 0 && warning.secondary_column > 0)
                    {
                        ImGui::TextWrapped(
                            "B src: %s:%d:%d",
                            warning.secondary_file_path.c_str(),
                            warning.secondary_line,
                            warning.secondary_column
                        );
                    }

                    const RelayCore::debug::SourceLocation primary_location{
                        .file_path = warning.primary_file_path,
                        .line = warning.primary_line,
                        .column = warning.primary_column,
                    };
                    const RelayCore::debug::SourceLocation secondary_location{
                        .file_path = warning.secondary_file_path,
                        .line = warning.secondary_line,
                        .column = warning.secondary_column,
                    };
                    draw_source_open_button("Open A", primary_location, "Authoring warning source A");
                    if (secondary_location.valid())
                    {
                        ImGui::SameLine();
                        draw_source_open_button("Open B", secondary_location, "Authoring warning source B");
                    }
                    ImGui::EndChild();
                    ImGui::PopID();
                }
            }

            ImGui::SeparatorText("Runtime Registry");
            if (iteration.runtime_registry.empty())
            {
                ImGui::TextUnformatted("Runtime registry is empty.");
            } else
            {
                int registry_filter_index = static_cast<int>(runtime_registry_source_filter_);
                ImGui::SetNextItemWidth(160.0F);
                if (ImGui::Combo("Registry filter", &registry_filter_index, "All\0Direct\0Instanced\0Generated\0"))
                {
                    runtime_registry_source_filter_ = source_filter_from_index(registry_filter_index);
                }

                const auto registry_filter = runtime_registry_source_filter_;
                if (ImGui::Button("Open First##registry"))
                {
                    open_first_runtime_registry_requested_ = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Dump Filtered##registry"))
                {
                    dump_runtime_registry_requested_ = true;
                }
                for (std::size_t entry_index = 0; entry_index < iteration.runtime_registry.size(); ++entry_index)
                {
                    const render::RuntimeRegistryEntryDiagnostics& entry = iteration.runtime_registry[entry_index];
                    const std::string_view bucket = entry.source_bucket.empty() ? std::string_view{"untracked"}
                                                                                : std::string_view{entry.source_bucket};
                    if (!source_bucket_matches_filter(bucket, registry_filter)
                        && !(registry_filter == InspectorSourceFilter::Direct && bucket == "untracked"))
                    {
                        continue;
                    }

                    ImGui::PushID(static_cast<int>(entry_index) + 7000);
                    ImGui::BeginChild("runtime_registry_item", ImVec2(0.0F, 84.0F), true);
                    ImGui::Text("%s | %s", value_or_none(entry.runtime_handle), value_or_none(entry.object_kind));
                    ImGui::TextWrapped(
                        "name=%s | label=%s | target=%s | storage=%u",
                        value_or_none(entry.name),
                        value_or_none(entry.label),
                        value_or_none(entry.target),
                        entry.storage_index
                    );
                    ImGui::TextDisabled("Bucket: %s", bucket.data());
                    if (!entry.source_reference.empty())
                    {
                        ImGui::TextWrapped("Source: %s", entry.source_reference.c_str());
                    }
                    draw_source_open_button(
                        "Open Source",
                        RelayCore::debug::SourceLocation{
                            .file_path = entry.source_file_path,
                            .line = entry.source_line,
                            .column = entry.source_column,
                        },
                        "Runtime registry source"
                    );
                    ImGui::EndChild();
                    ImGui::PopID();
                }
            }

            ImGui::SeparatorText("Runtime Bindings");
            if (iteration.runtime_bindings.empty())
            {
                ImGui::TextUnformatted("No runtime bindings were produced for the current map.");
            } else
            {
                int binding_filter_index = static_cast<int>(runtime_binding_source_filter_);
                ImGui::SetNextItemWidth(160.0F);
                if (ImGui::Combo("Binding filter", &binding_filter_index, "All\0Direct\0Instanced\0Generated\0"))
                {
                    runtime_binding_source_filter_ = source_filter_from_index(binding_filter_index);
                }

                const auto binding_filter = runtime_binding_source_filter_;
                if (ImGui::Button("Open First##binding"))
                {
                    open_first_runtime_binding_requested_ = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Dump Filtered##binding"))
                {
                    dump_runtime_bindings_requested_ = true;
                }
                for (std::size_t binding_index = 0; binding_index < iteration.runtime_bindings.size(); ++binding_index)
                {
                    const render::RuntimeBindingDiagnostics& binding = iteration.runtime_bindings[binding_index];
                    const std::string_view bucket = binding.source_bucket.empty()
                                                        ? std::string_view{"untracked"}
                                                        : std::string_view{binding.source_bucket};
                    if (!source_bucket_matches_filter(bucket, binding_filter)
                        && !(binding_filter == InspectorSourceFilter::Direct && bucket == "untracked"))
                    {
                        continue;
                    }

                    ImGui::PushID(static_cast<int>(binding_index) + 9000);
                    ImGui::BeginChild("runtime_binding_item", ImVec2(0.0F, 92.0F), true);
                    ImGui::Text("%s | %s", value_or_none(binding.source_handle), value_or_none(binding.binding_kind));
                    ImGui::TextWrapped(
                        "Source=%s (%s) -> %s",
                        value_or_none(binding.source_name),
                        value_or_none(binding.source_kind),
                        value_or_none(binding.target)
                    );
                    ImGui::TextWrapped(
                        "Resolved: %u | %s",
                        binding.resolved_target_count,
                        value_or_none(binding.resolved_targets_summary)
                    );
                    ImGui::TextDisabled("Bucket: %s", bucket.data());
                    draw_source_open_button(
                        "Open Source",
                        RelayCore::debug::SourceLocation{
                            .file_path = binding.source_file_path,
                            .line = binding.source_line,
                            .column = binding.source_column,
                        },
                        "Runtime binding source"
                    );
                    ImGui::EndChild();
                    ImGui::PopID();
                }
            }
        }

        if (begin_section("Player", ImVec4(0.28F, 0.60F, 0.48F, 1.0F)))
        {
            if (begin_metric_table("player_metrics"))
            {
                begin_metric_row("Health");
                ImGui::Text(
                    "%.0f / %.0f | alive=%s | hazards=%u",
                    player.health,
                    player.max_health,
                    bool_label(player.alive),
                    player.active_hazard_contacts
                );
                begin_metric_row("Position");
                ImGui::Text("%.2f %.2f %.2f", player.position.x, player.position.y, player.position.z);
                begin_metric_row("Velocity");
                ImGui::Text(
                    "%.2f %.2f %.2f | speed %.2f",
                    player.velocity.x,
                    player.velocity.y,
                    player.velocity.z,
                    player.speed
                );
                begin_metric_row("State");
                ImGui::Text(
                    "%s | grounded=%s | collision=%s",
                    movement_mode_label(player.movement_mode),
                    bool_label(player.grounded),
                    bool_label(player.collision_enabled)
                );
                begin_metric_row("Combat");
                ImGui::Text(
                    "%s | shots %u/%u | kills %u | deaths %u",
                    player.primary_attack_label.empty() ? "Mouse1" : player.primary_attack_label.c_str(),
                    player.shots_hit,
                    player.shots_fired,
                    player.kills,
                    player.deaths
                );
                begin_metric_row("Threats");
                ImGui::Text(
                    "turrets %u/%u destroyed | shots this frame %u",
                    player.destroyed_turrets,
                    player.total_turrets,
                    player.turret_shots_last_frame
                );
                begin_metric_row("Collision world");
                ImGui::Text(
                    "%u active | %u static candidates | %u dynamic",
                    player.collision_world_solids,
                    player.static_collision_candidates,
                    player.dynamic_blockers
                );
                begin_metric_row("View");
                ImGui::Text("yaw %.2f | pitch %.2f", camera.yaw_radians(), camera.pitch_radians());
                ImGui::EndTable();
            }
            if (ImGui::TreeNodeEx("Collision Details"))
            {
                if (begin_metric_table("player_collision_details"))
                {
                    begin_metric_row("Ground query");
                    ImGui::Text(
                        "probe=%s | snap=%s",
                        bool_label(player.ground_probe_hit),
                        bool_label(player.ground_snap_applied)
                    );
                    begin_metric_row("Step query");
                    ImGui::Text(
                        "used=%s | attempted=%s",
                        bool_label(player.used_step_move),
                        bool_label(player.attempted_step_move)
                    );
                    begin_metric_row("Collision hits");
                    ImGui::Text(
                        "x=%s | y=%s | z=%s",
                        bool_label(player.hit_x),
                        bool_label(player.hit_y),
                        bool_label(player.hit_z)
                    );
                    begin_metric_row("Static total");
                    ImGui::Text("%u", player.static_collision_solids);
                    begin_metric_row("Cooldown");
                    ImGui::Text(
                        "weapon %.3f s | hurt %.2f",
                        player.weapon_cooldown_remaining_seconds,
                        player.hurt_flash_strength
                    );
                    begin_metric_row("Damage");
                    ImGui::Text("dealt %.1f | taken %.1f", player.damage_dealt, player.damage_taken);
                    begin_metric_row("Last hit");
                    ImGui::Text("%s", value_or_none(player.last_damage_source));
                    ImGui::EndTable();
                }
                ImGui::TreePop();
            }
        }

        if (begin_section("Interaction", ImVec4(0.78F, 0.52F, 0.18F, 1.0F), true))
        {
            if (interaction.has_objective)
            {
                ImGui::Text("Objective: %s", interaction.mission_complete ? "complete" : "active");
                ImGui::ProgressBar(
                    std::clamp(ratio_or_zero(interaction.objective_current, interaction.objective_total), 0.0F, 1.0F),
                    ImVec2(-FLT_MIN, 0.0F)
                );
                ImGui::TextWrapped("%s", interaction.objective_title.c_str());
                if (!interaction.objective_detail.empty())
                {
                    ImGui::TextWrapped("%s", interaction.objective_detail.c_str());
                }
            } else
            {
                ImGui::TextUnformatted("Objective: (none)");
            }

            if (begin_metric_table("interaction_metrics"))
            {
                begin_metric_row("Use");
                ImGui::Text(
                    "%s | %u usables | %u triggers",
                    value_or_none(interaction.use_binding_label),
                    interaction.usable_candidate_count,
                    interaction.trigger_candidate_count
                );
                begin_metric_row("Focus");
                ImGui::Text("%s", interaction.has_focused_usable ? value_or_none(interaction.focused_label) : "(none)");
                begin_metric_row("Focus state");
                ImGui::Text(
                    "enabled=%s | locked=%s | requirement=%s | consumed=%s",
                    bool_label(interaction.focused_enabled),
                    bool_label(interaction.focused_locked),
                    bool_label(interaction.focused_requirement_met),
                    bool_label(interaction.focused_consumed)
                );
                begin_metric_row("Last event");
                ImGui::Text(
                    "%s via %s",
                    value_or_none(interaction.last_event_kind),
                    value_or_none(interaction.last_event_source)
                );
                begin_metric_row("Last target");
                ImGui::Text(
                    "%s | %s | %s",
                    value_or_none(interaction.last_event_target),
                    value_or_none(interaction.last_event_item),
                    value_or_none(interaction.last_event_chain_target)
                );
                begin_metric_row("Doors / triggers");
                ImGui::Text(
                    "%u/%u doors | %u/%u triggers",
                    interaction.active_doors,
                    interaction.total_doors,
                    interaction.fired_triggers,
                    interaction.total_triggers
                );
                begin_metric_row("Usables");
                ImGui::Text(
                    "%u enabled | %u blocked | %u consumed | %u total",
                    interaction.enabled_usables,
                    interaction.blocked_usables,
                    interaction.consumed_usables,
                    interaction.total_usables
                );
                begin_metric_row("Targets");
                ImGui::Text(
                    "%u activated | %u signaled",
                    interaction.activated_target_count,
                    interaction.signaled_target_count
                );
                begin_metric_row("Inventory");
                ImGui::Text(
                    "%s",
                    value_or_none(interaction.has_inventory ? interaction.inventory_summary : std::string{})
                );
                ImGui::EndTable();
            }

            if (interaction.has_focused_usable && !interaction.focused_detail.empty())
            {
                ImGui::TextWrapped("%s", interaction.focused_detail.c_str());
            }
            if (interaction.has_focused_usable && interaction.focused_has_target_door)
            {
                ImGui::Text(
                    "Target door: %s (open %.2f)",
                    value_or_none(interaction.focused_target_door_state),
                    interaction.focused_target_door_open_fraction
                );
            }
            if (interaction.has_transient_event)
            {
                ImGui::TextColored(
                    hud_event_color(interaction.transient_event_style),
                    "HUD event: %s",
                    value_or_none(interaction.transient_event_text)
                );
            }
        }

        if (advanced_diagnostics_visible_ && begin_section("Inspector", ImVec4(0.70F, 0.42F, 0.20F, 1.0F), true))
        {
            int source_filter_index = static_cast<int>(inspector_source_filter_);
            ImGui::SetNextItemWidth(160.0F);
            if (ImGui::Combo("Source filter", &source_filter_index, "All\0Direct\0Instanced\0Generated\0"))
            {
                inspector_source_filter_ = source_filter_from_index(source_filter_index);
            }
            const auto source_filter = inspector_source_filter_;
            const bool matches_source_filter =
                inspector.has_pick && inspector_matches_source_filter(inspector, source_filter);

            ImGui::Text("Picked object: %s", inspector.has_pick ? inspector.object_kind.c_str() : "(none)");
            ImGui::TextDisabled(
                "Mode: %s | Source bucket: %s",
                inspector.pinned ? "pinned" : "live",
                inspector.has_pick ? inspector_source_bucket_label(inspector) : "(none)"
            );
            if (inspector.has_pick && !matches_source_filter)
            {
                ImGui::TextDisabled(
                    "Filtered out by source filter '%s'.",
                    inspector_source_filter_label(source_filter)
                );
                draw_optional_text_wrapped_value("Current source", inspector.authored_summary);
            } else if (inspector.has_pick)
            {
                if (begin_metric_table("inspector_metrics"))
                {
                    begin_metric_row("Distance");
                    ImGui::Text("%.2f", inspector.distance);
                    begin_metric_row("Hit point");
                    ImGui::Text("%.2f %.2f %.2f", inspector.hit_point.x, inspector.hit_point.y, inspector.hit_point.z);
                    begin_metric_row("Bounds");
                    ImGui::Text(
                        "%.2f %.2f %.2f -> %.2f %.2f %.2f",
                        inspector.bounds_mins.x,
                        inspector.bounds_mins.y,
                        inspector.bounds_mins.z,
                        inspector.bounds_maxs.x,
                        inspector.bounds_maxs.y,
                        inspector.bounds_maxs.z
                    );
                    if (inspector.has_origin)
                    {
                        begin_metric_row("Origin");
                        ImGui::Text("%.2f %.2f %.2f", inspector.origin.x, inspector.origin.y, inspector.origin.z);
                    }
                    if (!inspector.material.empty())
                    {
                        begin_metric_row("Material");
                        ImGui::Text("%s | bright %.2f", inspector.material.c_str(), inspector.brightness);
                    }
                    if (!inspector.state.empty())
                    {
                        begin_metric_row("State");
                        ImGui::Text("%s", inspector.state.c_str());
                    }
                    if (!inspector.action_kind.empty() || !inspector.signal_action.empty())
                    {
                        begin_metric_row("Action");
                        ImGui::Text(
                            "%s | signal=%s",
                            inspector.action_kind.empty() ? "(none)" : inspector.action_kind.c_str(),
                            inspector.signal_action.empty() ? "(none)" : inspector.signal_action.c_str()
                        );
                    }
                    if (!inspector.visual_kind.empty() || !inspector.panel_mount_axis.empty())
                    {
                        begin_metric_row("Visual");
                        ImGui::Text(
                            "%s | mount=%s",
                            inspector.visual_kind.empty() ? "(none)" : inspector.visual_kind.c_str(),
                            inspector.panel_mount_axis.empty() ? "(none)" : inspector.panel_mount_axis.c_str()
                        );
                    }
                    ImGui::EndTable();
                }

                draw_optional_text_value("Name", inspector.name);
                draw_optional_text_wrapped_value("Label", inspector.label);
                draw_optional_text_value("Target", inspector.target);
                draw_optional_text_value("Chain target", inspector.chain_target);
                draw_optional_text_value("Required item", inspector.required_item);
                draw_optional_text_value("Pickup item", inspector.item_id);
                if (inspector.has_runtime_handle)
                {
                    draw_optional_text_value("Runtime handle", inspector.runtime_handle);
                }
                if (inspector.object_kind == "usable")
                {
                    ImGui::Text(
                        "Usable: enabled=%s locked=%s requirement=%s single_use=%s consumed=%s tracked_door=%s",
                        bool_label(inspector.enabled),
                        bool_label(inspector.locked),
                        bool_label(inspector.requirement_met),
                        bool_label(inspector.single_use),
                        bool_label(inspector.consumed),
                        bool_label(inspector.tracks_target_door)
                    );
                    ImGui::Text("Use radius: %.2f", inspector.use_radius);
                }
                if (inspector.object_kind == "door")
                {
                    ImGui::Text(
                        "Door: blocks_when_closed=%s open_fraction=%.2f",
                        bool_label(inspector.blocks_when_closed),
                        inspector.door_open_fraction
                    );
                }
                if (inspector.object_kind == "trigger")
                {
                    ImGui::Text("Trigger: once=%s fired=%s", bool_label(inspector.once), bool_label(inspector.fired));
                }
                if (inspector.has_authored_source)
                {
                    ImGui::SeparatorText("Authored Source");
                    if (begin_metric_table("inspector_source_metrics"))
                    {
                        begin_metric_row("Kind/index");
                        ImGui::Text(
                            "%s / %d",
                            inspector.authored_source_kind.empty() ? "(none)" : inspector.authored_source_kind.c_str(),
                            inspector.authored_index
                        );
                        begin_metric_row("Bucket");
                        ImGui::Text("%s", inspector_source_bucket_label(inspector));
                        if (!inspector.authored_placement_kind.empty())
                        {
                            begin_metric_row("Placement kind");
                            ImGui::Text("%s", inspector.authored_placement_kind.c_str());
                        }
                        if (!inspector.authored_template_name.empty())
                        {
                            begin_metric_row("Template");
                            ImGui::Text("%s", inspector.authored_template_name.c_str());
                        }
                        if (!inspector.authored_instance_name.empty())
                        {
                            begin_metric_row("Instance");
                            ImGui::Text("%s", inspector.authored_instance_name.c_str());
                        }
                        ImGui::EndTable();
                    }
                    ImGui::TextWrapped(
                        "Summary: %s",
                        inspector.authored_summary.empty() ? "(none)" : inspector.authored_summary.c_str()
                    );
                    draw_optional_text_value("Classname", inspector.authored_classname);
                    draw_optional_text_value("Entity kind", inspector.authored_entity_kind);
                    draw_optional_text_value("Targetname", inspector.authored_targetname);
                    draw_optional_source_location(
                        "Definition",
                        inspector.authored_file_path,
                        inspector.authored_line,
                        inspector.authored_column
                    );
                    draw_optional_source_location(
                        "Placement",
                        inspector.authored_placement_file_path,
                        inspector.authored_placement_line,
                        inspector.authored_placement_column
                    );
                    const RelayCore::debug::SourceLocation primary_source =
                        RelayCore::debug::build_primary_inspector_source_location(inspector);
                    const RelayCore::debug::SourceLocation definition_source =
                        RelayCore::debug::build_definition_inspector_source_location(inspector);
                    const std::string source_ref = build_full_inspector_source_reference(inspector);
                    if (primary_source.valid() || !source_ref.empty())
                    {
                        ImGui::Separator();
                        draw_source_open_button("Open Source", primary_source, "Inspector source");
                        if (definition_source.valid()
                            && build_source_reference(primary_source) != build_source_reference(definition_source))
                        {
                            ImGui::SameLine();
                            draw_source_open_button(
                                "Open Definition",
                                definition_source,
                                "Inspector definition source"
                            );
                        }
                        if (!source_ref.empty())
                        {
                            ImGui::SameLine();
                            if (ImGui::Button("Copy Ref"))
                            {
                                copy_text_to_clipboard(source_ref, "Inspector source reference");
                            }
                        }
                        ImGui::TextWrapped("Source ref: %s", source_ref.c_str());
                        ImGui::TextDisabled("F8 opens the primary authored source");
                    }
                }
            }
        }

        if (advanced_diagnostics_visible_ && begin_section("Audio", ImVec4(0.22F, 0.62F, 0.42F, 1.0F)))
        {
            if (begin_metric_table("audio_metrics"))
            {
                begin_metric_row("State");
                ImGui::Text(
                    "%s | master %.2f | zones %u",
                    bool_label(audio.initialized),
                    audio.master_volume,
                    audio.ambient_zone_count
                );
                begin_metric_row("Ambient");
                ImGui::Text(
                    "%s | %s | switches %u",
                    audio.has_active_loop ? "active" : "idle",
                    value_or_none(audio.active_source_kind),
                    audio.ambient_switch_count
                );
                if (audio.has_active_zone)
                {
                    begin_metric_row("Zone");
                    ImGui::Text(
                        "%s (#%d, prio %d, gain %.2f)",
                        value_or_none(audio.active_zone_name),
                        audio.active_zone_index,
                        audio.active_zone_priority,
                        audio.active_zone_gain
                    );
                }
                begin_metric_row("Loop");
                ImGui::TextWrapped("%s", value_or_none(audio.active_loop_path));
                begin_metric_row("One-shots");
                ImGui::Text("%u | loop vol %.2f", audio.one_shot_count, audio.active_loop_volume);
                begin_metric_row("Last one-shot");
                ImGui::TextWrapped("%s", value_or_none(audio.last_one_shot_path));
                ImGui::EndTable();
            }
        }

        if (advanced_diagnostics_visible_ && begin_section("Render Tuning", ImVec4(0.42F, 0.48F, 0.54F, 1.0F)))
        {
            draw_shadow_controls_contents(renderer_config);
            if ((world.shadow_preview_texture_id != 0 || world.shadow_far_preview_texture_id != 0)
                && ImGui::TreeNodeEx("Shadow Preview"))
            {
                draw_shadow_preview_contents(world);
                ImGui::TreePop();
            }
        }
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
        if (!diagnostics_visible_)
        {
            advanced_diagnostics_visible_ = false;
        }
    }

    void ImGuiOverlay::toggle_diagnostics_visible() noexcept
    {
        diagnostics_visible_ = !diagnostics_visible_;
        if (!diagnostics_visible_)
        {
            advanced_diagnostics_visible_ = false;
        }
    }

    auto ImGuiOverlay::advanced_diagnostics_visible() const noexcept -> bool
    {
        return advanced_diagnostics_visible_;
    }

    void ImGuiOverlay::set_advanced_diagnostics_visible(const bool visible) noexcept
    {
        advanced_diagnostics_visible_ = visible;
        if (advanced_diagnostics_visible_)
        {
            diagnostics_visible_ = true;
        }
    }

    void ImGuiOverlay::toggle_advanced_diagnostics_visible() noexcept
    {
        advanced_diagnostics_visible_ = !advanced_diagnostics_visible_;
        if (advanced_diagnostics_visible_)
        {
            diagnostics_visible_ = true;
        }
    }

    auto ImGuiOverlay::should_render() const noexcept -> bool
    {
        return initialized_ && (diagnostics_visible_ || advanced_diagnostics_visible_ || config_.show_demo_window);
    }

    auto ImGuiOverlay::consume_save_runtime_snapshot_requested() noexcept -> bool
    {
        const bool requested = save_runtime_snapshot_requested_;
        save_runtime_snapshot_requested_ = false;
        return requested;
    }

    auto ImGuiOverlay::consume_restore_runtime_snapshot_requested() noexcept -> bool
    {
        const bool requested = restore_runtime_snapshot_requested_;
        restore_runtime_snapshot_requested_ = false;
        return requested;
    }

    auto ImGuiOverlay::consume_open_first_authoring_warning_requested() noexcept -> bool
    {
        const bool requested = open_first_authoring_warning_requested_;
        open_first_authoring_warning_requested_ = false;
        return requested;
    }

    auto ImGuiOverlay::consume_dump_authoring_warnings_requested() noexcept -> bool
    {
        const bool requested = dump_authoring_warnings_requested_;
        dump_authoring_warnings_requested_ = false;
        return requested;
    }

    auto ImGuiOverlay::consume_open_first_runtime_registry_requested() noexcept -> bool
    {
        const bool requested = open_first_runtime_registry_requested_;
        open_first_runtime_registry_requested_ = false;
        return requested;
    }

    auto ImGuiOverlay::consume_dump_runtime_registry_requested() noexcept -> bool
    {
        const bool requested = dump_runtime_registry_requested_;
        dump_runtime_registry_requested_ = false;
        return requested;
    }

    auto ImGuiOverlay::consume_open_first_runtime_binding_requested() noexcept -> bool
    {
        const bool requested = open_first_runtime_binding_requested_;
        open_first_runtime_binding_requested_ = false;
        return requested;
    }

    auto ImGuiOverlay::consume_dump_runtime_bindings_requested() noexcept -> bool
    {
        const bool requested = dump_runtime_bindings_requested_;
        dump_runtime_bindings_requested_ = false;
        return requested;
    }

    auto ImGuiOverlay::consume_dump_render_authoring_contract_requested() noexcept -> bool
    {
        const bool requested = dump_render_authoring_contract_requested_;
        dump_render_authoring_contract_requested_ = false;
        return requested;
    }

    auto ImGuiOverlay::authoring_warning_source_filter() const noexcept -> render::DiagnosticsSourceFilter
    {
        return authoring_warning_source_filter_;
    }

    auto ImGuiOverlay::runtime_registry_source_filter() const noexcept -> render::DiagnosticsSourceFilter
    {
        return runtime_registry_source_filter_;
    }

    auto ImGuiOverlay::runtime_binding_source_filter() const noexcept -> render::DiagnosticsSourceFilter
    {
        return runtime_binding_source_filter_;
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

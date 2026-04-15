#include "relaycore/debug/imgui_overlay_summary_panel.h"

#include "relaycore/debug/imgui_overlay_widgets.h"

#include <algorithm>
#include <cfloat>
#include <imgui.h>
#include <string>

namespace RelayCore::debug
{
    void draw_summary_diagnostics_section(
        const ImGuiOverlaySummaryFrameStats& frame_stats,
        const render::RuntimeDiagnostics& runtime,
        const render::PlayerDiagnostics& player,
        const render::InteractionDiagnostics& interaction,
        const render::AudioDiagnostics& audio,
        const render::IterationDiagnostics& iteration
    )
    {
        if (!begin_section("Summary", ImVec4(0.25F, 0.48F, 0.82F, 1.0F), true))
        {
            return;
        }

        ImGui::Text(
            "Frame %.2f ms | avg %.2f ms | FPS %.1f",
            frame_stats.frame_time_ms,
            frame_stats.avg_frame_time_ms,
            frame_stats.fps
        );
        ImGui::PlotLines(
            "##frame_times",
            frame_stats.ordered_frame_times.data(),
            static_cast<int>(frame_stats.ordered_frame_times.size()),
            0,
            nullptr,
            0.0F,
            frame_stats.frame_plot_max,
            ImVec2(-FLT_MIN, 72.0F)
        );

        if (frame_stats.refresh_rate_hz > 0.0F)
        {
            ImGui::Text(
                "Budget %.2f ms @ %.1f Hz | VSync %s",
                frame_stats.target_frame_ms,
                frame_stats.refresh_rate_hz,
                frame_stats.vsync_label
            );
        } else
        {
            ImGui::Text(
                "Budget %.2f ms | Refresh unknown | VSync %s",
                frame_stats.target_frame_ms,
                frame_stats.vsync_label
            );
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
                ImGui::Text("%u / %u @ %.2f ms", runtime.fixed_steps, runtime.fixed_step_budget, runtime.fixed_step_ms);
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
                    std::clamp(ratio_or_zero(interaction.objective_current, interaction.objective_total), 0.0F, 1.0F),
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

        const bool frame_over_budget = frame_stats.avg_frame_time_ms > frame_stats.target_frame_ms * 1.05F;
        const bool sim_over_budget = runtime.fixed_steps > runtime.fixed_step_budget
                                     || runtime.simulation_backlog_ms > std::max(runtime.fixed_step_ms, 0.001F);
        const bool blocked_focus =
            interaction.has_focused_usable
            && (!interaction.focused_enabled || interaction.focused_locked || !interaction.focused_requirement_met);
        const bool has_attention = frame_over_budget || sim_over_budget || blocked_focus || !audio.initialized
                                   || !player.alive || player.health < 35.0F;
        if (!has_attention)
        {
            return;
        }

        ImGui::SeparatorText("Attention");
        if (frame_over_budget)
        {
            ImGui::TextColored(
                ImVec4(1.0F, 0.78F, 0.28F, 1.0F),
                "Frame average over budget: %.2f / %.2f ms",
                frame_stats.avg_frame_time_ms,
                frame_stats.target_frame_ms
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
} // namespace RelayCore::debug

#include "relaycore/debug/imgui_overlay_gameplay_panel.h"

#include "relaycore/debug/imgui_overlay_widgets.h"
#include "relaycore/render/camera.h"

#include <algorithm>
#include <cfloat>
#include <imgui.h>
#include <string>

namespace RelayCore::debug
{
    void
    draw_player_diagnostics_section(const render::PlayerDiagnostics& player, const render::PerspectiveCamera& camera)
    {
        if (!begin_section("Player", ImVec4(0.28F, 0.60F, 0.48F, 1.0F)))
        {
            return;
        }

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

    void draw_interaction_diagnostics_section(const render::InteractionDiagnostics& interaction)
    {
        if (!begin_section("Interaction", ImVec4(0.78F, 0.52F, 0.18F, 1.0F), true))
        {
            return;
        }

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
            ImGui::Text("%s", value_or_none(interaction.has_inventory ? interaction.inventory_summary : std::string{}));
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
} // namespace RelayCore::debug

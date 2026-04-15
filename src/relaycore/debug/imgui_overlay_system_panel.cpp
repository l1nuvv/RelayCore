#include "relaycore/debug/imgui_overlay_system_panel.h"

#include "relaycore/core/engine_config.h"
#include "relaycore/debug/imgui_overlay_widgets.h"

#include <imgui.h>
#include <string>

namespace RelayCore::debug
{
    void draw_world_diagnostics_section(const render::WorldDiagnostics& world)
    {
        if (!begin_section("World", ImVec4(0.20F, 0.55F, 0.75F, 1.0F), true))
        {
            return;
        }

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

    void draw_runtime_diagnostics_section(const render::RuntimeDiagnostics& runtime)
    {
        if (!begin_section("Runtime", ImVec4(0.32F, 0.44F, 0.58F, 1.0F)))
        {
            return;
        }

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

    void draw_audio_diagnostics_section(const render::AudioDiagnostics& audio)
    {
        if (!begin_section("Audio", ImVec4(0.22F, 0.62F, 0.42F, 1.0F)))
        {
            return;
        }

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

    void draw_render_tuning_section(const render::WorldDiagnostics& world, render::RendererConfig& renderer_config)
    {
        if (!begin_section("Render Tuning", ImVec4(0.42F, 0.48F, 0.54F, 1.0F)))
        {
            return;
        }

        draw_shadow_controls_contents(renderer_config);
        if ((world.shadow_preview_texture_id != 0 || world.shadow_far_preview_texture_id != 0)
            && ImGui::TreeNodeEx("Shadow Preview"))
        {
            draw_shadow_preview_contents(world);
            ImGui::TreePop();
        }
    }
} // namespace RelayCore::debug

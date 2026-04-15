#include "relaycore/debug/imgui_overlay_inspector_panel.h"

#include "relaycore/debug/imgui_overlay_widgets.h"
#include "relaycore/debug/inspector_source_reference.h"

#include <imgui.h>
#include <string>

namespace RelayCore::debug
{
    void draw_inspector_diagnostics_section(
        const render::InspectorDiagnostics& inspector,
        ImGuiOverlayInspectorPanelState& state
    )
    {
        if (!begin_section("Inspector", ImVec4(0.70F, 0.42F, 0.20F, 1.0F), true))
        {
            return;
        }

        int source_filter_index = static_cast<int>(state.inspector_source_filter);
        ImGui::SetNextItemWidth(160.0F);
        if (ImGui::Combo("Source filter", &source_filter_index, "All\0Direct\0Instanced\0Generated\0"))
        {
            state.inspector_source_filter = source_filter_from_index(source_filter_index);
        }
        const auto source_filter = state.inspector_source_filter;
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
            ImGui::TextDisabled("Filtered out by source filter '%s'.", inspector_source_filter_label(source_filter));
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
                const SourceLocation primary_source = build_primary_inspector_source_location(inspector);
                const SourceLocation definition_source = build_definition_inspector_source_location(inspector);
                const std::string source_ref = build_full_inspector_source_reference(inspector);
                if (primary_source.valid() || !source_ref.empty())
                {
                    ImGui::Separator();
                    draw_source_open_button("Open Source", primary_source, "Inspector source");
                    if (definition_source.valid()
                        && build_source_reference(primary_source) != build_source_reference(definition_source))
                    {
                        ImGui::SameLine();
                        draw_source_open_button("Open Definition", definition_source, "Inspector definition source");
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
} // namespace RelayCore::debug

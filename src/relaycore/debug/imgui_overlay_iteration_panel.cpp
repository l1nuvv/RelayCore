#include "relaycore/debug/imgui_overlay_iteration_panel.h"

#include "relaycore/debug/imgui_overlay_widgets.h"

#include <algorithm>
#include <imgui.h>
#include <string_view>

namespace RelayCore::debug
{
    namespace
    {
        [[nodiscard]] auto count_problem_runtime_bindings(const render::IterationDiagnostics& iteration) -> std::size_t
        {
            return static_cast<std::size_t>(std::count_if(
                iteration.runtime_bindings.begin(),
                iteration.runtime_bindings.end(),
                [](const render::RuntimeBindingDiagnostics& binding) {
                    return binding.unresolved_target || binding.ambiguous_target;
                }
            ));
        }
    } // namespace

    void draw_iteration_diagnostics_section(
        const render::IterationDiagnostics& iteration,
        ImGuiOverlayIterationPanelState& state
    )
    {
        if (!begin_section("Iteration", ImVec4(0.46F, 0.36F, 0.24F, 1.0F), true))
        {
            return;
        }

        if (begin_metric_table("iteration_metrics"))
        {
            begin_metric_row("Last reload");
            ImGui::Text("%s", iteration.has_last_reload_result ? value_or_none(iteration.last_reload_summary) : "idle");
            begin_metric_row("Result");
            ImGui::Text(
                "%s",
                iteration.has_last_reload_result ? (iteration.last_reload_succeeded ? "ok" : "failed") : "not attempted"
            );
            begin_metric_row("Last tool");
            ImGui::Text(
                "%s",
                iteration.has_last_command_result ? value_or_none(iteration.last_command_summary) : "idle"
            );
            begin_metric_row("Warnings");
            ImGui::Text("%u", static_cast<unsigned>(iteration.authoring_warnings.size()));
            begin_metric_row("Binding issues");
            ImGui::Text("%u", static_cast<unsigned>(count_problem_runtime_bindings(iteration)));
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
                iteration.last_command_succeeded ? ImVec4(0.45F, 0.90F, 0.55F, 1.0F) : ImVec4(1.0F, 0.55F, 0.35F, 1.0F),
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
            state.save_runtime_snapshot_requested = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Restore Snapshot"))
        {
            state.restore_runtime_snapshot_requested = true;
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
        ImGui::TextUnformatted("Dump the current material vocabulary and light contract from render_authoring.toml.");
        if (ImGui::Button("Dump Contract to Log"))
        {
            state.dump_render_authoring_contract_requested = true;
        }

        ImGui::SeparatorText("Authoring Warnings");
        if (iteration.authoring_warnings.empty())
        {
            ImGui::TextUnformatted("No authored warnings for the current map.");
        } else
        {
            int warning_filter_index = static_cast<int>(state.authoring_warning_source_filter);
            ImGui::SetNextItemWidth(160.0F);
            if (ImGui::Combo("Warning filter", &warning_filter_index, "All\0Direct\0Instanced\0Generated\0"))
            {
                state.authoring_warning_source_filter = source_filter_from_index(warning_filter_index);
            }

            const auto warning_filter = state.authoring_warning_source_filter;
            if (ImGui::Button("Open First##warning"))
            {
                state.open_first_authoring_warning_requested = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Dump Filtered##warning"))
            {
                state.dump_authoring_warnings_requested = true;
            }
            for (std::size_t warning_index = 0; warning_index < iteration.authoring_warnings.size(); ++warning_index)
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

                const SourceLocation primary_location{
                    .file_path = warning.primary_file_path,
                    .line = warning.primary_line,
                    .column = warning.primary_column,
                };
                const SourceLocation secondary_location{
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
            int registry_filter_index = static_cast<int>(state.runtime_registry_source_filter);
            ImGui::SetNextItemWidth(160.0F);
            if (ImGui::Combo("Registry filter", &registry_filter_index, "All\0Direct\0Instanced\0Generated\0"))
            {
                state.runtime_registry_source_filter = source_filter_from_index(registry_filter_index);
            }

            const auto registry_filter = state.runtime_registry_source_filter;
            if (ImGui::Button("Open First##registry"))
            {
                state.open_first_runtime_registry_requested = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Dump Filtered##registry"))
            {
                state.dump_runtime_registry_requested = true;
            }
            for (std::size_t entry_index = 0; entry_index < iteration.runtime_registry.size(); ++entry_index)
            {
                const render::RuntimeRegistryEntryDiagnostics& entry = iteration.runtime_registry[entry_index];
                const std::string_view bucket =
                    entry.source_bucket.empty() ? std::string_view{"untracked"} : std::string_view{entry.source_bucket};
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
                ImGui::TextDisabled("Bucket: %.*s", static_cast<int>(bucket.size()), bucket.data());
                if (!entry.source_reference.empty())
                {
                    ImGui::TextWrapped("Source: %s", entry.source_reference.c_str());
                }
                draw_source_open_button(
                    "Open Source",
                    SourceLocation{
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
            int binding_filter_index = static_cast<int>(state.runtime_binding_source_filter);
            ImGui::SetNextItemWidth(160.0F);
            if (ImGui::Combo("Binding filter", &binding_filter_index, "All\0Direct\0Instanced\0Generated\0"))
            {
                state.runtime_binding_source_filter = source_filter_from_index(binding_filter_index);
            }

            const auto binding_filter = state.runtime_binding_source_filter;
            if (ImGui::Button("Open First##binding"))
            {
                state.open_first_runtime_binding_requested = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Dump Filtered##binding"))
            {
                state.dump_runtime_bindings_requested = true;
            }
            for (std::size_t binding_index = 0; binding_index < iteration.runtime_bindings.size(); ++binding_index)
            {
                const render::RuntimeBindingDiagnostics& binding = iteration.runtime_bindings[binding_index];
                const std::string_view bucket = binding.source_bucket.empty() ? std::string_view{"untracked"}
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
                if (binding.unresolved_target)
                {
                    ImGui::TextColored(
                        ImVec4(1.0F, 0.55F, 0.35F, 1.0F),
                        "Resolved: 0 | unresolved runtime object link"
                    );
                } else if (binding.ambiguous_target)
                {
                    ImGui::TextColored(
                        ImVec4(1.0F, 0.78F, 0.28F, 1.0F),
                        "Resolved: %u | ambiguous runtime object link",
                        binding.resolved_target_count
                    );
                } else
                {
                    ImGui::TextWrapped(
                        "Resolved: %u | %s",
                        binding.resolved_target_count,
                        value_or_none(binding.resolved_targets_summary)
                    );
                }
                ImGui::TextDisabled("Bucket: %.*s", static_cast<int>(bucket.size()), bucket.data());
                draw_source_open_button(
                    "Open Source",
                    SourceLocation{
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
} // namespace RelayCore::debug

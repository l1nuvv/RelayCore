#include "relaycore/core/log.h"
#include "relaycore/debug/inspector_source_reference.h"
#include "relaycore/render/renderer.h"
#include "relaygame/demo_game_debug_commands_internal.h"

namespace RelayGame::detail
{
    auto handle_authoring_warning_commands(
        DemoGameRuntimeState state,
        RelayCore::EngineServices& services,
        const IterationCommandRequests& requests
    ) -> bool
    {
        if (requests.overlay.open_first_authoring_warning)
        {
            const auto filter = requests.overlay.authoring_warning_source_filter;
            for (const auto& warning : state.iteration_diagnostics.authoring_warnings)
            {
                if (!warning_matches_filter(warning, filter))
                {
                    continue;
                }

                RelayCore::debug::SourceLocation location{
                    .file_path = warning.primary_file_path,
                    .line = warning.primary_line,
                    .column = warning.primary_column,
                };
                if (!location.valid())
                {
                    location = RelayCore::debug::SourceLocation{
                        .file_path = warning.secondary_file_path,
                        .line = warning.secondary_line,
                        .column = warning.secondary_column,
                    };
                }
                if (!location.valid())
                {
                    continue;
                }

                const std::string source_ref = RelayCore::debug::build_source_reference(location);
                const RelayCore::debug::SourceOpenResult result = RelayCore::debug::open_source_location(location);
                if (!result.launched)
                {
                    publish_and_log_command_result(
                        state,
                        services,
                        false,
                        "Open first authoring warning failed",
                        "Failed to open authoring warning source " + source_ref + ": "
                            + (result.error.empty() ? std::string{"unknown error"} : result.error)
                    );
                } else
                {
                    publish_and_log_command_result(
                        state,
                        services,
                        true,
                        "Opened first authoring warning (" + std::string(diagnostics_filter_label(filter)) + ") via "
                            + (result.strategy.empty() ? std::string{"editor launcher"} : result.strategy) + ": "
                            + source_ref
                    );
                }
                return true;
            }

            publish_and_log_command_result(
                state,
                services,
                false,
                "Open first authoring warning failed",
                "No filtered authoring warning with a valid source location"
            );
            return true;
        }

        if (requests.overlay.dump_authoring_warnings)
        {
            const auto filter = requests.overlay.authoring_warning_source_filter;
            std::size_t match_count = 0;
            RelayCore::log::info("Authoring warnings dump [{}]", diagnostics_filter_label(filter));
            for (const auto& warning : state.iteration_diagnostics.authoring_warnings)
            {
                if (!warning_matches_filter(warning, filter))
                {
                    continue;
                }

                ++match_count;
                const std::string primary_ref = source_reference_for_location(
                    warning.primary_file_path,
                    warning.primary_line,
                    warning.primary_column
                );
                const std::string secondary_ref = source_reference_for_location(
                    warning.secondary_file_path,
                    warning.secondary_line,
                    warning.secondary_column
                );
                RelayCore::log::warn(
                    "[{}] {} | bucket={} | A={} | B={}",
                    match_count,
                    warning.message,
                    warning.source_bucket.empty() ? std::string{"direct"} : warning.source_bucket,
                    primary_ref.empty() ? std::string{"(none)"} : primary_ref,
                    secondary_ref.empty() ? std::string{"(none)"} : secondary_ref
                );
            }
            publish_and_log_command_result(
                state,
                services,
                true,
                "Dumped " + std::to_string(match_count) + " authoring warnings ("
                    + std::string(diagnostics_filter_label(filter)) + ")"
            );
            return true;
        }

        return false;
    }

    auto handle_runtime_registry_commands(
        DemoGameRuntimeState state,
        RelayCore::EngineServices& services,
        const IterationCommandRequests& requests
    ) -> bool
    {
        const bool open_requested = requests.overlay.open_first_runtime_registry;
        const bool dump_requested = requests.overlay.dump_runtime_registry;
        if (!open_requested && !dump_requested)
        {
            return false;
        }

        if (open_requested)
        {
            const auto filter = requests.overlay.runtime_registry_source_filter;
            for (const auto& entry : state.iteration_diagnostics.runtime_registry)
            {
                const std::string_view bucket =
                    entry.source_bucket.empty() ? std::string_view{"untracked"} : std::string_view{entry.source_bucket};
                if (!source_bucket_matches_filter(bucket, filter, true))
                {
                    continue;
                }

                const RelayCore::debug::SourceLocation location{
                    .file_path = entry.source_file_path,
                    .line = entry.source_line,
                    .column = entry.source_column,
                };
                if (!location.valid())
                {
                    continue;
                }

                const std::string source_ref = RelayCore::debug::build_source_reference(location);
                const RelayCore::debug::SourceOpenResult result = RelayCore::debug::open_source_location(location);
                if (!result.launched)
                {
                    publish_and_log_command_result(
                        state,
                        services,
                        false,
                        "Open first runtime registry source failed",
                        "Failed to open runtime registry source " + source_ref + ": "
                            + (result.error.empty() ? std::string{"unknown error"} : result.error)
                    );
                } else
                {
                    publish_and_log_command_result(
                        state,
                        services,
                        true,
                        "Opened first runtime registry source (" + std::string(diagnostics_filter_label(filter))
                            + ") via " + (result.strategy.empty() ? std::string{"editor launcher"} : result.strategy)
                            + ": " + source_ref
                    );
                }
                return true;
            }

            publish_and_log_command_result(
                state,
                services,
                false,
                "Open first runtime registry source failed",
                "No filtered runtime registry entry with a valid source location"
            );
            return true;
        }

        const auto filter = requests.overlay.runtime_registry_source_filter;
        std::size_t match_count = 0;
        RelayCore::log::info("Runtime registry dump [{}]", diagnostics_filter_label(filter));
        for (const auto& entry : state.iteration_diagnostics.runtime_registry)
        {
            const std::string_view bucket =
                entry.source_bucket.empty() ? std::string_view{"untracked"} : std::string_view{entry.source_bucket};
            if (!source_bucket_matches_filter(bucket, filter, true))
            {
                continue;
            }

            ++match_count;
            RelayCore::log::info(
                "[{}] {} | {} | name={} | label={} | target={} | source={}",
                match_count,
                entry.runtime_handle,
                entry.object_kind,
                entry.name.empty() ? std::string{"(none)"} : entry.name,
                entry.label.empty() ? std::string{"(none)"} : entry.label,
                entry.target.empty() ? std::string{"(none)"} : entry.target,
                entry.source_reference.empty() ? std::string{"(none)"} : entry.source_reference
            );
        }
        publish_and_log_command_result(
            state,
            services,
            true,
            "Dumped " + std::to_string(match_count) + " runtime registry entries ("
                + std::string(diagnostics_filter_label(filter)) + ")"
        );
        return true;
    }

    auto handle_runtime_binding_commands(
        DemoGameRuntimeState state,
        RelayCore::EngineServices& services,
        const IterationCommandRequests& requests
    ) -> bool
    {
        if (requests.overlay.open_first_runtime_binding)
        {
            const auto filter = requests.overlay.runtime_binding_source_filter;
            for (const auto& binding : state.iteration_diagnostics.runtime_bindings)
            {
                const std::string_view bucket = binding.source_bucket.empty() ? std::string_view{"untracked"}
                                                                              : std::string_view{binding.source_bucket};
                if (!source_bucket_matches_filter(bucket, filter, true))
                {
                    continue;
                }

                const RelayCore::debug::SourceLocation location{
                    .file_path = binding.source_file_path,
                    .line = binding.source_line,
                    .column = binding.source_column,
                };
                if (!location.valid())
                {
                    continue;
                }

                const std::string source_ref = RelayCore::debug::build_source_reference(location);
                const RelayCore::debug::SourceOpenResult result = RelayCore::debug::open_source_location(location);
                if (!result.launched)
                {
                    publish_and_log_command_result(
                        state,
                        services,
                        false,
                        "Open first runtime binding source failed",
                        "Failed to open runtime binding source " + source_ref + ": "
                            + (result.error.empty() ? std::string{"unknown error"} : result.error)
                    );
                } else
                {
                    publish_and_log_command_result(
                        state,
                        services,
                        true,
                        "Opened first runtime binding source (" + std::string(diagnostics_filter_label(filter))
                            + ") via " + (result.strategy.empty() ? std::string{"editor launcher"} : result.strategy)
                            + ": " + source_ref
                    );
                }
                return true;
            }

            publish_and_log_command_result(
                state,
                services,
                false,
                "Open first runtime binding source failed",
                "No filtered runtime binding with a valid source location"
            );
            return true;
        }

        if (requests.overlay.dump_runtime_bindings)
        {
            const auto filter = requests.overlay.runtime_binding_source_filter;
            std::size_t match_count = 0;
            RelayCore::log::info("Runtime bindings dump [{}]", diagnostics_filter_label(filter));
            for (const auto& binding : state.iteration_diagnostics.runtime_bindings)
            {
                const std::string_view bucket = binding.source_bucket.empty() ? std::string_view{"untracked"}
                                                                              : std::string_view{binding.source_bucket};
                if (!source_bucket_matches_filter(bucket, filter, true))
                {
                    continue;
                }

                ++match_count;
                RelayCore::log::info(
                    "[{}] {} ({}) -> {} | resolved={} | status={} | targets={} | source={}",
                    match_count,
                    binding.source_name.empty() ? std::string{"(none)"} : binding.source_name,
                    binding.source_kind.empty() ? std::string{"(none)"} : binding.source_kind,
                    binding.target.empty() ? std::string{"(none)"} : binding.target,
                    binding.resolved_target_count,
                    binding.unresolved_target  ? std::string{"unresolved"}
                    : binding.ambiguous_target ? std::string{"ambiguous"}
                                               : std::string{"ok"},
                    binding.resolved_targets_summary.empty() ? std::string{"(none)"} : binding.resolved_targets_summary,
                    binding.source_reference.empty() ? std::string{"(none)"} : binding.source_reference
                );
            }
            publish_and_log_command_result(
                state,
                services,
                true,
                "Dumped " + std::to_string(match_count) + " runtime bindings ("
                    + std::string(diagnostics_filter_label(filter)) + ")"
            );
            return true;
        }

        return false;
    }

    void handle_open_inspector_source_command(
        DemoGameRuntimeState state,
        RelayCore::EngineServices& services,
        const IterationCommandRequests& requests
    )
    {
        if (!requests.runtime.open_inspector_source)
        {
            return;
        }

        RelayCore::Renderer& renderer = services.renderer();
        const RelayCore::render::InspectorDiagnostics& inspector = renderer.inspector_diagnostics();
        const RelayCore::debug::SourceLocation source_location =
            RelayCore::debug::build_primary_inspector_source_location(inspector);
        const std::string source_ref = RelayCore::debug::build_source_reference(source_location);
        if (!source_location.valid())
        {
            RelayCore::log::info("Inspector source open ignored: no authored source available");
            return;
        }

        const RelayCore::debug::SourceOpenResult result = RelayCore::debug::open_source_location(source_location);
        if (!result.launched)
        {
            RelayCore::log::warn(
                "Failed to open inspector source {}: {}",
                source_ref,
                result.error.empty() ? std::string{"unknown error"} : result.error
            );
            return;
        }

        RelayCore::log::info(
            "Opened inspector source via {}: {}",
            result.strategy.empty() ? std::string{"editor launcher"} : result.strategy,
            source_ref
        );
    }
} // namespace RelayGame::detail

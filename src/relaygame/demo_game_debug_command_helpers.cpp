#include "relaycore/core/log.h"
#include "relaycore/debug/inspector_source_reference.h"
#include "relaycore/render/renderer.h"
#include "relaygame/demo_game_debug_commands_internal.h"

namespace RelayGame::detail
{
    auto capture_iteration_command_requests(DemoGameRuntimeState state, RelayCore::Renderer& renderer)
        -> IterationCommandRequests
    {
        IterationCommandRequests requests{};
        requests.runtime = state.runtime.consume_iteration_command_requests();
        const RelayCore::Renderer::IterationCommandRequests overlay_requests =
            renderer.consume_iteration_command_requests();
        requests.overlay.save_runtime_snapshot = overlay_requests.save_runtime_snapshot;
        requests.overlay.restore_runtime_snapshot = overlay_requests.restore_runtime_snapshot;
        requests.overlay.open_first_authoring_warning = overlay_requests.open_first_authoring_warning;
        requests.overlay.dump_authoring_warnings = overlay_requests.dump_authoring_warnings;
        requests.overlay.open_first_runtime_registry = overlay_requests.open_first_runtime_registry;
        requests.overlay.dump_runtime_registry = overlay_requests.dump_runtime_registry;
        requests.overlay.open_first_runtime_binding = overlay_requests.open_first_runtime_binding;
        requests.overlay.dump_runtime_bindings = overlay_requests.dump_runtime_bindings;
        requests.overlay.dump_render_authoring_contract = overlay_requests.dump_render_authoring_contract;
        requests.overlay.authoring_warning_source_filter = overlay_requests.authoring_warning_source_filter;
        requests.overlay.runtime_registry_source_filter = overlay_requests.runtime_registry_source_filter;
        requests.overlay.runtime_binding_source_filter = overlay_requests.runtime_binding_source_filter;
        return requests;
    }

    auto diagnostics_filter_label(const RelayCore::render::DiagnosticsSourceFilter filter) -> std::string_view
    {
        switch (filter)
        {
            case RelayCore::render::DiagnosticsSourceFilter::Direct:
                return "direct";
            case RelayCore::render::DiagnosticsSourceFilter::Instanced:
                return "instanced";
            case RelayCore::render::DiagnosticsSourceFilter::Generated:
                return "generated";
            case RelayCore::render::DiagnosticsSourceFilter::All:
            default:
                return "all";
        }
    }

    auto source_bucket_matches_filter(
        const std::string_view bucket,
        const RelayCore::render::DiagnosticsSourceFilter filter,
        const bool direct_matches_untracked
    ) -> bool
    {
        if (filter == RelayCore::render::DiagnosticsSourceFilter::All)
        {
            return true;
        }

        if (direct_matches_untracked && filter == RelayCore::render::DiagnosticsSourceFilter::Direct
            && bucket == "untracked")
        {
            return true;
        }

        switch (filter)
        {
            case RelayCore::render::DiagnosticsSourceFilter::Direct:
                return bucket == "direct";
            case RelayCore::render::DiagnosticsSourceFilter::Instanced:
                return bucket == "instanced";
            case RelayCore::render::DiagnosticsSourceFilter::Generated:
                return bucket == "generated";
            case RelayCore::render::DiagnosticsSourceFilter::All:
            default:
                return true;
        }
    }

    auto warning_matches_filter(
        const RelayCore::render::AuthoringWarningDiagnostics& warning,
        const RelayCore::render::DiagnosticsSourceFilter filter
    ) -> bool
    {
        const std::string_view bucket =
            warning.source_bucket.empty() ? std::string_view{"direct"} : std::string_view{warning.source_bucket};
        return source_bucket_matches_filter(bucket, filter);
    }

    auto source_reference_for_location(const std::string& file_path, const int line, const int column) -> std::string
    {
        return RelayCore::debug::build_source_reference(RelayCore::debug::SourceLocation{
            .file_path = file_path,
            .line = line,
            .column = column,
        });
    }

    void publish_and_log_command_result(
        const DemoGameRuntimeState state,
        RelayCore::EngineServices& services,
        const bool succeeded,
        std::string summary,
        std::string error
    )
    {
        publish_command_result(state.iteration_diagnostics, succeeded, std::move(summary), std::move(error));
        push_iteration_diagnostics(state, services);
        if (state.iteration_diagnostics.last_command_succeeded)
        {
            RelayCore::log::info("{}", state.iteration_diagnostics.last_command_summary);
        } else
        {
            RelayCore::log::warn(
                "{}",
                state.iteration_diagnostics.last_command_error.empty()
                    ? state.iteration_diagnostics.last_command_summary
                    : state.iteration_diagnostics.last_command_error
            );
        }
    }
} // namespace RelayGame::detail

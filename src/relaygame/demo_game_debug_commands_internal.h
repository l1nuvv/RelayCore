#pragma once

#include "relaygame/demo_game_iteration_internal.h"

#include <string>
#include <string_view>

namespace RelayGame::detail
{
    struct OverlayIterationCommandRequests {
        bool save_runtime_snapshot = false;
        bool restore_runtime_snapshot = false;
        bool open_first_authoring_warning = false;
        bool dump_authoring_warnings = false;
        bool open_first_runtime_registry = false;
        bool dump_runtime_registry = false;
        bool open_first_runtime_binding = false;
        bool dump_runtime_bindings = false;
        bool dump_render_authoring_contract = false;
        RelayCore::render::DiagnosticsSourceFilter authoring_warning_source_filter =
            RelayCore::render::DiagnosticsSourceFilter::All;
        RelayCore::render::DiagnosticsSourceFilter runtime_registry_source_filter =
            RelayCore::render::DiagnosticsSourceFilter::All;
        RelayCore::render::DiagnosticsSourceFilter runtime_binding_source_filter =
            RelayCore::render::DiagnosticsSourceFilter::All;
    };

    struct IterationCommandRequests {
        RelayGame::GameWorldRuntime::IterationCommandRequests runtime{};
        OverlayIterationCommandRequests overlay{};
    };

    [[nodiscard]] auto diagnostics_filter_label(RelayCore::render::DiagnosticsSourceFilter filter) -> std::string_view;

    [[nodiscard]] auto source_bucket_matches_filter(
        std::string_view bucket,
        RelayCore::render::DiagnosticsSourceFilter filter,
        bool direct_matches_untracked = false
    ) -> bool;

    [[nodiscard]] auto warning_matches_filter(
        const RelayCore::render::AuthoringWarningDiagnostics& warning,
        RelayCore::render::DiagnosticsSourceFilter filter
    ) -> bool;

    [[nodiscard]] auto source_reference_for_location(const std::string& file_path, int line, int column) -> std::string;

    void publish_and_log_command_result(
        DemoGameRuntimeState state,
        RelayCore::EngineServices& services,
        bool succeeded,
        std::string summary,
        std::string error = {}
    );

    [[nodiscard]] auto capture_iteration_command_requests(DemoGameRuntimeState state, RelayCore::Renderer& renderer)
        -> IterationCommandRequests;

    [[nodiscard]] auto handle_runtime_snapshot_commands(
        DemoGameRuntimeState state,
        RelayCore::EngineServices& services,
        const IterationCommandRequests& requests
    ) -> bool;

    [[nodiscard]] auto handle_authoring_warning_commands(
        DemoGameRuntimeState state,
        RelayCore::EngineServices& services,
        const IterationCommandRequests& requests
    ) -> bool;

    [[nodiscard]] auto handle_runtime_registry_commands(
        DemoGameRuntimeState state,
        RelayCore::EngineServices& services,
        const IterationCommandRequests& requests
    ) -> bool;

    [[nodiscard]] auto handle_runtime_binding_commands(
        DemoGameRuntimeState state,
        RelayCore::EngineServices& services,
        const IterationCommandRequests& requests
    ) -> bool;

    [[nodiscard]] auto handle_render_authoring_commands(
        DemoGameRuntimeState state,
        RelayCore::EngineServices& services,
        const IterationCommandRequests& requests
    ) -> bool;

    void handle_open_inspector_source_command(
        DemoGameRuntimeState state,
        RelayCore::EngineServices& services,
        const IterationCommandRequests& requests
    );
} // namespace RelayGame::detail

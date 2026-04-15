#pragma once

#include "relaycore/render/render_diagnostics.h"

namespace RelayCore::debug
{
    struct ImGuiOverlayIterationPanelState {
        bool& save_runtime_snapshot_requested;
        bool& restore_runtime_snapshot_requested;
        bool& open_first_authoring_warning_requested;
        bool& dump_authoring_warnings_requested;
        bool& open_first_runtime_registry_requested;
        bool& dump_runtime_registry_requested;
        bool& open_first_runtime_binding_requested;
        bool& dump_runtime_bindings_requested;
        bool& dump_render_authoring_contract_requested;
        render::DiagnosticsSourceFilter& authoring_warning_source_filter;
        render::DiagnosticsSourceFilter& runtime_registry_source_filter;
        render::DiagnosticsSourceFilter& runtime_binding_source_filter;
    };

    void draw_iteration_diagnostics_section(
        const render::IterationDiagnostics& iteration,
        ImGuiOverlayIterationPanelState& state
    );
} // namespace RelayCore::debug

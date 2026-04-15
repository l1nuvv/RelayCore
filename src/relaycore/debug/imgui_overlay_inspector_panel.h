#pragma once

#include "relaycore/render/render_diagnostics.h"

namespace RelayCore::debug
{
    struct ImGuiOverlayInspectorPanelState {
        render::DiagnosticsSourceFilter& inspector_source_filter;
    };

    void draw_inspector_diagnostics_section(
        const render::InspectorDiagnostics& inspector,
        ImGuiOverlayInspectorPanelState& state
    );
} // namespace RelayCore::debug

#pragma once

#include "relaycore/render/render_diagnostics.h"

#include <span>

namespace RelayCore::debug
{
    struct ImGuiOverlaySummaryFrameStats {
        float frame_time_ms = 0.0F;
        float avg_frame_time_ms = 0.0F;
        float fps = 0.0F;
        float target_frame_ms = 0.0F;
        float refresh_rate_hz = 0.0F;
        float frame_plot_max = 0.0F;
        const char* vsync_label = "off";
        std::span<const float> ordered_frame_times{};
    };

    void draw_summary_diagnostics_section(
        const ImGuiOverlaySummaryFrameStats& frame_stats,
        const render::RuntimeDiagnostics& runtime,
        const render::PlayerDiagnostics& player,
        const render::InteractionDiagnostics& interaction,
        const render::AudioDiagnostics& audio,
        const render::IterationDiagnostics& iteration
    );
} // namespace RelayCore::debug

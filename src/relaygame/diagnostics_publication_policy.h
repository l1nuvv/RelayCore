#pragma once

namespace RelayGame
{
    struct DiagnosticsPublicationNeeds {
        bool runtime_and_audio = false;
        bool inspector = false;
    };

    [[nodiscard]] constexpr auto diagnostics_publication_needs(
        const bool diagnostics_visible,
        const bool advanced_diagnostics_visible,
        const bool inspector_visualization_enabled
    ) noexcept -> DiagnosticsPublicationNeeds
    {
        return DiagnosticsPublicationNeeds{
            .runtime_and_audio = diagnostics_visible || advanced_diagnostics_visible,
            .inspector = advanced_diagnostics_visible || inspector_visualization_enabled,
        };
    }
} // namespace RelayGame

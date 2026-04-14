#pragma once

#include "relaycore/render/render_diagnostics.h"

#include <string>

namespace RelayCore::debug
{
    struct SourceLocation {
        std::string file_path{};
        int line = 0;
        int column = 0;

        [[nodiscard]] auto valid() const noexcept -> bool
        {
            return !file_path.empty() && line > 0 && column > 0;
        }
    };

    struct SourceOpenResult {
        bool launched = false;
        std::string strategy{};
        std::string error{};
    };

    [[nodiscard]] auto has_copyable_inspector_source(const render::InspectorDiagnostics& inspector) -> bool;

    [[nodiscard]] auto build_source_reference(const SourceLocation& location) -> std::string;

    [[nodiscard]] auto build_definition_inspector_source_location(const render::InspectorDiagnostics& inspector)
        -> SourceLocation;

    [[nodiscard]] auto build_placement_inspector_source_location(const render::InspectorDiagnostics& inspector)
        -> SourceLocation;

    [[nodiscard]] auto build_primary_inspector_source_location(const render::InspectorDiagnostics& inspector)
        -> SourceLocation;

    [[nodiscard]] auto build_primary_inspector_source_reference(const render::InspectorDiagnostics& inspector)
        -> std::string;

    [[nodiscard]] auto build_full_inspector_source_reference(const render::InspectorDiagnostics& inspector)
        -> std::string;

    [[nodiscard]] auto open_source_location(const SourceLocation& location) -> SourceOpenResult;
} // namespace RelayCore::debug

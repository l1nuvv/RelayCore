#pragma once

#include "relaycore/debug/inspector_source_reference.h"
#include "relaycore/render/render_diagnostics.h"

#include <cstdint>
#include <imgui.h>
#include <string>
#include <string_view>

namespace RelayCore::render
{
    struct RendererConfig;
}

namespace RelayCore::debug
{
    using InspectorSourceFilter = render::DiagnosticsSourceFilter;

    [[nodiscard]] auto movement_mode_label(std::uint32_t movement_mode) -> const char*;

    [[nodiscard]] auto bool_label(bool value) -> const char*;

    [[nodiscard]] auto value_or_none(const std::string& value) -> const char*;

    [[nodiscard]] auto ratio_or_zero(std::uint32_t value, std::uint32_t total) -> float;

    [[nodiscard]] auto hud_event_color(render::HudEventStyle style) -> ImVec4;

    [[nodiscard]] auto inspector_source_bucket_label(const render::InspectorDiagnostics& inspector) -> const char*;

    [[nodiscard]] auto inspector_source_filter_label(InspectorSourceFilter filter) -> const char*;

    [[nodiscard]] auto source_filter_from_index(int filter_index) -> InspectorSourceFilter;

    [[nodiscard]] auto
    inspector_matches_source_filter(const render::InspectorDiagnostics& inspector, InspectorSourceFilter filter)
        -> bool;

    [[nodiscard]] auto warning_source_bucket_label(const render::AuthoringWarningDiagnostics& warning) -> const char*;

    [[nodiscard]] auto source_bucket_matches_filter(std::string_view bucket, InspectorSourceFilter filter) -> bool;

    [[nodiscard]] auto
    warning_matches_source_filter(const render::AuthoringWarningDiagnostics& warning, InspectorSourceFilter filter)
        -> bool;

    void draw_text_value(const char* label, const std::string& value);

    void draw_text_wrapped_value(const char* label, const std::string& value);

    void draw_optional_text_value(const char* label, const std::string& value);

    void draw_optional_text_wrapped_value(const char* label, const std::string& value);

    void draw_optional_source_location(const char* label, const std::string& file_path, int line, int column);

    void copy_text_to_clipboard(const std::string& text, const char* context_label);

    void draw_source_open_button(const char* button_label, const SourceLocation& location, const char* context_label);

    [[nodiscard]] auto begin_section(const char* label, const ImVec4& accent, bool default_open = false) -> bool;

    [[nodiscard]] auto begin_metric_table(const char* id) -> bool;

    void begin_metric_row(const char* label);

    void configure_imgui_font(ImGuiIO& io);

    void draw_shadow_controls_contents(render::RendererConfig& renderer_config);

    void draw_shadow_preview_contents(const render::WorldDiagnostics& world);
} // namespace RelayCore::debug

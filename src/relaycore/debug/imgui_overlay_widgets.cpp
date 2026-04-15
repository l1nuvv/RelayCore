#include "relaycore/debug/imgui_overlay_widgets.h"

#include "relaycore/core/engine_config.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"

#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_error.h>
#include <algorithm>
#include <cstdint>
#include <filesystem>

namespace RelayCore::debug
{
    auto movement_mode_label(const std::uint32_t movement_mode) -> const char*
    {
        switch (movement_mode)
        {
            case 0:
                return "grounded";
            case 1:
                return "air";
            case 2:
                return "noclip";
            default:
                return "unknown";
        }
    }

    auto bool_label(const bool value) -> const char*
    {
        return value ? "yes" : "no";
    }

    auto value_or_none(const std::string& value) -> const char*
    {
        return value.empty() ? "(none)" : value.c_str();
    }

    auto ratio_or_zero(const std::uint32_t value, const std::uint32_t total) -> float
    {
        if (total == 0)
        {
            return 0.0F;
        }

        return static_cast<float>(value) / static_cast<float>(total);
    }

    auto hud_event_color(const render::HudEventStyle style) -> ImVec4
    {
        switch (style)
        {
            case render::HudEventStyle::Success:
                return ImVec4(0.45F, 0.90F, 0.55F, 1.0F);
            case render::HudEventStyle::Warning:
                return ImVec4(1.0F, 0.78F, 0.28F, 1.0F);
            default:
                return ImVec4(0.80F, 0.84F, 0.90F, 1.0F);
        }
    }

    auto inspector_source_bucket_label(const render::InspectorDiagnostics& inspector) -> const char*
    {
        if (!inspector.has_authored_source)
        {
            return "untracked";
        }
        if (inspector.authored_source_kind == "generated" || inspector.authored_source_kind == "generated_from_entity")
        {
            return "generated";
        }
        if (inspector.has_authored_placement_location)
        {
            return "instanced";
        }
        return "direct";
    }

    auto inspector_source_filter_label(const InspectorSourceFilter filter) -> const char*
    {
        switch (filter)
        {
            case InspectorSourceFilter::Direct:
                return "Direct";
            case InspectorSourceFilter::Instanced:
                return "Instanced";
            case InspectorSourceFilter::Generated:
                return "Generated";
            case InspectorSourceFilter::All:
            default:
                return "All";
        }
    }

    auto source_filter_from_index(const int filter_index) -> InspectorSourceFilter
    {
        switch (std::clamp(filter_index, 0, 3))
        {
            case 1:
                return InspectorSourceFilter::Direct;
            case 2:
                return InspectorSourceFilter::Instanced;
            case 3:
                return InspectorSourceFilter::Generated;
            case 0:
            default:
                return InspectorSourceFilter::All;
        }
    }

    auto
    inspector_matches_source_filter(const render::InspectorDiagnostics& inspector, const InspectorSourceFilter filter)
        -> bool
    {
        if (filter == InspectorSourceFilter::All)
        {
            return true;
        }

        const std::string_view bucket = inspector_source_bucket_label(inspector);
        switch (filter)
        {
            case InspectorSourceFilter::Direct:
                return bucket == "direct";
            case InspectorSourceFilter::Instanced:
                return bucket == "instanced";
            case InspectorSourceFilter::Generated:
                return bucket == "generated";
            case InspectorSourceFilter::All:
            default:
                return true;
        }
    }

    auto warning_source_bucket_label(const render::AuthoringWarningDiagnostics& warning) -> const char*
    {
        return warning.source_bucket.empty() ? "direct" : warning.source_bucket.c_str();
    }

    auto source_bucket_matches_filter(const std::string_view bucket, const InspectorSourceFilter filter) -> bool
    {
        if (filter == InspectorSourceFilter::All)
        {
            return true;
        }

        switch (filter)
        {
            case InspectorSourceFilter::Direct:
                return bucket == "direct";
            case InspectorSourceFilter::Instanced:
                return bucket == "instanced";
            case InspectorSourceFilter::Generated:
                return bucket == "generated";
            case InspectorSourceFilter::All:
            default:
                return true;
        }
    }

    auto warning_matches_source_filter(
        const render::AuthoringWarningDiagnostics& warning,
        const InspectorSourceFilter filter
    ) -> bool
    {
        return source_bucket_matches_filter(warning_source_bucket_label(warning), filter);
    }

    void draw_text_value(const char* label, const std::string& value)
    {
        ImGui::Text("%s: %s", label, value_or_none(value));
    }

    void draw_text_wrapped_value(const char* label, const std::string& value)
    {
        ImGui::TextWrapped("%s: %s", label, value_or_none(value));
    }

    void draw_optional_text_value(const char* label, const std::string& value)
    {
        if (!value.empty())
        {
            ImGui::Text("%s: %s", label, value.c_str());
        }
    }

    void draw_optional_text_wrapped_value(const char* label, const std::string& value)
    {
        if (!value.empty())
        {
            ImGui::TextWrapped("%s: %s", label, value.c_str());
        }
    }

    void
    draw_optional_source_location(const char* label, const std::string& file_path, const int line, const int column)
    {
        if (file_path.empty() || line <= 0 || column <= 0)
        {
            return;
        }

        ImGui::TextWrapped("%s: %s", label, file_path.c_str());
        ImGui::Text("%s loc: %d:%d", label, line, column);
    }

    void copy_text_to_clipboard(const std::string& text, const char* context_label)
    {
        if (text.empty())
        {
            RelayCore::log::info("{} copy ignored: no text available", context_label);
            return;
        }

        if (!SDL_SetClipboardText(text.c_str()))
        {
            RelayCore::log::warn("{} copy failed: {}", context_label, SDL_GetError());
            return;
        }

        RelayCore::log::info("{} copied: {}", context_label, text);
    }

    void draw_source_open_button(const char* button_label, const SourceLocation& location, const char* context_label)
    {
        if (!location.valid())
        {
            return;
        }

        if (!ImGui::Button(button_label))
        {
            return;
        }

        const SourceOpenResult result = open_source_location(location);
        const std::string source_ref = build_source_reference(location);
        if (result.launched)
        {
            RelayCore::log::info(
                "{} opened via {}: {}",
                context_label,
                result.strategy.empty() ? std::string{"editor launcher"} : result.strategy,
                source_ref
            );
            return;
        }

        RelayCore::log::warn(
            "{} open failed for {}: {}",
            context_label,
            source_ref.empty() ? std::string{"<unknown source>"} : source_ref,
            result.error.empty() ? std::string{"unknown error"} : result.error
        );
    }

    auto begin_section(const char* label, const ImVec4& accent, const bool default_open) -> bool
    {
        const ImVec4 header{accent.x * 0.45F, accent.y * 0.45F, accent.z * 0.45F, 0.78F};
        const ImVec4 hovered{accent.x * 0.62F, accent.y * 0.62F, accent.z * 0.62F, 0.88F};
        const ImVec4 active{accent.x * 0.78F, accent.y * 0.78F, accent.z * 0.78F, 0.96F};

        ImGui::PushStyleColor(ImGuiCol_Header, header);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, hovered);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, active);
        const bool open = ImGui::CollapsingHeader(
            label,
            (default_open ? ImGuiTreeNodeFlags_DefaultOpen : 0) | ImGuiTreeNodeFlags_SpanAvailWidth
        );
        ImGui::PopStyleColor(3);
        return open;
    }

    auto begin_metric_table(const char* id) -> bool
    {
        return ImGui::BeginTable(
            id,
            2,
            ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_PadOuterX
                | ImGuiTableFlags_RowBg
        );
    }

    void begin_metric_row(const char* label)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("%s", label);
        ImGui::TableSetColumnIndex(1);
    }

    void configure_imgui_font(ImGuiIO& io)
    {
        io.Fonts->Clear();

        const ImWchar* glyph_ranges = io.Fonts->GetGlyphRangesCyrillic();
        ImFontConfig font_config{};
        font_config.OversampleH = 2;
        font_config.OversampleV = 2;
        font_config.PixelSnapH = false;

        const std::filesystem::path asset_font = core::asset_path("fonts/RobotoMono-Regular.ttf");
        const std::filesystem::path segoe_ui_font = "C:/Windows/Fonts/segoeui.ttf";
        const std::filesystem::path arial_font = "C:/Windows/Fonts/arial.ttf";

        for (const std::filesystem::path& font_path : {asset_font, segoe_ui_font, arial_font})
        {
            if (!std::filesystem::exists(font_path))
            {
                continue;
            }

            if (io.Fonts->AddFontFromFileTTF(font_path.string().c_str(), 19.0F, &font_config, glyph_ranges) != nullptr)
            {
                log::info("ImGui font loaded from {}", font_path.string());
                return;
            }
        }

        io.Fonts->AddFontDefault();
        log::warn("ImGui font fallback to default font; Cyrillic glyphs may be unavailable");
    }

    void draw_shadow_controls_contents(render::RendererConfig& renderer_config)
    {
        ImGui::Checkbox("Enable shadows", &renderer_config.shadow_enabled);
        if (ImGui::TreeNodeEx("Lighting", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::SliderFloat("Indirect bounce", &renderer_config.indirect_bounce_strength, 0.0F, 0.30F, "%.3f");
            ImGui::SliderFloat("Shadow strength", &renderer_config.shadow_strength, 0.0F, 1.0F, "%.3f");
            ImGui::SliderFloat("Shadow contact", &renderer_config.shadow_contact_strength, 0.0F, 0.30F, "%.3f");
            ImGui::SliderFloat("Shadow light intensity", &renderer_config.shadow_light_intensity, 0.0F, 2.0F, "%.2f");
            ImGui::ColorEdit3("Shadow light color", &renderer_config.shadow_light_color.x);
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("Near Cascade", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::SliderFloat("Near shadow bias", &renderer_config.shadow_bias_near, 0.0001F, 0.0100F, "%.4f");
            ImGui::SliderFloat("Shadow slope bias", &renderer_config.shadow_slope_bias_scale, 0.0F, 0.0200F, "%.4f");
            ImGui::SliderFloat("Near normal offset", &renderer_config.shadow_normal_offset_near, 0.0F, 0.080F, "%.3f");
            ImGui::SliderFloat("Shadow near PCF", &renderer_config.shadow_pcf_radius_near, 0.5F, 3.0F, "%.2f");
            ImGui::SliderFloat(
                "Shadow split distance",
                &renderer_config.shadow_cascade_split_distance,
                2.0F,
                24.0F,
                "%.2f"
            );
            ImGui::SliderFloat("Shadow blend range", &renderer_config.shadow_cascade_blend_range, 0.0F, 6.0F, "%.2f");
            ImGui::Checkbox("Debug cascade tint", &renderer_config.shadow_debug_cascades);
            ImGui::SliderFloat("Shadow focus distance", &renderer_config.shadow_focus_distance, 0.0F, 24.0F, "%.2f");
            ImGui::SliderFloat("Shadow radius", &renderer_config.shadow_projection_radius, 2.0F, 28.0F, "%.2f");
            ImGui::SliderFloat("Shadow depth", &renderer_config.shadow_projection_depth, 4.0F, 48.0F, "%.2f");
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("Far Cascade"))
        {
            ImGui::SliderFloat("Shadow far PCF", &renderer_config.shadow_pcf_radius_far, 0.5F, 4.0F, "%.2f");
            ImGui::SliderFloat("Far shadow bias", &renderer_config.shadow_bias_far, 0.0001F, 0.0160F, "%.4f");
            ImGui::SliderFloat("Far normal offset", &renderer_config.shadow_normal_offset_far, 0.0F, 0.120F, "%.3f");
            ImGui::SliderFloat("Far focus distance", &renderer_config.shadow_far_focus_distance, 6.0F, 40.0F, "%.2f");
            ImGui::SliderFloat("Far radius", &renderer_config.shadow_far_projection_radius, 6.0F, 48.0F, "%.2f");
            ImGui::SliderFloat("Far depth", &renderer_config.shadow_far_projection_depth, 8.0F, 80.0F, "%.2f");
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("Light Direction"))
        {
            ImGui::SliderFloat3(
                "Shadow light direction",
                &renderer_config.shadow_light_direction.x,
                -1.0F,
                1.0F,
                "%.2f"
            );
            ImGui::TreePop();
        }
    }

    void draw_shadow_preview_contents(const render::WorldDiagnostics& world)
    {
        constexpr float preview_size = 192.0F;
        const ImTextureID near_texture_id =
            static_cast<ImTextureID>(static_cast<std::intptr_t>(world.shadow_preview_texture_id));
        ImGui::Text("Near");
        ImGui::Image(near_texture_id, ImVec2(preview_size, preview_size), ImVec2(0.0F, 1.0F), ImVec2(1.0F, 0.0F));

        if (world.shadow_far_preview_texture_id != 0)
        {
            const ImTextureID far_texture_id =
                static_cast<ImTextureID>(static_cast<std::intptr_t>(world.shadow_far_preview_texture_id));
            ImGui::SameLine();
            ImGui::BeginGroup();
            ImGui::Text("Far");
            ImGui::Image(far_texture_id, ImVec2(preview_size, preview_size), ImVec2(0.0F, 1.0F), ImVec2(1.0F, 0.0F));
            ImGui::EndGroup();
        }
    }
} // namespace RelayCore::debug

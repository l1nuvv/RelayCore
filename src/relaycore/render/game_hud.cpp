#include "relaycore/render/game_hud.h"

#include "relaycore/core/profiling.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <imgui.h>
#include <string>

namespace RelayCore::render
{
    namespace
    {
        constexpr float kObjectiveMargin = 28.0F;
        constexpr float kInventoryMargin = 28.0F;
        constexpr float kInventoryWrapWidth = 260.0F;
        constexpr float kInventoryRounding = 8.0F;
        constexpr float kObjectiveWrapWidth = 320.0F;
        constexpr float kObjectiveRounding = 8.0F;
        constexpr float kPromptBottomMargin = 64.0F;
        constexpr float kPromptRounding = 8.0F;
        constexpr float kPromptWrapWidth = 380.0F;
        constexpr float kEventTopMargin = 92.0F;
        constexpr float kEventRounding = 10.0F;
        constexpr float kStatusMargin = 28.0F;
        constexpr float kStatusBottomMargin = 40.0F;
        constexpr float kStatusRounding = 8.0F;

        [[nodiscard]] auto
        calc_wrapped_text_size(ImFont* font, const float font_size, const float wrap_width, const std::string& text)
            -> ImVec2
        {
            if (text.empty())
            {
                return {0.0F, 0.0F};
            }

            return font->CalcTextSizeA(font_size, FLT_MAX, wrap_width, text.c_str(), nullptr, nullptr);
        }

        void draw_card_background(
            ImDrawList* draw_list,
            const ImVec2 min,
            const ImVec2 max,
            const ImU32 fill_color,
            const ImU32 border_color,
            const float rounding
        )
        {
            draw_list->AddRectFilled(min, max, fill_color, rounding);
            draw_list->AddRect(min, max, border_color, rounding, 0, 1.0F);
        }
    } // namespace

    auto GameHud::has_player_status(const PlayerDiagnostics& player) noexcept -> bool
    {
        return player.max_health > 0.0F;
    }

    auto GameHud::has_interaction_prompt(const InteractionDiagnostics& interaction) noexcept -> bool
    {
        return interaction.has_focused_usable && !interaction.focused_label.empty();
    }

    auto GameHud::has_objective_prompt(const InteractionDiagnostics& interaction) noexcept -> bool
    {
        return interaction.has_objective
               && (!interaction.objective_title.empty() || !interaction.objective_detail.empty());
    }

    auto GameHud::has_inventory_prompt(const InteractionDiagnostics& interaction) noexcept -> bool
    {
        return interaction.has_inventory && !interaction.inventory_summary.empty();
    }

    auto GameHud::has_transient_event(const InteractionDiagnostics& interaction) noexcept -> bool
    {
        return interaction.has_transient_event && !interaction.transient_event_text.empty()
               && interaction.transient_event_strength > 0.001F;
    }

    auto
    GameHud::should_render(const PlayerDiagnostics& player, const InteractionDiagnostics& interaction) const noexcept
        -> bool
    {
        return has_player_status(player) || has_interaction_prompt(interaction) || has_objective_prompt(interaction)
               || has_inventory_prompt(interaction) || has_transient_event(interaction);
    }

    void GameHud::draw(const PlayerDiagnostics& player, const InteractionDiagnostics& interaction) const
    {
        RELAYCORE_PROFILE_ZONE_N("GameHud::draw");
        if (has_player_status(player))
        {
            draw_player_status_card(player);
        }

        if (has_transient_event(interaction))
        {
            draw_transient_event_card(interaction);
        }

        if (has_inventory_prompt(interaction))
        {
            draw_inventory_card(interaction);
        }

        if (has_objective_prompt(interaction))
        {
            draw_objective_card(interaction);
        }

        if (has_interaction_prompt(interaction))
        {
            draw_interaction_prompt_card(interaction);
        }
    }

    void GameHud::draw_player_status_card(const PlayerDiagnostics& player)
    {
        ImGuiViewport* viewport = const_cast<ImGuiViewport*>(ImGui::GetMainViewport());
        ImDrawList* draw_list = ImGui::GetForegroundDrawList();
        ImFont* font = ImGui::GetFont();
        const float font_size = ImGui::GetFontSize();
        const ImVec2 padding{16.0F, 12.0F};
        const float health_ratio =
            player.max_health > 0.001F ? std::clamp(player.health / player.max_health, 0.0F, 1.0F) : 0.0F;

        const std::string header = player.alive ? "БОЕВОЙ СТАТУС" : "КРИТИЧЕСКОЕ СОСТОЯНИЕ";
        const std::string health_text = "Здоровье: " + std::to_string(static_cast<int>(std::lround(player.health)))
                                        + "/" + std::to_string(static_cast<int>(std::lround(player.max_health)));
        const std::string attack_label = player.primary_attack_label.empty() ? "Mouse1" : player.primary_attack_label;
        const std::string weapon_text =
            player.alive ? (player.weapon_cooldown_remaining_seconds <= 0.001F
                                ? "[" + attack_label + "] Сайдарм готов"
                                : "[" + attack_label + "] Перезарядка "
                                      + std::to_string(player.weapon_cooldown_remaining_seconds) + "с")
                         : "Возврат через " + std::to_string(player.respawn_remaining_seconds) + "с";
        const std::string threat_text =
            player.total_turrets > 0
                ? ("Турели: " + std::to_string(player.destroyed_turrets) + "/" + std::to_string(player.total_turrets))
                : ("Выстрелы: " + std::to_string(player.shots_hit) + "/" + std::to_string(player.shots_fired));
        const std::string damage_text =
            player.active_hazard_contacts > 0
                ? std::string{"Опасная поверхность под ногами"}
                : (player.last_damage_source.empty() ? std::string{"Контакт с угрозой отсутствует"}
                                                     : "Последний урон: " + player.last_damage_source);

        const ImVec2 header_size = ImGui::CalcTextSize(header.c_str());
        const ImVec2 health_size = calc_wrapped_text_size(font, font_size, 320.0F, health_text);
        const ImVec2 weapon_size = calc_wrapped_text_size(font, font_size, 320.0F, weapon_text);
        const ImVec2 threat_size = calc_wrapped_text_size(font, font_size, 320.0F, threat_text);
        const ImVec2 damage_size = calc_wrapped_text_size(font, font_size, 320.0F, damage_text);
        const float card_width =
            std::max({header_size.x, health_size.x, weapon_size.x, threat_size.x, damage_size.x}) + (padding.x * 2.0F);
        const float card_height = padding.y + header_size.y + 10.0F + health_size.y + 8.0F + 10.0F + weapon_size.y
                                  + 4.0F + threat_size.y + 4.0F + damage_size.y + padding.y;
        const ImVec2 min{
            viewport->WorkPos.x + kStatusMargin,
            viewport->WorkPos.y + viewport->WorkSize.y - kStatusBottomMargin - card_height,
        };
        const ImVec2 max{min.x + card_width, min.y + card_height};

        ImVec4 fill{0.07F, 0.09F, 0.12F, 0.88F};
        ImVec4 border{0.34F, 0.72F, 0.86F, 0.54F};
        ImVec4 header_color{0.84F, 0.94F, 1.0F, 1.0F};
        if (!player.alive)
        {
            fill = ImVec4(0.16F, 0.07F, 0.08F, 0.90F);
            border = ImVec4(0.94F, 0.40F, 0.34F, 0.60F);
            header_color = ImVec4(1.0F, 0.86F, 0.84F, 1.0F);
        } else if (health_ratio < 0.35F)
        {
            fill = ImVec4(0.15F, 0.08F, 0.08F, 0.88F);
            border = ImVec4(0.96F, 0.50F, 0.40F, 0.58F);
            header_color = ImVec4(1.0F, 0.88F, 0.84F, 1.0F);
        } else if (player.active_hazard_contacts > 0)
        {
            fill = ImVec4(0.15F, 0.10F, 0.05F, 0.88F);
            border = ImVec4(0.94F, 0.70F, 0.30F, 0.56F);
            header_color = ImVec4(1.0F, 0.94F, 0.84F, 1.0F);
        }

        draw_card_background(
            draw_list,
            min,
            max,
            ImGui::ColorConvertFloat4ToU32(fill),
            ImGui::ColorConvertFloat4ToU32(border),
            kStatusRounding
        );

        ImVec2 cursor{min.x + padding.x, min.y + padding.y};
        draw_list->AddText(cursor, ImGui::ColorConvertFloat4ToU32(header_color), header.c_str());
        cursor.y += header_size.y + 10.0F;

        draw_list->AddText(
            font,
            font_size,
            cursor,
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.96F, 0.98F, 1.0F, 1.0F)),
            health_text.c_str(),
            nullptr,
            320.0F
        );
        cursor.y += health_size.y + 8.0F;

        const ImVec2 bar_min{cursor.x, cursor.y};
        const ImVec2 bar_max{max.x - padding.x, cursor.y + 10.0F};
        draw_list
            ->AddRectFilled(bar_min, bar_max, ImGui::ColorConvertFloat4ToU32(ImVec4(0.08F, 0.10F, 0.12F, 0.94F)), 4.0F);
        draw_list->AddRectFilled(
            bar_min,
            ImVec2(bar_min.x + ((bar_max.x - bar_min.x) * health_ratio), bar_max.y),
            ImGui::ColorConvertFloat4ToU32(
                health_ratio < 0.35F ? ImVec4(0.96F, 0.34F, 0.30F, 0.94F) : ImVec4(0.36F, 0.86F, 0.58F, 0.94F)
            ),
            4.0F
        );
        cursor.y += 14.0F;

        draw_list->AddText(
            font,
            font_size,
            cursor,
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.88F, 0.92F, 0.96F, 1.0F)),
            weapon_text.c_str(),
            nullptr,
            320.0F
        );
        cursor.y += weapon_size.y + 4.0F;
        draw_list->AddText(
            font,
            font_size,
            cursor,
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.74F, 0.84F, 0.92F, 0.96F)),
            threat_text.c_str(),
            nullptr,
            320.0F
        );
        cursor.y += threat_size.y + 4.0F;
        draw_list->AddText(
            font,
            font_size,
            cursor,
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.82F, 0.86F, 0.90F, 0.94F)),
            damage_text.c_str(),
            nullptr,
            320.0F
        );
    }

    void GameHud::draw_inventory_card(const InteractionDiagnostics& interaction)
    {
        ImGuiViewport* viewport = const_cast<ImGuiViewport*>(ImGui::GetMainViewport());
        ImDrawList* draw_list = ImGui::GetForegroundDrawList();
        ImFont* font = ImGui::GetFont();
        const float font_size = ImGui::GetFontSize();
        const ImVec2 padding{16.0F, 12.0F};
        const char* header = "ИНВЕНТАРЬ";
        const ImVec2 header_size = ImGui::CalcTextSize(header);
        const ImVec2 body_size =
            calc_wrapped_text_size(font, font_size, kInventoryWrapWidth, interaction.inventory_summary);
        const float card_width = std::max(header_size.x, body_size.x) + (padding.x * 2.0F);
        const float card_height = padding.y + header_size.y + 10.0F + body_size.y + padding.y;
        const ImVec2 min{viewport->WorkPos.x + kInventoryMargin, viewport->WorkPos.y + kInventoryMargin};
        const ImVec2 max{min.x + card_width, min.y + card_height};
        const ImU32 background_color = ImGui::ColorConvertFloat4ToU32(ImVec4(0.06F, 0.11F, 0.10F, 0.88F));
        const ImU32 border_color = ImGui::ColorConvertFloat4ToU32(ImVec4(0.34F, 0.82F, 0.74F, 0.52F));
        const ImU32 header_color = ImGui::ColorConvertFloat4ToU32(ImVec4(0.82F, 1.0F, 0.96F, 1.0F));
        const ImU32 body_color = ImGui::ColorConvertFloat4ToU32(ImVec4(0.76F, 0.90F, 0.88F, 1.0F));

        draw_card_background(draw_list, min, max, background_color, border_color, kInventoryRounding);
        draw_list->AddText(ImVec2(min.x + padding.x, min.y + padding.y), header_color, header);
        draw_list->AddText(
            font,
            font_size,
            ImVec2(min.x + padding.x, min.y + padding.y + header_size.y + 10.0F),
            body_color,
            interaction.inventory_summary.c_str(),
            nullptr,
            kInventoryWrapWidth
        );
    }

    void GameHud::draw_objective_card(const InteractionDiagnostics& interaction)
    {
        ImGuiViewport* viewport = const_cast<ImGuiViewport*>(ImGui::GetMainViewport());
        ImDrawList* draw_list = ImGui::GetForegroundDrawList();
        ImFont* font = ImGui::GetFont();
        const float font_size = ImGui::GetFontSize();
        const bool mission_complete = interaction.mission_complete;
        const std::string header = mission_complete ? "ЗАДАЧА ВЫПОЛНЕНА"
                                                    : "ЗАДАЧА " + std::to_string(interaction.objective_current) + "/"
                                                          + std::to_string(interaction.objective_total);
        const ImVec2 padding{18.0F, 14.0F};
        const ImVec2 header_size = ImGui::CalcTextSize(header.c_str());
        const ImVec2 title_size =
            calc_wrapped_text_size(font, font_size, kObjectiveWrapWidth, interaction.objective_title);
        const ImVec2 detail_size =
            calc_wrapped_text_size(font, font_size, kObjectiveWrapWidth, interaction.objective_detail);
        const bool has_title = !interaction.objective_title.empty();
        const bool has_detail = !interaction.objective_detail.empty();

        float card_width = std::max(header_size.x, std::max(title_size.x, detail_size.x)) + (padding.x * 2.0F);
        if (has_title || has_detail)
        {
            card_width = std::max(card_width, kObjectiveWrapWidth + (padding.x * 2.0F));
        }

        float card_height = padding.y + header_size.y + padding.y;
        if (has_title)
        {
            card_height += 10.0F + 1.0F + 10.0F + title_size.y;
        }
        if (has_detail)
        {
            card_height += (has_title ? 8.0F : 10.0F) + detail_size.y;
        }

        const ImVec2 min{
            viewport->WorkPos.x + viewport->WorkSize.x - card_width - kObjectiveMargin,
            viewport->WorkPos.y + kObjectiveMargin,
        };
        const ImVec2 max{min.x + card_width, min.y + card_height};

        const ImU32 background_color = ImGui::ColorConvertFloat4ToU32(
            mission_complete ? ImVec4(0.16F, 0.10F, 0.05F, 0.94F) : ImVec4(0.05F, 0.10F, 0.14F, 0.90F)
        );
        const ImU32 border_color = ImGui::ColorConvertFloat4ToU32(
            mission_complete ? ImVec4(0.78F, 0.60F, 0.30F, 0.52F) : ImVec4(0.30F, 0.56F, 0.74F, 0.52F)
        );
        const ImU32 header_color = ImGui::ColorConvertFloat4ToU32(
            mission_complete ? ImVec4(0.98F, 0.88F, 0.67F, 1.0F) : ImVec4(0.86F, 0.96F, 1.0F, 1.0F)
        );
        const ImU32 detail_color = ImGui::ColorConvertFloat4ToU32(ImVec4(0.80F, 0.86F, 0.92F, 1.0F));

        draw_card_background(draw_list, min, max, background_color, border_color, kObjectiveRounding);

        ImVec2 cursor{min.x + padding.x, min.y + padding.y};
        draw_list->AddText(cursor, header_color, header.c_str());
        cursor.y += header_size.y;

        if (has_title)
        {
            cursor.y += 10.0F;
            draw_list
                ->AddLine(ImVec2(min.x + padding.x, cursor.y), ImVec2(max.x - padding.x, cursor.y), border_color, 1.0F);
            cursor.y += 10.0F;
            draw_list->AddText(
                font,
                font_size,
                cursor,
                header_color,
                interaction.objective_title.c_str(),
                nullptr,
                kObjectiveWrapWidth
            );
            cursor.y += title_size.y;
        }

        if (has_detail)
        {
            cursor.y += has_title ? 8.0F : 10.0F;
            draw_list->AddText(
                font,
                font_size,
                cursor,
                detail_color,
                interaction.objective_detail.c_str(),
                nullptr,
                kObjectiveWrapWidth
            );
        }
    }

    void GameHud::draw_interaction_prompt_card(const InteractionDiagnostics& interaction)
    {
        ImGuiViewport* viewport = const_cast<ImGuiViewport*>(ImGui::GetMainViewport());
        ImDrawList* draw_list = ImGui::GetForegroundDrawList();
        ImFont* font = ImGui::GetFont();
        const float font_size = ImGui::GetFontSize();
        const std::string binding_label = interaction.use_binding_label.empty() ? "E" : interaction.use_binding_label;
        const std::string prompt_label = "[" + binding_label + "] " + interaction.focused_label;
        const bool has_detail = !interaction.focused_detail.empty();
        const ImVec2 padding{16.0F, 10.0F};
        const ImVec2 prompt_size = calc_wrapped_text_size(font, font_size, kPromptWrapWidth, prompt_label);
        const ImVec2 detail_size =
            has_detail ? calc_wrapped_text_size(font, font_size, kPromptWrapWidth, interaction.focused_detail)
                       : ImVec2{0.0F, 0.0F};
        const float text_width = std::max(prompt_size.x, detail_size.x);
        float card_height = padding.y + prompt_size.y + padding.y;
        if (has_detail)
        {
            card_height += 4.0F + detail_size.y;
        }
        const ImVec2 card_size{text_width + (padding.x * 2.0F), card_height};
        const ImVec2 min{
            viewport->WorkPos.x + (viewport->WorkSize.x - card_size.x) * 0.5F,
            viewport->WorkPos.y + viewport->WorkSize.y - kPromptBottomMargin - card_size.y,
        };
        const ImVec2 max{min.x + card_size.x, min.y + card_size.y};

        draw_card_background(
            draw_list,
            min,
            max,
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.07F, 0.08F, 0.09F, 0.88F)),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.82F, 0.74F, 0.34F, 0.50F)),
            kPromptRounding
        );
        draw_list->AddText(
            font,
            font_size,
            ImVec2(min.x + padding.x, min.y + padding.y),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.96F, 0.93F, 0.72F, 1.0F)),
            prompt_label.c_str(),
            nullptr,
            kPromptWrapWidth
        );

        if (has_detail)
        {
            draw_list->AddText(
                font,
                font_size,
                ImVec2(min.x + padding.x, min.y + padding.y + prompt_size.y + 4.0F),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.78F, 0.84F, 0.92F, 0.96F)),
                interaction.focused_detail.c_str(),
                nullptr,
                kPromptWrapWidth
            );
        }
    }

    void GameHud::draw_transient_event_card(const InteractionDiagnostics& interaction)
    {
        ImGuiViewport* viewport = const_cast<ImGuiViewport*>(ImGui::GetMainViewport());
        ImDrawList* draw_list = ImGui::GetForegroundDrawList();
        ImFont* font = ImGui::GetFont();
        const float font_size = ImGui::GetFontSize();
        const float strength = std::clamp(interaction.transient_event_strength, 0.0F, 1.0F);
        const ImVec2 padding{18.0F, 12.0F};
        const ImVec2 text_size = calc_wrapped_text_size(font, font_size, 420.0F, interaction.transient_event_text);
        const ImVec2 card_size{text_size.x + (padding.x * 2.0F), text_size.y + (padding.y * 2.0F)};
        const ImVec2 min{
            viewport->WorkPos.x + (viewport->WorkSize.x - card_size.x) * 0.5F,
            viewport->WorkPos.y + kEventTopMargin,
        };
        const ImVec2 max{min.x + card_size.x, min.y + card_size.y};

        ImVec4 fill = ImVec4(0.10F, 0.12F, 0.15F, 0.86F);
        ImVec4 border = ImVec4(0.72F, 0.78F, 0.86F, 0.55F);
        ImVec4 text = ImVec4(0.92F, 0.96F, 1.0F, 1.0F);
        ImVec4 flash = ImVec4(0.22F, 0.28F, 0.38F, 0.12F);

        switch (interaction.transient_event_style)
        {
            case HudEventStyle::Success:
                fill = ImVec4(0.08F, 0.14F, 0.10F, 0.88F);
                border = ImVec4(0.42F, 0.86F, 0.66F, 0.60F);
                text = ImVec4(0.90F, 1.0F, 0.94F, 1.0F);
                flash = ImVec4(0.18F, 0.40F, 0.28F, 0.16F);
                break;
            case HudEventStyle::Warning:
                fill = ImVec4(0.16F, 0.08F, 0.08F, 0.90F);
                border = ImVec4(0.96F, 0.42F, 0.36F, 0.62F);
                text = ImVec4(1.0F, 0.90F, 0.88F, 1.0F);
                flash = ImVec4(0.46F, 0.10F, 0.08F, 0.18F);
                break;
            case HudEventStyle::Neutral:
            default:
                break;
        }

        const ImU32 flash_color = ImGui::ColorConvertFloat4ToU32(ImVec4(flash.x, flash.y, flash.z, flash.w * strength));
        draw_list->AddRectFilled(
            viewport->WorkPos,
            ImVec2(viewport->WorkPos.x + viewport->WorkSize.x, viewport->WorkPos.y + viewport->WorkSize.y),
            flash_color
        );
        draw_card_background(
            draw_list,
            min,
            max,
            ImGui::ColorConvertFloat4ToU32(ImVec4(fill.x, fill.y, fill.z, fill.w * (0.72F + (strength * 0.28F)))),
            ImGui::ColorConvertFloat4ToU32(
                ImVec4(border.x, border.y, border.z, border.w * (0.68F + (strength * 0.32F)))
            ),
            kEventRounding
        );
        draw_list->AddText(
            font,
            font_size,
            ImVec2(min.x + padding.x, min.y + padding.y),
            ImGui::ColorConvertFloat4ToU32(text),
            interaction.transient_event_text.c_str(),
            nullptr,
            420.0F
        );
    }
} // namespace RelayCore::render

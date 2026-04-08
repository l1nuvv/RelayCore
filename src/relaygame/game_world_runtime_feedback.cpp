#include "relaycore/app/engine_services.h"
#include "relaycore/core/paths.h"
#include "relaygame/game_world_runtime.h"

#include <algorithm>

namespace RelayGame
{
    void GameWorldRuntime::configure_audio_cues(const RelayCore::EngineServices& services)
    {
        const auto& gameplay_audio = services.config().gameplay.audio;
        audio_cues_.ambient_loop = gameplay_audio.ambient_loop.empty()
                                       ? std::filesystem::path{}
                                       : RelayCore::core::asset_path(gameplay_audio.ambient_loop);
        audio_cues_.panel_press = gameplay_audio.panel_press.empty()
                                      ? std::filesystem::path{}
                                      : RelayCore::core::asset_path(gameplay_audio.panel_press);
        audio_cues_.door_open = gameplay_audio.door_open.empty()
                                    ? std::filesystem::path{}
                                    : RelayCore::core::asset_path(gameplay_audio.door_open);
        audio_cues_.door_close = gameplay_audio.door_close.empty()
                                     ? std::filesystem::path{}
                                     : RelayCore::core::asset_path(gameplay_audio.door_close);
        audio_cues_.access_denied = gameplay_audio.access_denied.empty()
                                        ? audio_cues_.door_close
                                        : RelayCore::core::asset_path(gameplay_audio.access_denied);
    }

    void GameWorldRuntime::process_use_input(const bool use_pressed_this_step, RelayCore::EngineServices& services)
    {
        interaction_state_.use_pressed = interaction_state_.use_pressed || use_pressed_this_step;
        if (!use_pressed_this_step || interaction_state_.focused_usable_index < 0)
        {
            return;
        }

        RelayCore::gameplay::RuntimeUsable& usable =
            usables_[static_cast<std::size_t>(interaction_state_.focused_usable_index)];
        const bool has_required_item = usable.required_item.empty() || has_inventory_item(usable.required_item);

        if (has_required_item && usable.action_kind == RelayCore::gameplay::UsableActionKind::PickupItem)
        {
            collect_inventory_item(usable);
            usable.consumed = true;
            usable.enabled = false;
            play_gameplay_audio(services, audio_cues_.panel_press);
            const std::string item_label = usable.item_name.empty() ? usable.item_id : usable.item_name;
            push_transient_hud_event(
                item_label.empty() ? std::string{"Предмет получен"} : "Получено: " + item_label,
                RelayCore::render::HudEventStyle::Success,
                1.8F
            );
            return;
        }

        bool activated_target = false;
        if (has_required_item && usable.action_kind == RelayCore::gameplay::UsableActionKind::SignalTarget)
        {
            activated_target = !usable.target.empty();
        } else if (has_required_item && usable.action_kind == RelayCore::gameplay::UsableActionKind::ActivateTarget)
        {
            activated_target = RelayCore::gameplay::activate_target(usable.target, doors_);
        }

        if (activated_target)
        {
            interaction_state_.last_target = usable.target;
            record_activated_target(usable.target);
            play_gameplay_audio(services, audio_cues_.panel_press);
            push_usable_feedback(usable, RelayCore::gameplay::UsableFeedbackStyle::Success, 0.34F);
            if (!usable.success_message.empty())
            {
                push_transient_hud_event(usable.success_message, RelayCore::render::HudEventStyle::Success, 1.35F);
            }
            return;
        }

        if (!has_required_item)
        {
            play_gameplay_audio(services, audio_cues_.access_denied);
            push_usable_feedback(usable, RelayCore::gameplay::UsableFeedbackStyle::Warning, 0.44F);
            const std::string requirement =
                usable.required_item_name.empty() ? usable.required_item : usable.required_item_name;
            push_transient_hud_event(
                !usable.denied_message.empty() ? usable.denied_message
                                               : (requirement.empty() ? std::string{"Доступ запрещен"}
                                                                      : "Доступ запрещен: требуется " + requirement),
                RelayCore::render::HudEventStyle::Warning,
                1.4F
            );
        }
    }

    void GameWorldRuntime::play_gameplay_audio(
        RelayCore::EngineServices& services,
        const std::filesystem::path& cue_path
    ) const
    {
        if (cue_path.empty())
        {
            return;
        }
        services.audio_system().play_one_shot(cue_path);
    }

    void GameWorldRuntime::play_door_state_change_audio(RelayCore::EngineServices& services)
    {
        const std::size_t state_count = std::min(previous_door_states_.size(), doors_.size());
        for (std::size_t i = 0; i < state_count; ++i)
        {
            const RelayCore::gameplay::DoorState previous_state = previous_door_states_[i];
            const RelayCore::gameplay::DoorState current_state = doors_[i].state;
            if (previous_state == current_state)
            {
                continue;
            }

            if (current_state == RelayCore::gameplay::DoorState::Opening)
            {
                play_gameplay_audio(services, audio_cues_.door_open);
            } else if (current_state == RelayCore::gameplay::DoorState::Closing)
            {
                play_gameplay_audio(services, audio_cues_.door_close);
            }
        }
    }

    void GameWorldRuntime::sync_door_audio_state()
    {
        previous_door_states_.resize(doors_.size(), RelayCore::gameplay::DoorState::Closed);
        for (std::size_t i = 0; i < doors_.size(); ++i)
        {
            previous_door_states_[i] = doors_[i].state;
        }
    }

    void GameWorldRuntime::tick_transient_hud_event(const float delta_seconds) noexcept
    {
        if (transient_hud_event_.remaining_seconds <= 0.0F)
        {
            transient_hud_event_.remaining_seconds = 0.0F;
            return;
        }

        transient_hud_event_.remaining_seconds =
            std::max(0.0F, transient_hud_event_.remaining_seconds - std::max(delta_seconds, 0.0F));
        if (transient_hud_event_.remaining_seconds <= 0.0F)
        {
            transient_hud_event_.text.clear();
            transient_hud_event_.style = RelayCore::render::HudEventStyle::Neutral;
            transient_hud_event_.total_seconds = 0.0F;
        }
    }

    void GameWorldRuntime::tick_usable_feedback(const float delta_seconds) noexcept
    {
        const float step = std::max(delta_seconds, 0.0F);
        for (RelayCore::gameplay::RuntimeUsable& usable : usables_)
        {
            if (usable.feedback_remaining_seconds <= 0.0F)
            {
                usable.feedback_remaining_seconds = 0.0F;
                usable.feedback_total_seconds = 0.0F;
                usable.feedback_style = RelayCore::gameplay::UsableFeedbackStyle::None;
                continue;
            }

            usable.feedback_remaining_seconds = std::max(0.0F, usable.feedback_remaining_seconds - step);
            if (usable.feedback_remaining_seconds <= 0.0F)
            {
                usable.feedback_total_seconds = 0.0F;
                usable.feedback_style = RelayCore::gameplay::UsableFeedbackStyle::None;
            }
        }
    }

    void GameWorldRuntime::push_usable_feedback(
        RelayCore::gameplay::RuntimeUsable& usable,
        const RelayCore::gameplay::UsableFeedbackStyle style,
        const float duration_seconds
    ) noexcept
    {
        usable.feedback_style = style;
        usable.feedback_remaining_seconds = std::max(duration_seconds, 0.0F);
        usable.feedback_total_seconds = usable.feedback_remaining_seconds;
    }

    void GameWorldRuntime::push_transient_hud_event(
        std::string text,
        const RelayCore::render::HudEventStyle style,
        const float duration_seconds
    )
    {
        transient_hud_event_.text = std::move(text);
        transient_hud_event_.style = style;
        transient_hud_event_.remaining_seconds = std::max(duration_seconds, 0.0F);
        transient_hud_event_.total_seconds = transient_hud_event_.remaining_seconds;
    }

    void GameWorldRuntime::push_mission_status_event(
        const RelayCore::gameplay::MissionStatus& mission,
        const RelayCore::render::HudEventStyle style,
        const float duration_seconds
    )
    {
        if (!mission.active && !mission.completed)
        {
            return;
        }

        std::string text = mission.title.empty()
                               ? (mission.completed ? std::string{"Задача выполнена"} : std::string{"Задача обновлена"})
                               : mission.title;
        if (!mission.detail.empty())
        {
            text += "\n";
            text += mission.detail;
        }

        push_transient_hud_event(std::move(text), style, duration_seconds);
    }

    void GameWorldRuntime::record_activated_target(const std::string_view target)
    {
        if (target.empty())
        {
            return;
        }

        const auto found = std::ranges::find_if(activated_target_ids_, [target](const std::string& activated_target) {
            return activated_target == target;
        });
        if (found == activated_target_ids_.end())
        {
            activated_target_ids_.emplace_back(target);
        }
    }

    auto GameWorldRuntime::has_inventory_item(const std::string_view item_id) const -> bool
    {
        return !item_id.empty()
               && std::ranges::find_if(inventory_item_ids_, [item_id](const std::string& inventory_item) {
                      return inventory_item == item_id;
                  }) != inventory_item_ids_.end();
    }

    void GameWorldRuntime::collect_inventory_item(const RelayCore::gameplay::RuntimeUsable& usable)
    {
        if (usable.item_id.empty() || has_inventory_item(usable.item_id))
        {
            return;
        }

        inventory_item_ids_.push_back(usable.item_id);
    }

    auto GameWorldRuntime::build_focused_usable_label(const RelayCore::gameplay::RuntimeUsable& usable) const
        -> std::string
    {
        if (usable.required_item.empty() || has_inventory_item(usable.required_item))
        {
            return usable.label;
        }

        const std::string requirement =
            usable.required_item_name.empty() ? usable.required_item : usable.required_item_name;
        return usable.label + " [Нужно: " + requirement + "]";
    }

    auto GameWorldRuntime::build_inventory_summary() const -> std::string
    {
        std::string summary{};
        for (const RelayCore::gameplay::RuntimeUsable& usable : usables_)
        {
            if (usable.action_kind != RelayCore::gameplay::UsableActionKind::PickupItem || !usable.consumed)
            {
                continue;
            }

            const std::string& label = usable.item_name.empty() ? usable.item_id : usable.item_name;
            if (label.empty())
            {
                continue;
            }

            if (!summary.empty())
            {
                summary += ", ";
            }
            summary += label;
        }

        return summary;
    }
} // namespace RelayGame

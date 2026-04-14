#include "relaycore/app/engine_services.h"
#include "relaycore/core/paths.h"
#include "relaycore/core/profiling.h"
#include "relaygame/game_world_runtime.h"

#include <algorithm>

namespace RelayGame
{
    namespace
    {
        constexpr float kAmbientFadeSeconds = 0.55F;

        void append_unique_string(std::vector<std::string>& values, std::string_view value)
        {
            if (value.empty())
            {
                return;
            }

            if (std::ranges::find(values, value) == values.end())
            {
                values.emplace_back(value);
            }
        }
    } // namespace

    void GameWorldRuntime::configure_audio_cues(const RelayCore::core::GameplayAudioConfig& gameplay_audio)
    {
        RELAYCORE_PROFILE_FUNCTION();
        audio_cues_.ambient_loop = gameplay_audio.ambient_loop.empty()
                                       ? std::filesystem::path{}
                                       : RelayCore::core::asset_path(gameplay_audio.ambient_loop);
        audio_cues_.panel_press = gameplay_audio.panel_press.empty()
                                      ? std::filesystem::path{}
                                      : RelayCore::core::asset_path(gameplay_audio.panel_press);
        audio_cues_.pickup = gameplay_audio.pickup.empty() ? audio_cues_.panel_press
                                                           : RelayCore::core::asset_path(gameplay_audio.pickup);
        audio_cues_.door_open = gameplay_audio.door_open.empty()
                                    ? std::filesystem::path{}
                                    : RelayCore::core::asset_path(gameplay_audio.door_open);
        audio_cues_.door_close = gameplay_audio.door_close.empty()
                                     ? std::filesystem::path{}
                                     : RelayCore::core::asset_path(gameplay_audio.door_close);
        audio_cues_.access_denied = gameplay_audio.access_denied.empty()
                                        ? audio_cues_.door_close
                                        : RelayCore::core::asset_path(gameplay_audio.access_denied);
        audio_cues_.mission_complete = gameplay_audio.mission_complete.empty()
                                           ? audio_cues_.door_open
                                           : RelayCore::core::asset_path(gameplay_audio.mission_complete);
        audio_cues_.weapon_fire = gameplay_audio.weapon_fire.empty()
                                      ? audio_cues_.panel_press
                                      : RelayCore::core::asset_path(gameplay_audio.weapon_fire);
        audio_cues_.turret_fire = gameplay_audio.turret_fire.empty()
                                      ? audio_cues_.access_denied
                                      : RelayCore::core::asset_path(gameplay_audio.turret_fire);
        audio_cues_.turret_destroyed = gameplay_audio.turret_destroyed.empty()
                                           ? audio_cues_.door_open
                                           : RelayCore::core::asset_path(gameplay_audio.turret_destroyed);
        audio_cues_.player_hurt = gameplay_audio.player_hurt.empty()
                                      ? audio_cues_.access_denied
                                      : RelayCore::core::asset_path(gameplay_audio.player_hurt);
        audio_cues_.player_respawn = gameplay_audio.player_respawn.empty()
                                         ? audio_cues_.door_close
                                         : RelayCore::core::asset_path(gameplay_audio.player_respawn);
    }

    void GameWorldRuntime::process_use_input(const bool use_pressed_this_step, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_FUNCTION();
        interaction_state_.use_pressed = interaction_state_.use_pressed || use_pressed_this_step;
        if (!use_pressed_this_step || interaction_state_.focused_usable_index < 0)
        {
            return;
        }

        RelayCore::gameplay::RuntimeUsable& usable =
            usables_[static_cast<std::size_t>(interaction_state_.focused_usable_index)];
        const RelayCore::gameplay::InteractionEvent event = RelayCore::gameplay::apply_usable_interaction(
            usable,
            doors_,
            usables_,
            inventory_item_ids_,
            activated_target_ids_,
            signaled_target_ids_
        );
        remember_interaction_event(event);
        const RelayCore::gameplay::UseInteractionOutcome outcome = event.kind;

        if (outcome == RelayCore::gameplay::UseInteractionOutcome::Locked)
        {
            play_gameplay_audio(services, audio_cues_.access_denied);
            push_usable_feedback(usable, RelayCore::gameplay::UsableFeedbackStyle::Warning, 0.44F);
            push_transient_hud_event(
                usable.locked_message.empty() ? std::string{"Заблокировано"} : usable.locked_message,
                RelayCore::render::HudEventStyle::Warning,
                1.4F
            );
            return;
        }

        if (outcome == RelayCore::gameplay::UseInteractionOutcome::PickedUpItem)
        {
            play_gameplay_audio(services, audio_cues_.pickup);
            push_transient_hud_event(
                !usable.success_message.empty() ? usable.success_message : ([&usable]() {
                    const std::string item_label = usable.item_name.empty() ? usable.item_id : usable.item_name;
                    return item_label.empty() ? std::string{"Предмет получен"} : "Получено: " + item_label;
                })(),
                RelayCore::render::HudEventStyle::Success,
                1.8F
            );
            return;
        }

        if (outcome == RelayCore::gameplay::UseInteractionOutcome::ActivatedTarget)
        {
            play_gameplay_audio(services, audio_cues_.panel_press);
            push_usable_feedback(usable, RelayCore::gameplay::UsableFeedbackStyle::Success, 0.34F);
            if (!usable.success_message.empty())
            {
                push_transient_hud_event(usable.success_message, RelayCore::render::HudEventStyle::Success, 1.35F);
            }
            return;
        }

        if (outcome == RelayCore::gameplay::UseInteractionOutcome::SignaledTarget)
        {
            play_gameplay_audio(services, audio_cues_.panel_press);
            push_usable_feedback(usable, RelayCore::gameplay::UsableFeedbackStyle::Success, 0.34F);
            if (!usable.success_message.empty())
            {
                push_transient_hud_event(usable.success_message, RelayCore::render::HudEventStyle::Success, 1.35F);
            }
            return;
        }

        if (outcome == RelayCore::gameplay::UseInteractionOutcome::MissingRequiredItem)
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

    void GameWorldRuntime::remember_interaction_event(const RelayCore::gameplay::InteractionEvent& event) noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (event.kind == RelayCore::gameplay::UseInteractionOutcome::None)
        {
            return;
        }

        interaction_state_.last_event = event;
    }

    void GameWorldRuntime::process_fire_input(const bool fire_pressed_this_step, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_FUNCTION();
        combat_state_.fire_pressed = combat_state_.fire_pressed || fire_pressed_this_step;
        if (!fire_pressed_this_step || !combat_state_.player.alive)
        {
            return;
        }

        const RelayCore::gameplay::CombatFireResult fire_result = RelayCore::gameplay::apply_player_fire(
            level_,
            doors_,
            player_controller_.eye_position_world(),
            player_controller_.view_forward(),
            combat_state_.player,
            combat_state_.turrets
        );
        if (!fire_result.fired)
        {
            return;
        }

        play_gameplay_audio(services, audio_cues_.weapon_fire);
        if (!fire_result.hit_turret)
        {
            return;
        }

        if (fire_result.killed_turret)
        {
            play_gameplay_audio(services, audio_cues_.turret_destroyed);
            apply_combat_target_event(fire_result.target_event);
            const RelayCore::gameplay::RuntimeTurret& turret =
                combat_state_.turrets[static_cast<std::size_t>(fire_result.turret_index)];
            const std::string label = turret.label.empty() ? turret.name : turret.label;
            push_transient_hud_event(
                label.empty() ? std::string{"Защитная турель уничтожена"} : label + " уничтожена",
                RelayCore::render::HudEventStyle::Success,
                1.4F
            );
        }
    }

    void GameWorldRuntime::apply_combat_target_event(const RelayCore::gameplay::CombatTargetEvent& event)
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (event.target.empty() || event.action == RelayCore::gameplay::CombatTargetActionKind::None)
        {
            return;
        }

        switch (event.action)
        {
            case RelayCore::gameplay::CombatTargetActionKind::ActivateTarget:
                (void) RelayCore::gameplay::activate_target(event.target, doors_);
                append_unique_string(activated_target_ids_, event.target);
                break;
            case RelayCore::gameplay::CombatTargetActionKind::SignalTarget:
                if (RelayCore::gameplay::signal_target(event.target, doors_, usables_))
                {
                    append_unique_string(signaled_target_ids_, event.target);
                }
                break;
            case RelayCore::gameplay::CombatTargetActionKind::None:
            default:
                break;
        }
    }

    void
    GameWorldRuntime::play_gameplay_audio(RelayCore::EngineServices& services, const std::filesystem::path& cue_path)
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (cue_path.empty())
        {
            return;
        }
        audio_state_.last_one_shot_path = cue_path;
        ++audio_state_.one_shot_count;
        services.audio_system().play_one_shot(cue_path);
    }

    void GameWorldRuntime::play_door_state_change_audio(RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_FUNCTION();
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
        RELAYCORE_PROFILE_FUNCTION();
        previous_door_states_.resize(doors_.size(), RelayCore::gameplay::DoorState::Closed);
        for (std::size_t i = 0; i < doors_.size(); ++i)
        {
            previous_door_states_[i] = doors_[i].state;
        }
    }

    void GameWorldRuntime::update_ambient_audio(RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_FUNCTION();
        const int next_zone_index = RelayGame::find_active_audio_zone(
            audio_zones_,
            player_controller_.player_bounds(),
            audio_state_.active_zone_index
        );

        std::filesystem::path desired_loop_path{};
        float desired_loop_gain = 0.0F;
        std::string desired_source_kind{};
        std::string desired_zone_name{};

        if (next_zone_index >= 0 && static_cast<std::size_t>(next_zone_index) < audio_zones_.size())
        {
            const RuntimeAudioZone& zone = audio_zones_[static_cast<std::size_t>(next_zone_index)];
            desired_loop_path = zone.loop_path;
            desired_loop_gain = zone.gain;
            desired_source_kind = "zone";
            desired_zone_name = zone.name;
        } else if (!audio_cues_.ambient_loop.empty())
        {
            desired_loop_path = audio_cues_.ambient_loop;
            desired_loop_gain = 1.0F;
            desired_source_kind = "global";
        }

        const bool ambient_source_changed = next_zone_index != audio_state_.active_zone_index
                                            || desired_source_kind != audio_state_.active_source_kind
                                            || desired_loop_path != audio_state_.active_loop_path;

        if (desired_loop_path.empty())
        {
            if (audio_state_.has_active_loop || !audio_state_.active_loop_path.empty())
            {
                services.audio_system().stop_loop(kAmbientFadeSeconds);
            }
        } else
        {
            services.audio_system().play_loop(desired_loop_path, desired_loop_gain, kAmbientFadeSeconds);
        }

        if (ambient_source_changed)
        {
            ++audio_state_.ambient_switch_count;
        }

        audio_state_.active_zone_index = next_zone_index;
        audio_state_.active_source_kind = std::move(desired_source_kind);
        audio_state_.active_zone_name = std::move(desired_zone_name);
    }

    void GameWorldRuntime::tick_transient_hud_event(const float delta_seconds) noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
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
        RELAYCORE_PROFILE_FUNCTION();
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
        RELAYCORE_PROFILE_FUNCTION();
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
        RELAYCORE_PROFILE_FUNCTION();
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
        RELAYCORE_PROFILE_FUNCTION();
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

    void GameWorldRuntime::respawn_player_at_spawn() noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        player_controller_.initialize(combat_state_.spawn_position, combat_state_.spawn_view_angles);
        RelayCore::gameplay::respawn_player_combat_state(combat_state_.player);
        refresh_runtime_usables_and_focus();
    }

    auto GameWorldRuntime::has_inventory_item(const std::string_view item_id) const -> bool
    {
        RELAYCORE_PROFILE_FUNCTION();
        return !item_id.empty()
               && std::ranges::find_if(inventory_item_ids_, [item_id](const std::string& inventory_item) {
                      return inventory_item == item_id;
                  }) != inventory_item_ids_.end();
    }

    auto GameWorldRuntime::build_focused_usable_label(const RelayCore::gameplay::RuntimeUsable& usable) const
        -> std::string
    {
        RELAYCORE_PROFILE_FUNCTION();
        return usable.label;
    }

    auto GameWorldRuntime::build_focused_usable_detail(const RelayCore::gameplay::RuntimeUsable& usable) const
        -> std::string
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (usable.locked)
        {
            return usable.locked_message.empty() ? std::string{"Заблокировано"} : usable.locked_message;
        }

        if (usable.required_item.empty() || has_inventory_item(usable.required_item))
        {
            return {};
        }

        const std::string requirement =
            usable.required_item_name.empty() ? usable.required_item : usable.required_item_name;
        return "Требуется: " + requirement;
    }

    auto GameWorldRuntime::interaction_query_distance() const noexcept -> float
    {
        RELAYCORE_PROFILE_FUNCTION();
        float max_use_radius = 2.25F;
        for (const RelayCore::gameplay::RuntimeUsable& usable : usables_)
        {
            if (!usable.enabled)
            {
                continue;
            }
            max_use_radius = std::max(max_use_radius, usable.use_radius);
        }

        return std::clamp(max_use_radius, 2.25F, 3.0F);
    }

    auto GameWorldRuntime::build_inventory_summary() const -> std::string
    {
        RELAYCORE_PROFILE_FUNCTION();
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

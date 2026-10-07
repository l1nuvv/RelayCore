#include "relaycore/core/profiling.h"
#include "relaygame/game_world_runtime.h"
#include "relaygame/world_inspector.h"

#include <algorithm>
#include <cmath>

namespace RelayGame
{
    namespace
    {
        [[nodiscard]] auto interaction_event_kind_label(const RelayCore::gameplay::UseInteractionOutcome kind)
            -> std::string
        {
            RELAYCORE_PROFILE_FUNCTION();
            switch (kind)
            {
                case RelayCore::gameplay::UseInteractionOutcome::Locked:
                    return "locked";
                case RelayCore::gameplay::UseInteractionOutcome::MissingRequiredItem:
                    return "missing_required_item";
                case RelayCore::gameplay::UseInteractionOutcome::PickedUpItem:
                    return "pickup_item";
                case RelayCore::gameplay::UseInteractionOutcome::ActivatedTarget:
                    return "activate_target";
                case RelayCore::gameplay::UseInteractionOutcome::SignaledTarget:
                    return "signal_target";
                case RelayCore::gameplay::UseInteractionOutcome::None:
                default:
                    return {};
            }
        }

        [[nodiscard]] auto
        interaction_event_source_label(const RelayCore::gameplay::InteractionEventSourceKind source_kind) -> std::string
        {
            RELAYCORE_PROFILE_FUNCTION();
            switch (source_kind)
            {
                case RelayCore::gameplay::InteractionEventSourceKind::Usable:
                    return "usable";
                case RelayCore::gameplay::InteractionEventSourceKind::Trigger:
                    return "trigger";
                case RelayCore::gameplay::InteractionEventSourceKind::None:
                default:
                    return {};
            }
        }

        [[nodiscard]] auto usable_action_kind_label(const RelayCore::gameplay::UsableActionKind action_kind)
            -> std::string
        {
            RELAYCORE_PROFILE_FUNCTION();
            switch (action_kind)
            {
                case RelayCore::gameplay::UsableActionKind::PickupItem:
                    return "pickup_item";
                case RelayCore::gameplay::UsableActionKind::SignalTarget:
                    return "signal_target";
                case RelayCore::gameplay::UsableActionKind::ActivateTarget:
                default:
                    return "activate_target";
            }
        }

        [[nodiscard]] auto signal_action_label(const RelayCore::gameplay::UsableSignalAction signal_action)
            -> std::string
        {
            RELAYCORE_PROFILE_FUNCTION();
            switch (signal_action)
            {
                case RelayCore::gameplay::UsableSignalAction::Unlock:
                    return "unlock";
                case RelayCore::gameplay::UsableSignalAction::Lock:
                    return "lock";
                case RelayCore::gameplay::UsableSignalAction::ToggleLock:
                    return "toggle_lock";
                case RelayCore::gameplay::UsableSignalAction::None:
                default:
                    return "none";
            }
        }

        [[nodiscard]] auto visual_kind_label(const RelayCore::gameplay::UsableVisualKind visual_kind) -> std::string
        {
            RELAYCORE_PROFILE_FUNCTION();
            switch (visual_kind)
            {
                case RelayCore::gameplay::UsableVisualKind::Panel:
                    return "panel";
                case RelayCore::gameplay::UsableVisualKind::Pickup:
                    return "pickup";
                case RelayCore::gameplay::UsableVisualKind::None:
                default:
                    return "none";
            }
        }

        [[nodiscard]] auto panel_mount_axis_label(const RelayCore::gameplay::PanelMountAxis axis) -> std::string
        {
            RELAYCORE_PROFILE_FUNCTION();
            switch (axis)
            {
                case RelayCore::gameplay::PanelMountAxis::PositiveX:
                    return "+x";
                case RelayCore::gameplay::PanelMountAxis::NegativeX:
                    return "-x";
                case RelayCore::gameplay::PanelMountAxis::PositiveZ:
                    return "+z";
                case RelayCore::gameplay::PanelMountAxis::NegativeZ:
                    return "-z";
                case RelayCore::gameplay::PanelMountAxis::Centered:
                default:
                    return "centered";
            }
        }

        [[nodiscard]] auto door_state_label(const RelayCore::gameplay::DoorState state) -> std::string
        {
            RELAYCORE_PROFILE_FUNCTION();
            switch (state)
            {
                case RelayCore::gameplay::DoorState::Opening:
                    return "opening";
                case RelayCore::gameplay::DoorState::Open:
                    return "open";
                case RelayCore::gameplay::DoorState::Closing:
                    return "closing";
                case RelayCore::gameplay::DoorState::Closed:
                default:
                    return "closed";
            }
        }
    } // namespace

    auto GameWorldRuntime::build_player_diagnostics() const -> RelayCore::render::PlayerDiagnostics
    {
        RELAYCORE_PROFILE_ZONE_N("GameWorldRuntime::player_diagnostics_build");
        RelayCore::render::PlayerDiagnostics player_diagnostics{};
        player_diagnostics.position = player_controller_.position();
        player_diagnostics.velocity = player_controller_.velocity();
        player_diagnostics.grounded = player_controller_.grounded();
        player_diagnostics.alive = combat_state_.player.alive;
        player_diagnostics.collision_enabled = (player_controller_.movement_mode() != RelayGame::MovementMode::Noclip);
        player_diagnostics.fire_pressed = combat_state_.fire_pressed;
        player_diagnostics.movement_mode = static_cast<std::uint32_t>(player_controller_.movement_mode());
        player_diagnostics.shots_fired = combat_state_.player.shots_fired;
        player_diagnostics.shots_hit = combat_state_.player.shots_hit;
        player_diagnostics.kills = combat_state_.player.kills;
        player_diagnostics.deaths = combat_state_.player.deaths;
        player_diagnostics.total_turrets = static_cast<std::uint32_t>(combat_state_.turrets.size());
        player_diagnostics.destroyed_turrets = static_cast<std::uint32_t>(std::count_if(
            combat_state_.turrets.begin(),
            combat_state_.turrets.end(),
            [](const RelayCore::gameplay::RuntimeTurret& turret) {
                return turret.destroyed;
            }
        ));
        player_diagnostics.active_hazard_contacts = combat_state_.active_hazard_contacts;
        player_diagnostics.turret_shots_last_frame = combat_state_.turret_shots_last_frame;
        player_diagnostics.health = combat_state_.player.health;
        player_diagnostics.max_health = combat_state_.player.max_health;
        player_diagnostics.weapon_cooldown_remaining_seconds = combat_state_.player.weapon_cooldown_remaining_seconds;
        player_diagnostics.hurt_flash_strength =
            combat_state_.player.hurt_flash_remaining_seconds > 0.0F
                ? std::clamp(combat_state_.player.hurt_flash_remaining_seconds / 0.30F, 0.0F, 1.0F)
                : 0.0F;
        player_diagnostics.hit_confirmation_strength =
            std::clamp(combat_state_.player.hit_confirmation_remaining_seconds / 0.16F, 0.0F, 1.0F);
        player_diagnostics.respawn_remaining_seconds = combat_state_.player.respawn_remaining_seconds;
        player_diagnostics.damage_dealt = combat_state_.player.damage_dealt;
        player_diagnostics.damage_taken = combat_state_.player.damage_taken;
        player_diagnostics.primary_attack_label = combat_state_.fire_binding_label;
        player_diagnostics.weapon_label = selected_weapon_ == PlayerWeapon::Shotgun ? "Дробовик" : "Точное оружие";
        player_diagnostics.weapon_reticle_radius = selected_weapon_ == PlayerWeapon::Shotgun ? 12.0F : 0.0F;
        if (std::ranges::find(inventory_item_ids_, shotgun_item_id) != inventory_item_ids_.end())
            player_diagnostics.weapon_selection_hint = "[1] Точное  [2] Дробовик";
        player_diagnostics.last_damage_source = combat_state_.player.last_damage_source;
        const auto& collision_diagnostics = player_controller_.collision_query_diagnostics();
        player_diagnostics.static_collision_solids = collision_diagnostics.static_world_solids;
        player_diagnostics.static_collision_candidates = collision_diagnostics.static_candidate_count;
        player_diagnostics.dynamic_blockers = collision_diagnostics.dynamic_blocker_count;
        player_diagnostics.collision_world_solids = collision_diagnostics.collision_world_solids;
        player_diagnostics.ground_probe_hit = collision_diagnostics.ground_probe_hit;
        player_diagnostics.ground_snap_applied = collision_diagnostics.ground_snap_applied;
        player_diagnostics.attempted_step_move = collision_diagnostics.attempted_step_move;
        player_diagnostics.used_step_move = collision_diagnostics.used_step_move;
        player_diagnostics.hit_x = collision_diagnostics.hit_x;
        player_diagnostics.hit_y = collision_diagnostics.hit_y;
        player_diagnostics.hit_z = collision_diagnostics.hit_z;

        const auto& velocity = player_diagnostics.velocity;
        player_diagnostics.speed = std::sqrt((velocity.x * velocity.x) + (velocity.z * velocity.z));
        return player_diagnostics;
    }

    auto GameWorldRuntime::build_runtime_diagnostics() const -> RelayCore::render::RuntimeDiagnostics
    {
        RELAYCORE_PROFILE_ZONE_N("GameWorldRuntime::runtime_diagnostics_build");
        RelayCore::render::RuntimeDiagnostics diagnostics{};
        diagnostics.has_level = !runtime_state_.level_name.empty();
        diagnostics.stress_level = runtime_state_.stress_level;
        diagnostics.level_entity_count = static_cast<std::uint32_t>(level_.entities.size());
        diagnostics.fixed_steps = runtime_state_.fixed_steps_last_frame;
        diagnostics.fixed_step_budget = simulation_loop_.max_steps_per_frame();
        diagnostics.active_trigger_overlaps = runtime_state_.active_trigger_overlaps;
        diagnostics.simulation_tick = simulation_loop_.tick_index();
        diagnostics.fixed_step_ms = static_cast<float>(simulation_loop_.step_seconds() * 1000.0);
        diagnostics.simulation_backlog_ms = static_cast<float>(simulation_loop_.pending_seconds() * 1000.0);
        diagnostics.level_name = runtime_state_.level_name;
        diagnostics.level_path = runtime_state_.level_path;
        return diagnostics;
    }

    auto GameWorldRuntime::build_interaction_diagnostics() const -> RelayCore::render::InteractionDiagnostics
    {
        RELAYCORE_PROFILE_ZONE_N("GameWorldRuntime::interaction_diagnostics_build");
        RelayCore::render::InteractionDiagnostics diagnostics{};
        diagnostics.has_focused_usable = interaction_state_.focused_usable_index >= 0;
        diagnostics.focused_distance = interaction_state_.focused_distance;
        diagnostics.active_doors = interaction_state_.active_doors;
        diagnostics.fired_triggers = interaction_state_.fired_triggers;
        diagnostics.total_doors = static_cast<std::uint32_t>(doors_.size());
        diagnostics.total_triggers = static_cast<std::uint32_t>(triggers_.size());
        diagnostics.total_usables = static_cast<std::uint32_t>(usables_.size());
        diagnostics.consumed_usables =
            static_cast<std::uint32_t>(std::count_if(usables_.begin(), usables_.end(), [](const auto& usable) {
                return usable.consumed;
            }));
        diagnostics.blocked_usables =
            static_cast<std::uint32_t>(std::count_if(usables_.begin(), usables_.end(), [](const auto& usable) {
                return usable.enabled && !usable.consumed && (usable.locked || !usable.requirement_met);
            }));
        diagnostics.enabled_usables =
            static_cast<std::uint32_t>(std::count_if(usables_.begin(), usables_.end(), [](const auto& usable) {
                return usable.enabled && !usable.consumed && !usable.locked && usable.requirement_met;
            }));
        diagnostics.activated_target_count = static_cast<std::uint32_t>(activated_target_ids_.size());
        diagnostics.signaled_target_count = static_cast<std::uint32_t>(signaled_target_ids_.size());
        diagnostics.trigger_candidate_count = static_cast<std::uint32_t>(trigger_candidates_.size());
        diagnostics.usable_candidate_count = static_cast<std::uint32_t>(usable_candidates_.size());
        diagnostics.use_pressed = interaction_state_.use_pressed;

        if (interaction_state_.focused_usable_index >= 0)
        {
            const auto& usable = usables_[static_cast<std::size_t>(interaction_state_.focused_usable_index)];
            diagnostics.focused_label = build_focused_usable_label(usable);
            diagnostics.focused_detail = build_focused_usable_detail(usable);
            diagnostics.focused_name = usable.name;
            diagnostics.focused_target = usable.target;
            diagnostics.focused_chain_target = usable.chain_target;
            diagnostics.focused_required_item = usable.required_item;
            diagnostics.focused_item_id = usable.item_id;
            diagnostics.focused_action_kind = usable_action_kind_label(usable.action_kind);
            diagnostics.focused_signal_action = signal_action_label(usable.signal_action);
            diagnostics.focused_visual_kind = visual_kind_label(usable.visual_kind);
            diagnostics.focused_panel_mount_axis = panel_mount_axis_label(usable.panel_mount_axis);
            diagnostics.focused_origin = usable.origin;
            diagnostics.focused_use_radius = usable.use_radius;
            diagnostics.focused_enabled = usable.enabled;
            diagnostics.focused_locked = usable.locked;
            diagnostics.focused_requirement_met = usable.requirement_met;
            diagnostics.focused_single_use = usable.single_use;
            diagnostics.focused_consumed = usable.consumed;
            diagnostics.focused_tracks_target_door = usable.tracks_target_door;

            const auto target_door_it = std::find_if(doors_.begin(), doors_.end(), [&usable](const auto& door) {
                return !usable.target.empty() && door.name == usable.target;
            });
            if (target_door_it != doors_.end())
            {
                diagnostics.focused_has_target_door = true;
                diagnostics.focused_target_door_state = door_state_label(target_door_it->state);
                diagnostics.focused_target_door_open_fraction = target_door_it->open_fraction;
            }
        }

        diagnostics.use_binding_label = interaction_state_.use_binding_label;
        diagnostics.last_event_kind = interaction_event_kind_label(interaction_state_.last_event.kind);
        diagnostics.last_event_source = interaction_event_source_label(interaction_state_.last_event.source_kind);
        diagnostics.last_event_target = interaction_state_.last_event.target;
        diagnostics.last_event_chain_target = interaction_state_.last_event.chain_target;
        diagnostics.last_event_item = interaction_state_.last_event.item_id;

        const RelayGame::MissionStatus mission = mission_runtime_.status();
        diagnostics.has_objective = mission.active;
        diagnostics.mission_complete = mission.completed;
        diagnostics.objective_current = mission.current_step;
        diagnostics.objective_total = mission.total_steps;
        diagnostics.objective_title = mission.title;
        diagnostics.objective_detail = mission.detail;
        diagnostics.inventory_summary = build_inventory_summary();
        diagnostics.has_inventory = !diagnostics.inventory_summary.empty();
        diagnostics.has_transient_event =
            transient_hud_event_.remaining_seconds > 0.0F && !transient_hud_event_.text.empty();
        diagnostics.transient_event_text = transient_hud_event_.text;
        diagnostics.transient_event_style = transient_hud_event_.style;
        diagnostics.transient_event_strength =
            transient_hud_event_.total_seconds > 0.001F
                ? std::clamp(transient_hud_event_.remaining_seconds / transient_hud_event_.total_seconds, 0.0F, 1.0F)
                : 0.0F;
        return diagnostics;
    }

    auto GameWorldRuntime::build_audio_diagnostics() const -> RelayCore::render::AudioDiagnostics
    {
        RELAYCORE_PROFILE_FUNCTION();
        RelayCore::render::AudioDiagnostics diagnostics{};
        diagnostics.initialized = audio_state_.initialized;
        diagnostics.has_active_loop = audio_state_.has_active_loop;
        diagnostics.has_active_zone = audio_state_.active_zone_index >= 0;
        diagnostics.master_volume = audio_state_.master_volume;
        diagnostics.active_loop_volume = audio_state_.active_loop_volume;
        diagnostics.ambient_zone_count = static_cast<std::uint32_t>(audio_zones_.size());
        diagnostics.ambient_switch_count = audio_state_.ambient_switch_count;
        diagnostics.one_shot_count = audio_state_.one_shot_count;
        diagnostics.active_zone_index = audio_state_.active_zone_index;
        diagnostics.active_source_kind = audio_state_.active_source_kind;
        diagnostics.active_zone_name = audio_state_.active_zone_name;
        diagnostics.active_loop_path = audio_state_.active_loop_path.string();
        diagnostics.last_one_shot_path = audio_state_.last_one_shot_path.string();
        if (audio_state_.active_zone_index >= 0
            && static_cast<std::size_t>(audio_state_.active_zone_index) < audio_zones_.size())
        {
            const RuntimeAudioZone& zone = audio_zones_[static_cast<std::size_t>(audio_state_.active_zone_index)];
            diagnostics.active_zone_priority = zone.priority;
            diagnostics.active_zone_gain = zone.gain;
        }
        return diagnostics;
    }

    auto GameWorldRuntime::build_live_inspector_diagnostics() const -> RelayCore::render::InspectorDiagnostics
    {
        RELAYCORE_PROFILE_ZONE_N("GameWorldRuntime::live_inspector_diagnostics_build");
        auto diagnostics = RelayGame::build_world_inspector_diagnostics(
            level_,
            doors_,
            triggers_,
            usables_,
            runtime_registry_,
            player_controller_.eye_position_world(),
            player_controller_.view_forward()
        );
        if (physics_fixture_)
        {
            const auto hit = physics_fixture_->raycast(
                player_controller_.eye_position_world(),
                player_controller_.view_forward(),
                24.0F
            );
            if (hit && hit->object.kind == RelayCore::physics::WorldObjectKind::CollisionMesh
                && (!diagnostics.has_pick || hit->distance < diagnostics.distance))
                diagnostics =
                    build_collision_inspector_diagnostics(level_, *hit, *physics_fixture_->object_bounds(hit->object));
        }
        return diagnostics;
    }

    auto GameWorldRuntime::build_inspector_diagnostics() const -> RelayCore::render::InspectorDiagnostics
    {
        RELAYCORE_PROFILE_ZONE_N("GameWorldRuntime::inspector_diagnostics_build");
        if (inspector_state_.pinned)
        {
            return inspector_state_.pinned_diagnostics;
        }

        RelayCore::render::InspectorDiagnostics diagnostics = build_live_inspector_diagnostics();
        diagnostics.pinned = false;
        return diagnostics;
    }

    auto GameWorldRuntime::runtime_registry_diagnostics() const
        -> std::vector<RelayCore::render::RuntimeRegistryEntryDiagnostics>
    {
        RELAYCORE_PROFILE_FUNCTION();
        std::vector<RelayCore::render::RuntimeRegistryEntryDiagnostics> diagnostics{};
        diagnostics.reserve(runtime_registry_.objects().size());
        for (const RelayGame::RuntimeObjectHandle& handle : runtime_registry_.objects())
        {
            diagnostics.push_back(RelayCore::render::RuntimeRegistryEntryDiagnostics{
                .runtime_id = handle.id,
                .storage_index = static_cast<std::uint32_t>(handle.storage_index),
                .runtime_handle = handle.debug_handle,
                .object_kind = RelayGame::runtime_object_kind_label(handle.kind),
                .source_bucket = handle.source_bucket,
                .name = handle.name,
                .label = handle.label,
                .target = handle.target,
                .source_reference = handle.source_reference,
                .source_file_path = handle.source_file_path,
                .source_line = handle.source_line,
                .source_column = handle.source_column,
            });
        }
        return diagnostics;
    }

    auto GameWorldRuntime::runtime_binding_diagnostics() const
        -> std::vector<RelayCore::render::RuntimeBindingDiagnostics>
    {
        RELAYCORE_PROFILE_FUNCTION();
        std::vector<RelayCore::render::RuntimeBindingDiagnostics> diagnostics{};
        diagnostics.reserve(runtime_registry_.bindings().size());
        for (const RelayGame::RuntimeBindingInfo& binding : runtime_registry_.bindings())
        {
            const bool expects_resolved_target = RelayGame::runtime_binding_requires_resolved_target(binding.kind);
            diagnostics.push_back(RelayCore::render::RuntimeBindingDiagnostics{
                .source_handle = binding.source_handle,
                .source_kind = RelayGame::runtime_object_kind_label(binding.source_kind),
                .source_name = binding.source_name,
                .source_bucket = binding.source_bucket,
                .binding_kind = RelayGame::runtime_binding_kind_label(binding.kind),
                .target = binding.target,
                .source_reference = binding.source_reference,
                .source_file_path = binding.source_file_path,
                .source_line = binding.source_line,
                .source_column = binding.source_column,
                .expects_resolved_target = expects_resolved_target,
                .unresolved_target =
                    expects_resolved_target && !binding.target.empty() && binding.resolved_target_count == 0,
                .ambiguous_target = expects_resolved_target && binding.resolved_target_count > 1,
                .resolved_target_count = binding.resolved_target_count,
                .resolved_targets_summary = binding.resolved_targets_summary,
            });
        }
        return diagnostics;
    }
} // namespace RelayGame

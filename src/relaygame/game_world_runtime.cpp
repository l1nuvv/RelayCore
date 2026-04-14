#include "relaygame/game_world_runtime.h"

#include "relaycore/app/engine_services.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/core/profiling.h"
#include "relaycore/debug/inspector_source_reference.h"
#include "relaycore/physics/collision.h"
#include "relaycore/render/renderer.h"
#include "relaygame/player_controller_config.h"
#include "relaygame/world_inspector.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

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

        [[nodiscard]] auto level_name_from_path(const std::string& level_path) -> std::string
        {
            RELAYCORE_PROFILE_FUNCTION();
            if (level_path.empty())
            {
                return {};
            }

            const std::filesystem::path path{level_path};
            const std::string stem = path.stem().string();
            return stem.empty() ? path.filename().string() : stem;
        }

        [[nodiscard]] auto is_stress_level_name(const std::string& level_name) -> bool
        {
            RELAYCORE_PROFILE_FUNCTION();
            return level_name.starts_with("stress_room_");
        }
    } // namespace

    void GameWorldRuntime::startup(RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("GameWorldRuntime::startup");
        load_level_session(services.config(), LevelSessionBuilder{services.config().gameplay}.build());
        activate_level_session(services);
    }

    void GameWorldRuntime::load_level_session(const RelayCore::core::EngineConfig& config, LevelSession session)
    {
        RELAYCORE_PROFILE_ZONE_N("GameWorldRuntime::load_level_session");
        const glm::vec3 spawn_position = session.spawn_position;
        const auto spawn_view_angles = session.spawn_view_angles;
        level_ = std::move(session.level);
        runtime_state_ = {};
        runtime_state_.level_path = session.level_path.string();
        runtime_state_.level_name = level_name_from_path(runtime_state_.level_path);
        runtime_state_.stress_level = is_stress_level_name(runtime_state_.level_name);
        doors_ = std::move(session.doors);
        triggers_ = std::move(session.triggers);
        usables_ = std::move(session.usables);
        trigger_spatial_index_ = std::move(session.trigger_spatial_index);
        usable_spatial_index_ = std::move(session.usable_spatial_index);
        runtime_registry_ = std::move(session.runtime_registry);
        authoring_warnings_ = std::move(session.authoring_warnings);
        inventory_item_ids_.clear();
        activated_target_ids_.clear();
        signaled_target_ids_.clear();
        interaction_state_ = {};
        combat_state_ = {};
        combat_state_.spawn_position = spawn_position;
        combat_state_.spawn_view_angles = spawn_view_angles;
        combat_state_.fire_binding_label = config.gameplay.controls.primary_attack.empty()
                                               ? std::string{"Mouse1"}
                                               : config.gameplay.controls.primary_attack;
        const RelayCore::gameplay::CombatBuildResult combat = RelayCore::gameplay::build_runtime_combat(level_);
        combat_state_.turrets = combat.turrets;
        combat_state_.hazards = combat.hazards;
        RelayCore::gameplay::reset_player_combat_state(combat_state_.player);
        transient_hud_event_ = {};
        inspector_state_ = {};
        mission_runtime_.reset(RelayCore::gameplay::MissionBuilder{level_}.build());
        interaction_state_.use_binding_label =
            config.gameplay.controls.use.empty() ? std::string{"E"} : config.gameplay.controls.use;
        configure_audio_cues(config.gameplay.audio);
        audio_zones_ = RelayGame::build_audio_zones(level_, RelayCore::core::asset_root_path());
        audio_state_ = {};
        if (const RelayCore::gameplay::MissionStatus mission = mission_runtime_.status();
            mission.active && !mission.completed)
        {
            push_mission_status_event(mission, RelayCore::render::HudEventStyle::Neutral, 2.8F);
        }
        sync_door_audio_state();
        player_controller_.set_config(build_player_controller_config(config));
        player_controller_.initialize(spawn_position, spawn_view_angles);
        player_controller_.set_static_collision_world(level_);
        simulation_loop_.reset();
    }

    void GameWorldRuntime::activate_level_session(RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("GameWorldRuntime::activate_level_session");
        player_controller_.enable_capture(services.window());
        player_controller_.apply_camera(services.camera());
        update_ambient_audio(services);
        services.audio_system().update(0.0F);
        audio_state_.initialized = services.audio_system().is_initialized();
        audio_state_.master_volume = services.audio_system().config().master_volume;
        audio_state_.has_active_loop = services.audio_system().has_active_loop();
        audio_state_.active_loop_path = services.audio_system().current_loop_path();
        audio_state_.active_loop_volume = services.audio_system().current_loop_volume();
    }

    void GameWorldRuntime::shutdown(RelayCore::EngineServices& services) noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        services.audio_system().stop_loop();
        audio_state_ = {};
        runtime_state_ = {};
        runtime_registry_.clear();
        authoring_warnings_.clear();
        audio_zones_.clear();
        combat_state_ = {};
    }

    void GameWorldRuntime::on_event(const SDL_Event& event, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_FUNCTION();
        player_controller_.on_event(event, services.window(), services);
    }

    void GameWorldRuntime::update(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("GameWorldRuntime::update");

        interaction_state_.use_pressed = false;
        interaction_state_.fired_triggers = 0;
        combat_state_.fire_pressed = false;
        combat_state_.active_hazard_contacts = 0;
        combat_state_.turret_shots_last_frame = 0;
        player_controller_.flush_pending_look();
        tick_transient_hud_event(static_cast<float>(frame.delta_seconds));
        tick_usable_feedback(static_cast<float>(frame.delta_seconds));

        runtime_state_.fixed_steps_last_frame =
            simulation_loop_.advance(frame, [this, &services](const RelayCore::FrameInfo& sim_frame) {
                simulate_fixed_step(sim_frame, services);
            });

        refresh_runtime_usables_and_focus();
        if (player_controller_.consume_inspector_pin_pressed())
        {
            if (inspector_state_.pinned)
            {
                inspector_state_ = {};
                RelayCore::log::info("Inspector pin cleared");
            } else
            {
                const RelayCore::render::InspectorDiagnostics live_inspector = build_live_inspector_diagnostics();
                if (live_inspector.has_pick)
                {
                    inspector_state_.pinned = true;
                    inspector_state_.pinned_diagnostics = live_inspector;
                    inspector_state_.pinned_diagnostics.pinned = true;
                    RelayCore::log::info(
                        "Inspector pinned {} {}",
                        inspector_state_.pinned_diagnostics.object_kind.empty()
                            ? std::string{"object"}
                            : inspector_state_.pinned_diagnostics.object_kind,
                        inspector_state_.pinned_diagnostics.name.empty()
                            ? std::string{}
                            : ("'" + inspector_state_.pinned_diagnostics.name + "'")
                    );
                } else
                {
                    RelayCore::log::info("Inspector pin ignored: no object under crosshair");
                }
            }
        }
        runtime_state_.active_trigger_overlaps = 0;
        const RelayCore::physics::Aabb player_bounds = player_controller_.player_bounds();
        RelayCore::gameplay::query_trigger_candidates(trigger_spatial_index_, player_bounds, trigger_candidates_);
        for (const std::uint32_t candidate_index : trigger_candidates_)
        {
            if (candidate_index >= triggers_.size())
            {
                continue;
            }

            if (RelayCore::physics::overlaps(player_bounds, triggers_[candidate_index].bounds, 0.001F))
            {
                ++runtime_state_.active_trigger_overlaps;
            }
        }
        interaction_state_.active_doors =
            static_cast<std::uint32_t>(std::count_if(doors_.begin(), doors_.end(), [](const auto& door) {
                return RelayCore::gameplay::is_door_active(door);
            }));
        update_ambient_audio(services);
        services.audio_system().update(static_cast<float>(frame.delta_seconds));
        audio_state_.initialized = services.audio_system().is_initialized();
        audio_state_.master_volume = services.audio_system().config().master_volume;
        audio_state_.has_active_loop = services.audio_system().has_active_loop();
        audio_state_.active_loop_path = services.audio_system().current_loop_path();
        audio_state_.active_loop_volume = services.audio_system().current_loop_volume();
        player_controller_.apply_camera(services.camera());
    }

    void GameWorldRuntime::publish_diagnostics(RelayCore::Renderer& renderer) const
    {
        RELAYCORE_PROFILE_FUNCTION();
        const RelayCore::render::PlayerDiagnostics player = build_player_diagnostics();
        const RelayCore::render::RuntimeDiagnostics runtime = build_runtime_diagnostics();
        const RelayCore::render::InteractionDiagnostics interaction = build_interaction_diagnostics();
        const RelayCore::render::InspectorDiagnostics inspector = build_inspector_diagnostics();
        const RelayCore::render::AudioDiagnostics audio = build_audio_diagnostics();

        renderer.set_player_diagnostics(player);
        renderer.set_runtime_diagnostics(runtime);
        renderer.set_interaction_diagnostics(interaction);
        renderer.set_inspector_diagnostics(inspector);
        renderer.set_audio_diagnostics(audio);

        RELAYCORE_PROFILE_PLOT("Runtime/FixedSteps", runtime.fixed_steps);
        RELAYCORE_PROFILE_PLOT("Runtime/SimBacklogMs", runtime.simulation_backlog_ms);
        RELAYCORE_PROFILE_PLOT("Runtime/ActiveTriggerOverlaps", runtime.active_trigger_overlaps);
        RELAYCORE_PROFILE_PLOT("Gameplay/ActiveDoors", interaction.active_doors);
        RELAYCORE_PROFILE_PLOT("Gameplay/UsablesEnabled", interaction.enabled_usables);
        RELAYCORE_PROFILE_PLOT("Gameplay/UsablesBlocked", interaction.blocked_usables);
        RELAYCORE_PROFILE_PLOT("Gameplay/MissionStep", interaction.objective_current);
        RELAYCORE_PROFILE_PLOT("Player/CollisionWorldSolids", player.collision_world_solids);
        RELAYCORE_PROFILE_PLOT("Player/Health", player.health);
        RELAYCORE_PROFILE_PLOT("Gameplay/TurretsDestroyed", player.destroyed_turrets);
    }

    auto GameWorldRuntime::level() const noexcept -> const RelayCore::BlockoutLevel&
    {
        RELAYCORE_PROFILE_FUNCTION();
        return level_;
    }

    auto GameWorldRuntime::doors() const noexcept -> const std::vector<RelayCore::gameplay::RuntimeDoor>&
    {
        RELAYCORE_PROFILE_FUNCTION();
        return doors_;
    }

    auto GameWorldRuntime::triggers() const noexcept -> const std::vector<RelayCore::gameplay::RuntimeTrigger>&
    {
        RELAYCORE_PROFILE_FUNCTION();
        return triggers_;
    }

    auto GameWorldRuntime::usables() const noexcept -> const std::vector<RelayCore::gameplay::RuntimeUsable>&
    {
        RELAYCORE_PROFILE_FUNCTION();
        return usables_;
    }

    auto GameWorldRuntime::turrets() const noexcept -> const std::vector<RelayCore::gameplay::RuntimeTurret>&
    {
        RELAYCORE_PROFILE_FUNCTION();
        return combat_state_.turrets;
    }

    auto GameWorldRuntime::focused_usable_index() const noexcept -> int
    {
        RELAYCORE_PROFILE_FUNCTION();
        return interaction_state_.focused_usable_index;
    }

    auto GameWorldRuntime::consume_reload_level_requested() noexcept -> bool
    {
        RELAYCORE_PROFILE_FUNCTION();
        return player_controller_.consume_reload_level_pressed();
    }

    auto GameWorldRuntime::consume_open_inspector_source_requested() noexcept -> bool
    {
        RELAYCORE_PROFILE_FUNCTION();
        return player_controller_.consume_open_inspector_source_pressed();
    }

    auto GameWorldRuntime::capture_pinned_inspector_restore_state() const -> std::optional<InspectorRestoreState>
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (!inspector_state_.pinned)
        {
            return std::nullopt;
        }

        const std::string primary_source_reference =
            RelayCore::debug::build_primary_inspector_source_reference(inspector_state_.pinned_diagnostics);
        if (inspector_state_.pinned_diagnostics.runtime_id == 0 && primary_source_reference.empty())
        {
            return std::nullopt;
        }

        return InspectorRestoreState{
            .runtime_object_id = inspector_state_.pinned_diagnostics.runtime_id,
            .primary_source_reference = primary_source_reference,
        };
    }

    void GameWorldRuntime::restore_pinned_inspector(const InspectorRestoreState& state)
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (state.runtime_object_id != 0)
        {
            RelayCore::render::InspectorDiagnostics restored =
                RelayGame::find_world_inspector_diagnostics_by_runtime_id(
                    level_,
                    doors_,
                    triggers_,
                    usables_,
                    runtime_registry_,
                    state.runtime_object_id,
                    player_controller_.eye_position_world()
                );
            if (restored.has_pick)
            {
                restored.pinned = true;
                inspector_state_.pinned = true;
                inspector_state_.pinned_diagnostics = std::move(restored);
                return;
            }
        }

        if (state.primary_source_reference.empty())
        {
            inspector_state_ = {};
            return;
        }

        RelayCore::render::InspectorDiagnostics restored =
            RelayGame::find_world_inspector_diagnostics_by_authored_source(
                level_,
                doors_,
                triggers_,
                usables_,
                {},
                state.primary_source_reference,
                player_controller_.eye_position_world()
            );
        if (!restored.has_pick)
        {
            inspector_state_ = {};
            return;
        }

        restored.pinned = true;
        inspector_state_.pinned = true;
        inspector_state_.pinned_diagnostics = std::move(restored);
    }

    auto GameWorldRuntime::authoring_warnings() const noexcept
        -> const std::vector<RelayCore::render::AuthoringWarningDiagnostics>&
    {
        RELAYCORE_PROFILE_FUNCTION();
        return authoring_warnings_;
    }

    auto GameWorldRuntime::capture_runtime_snapshot() const -> RelayCore::gameplay::RuntimeLevelSnapshot
    {
        RELAYCORE_PROFILE_FUNCTION();
        return RelayCore::gameplay::capture_runtime_level_snapshot(
            runtime_state_.level_path,
            runtime_registry_,
            doors_,
            triggers_,
            usables_,
            combat_state_.turrets,
            inventory_item_ids_,
            activated_target_ids_,
            signaled_target_ids_,
            combat_state_.player,
            mission_runtime_,
            simulation_loop_.tick_index()
        );
    }

    auto GameWorldRuntime::restore_runtime_snapshot(const RelayCore::gameplay::RuntimeLevelSnapshot& snapshot)
        -> RelayCore::gameplay::SnapshotRestoreResult
    {
        RELAYCORE_PROFILE_FUNCTION();
        const RelayCore::gameplay::SnapshotRestoreResult result = RelayCore::gameplay::restore_runtime_level_snapshot(
            snapshot,
            runtime_state_.level_path,
            runtime_registry_,
            doors_,
            triggers_,
            usables_,
            combat_state_.turrets,
            inventory_item_ids_,
            activated_target_ids_,
            signaled_target_ids_,
            combat_state_.player,
            mission_runtime_
        );
        if (result.restored)
        {
            transient_hud_event_ = {};
            interaction_state_.last_event = {};
            interaction_state_.fired_triggers = 0;
            combat_state_.active_hazard_contacts = 0;
            combat_state_.turret_shots_last_frame = 0;
            interaction_state_.active_doors =
                static_cast<std::uint32_t>(std::count_if(doors_.begin(), doors_.end(), [](const auto& door) {
                    return RelayCore::gameplay::is_door_active(door);
                }));
            sync_door_audio_state();
            refresh_runtime_usables_and_focus();
            if (inspector_state_.pinned)
            {
                restore_pinned_inspector(
                    InspectorRestoreState{
                        .runtime_object_id = inspector_state_.pinned_diagnostics.runtime_id,
                        .primary_source_reference = RelayCore::debug::build_primary_inspector_source_reference(
                            inspector_state_.pinned_diagnostics
                        ),
                    }
                );
            }
        }
        return result;
    }

    auto GameWorldRuntime::runtime_registry() const noexcept -> const RelayCore::gameplay::RuntimeObjectRegistry&
    {
        RELAYCORE_PROFILE_FUNCTION();
        return runtime_registry_;
    }

    auto GameWorldRuntime::level_path() const noexcept -> const std::string&
    {
        RELAYCORE_PROFILE_FUNCTION();
        return runtime_state_.level_path;
    }

    auto GameWorldRuntime::simulation_tick() const noexcept -> std::uint64_t
    {
        RELAYCORE_PROFILE_FUNCTION();
        return simulation_loop_.tick_index();
    }

    auto GameWorldRuntime::consume_save_runtime_snapshot_requested() noexcept -> bool
    {
        RELAYCORE_PROFILE_FUNCTION();
        return player_controller_.consume_save_runtime_snapshot_pressed();
    }

    auto GameWorldRuntime::consume_restore_runtime_snapshot_requested() noexcept -> bool
    {
        RELAYCORE_PROFILE_FUNCTION();
        return player_controller_.consume_restore_runtime_snapshot_pressed();
    }

    void
    GameWorldRuntime::simulate_fixed_step(const RelayCore::FrameInfo& sim_frame, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_FUNCTION();
        const float sim_delta_seconds = static_cast<float>(sim_frame.delta_seconds);
        RelayCore::gameplay::tick_combat_state(sim_delta_seconds, combat_state_.player, combat_state_.turrets);

        if (!combat_state_.player.alive)
        {
            interaction_state_.active_doors =
                RelayCore::gameplay::update_doors(sim_delta_seconds, player_controller_.player_bounds(), doors_);
            sync_door_audio_state();
            if (combat_state_.player.respawn_remaining_seconds <= 0.0F)
            {
                respawn_player_at_spawn();
                play_gameplay_audio(services, audio_cues_.player_respawn);
                push_transient_hud_event("Возврат к точке входа", RelayCore::render::HudEventStyle::Neutral, 1.2F);
            }
            return;
        }

        const RelayCore::gameplay::MissionStatus mission_before = mission_runtime_.status();
        const bool mission_was_complete = mission_before.completed;
        RelayCore::gameplay::collect_blocking_doors(doors_, player_controller_.player_bounds(), dynamic_blockers_);
        player_controller_.update(sim_frame, level_, dynamic_blockers_);
        refresh_runtime_usables_and_focus();

        const bool fire_pressed_this_step = player_controller_.consume_fire_pressed();
        process_fire_input(fire_pressed_this_step, services);
        const bool use_pressed_this_step = player_controller_.consume_use_pressed();
        process_use_input(use_pressed_this_step, services);

        RelayCore::gameplay::query_trigger_candidates(
            trigger_spatial_index_,
            player_controller_.player_bounds(),
            trigger_candidates_
        );
        interaction_state_.fired_triggers += RelayCore::gameplay::update_trigger_overlaps(
            player_controller_.player_bounds(),
            trigger_candidates_,
            triggers_,
            doors_,
            usables_,
            activated_target_ids_,
            signaled_target_ids_,
            interaction_state_.last_event
        );
        play_door_state_change_audio(services);

        const float player_health_before_threats = combat_state_.player.health;
        const RelayCore::gameplay::CombatThreatResult combat_result = RelayCore::gameplay::update_combat_threats(
            level_,
            doors_,
            player_controller_.player_bounds(),
            player_controller_.eye_position_world(),
            combat_state_.player,
            combat_state_.turrets,
            combat_state_.hazards,
            sim_delta_seconds
        );
        combat_state_.active_hazard_contacts = combat_result.hazard_contacts;
        combat_state_.turret_shots_last_frame += combat_result.turret_shots;
        if (combat_result.turret_shots > 0)
        {
            play_gameplay_audio(services, audio_cues_.turret_fire);
        }
        if ((combat_state_.player.health + 0.001F) < player_health_before_threats)
        {
            play_gameplay_audio(services, audio_cues_.player_hurt);
        }

        interaction_state_.active_doors =
            RelayCore::gameplay::update_doors(sim_delta_seconds, player_controller_.player_bounds(), doors_);
        sync_door_audio_state();
        if (!combat_state_.player.alive)
        {
            push_transient_hud_event(
                "Оператор выведен из строя. Возврат к точке входа...",
                RelayCore::render::HudEventStyle::Warning,
                combat_state_.player.respawn_delay_seconds
            );
            return;
        }
        mission_runtime_.update(
            player_controller_.player_bounds(),
            doors_,
            inventory_item_ids_,
            activated_target_ids_,
            signaled_target_ids_
        );
        const RelayCore::gameplay::MissionStatus mission_after = mission_runtime_.status();
        if (!mission_was_complete && mission_after.completed)
        {
            play_gameplay_audio(services, audio_cues_.mission_complete);
            push_transient_hud_event(
                mission_after.title.empty() ? std::string{"Задача выполнена"} : mission_after.title,
                RelayCore::render::HudEventStyle::Success,
                2.6F
            );
        } else if (mission_after.active && !mission_after.completed
                   && (!mission_before.active || mission_after.current_step != mission_before.current_step))
        {
            RELAYCORE_PROFILE_FUNCTION();
            push_mission_status_event(mission_after, RelayCore::render::HudEventStyle::Neutral, 2.4F);
        }
    }

    void GameWorldRuntime::refresh_runtime_usables_and_focus()
    {
        RELAYCORE_PROFILE_FUNCTION();
        RelayCore::gameplay::update_runtime_usables(doors_, usables_);
        for (RelayCore::gameplay::RuntimeUsable& usable : usables_)
        {
            usable.requirement_met =
                usable.required_item.empty() || has_inventory_item(std::string_view{usable.required_item});
        }
        RelayCore::gameplay::rebuild_usable_spatial_index(usables_, usable_spatial_index_);
        refresh_interaction_focus();
    }

    void GameWorldRuntime::refresh_interaction_focus()
    {
        RELAYCORE_PROFILE_FUNCTION();
        const float interaction_distance = interaction_query_distance();
        RelayCore::gameplay::query_usable_candidates(
            usable_spatial_index_,
            player_controller_.eye_position_world(),
            interaction_distance,
            usable_candidates_
        );
        const auto focus = RelayCore::gameplay::find_focused_usable(
            player_controller_.eye_position_world(),
            player_controller_.view_forward(),
            usables_,
            usable_candidates_,
            interaction_distance
        );
        interaction_state_.focused_usable_index = focus.usable_index;
        interaction_state_.focused_distance = focus.distance;
    }

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
        player_diagnostics.respawn_remaining_seconds = combat_state_.player.respawn_remaining_seconds;
        player_diagnostics.damage_dealt = combat_state_.player.damage_dealt;
        player_diagnostics.damage_taken = combat_state_.player.damage_taken;
        player_diagnostics.primary_attack_label = combat_state_.fire_binding_label;
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

        const RelayCore::gameplay::MissionStatus mission = mission_runtime_.status();
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
        return RelayGame::build_world_inspector_diagnostics(
            level_,
            doors_,
            triggers_,
            usables_,
            runtime_registry_,
            player_controller_.eye_position_world(),
            player_controller_.view_forward()
        );
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
        for (const RelayCore::gameplay::RuntimeObjectHandle& handle : runtime_registry_.objects())
        {
            diagnostics.push_back(
                RelayCore::render::RuntimeRegistryEntryDiagnostics{
                    .runtime_id = handle.id,
                    .storage_index = static_cast<std::uint32_t>(handle.storage_index),
                    .runtime_handle = handle.debug_handle,
                    .object_kind = RelayCore::gameplay::runtime_object_kind_label(handle.kind),
                    .source_bucket = handle.source_bucket,
                    .name = handle.name,
                    .label = handle.label,
                    .target = handle.target,
                    .source_reference = handle.source_reference,
                    .source_file_path = handle.source_file_path,
                    .source_line = handle.source_line,
                    .source_column = handle.source_column,
                }
            );
        }
        return diagnostics;
    }

    auto GameWorldRuntime::runtime_binding_diagnostics() const
        -> std::vector<RelayCore::render::RuntimeBindingDiagnostics>
    {
        RELAYCORE_PROFILE_FUNCTION();
        std::vector<RelayCore::render::RuntimeBindingDiagnostics> diagnostics{};
        diagnostics.reserve(runtime_registry_.bindings().size());
        for (const RelayCore::gameplay::RuntimeBindingInfo& binding : runtime_registry_.bindings())
        {
            diagnostics.push_back(
                RelayCore::render::RuntimeBindingDiagnostics{
                    .source_handle = binding.source_handle,
                    .source_kind = RelayCore::gameplay::runtime_object_kind_label(binding.source_kind),
                    .source_name = binding.source_name,
                    .source_bucket = binding.source_bucket,
                    .binding_kind = RelayCore::gameplay::runtime_binding_kind_label(binding.kind),
                    .target = binding.target,
                    .source_reference = binding.source_reference,
                    .source_file_path = binding.source_file_path,
                    .source_line = binding.source_line,
                    .source_column = binding.source_column,
                    .resolved_target_count = binding.resolved_target_count,
                    .resolved_targets_summary = binding.resolved_targets_summary,
                }
            );
        }
        return diagnostics;
    }
} // namespace RelayGame

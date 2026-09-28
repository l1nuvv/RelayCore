#include "relaygame/game_world_runtime.h"

#include "relaycore/app/engine_services.h"
#include "relaycore/audio/audio_system.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/core/profiling.h"
#include "relaycore/debug/inspector_source_reference.h"
#include "relaycore/physics/collision.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/renderer.h"
#include "relaygame/diagnostics_publication_policy.h"
#include "relaygame/player_controller_config.h"
#include "relaygame/usable_interaction.h"
#include "relaygame/world_inspector.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace RelayGame
{
    namespace
    {
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

    void GameWorldRuntime::load_level_session(const RelayCore::core::EngineConfig& config, LevelSession session)
    {
        RELAYCORE_PROFILE_ZONE_N("GameWorldRuntime::load_level_session");
        const glm::vec3 spawn_position = session.spawn_position;
        const auto spawn_view_angles = session.spawn_view_angles;
        import_level_session(std::move(session));
        reset_runtime_for_loaded_level(config, spawn_position, spawn_view_angles);
    }

    void GameWorldRuntime::import_level_session(LevelSession session)
    {
        RELAYCORE_PROFILE_ZONE_N("GameWorldRuntime::import_level_session");
        level_ = std::move(session.level);
        exits_ = std::move(session.exits);
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
        enemies_ = std::move(session.enemies);
        enemy_blockers_.reserve(
            level_.solids.size() + level_.prop_colliders.size() + doors_.size() + enemies_.size() + 1
        );
        loaded_level_turrets_ = std::move(session.combat.turrets);
        loaded_level_hazards_ = std::move(session.combat.hazards);
        loaded_mission_definition_ = std::move(session.mission_definition);
        audio_zones_ = std::move(session.audio_zones);
        environment_ = std::move(session.environment);
        ending_ = session.ending;
    }

    void GameWorldRuntime::reset_runtime_for_loaded_level(
        const RelayCore::core::EngineConfig& config,
        const glm::vec3& spawn_position,
        const std::optional<glm::vec2>& spawn_view_angles
    )
    {
        RELAYCORE_PROFILE_ZONE_N("GameWorldRuntime::reset_runtime_for_loaded_level");
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
        combat_state_.turrets = loaded_level_turrets_;
        combat_state_.hazards = loaded_level_hazards_;
        RelayCore::gameplay::reset_player_combat_state(combat_state_.player);
        transient_hud_event_ = {};
        inspector_state_ = {};
        mission_runtime_.reset(loaded_mission_definition_);
        interaction_state_.use_binding_label =
            config.gameplay.controls.use.empty() ? std::string{"E"} : config.gameplay.controls.use;
        configure_audio_cues(config.gameplay.audio);
        audio_state_ = {};
        if (const RelayGame::MissionStatus mission = mission_runtime_.status(); mission.active && !mission.completed)
        {
            push_mission_status_event(mission, RelayCore::render::HudEventStyle::Neutral, 2.8F);
        }
        sync_door_audio_state();
        player_controller_.set_config(build_player_controller_config(config));
        player_controller_.initialize(spawn_position, spawn_view_angles);
        player_feedback_.reset();
        paused_ = false;
        step_variant_ = 0;
        const std::array<const char*, 3> surfaces{"concrete", "metal", "tile"};
        for (std::size_t surface = 0; surface < surfaces.size(); ++surface)
        {
            for (std::size_t variant = 0; variant < 4; ++variant)
                player_sounds_[surface * 4 + variant] = RelayCore::core::asset_path(
                    "audio/player/step_" + std::string{surfaces[surface]} + '_' + std::to_string(variant + 1) + ".wav"
                );
            player_sounds_[12 + surface] =
                RelayCore::core::asset_path("audio/player/land_" + std::string{surfaces[surface]} + ".wav");
        }
        player_sounds_[15] = RelayCore::core::asset_path("audio/player/flashlight_switch.wav");
        player_controller_.set_static_collision_world(level_);
        interaction_occluders_.clear();
        interaction_occluders_.reserve(level_.solids.size() + doors_.size());
        for (const auto& solid : level_.solids)
            interaction_occluders_.push_back({solid.mins, solid.maxs});
        for (const auto& solid : level_.prop_colliders)
            interaction_occluders_.push_back({solid.mins, solid.maxs});
        simulation_loop_.reset();
    }

    void GameWorldRuntime::prepare_level_session_activation(
        RelayCore::EngineServices& services,
        const bool allow_capture,
        const RelayCore::render::PerspectiveCameraConfig* projection
    )
    {
        auto camera = services.camera();
        if (projection)
            camera.apply_projection_config(*projection);
        player_controller_.apply_camera(camera);
        services.audio_system().preload_one_shots(player_sounds_);
        const std::array combat_cues{
            audio_cues_.weapon_fire,
            audio_cues_.weapon_hit,
            audio_cues_.threat_windup,
            audio_cues_.rover_attack,
            audio_cues_.turret_fire,
            audio_cues_.turret_destroyed,
            audio_cues_.player_hurt
        };
        for (const auto& cue : combat_cues)
            if (!cue.empty())
                services.audio_system().preload_one_shots(std::span{&cue, 1});
        player_controller_.set_session_capture(services.window(), allow_capture);
    }

    void GameWorldRuntime::activate_level_session(RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("GameWorldRuntime::activate_level_session");
        services.audio_system().stop_one_shots();
        player_controller_.apply_camera(services.camera());
        try
        {
            std::vector<RelayCore::AudioSystem::SpatialLoop> loops;
            for (const auto& effect : environment_)
            {
                const std::string path = environment_sound(effect.kind);
                loops.push_back(
                    {path.empty() ? std::filesystem::path{} : RelayCore::core::asset_path(path), effect.origin}
                );
            }
            services.audio_system().configure_spatial_loops(loops);
            update_ambient_audio(services);
            services.audio_system().update(0.0F);
            refresh_audio_runtime_state(services);
            spatial_audio_ready_ = true;
        } catch (const std::exception& error)
        {
            services.audio_system().stop_loop();
            services.audio_system().clear_spatial_loops();
            spatial_audio_ready_ = false;
            RelayCore::log::error("Level loaded, but audio activation failed: {}", error.what());
        }
    }

    void GameWorldRuntime::refresh_audio_runtime_state(RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("GameWorldRuntime::refresh_audio_runtime_state");
        audio_state_.initialized = services.audio_system().is_initialized();
        audio_state_.master_volume = services.audio_system().config().master_volume;
        audio_state_.has_active_loop = services.audio_system().has_active_loop();
        audio_state_.active_loop_path = services.audio_system().current_loop_path();
        audio_state_.active_loop_volume = services.audio_system().current_loop_volume();
    }

    void GameWorldRuntime::apply_player_settings(float sensitivity, float camera_motion) noexcept
    {
        player_controller_.set_mouse_sensitivity(sensitivity);
        camera_motion_ = camera_motion;
        player_controller_.set_eye_offset(player_feedback_.eye_offset(camera_motion_));
    }

    void GameWorldRuntime::set_paused(bool paused, RelayCore::EngineServices& services)
    {
        if (services.window().relative_mouse_mode() == paused)
            services.window().set_relative_mouse_mode(!paused);
        paused_ = paused;
        clear_pending_gameplay_actions();
        player_controller_.set_input_ownership({true, true});
        services.audio_system().set_paused(paused);
        if (!paused)
            player_controller_.apply_camera(services.camera());
    }

    void GameWorldRuntime::shutdown(RelayCore::EngineServices& services) noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        retire_level_session(services);
        static_cast<void>(SDL_SetWindowRelativeMouseMode(services.window().handle(), false));
    }

    void GameWorldRuntime::retire_level_session(RelayCore::EngineServices& services) noexcept
    {
        spatial_audio_ready_ = false;
        services.audio_system().stop_loop();
        services.audio_system().stop_one_shots();
        services.audio_system().clear_spatial_loops();
        clear_loaded_level_state();
    }

    void GameWorldRuntime::clear_loaded_level_state() noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        *this = GameWorldRuntime{};
        paused_ = true;
    }

    void GameWorldRuntime::on_event(const SDL_Event& event, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_FUNCTION();
        switch (event.type)
        {
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                window_has_focus_ = false;
                break;
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                window_has_focus_ = true;
                break;
            case SDL_EVENT_WINDOW_MINIMIZED:
                window_minimized_ = true;
                break;
            case SDL_EVENT_WINDOW_RESTORED:
                window_minimized_ = false;
                break;
            default:
                break;
        }
        sync_imgui_capture(services);
        if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_WINDOW_MINIMIZED)
        {
            clear_pending_gameplay_actions();
        }
        player_controller_.on_event(event, services.window(), services);
    }

    void GameWorldRuntime::sync_imgui_capture(RelayCore::EngineServices& services) noexcept
    {
        const GameplayInputOwnership ownership = resolve_gameplay_input_ownership(
            window_has_focus_ && !paused_,
            window_minimized_,
            services.window().relative_mouse_mode(),
            services.renderer().wants_keyboard_capture(),
            services.renderer().wants_mouse_capture(),
            services.renderer().wants_text_input()
        );

        if (!input_ownership_.keyboard_blocked && ownership.keyboard_blocked)
        {
            frame_actions_.use_pressed = false;
            interaction_state_.use_pressed = false;
        }
        if (!input_ownership_.mouse_blocked && ownership.mouse_blocked)
        {
            frame_actions_.fire_pressed = false;
            combat_state_.fire_pressed = false;
        }

        player_controller_.set_input_ownership(ownership);
        input_ownership_ = ownership;
    }

    void GameWorldRuntime::clear_pending_gameplay_actions() noexcept
    {
        frame_actions_.use_pressed = false;
        frame_actions_.fire_pressed = false;
        frame_actions_.inspector_pin_pressed = false;
        frame_iteration_commands_ = {};
        interaction_state_.use_pressed = false;
        combat_state_.fire_pressed = false;
    }

    void GameWorldRuntime::publish_diagnostics(RelayCore::Renderer& renderer) const
    {
        RELAYCORE_PROFILE_FUNCTION();
        const DiagnosticsPublicationNeeds needs = diagnostics_publication_needs(
            renderer.diagnostics_visible(),
            renderer.advanced_diagnostics_visible(),
            renderer.inspector_visualization_enabled()
        );
        RelayCore::render::PlayerDiagnostics player = build_player_diagnostics();
        RelayCore::render::InteractionDiagnostics interaction = build_interaction_diagnostics();

        RELAYCORE_PROFILE_PLOT("Runtime/FixedSteps", runtime_state_.fixed_steps_last_frame);
        RELAYCORE_PROFILE_PLOT("Runtime/SimBacklogMs", simulation_loop_.pending_seconds() * 1000.0);
        RELAYCORE_PROFILE_PLOT("Runtime/ActiveTriggerOverlaps", runtime_state_.active_trigger_overlaps);
        RELAYCORE_PROFILE_PLOT("Gameplay/ActiveDoors", interaction.active_doors);
        RELAYCORE_PROFILE_PLOT("Gameplay/UsablesEnabled", interaction.enabled_usables);
        RELAYCORE_PROFILE_PLOT("Gameplay/UsablesBlocked", interaction.blocked_usables);
        RELAYCORE_PROFILE_PLOT("Gameplay/MissionStep", interaction.objective_current);
        RELAYCORE_PROFILE_PLOT("Player/CollisionWorldSolids", player.collision_world_solids);
        RELAYCORE_PROFILE_PLOT("Player/Health", player.health);
        RELAYCORE_PROFILE_PLOT("Gameplay/TurretsDestroyed", player.destroyed_turrets);

        renderer.set_player_diagnostics(std::move(player));
        renderer.set_interaction_diagnostics(std::move(interaction));

        if (needs.runtime_and_audio)
        {
            renderer.set_runtime_diagnostics(build_runtime_diagnostics());
            renderer.set_audio_diagnostics(build_audio_diagnostics());
        }

        if (needs.inspector)
        {
            renderer.set_inspector_diagnostics(build_inspector_diagnostics());
        }
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

    auto GameWorldRuntime::consume_iteration_command_requests() noexcept -> IterationCommandRequests
    {
        RELAYCORE_PROFILE_FUNCTION();
        IterationCommandRequests commands = frame_iteration_commands_;
        frame_iteration_commands_ = {};
        return commands;
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

    auto GameWorldRuntime::mission_status() const -> RelayGame::MissionStatus
    {
        RELAYCORE_PROFILE_FUNCTION();
        return mission_runtime_.status();
    }

    auto GameWorldRuntime::player_combat_state() const noexcept -> const RelayCore::gameplay::PlayerCombatState&
    {
        RELAYCORE_PROFILE_FUNCTION();
        return combat_state_.player;
    }

    auto GameWorldRuntime::inventory_item_ids() const noexcept -> const std::vector<std::string>&
    {
        RELAYCORE_PROFILE_FUNCTION();
        return inventory_item_ids_;
    }

    auto GameWorldRuntime::activated_target_ids() const noexcept -> const std::vector<std::string>&
    {
        RELAYCORE_PROFILE_FUNCTION();
        return activated_target_ids_;
    }

    auto GameWorldRuntime::signaled_target_ids() const noexcept -> const std::vector<std::string>&
    {
        RELAYCORE_PROFILE_FUNCTION();
        return signaled_target_ids_;
    }

    auto GameWorldRuntime::debug_apply_usable_interaction(const std::size_t usable_index)
        -> RelayCore::gameplay::InteractionEvent
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (usable_index >= usables_.size())
        {
            return {};
        }

        RelayCore::gameplay::RuntimeUsable& usable = usables_[usable_index];
        const RelayCore::gameplay::InteractionEvent event = RelayGame::apply_usable_interaction(
            usable,
            doors_,
            usables_,
            inventory_item_ids_,
            activated_target_ids_,
            signaled_target_ids_
        );
        remember_interaction_event(event);
        refresh_runtime_usables_and_focus();
        refresh_active_door_count();
        return event;
    }

    auto GameWorldRuntime::debug_apply_player_fire(const glm::vec3& origin, const glm::vec3& direction)
        -> RelayCore::gameplay::CombatFireResult
    {
        RELAYCORE_PROFILE_FUNCTION();
        const auto attack = fire_player_weapon(
            level_,
            doors_,
            origin,
            direction,
            combat_state_.player,
            combat_state_.turrets,
            enemies_
        );

        const auto& fire_result = attack.combat;
        if (fire_result.killed_turret)
        {
            apply_combat_target_event(fire_result.target_event);
            refresh_runtime_usables_and_focus();
            refresh_active_door_count();
        }
        return fire_result;
    }

    auto GameWorldRuntime::scientist_pose() const noexcept -> ScientistPose
    {
        return {
            .position = player_controller_.position(),
            .forward = player_controller_.view_forward(),
            .stride_phase = player_feedback_.phase(),
            .stride_amount = player_feedback_.stride(),
            .landing = player_feedback_.landing(),
            .visible = combat_state_.player.alive && player_controller_.movement_mode() != MovementMode::Noclip,
        };
    }

    auto GameWorldRuntime::debug_player_position() const noexcept -> glm::vec3
    {
        RELAYCORE_PROFILE_FUNCTION();
        return player_controller_.position();
    }

    auto GameWorldRuntime::debug_player_eye_position() const noexcept -> glm::vec3
    {
        RELAYCORE_PROFILE_FUNCTION();
        return player_controller_.eye_position_world();
    }

    void GameWorldRuntime::debug_set_player_position(const glm::vec3& position) noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        player_controller_.initialize(position);
        player_feedback_.reset();
        refresh_runtime_usables_and_focus();
    }

    auto GameWorldRuntime::debug_apply_combat_threats(
        const RelayCore::physics::Aabb& player_bounds,
        const glm::vec3& player_eye_position,
        const float dt
    ) -> RelayCore::gameplay::CombatThreatResult
    {
        RELAYCORE_PROFILE_FUNCTION();
        const RelayCore::gameplay::CombatThreatResult combat_result = RelayCore::gameplay::update_combat_threats(
            level_,
            doors_,
            player_bounds,
            player_eye_position,
            combat_state_.player,
            combat_state_.turrets,
            combat_state_.hazards,
            dt
        );
        combat_state_.active_hazard_contacts = combat_result.hazard_contacts;
        combat_state_.turret_shots_last_frame = combat_result.turret_shots;
        return combat_result;
    }

    auto GameWorldRuntime::debug_advance_player_respawn(const float dt) noexcept -> bool
    {
        RELAYCORE_PROFILE_FUNCTION();
        RelayCore::gameplay::tick_combat_state(dt, combat_state_.player, combat_state_.turrets);
        if (combat_state_.player.alive)
        {
            return false;
        }

        return advance_dead_player_state(dt);
    }

    void GameWorldRuntime::debug_tick_combat(const float dt) noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        RelayCore::gameplay::tick_combat_state(dt, combat_state_.player, combat_state_.turrets);
    }

    void GameWorldRuntime::debug_update_mission(const RelayCore::physics::Aabb& actor_bounds)
    {
        RELAYCORE_PROFILE_FUNCTION();
        mission_runtime_.update(actor_bounds, doors_, inventory_item_ids_, activated_target_ids_, signaled_target_ids_);
        refresh_runtime_usables_and_focus();
        refresh_active_door_count();
    }

    auto GameWorldRuntime::capture_runtime_snapshot() const -> RelayGame::RuntimeLevelSnapshot
    {
        RELAYCORE_PROFILE_FUNCTION();
        return RelayGame::capture_runtime_level_snapshot(
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

    auto GameWorldRuntime::restore_runtime_snapshot(const RelayGame::RuntimeLevelSnapshot& snapshot)
        -> RelayGame::SnapshotRestoreResult
    {
        RELAYCORE_PROFILE_FUNCTION();
        const RelayGame::SnapshotRestoreResult result = RelayGame::restore_runtime_level_snapshot(
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
            refresh_active_door_count();
            sync_door_audio_state();
            refresh_runtime_usables_and_focus();
            if (inspector_state_.pinned)
            {
                restore_pinned_inspector(InspectorRestoreState{
                    .runtime_object_id = inspector_state_.pinned_diagnostics.runtime_id,
                    .primary_source_reference =
                        RelayCore::debug::build_primary_inspector_source_reference(inspector_state_.pinned_diagnostics),
                });
            }
        }
        return result;
    }

    auto GameWorldRuntime::capture_saved_game(float played_seconds, bool flashlight) const -> SavedGame
    {
        SavedGame save{
            capture_runtime_snapshot(),
            player_controller_.capture_state(),
            fingerprint_level(level_),
            played_seconds,
            flashlight
        };
        for (const auto& enemy : enemies_)
            save.enemies.push_back(enemy.state);
        return save;
    }

    auto GameWorldRuntime::active_level_exit() const -> const LevelExit*
    {
        if (!combat_state_.player.alive)
            return nullptr;
        for (const auto& exit : exits_)
            if (RelayCore::physics::overlaps(player_controller_.player_bounds(), exit.bounds, 0)
                && (!exit.requires_completion || mission_runtime_.status().completed))
                return &exit;
        return nullptr;
    }

    void GameWorldRuntime::restore_saved_game(const SavedGame& save)
    {
        if (save.level_fingerprint != fingerprint_level(level_))
            throw std::runtime_error("Карта изменилась после сохранения. Начните новый эпизод.");
        if (save.enemies.size() != enemies_.size())
            throw std::runtime_error("Сохранение содержит другой состав противников.");
        for (std::size_t i = 0; i < enemies_.size(); ++i)
            if (save.enemies[i].entity_index != enemies_[i].state.entity_index
                || save.enemies[i].health > enemies_[i].max_health)
                throw std::runtime_error("Сохранение противника несовместимо с картой.");
        const auto result = restore_runtime_snapshot(save.world);
        if (!result.restored)
            throw std::runtime_error("Сохранение несовместимо с текущим уровнем: " + result.error);
        for (std::size_t i = 0; i < enemies_.size(); ++i)
            enemies_[i].state = save.enemies[i];
        player_controller_.restore_state(save.player);
        player_feedback_.reset();
        simulation_loop_.reset(save.world.simulation_tick);
        update_world_environment(environment_, activated_target_ids_, 0);
        update_world_environment(environment_, activated_target_ids_, 30);
        refresh_runtime_usables_and_focus();
    }

    auto GameWorldRuntime::capture_encounter_snapshot(float played_seconds, float scene_time, bool flashlight) const
        -> EncounterSnapshot
    {
        return {capture_saved_game(played_seconds, flashlight), scene_time, environment_};
    }

    void GameWorldRuntime::restore_encounter_snapshot(const EncounterSnapshot& snapshot)
    {
        if (snapshot.environment.size() != environment_.size())
            throw std::runtime_error("Encounter snapshot contains incompatible environment effects.");
        restore_saved_game(snapshot.game);
        environment_ = snapshot.environment;
        clear_pending_gameplay_actions();
        combat_state_.player.shot_trace = {};
        combat_state_.player.hit_confirmation_remaining_seconds = 0;
        for (auto& turret : combat_state_.turrets)
        {
            turret.shot_trace = {};
            turret.hit_flash_remaining_seconds = 0;
            turret.fire_flash_remaining_seconds = 0;
        }
        for (auto& usable : usables_)
        {
            usable.feedback_style = RelayCore::gameplay::UsableFeedbackStyle::None;
            usable.feedback_remaining_seconds = 0;
            usable.feedback_total_seconds = 0;
        }
    }

    void GameWorldRuntime::begin_ending()
    {
        if (ending_)
            doors_[ending_->closing_door].state = RelayCore::gameplay::DoorState::Closing;
    }

    void GameWorldRuntime::update_ending(float delta_seconds, RelayCore::EngineServices& services)
    {
        update_door_runtime(delta_seconds);
        play_door_state_change_audio(services);
        sync_door_audio_state();
        update_presentation(services, delta_seconds);
    }

    auto GameWorldRuntime::runtime_registry() const noexcept -> const RelayGame::RuntimeObjectRegistry&
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

    auto GameWorldRuntime::advance_dead_player_state(const float sim_delta_seconds) noexcept -> bool
    {
        RELAYCORE_PROFILE_FUNCTION();
        update_door_runtime(sim_delta_seconds);
        sync_door_audio_state();
        if (combat_state_.player.respawn_remaining_seconds > 0.0F)
        {
            return false;
        }

        respawn_player_at_spawn();
        return true;
    }

    void GameWorldRuntime::simulate_fixed_step(
        const RelayCore::FrameInfo& sim_frame,
        FrameGameplayActions& actions,
        RelayCore::EngineServices& services
    )
    {
        RELAYCORE_PROFILE_FUNCTION();
        const float sim_delta_seconds = static_cast<float>(sim_frame.delta_seconds);
        RelayCore::gameplay::tick_combat_state(sim_delta_seconds, combat_state_.player, combat_state_.turrets);

        if (!combat_state_.player.alive)
        {
            if (advance_dead_player_state(sim_delta_seconds))
            {
                play_gameplay_audio(services, audio_cues_.player_respawn);
                push_transient_hud_event(
                    "Вы у входа. Выполненные задачи и состояние врагов сохранены.",
                    RelayCore::render::HudEventStyle::Neutral,
                    3.0F
                );
            }
            return;
        }

        const RelayGame::MissionStatus mission_before = mission_runtime_.status();
        const bool mission_was_complete = mission_before.completed;
        RelayCore::gameplay::collect_blocking_doors(doors_, player_controller_.player_bounds(), dynamic_blockers_);
        for (const auto& enemy : enemies_)
            if (enemy.state.mode != EnemyMode::Dead)
                dynamic_blockers_.push_back(enemy_bounds(enemy.state));
        const glm::vec3 previous_position = player_controller_.position();
        const bool was_grounded = player_controller_.grounded();
        const float previous_vertical_speed = player_controller_.velocity().y;
        player_controller_.update(sim_frame, level_, dynamic_blockers_);
        const glm::vec3 displacement = player_controller_.position() - previous_position;
        const auto feedback = player_feedback_.update(
            sim_delta_seconds,
            displacement,
            previous_vertical_speed,
            was_grounded,
            player_controller_.grounded(),
            player_controller_.movement_mode() == MovementMode::Noclip
        );
        player_controller_.set_eye_offset(player_feedback_.eye_offset(camera_motion_));
        if (feedback.step || feedback.landed > 0)
        {
            const auto surface = static_cast<std::size_t>(footstep_surface(level_, player_controller_.position()));
            const auto sound = feedback.landed > 0 ? 12 + surface : surface * 4 + ((step_variant_++ * 3) % 4);
            services.audio_system().play_one_shot(player_sounds_[sound]);
        }
        refresh_runtime_usables_and_focus();

        const bool fire_pressed_this_step = actions.fire_pressed;
        const bool use_pressed_this_step = actions.use_pressed;
        actions.fire_pressed = false;
        actions.use_pressed = false;

        process_fire_input(fire_pressed_this_step, services);
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
        update_door_runtime(sim_delta_seconds);
        play_door_state_change_audio(services);
        sync_door_audio_state();

        const float health_before_enemies = combat_state_.player.health;
        const auto enemy_threats = update_enemies(
            level_,
            doors_,
            player_controller_.player_bounds(),
            player_controller_.eye_position_world(),
            combat_state_.player,
            enemies_,
            sim_delta_seconds,
            enemy_blockers_
        );
        if (enemy_threats.strikes > 0)
            play_gameplay_audio(services, audio_cues_.rover_attack);
        if (combat_state_.player.health < health_before_enemies)
            play_gameplay_audio(services, audio_cues_.player_hurt);
        const float player_health_before_threats = combat_state_.player.health;
        const bool hazard_contact_before_threats = combat_state_.hazard_contact_last_step;
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
        combat_state_.hazard_contact_last_step = combat_result.hazard_contacts > 0;
        combat_state_.turret_shots_last_frame += combat_result.turret_shots;
        if (combat_state_.player.alive && (enemy_threats.windups > 0 || combat_result.turret_windups > 0))
            play_gameplay_audio(services, audio_cues_.threat_windup);
        if (combat_result.turret_shots > 0)
        {
            play_gameplay_audio(services, audio_cues_.turret_fire);
        }
        if ((combat_state_.player.health + 0.001F) < player_health_before_threats
            && (combat_result.turret_shots > 0 || !hazard_contact_before_threats))
        {
            play_gameplay_audio(services, audio_cues_.player_hurt);
        }

        if (!combat_state_.player.alive)
        {
            services.audio_system().stop_one_shots();
            play_gameplay_audio(services, audio_cues_.player_hurt);
            push_transient_hud_event(
                "Вы погибли. Возврат ко входу; выполненные задачи сохраняются.",
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
        const RelayGame::MissionStatus mission_after = mission_runtime_.status();
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
        interaction_occluders_.resize(level_.solids.size() + level_.prop_colliders.size());
        for (const auto& door : doors_)
            if (RelayCore::gameplay::is_door_blocking(door))
                interaction_occluders_.push_back(RelayCore::gameplay::current_door_bounds(door));
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
            interaction_distance,
            0.75F,
            interaction_occluders_
        );
        interaction_state_.focused_usable_index = focus.usable_index;
        interaction_state_.focused_distance = focus.distance;
    }

} // namespace RelayGame

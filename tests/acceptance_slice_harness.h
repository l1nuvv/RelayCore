#include "relaygame/usable_interaction.h"
#pragma once

#include "relaycore/core/engine_config.h"
#include "relaycore/gameplay/combat_runtime.h"
#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/physics/collision.h"
#include "relaygame/level_session_builder.h"
#include "relaygame/mission_runtime.h"
#include "relaygame/runtime_model.h"

#include <algorithm>
#include <filesystem>
#include <utility>
#include <vector>

namespace AcceptanceHarness
{
    struct TurretKillSequence {
        RelayCore::gameplay::CombatFireResult first_shot{};
        RelayCore::gameplay::CombatFireResult second_shot{};
    };

    inline auto make_actor_bounds(const glm::vec3& center, const glm::vec3& half_extents) -> RelayCore::physics::Aabb
    {
        return {center - half_extents, center + half_extents};
    }

    inline auto bounds_center(const RelayCore::physics::Aabb& bounds) -> glm::vec3
    {
        return (bounds.mins + bounds.maxs) * 0.5F;
    }

    inline void apply_combat_target_event(
        const RelayCore::gameplay::CombatTargetEvent& event,
        std::vector<RelayCore::gameplay::RuntimeDoor>& doors,
        std::vector<RelayCore::gameplay::RuntimeUsable>& usables,
        std::vector<std::string>& activated_targets,
        std::vector<std::string>& signaled_targets
    )
    {
        using RelayCore::gameplay::CombatTargetActionKind;

        if (event.target.empty())
        {
            return;
        }

        switch (event.action)
        {
            case CombatTargetActionKind::ActivateTarget:
                if (RelayCore::gameplay::activate_target(event.target, doors))
                {
                    activated_targets.push_back(event.target);
                }
                break;
            case CombatTargetActionKind::SignalTarget:
                if (RelayCore::gameplay::signal_target(event.target, doors, usables))
                {
                    signaled_targets.push_back(event.target);
                }
                break;
            case CombatTargetActionKind::None:
            default:
                break;
        }
    }

    struct TestRoomScenario {
        RelayGame::LevelSession session{};
        std::vector<RelayCore::gameplay::RuntimeDoor> doors{};
        std::vector<RelayCore::gameplay::RuntimeTrigger> triggers{};
        std::vector<RelayCore::gameplay::RuntimeUsable> usables{};
        std::vector<RelayCore::gameplay::RuntimeTurret> turrets{};
        std::vector<std::string> inventory_items{};
        std::vector<std::string> activated_targets{};
        std::vector<std::string> signaled_targets{};
        RelayGame::MissionRuntime mission_runtime{};
        RelayCore::gameplay::PlayerCombatState player_combat{};
        RelayCore::physics::Aabb spawn_bounds{};
        RelayCore::physics::Aabb airlock_bounds{};
        RelayCore::physics::Aabb gallery_bounds{};

        [[nodiscard]] static auto build(const std::filesystem::path& level_path) -> TestRoomScenario
        {
            RelayCore::core::GameplayConfig config{};
            config.level_path = level_path.string();

            TestRoomScenario scenario{};
            scenario.session = RelayGame::LevelSessionBuilder{config}.build();
            scenario.doors = scenario.session.doors;
            scenario.triggers = scenario.session.triggers;
            scenario.usables = scenario.session.usables;
            scenario.turrets = scenario.session.combat.turrets;
            scenario.mission_runtime.reset(scenario.session.mission_definition);
            RelayCore::gameplay::reset_player_combat_state(scenario.player_combat);
            scenario.player_combat.weapon_range = 16.0F;
            scenario.player_combat.weapon_damage = 25.0F;
            scenario.spawn_bounds = make_actor_bounds({0.0F, 1.0F, -1.4F}, {0.3F, 0.9F, 0.3F});
            scenario.airlock_bounds = make_actor_bounds(
                bounds_center(scenario.session.mission_definition.steps[1].bounds),
                {0.3F, 0.9F, 0.3F}
            );
            scenario.gallery_bounds = make_actor_bounds(
                bounds_center(scenario.session.mission_definition.steps.back().bounds),
                {0.3F, 0.9F, 0.3F}
            );
            return scenario;
        }

        [[nodiscard]] auto find_outer_panel() -> RelayCore::gameplay::RuntimeUsable*
        {
            return find_usable([](const RelayCore::gameplay::RuntimeUsable& usable) {
                return usable.label == "Открыть внешний шлюз";
            });
        }

        [[nodiscard]] auto find_pickup() -> RelayCore::gameplay::RuntimeUsable*
        {
            return find_usable([](const RelayCore::gameplay::RuntimeUsable& usable) {
                return usable.action_kind == RelayCore::gameplay::UsableActionKind::PickupItem
                       && usable.item_id == "lab_keycard";
            });
        }

        [[nodiscard]] auto find_reader() -> RelayCore::gameplay::RuntimeUsable*
        {
            return find_usable([](const RelayCore::gameplay::RuntimeUsable& usable) {
                return usable.label == "Подтвердить допуск";
            });
        }

        [[nodiscard]] auto find_terminal() -> RelayCore::gameplay::RuntimeUsable*
        {
            return find_usable([](const RelayCore::gameplay::RuntimeUsable& usable) {
                return usable.name == "lab_console_lock";
            });
        }

        [[nodiscard]] auto find_outer_door() -> RelayCore::gameplay::RuntimeDoor*
        {
            return find_door([](const RelayCore::gameplay::RuntimeDoor& door) {
                return door.name == "intake/outer_bulkhead";
            });
        }

        [[nodiscard]] auto find_inner_door() -> RelayCore::gameplay::RuntimeDoor*
        {
            return find_door([](const RelayCore::gameplay::RuntimeDoor& door) {
                return door.name == "inner_bulkhead";
            });
        }

        [[nodiscard]] auto open_outer_bulkhead() -> RelayCore::gameplay::UseInteractionOutcome
        {
            return RelayGame::apply_usable_interaction(
                       *find_outer_panel(),
                       doors,
                       usables,
                       inventory_items,
                       activated_targets,
                       signaled_targets
            )
                .kind;
        }

        void update_mission_at_spawn()
        {
            mission_runtime.update(spawn_bounds, doors, inventory_items, activated_targets, signaled_targets);
        }

        void update_mission_at_airlock()
        {
            mission_runtime.update(airlock_bounds, doors, inventory_items, activated_targets, signaled_targets);
        }

        void update_mission_at_gallery()
        {
            mission_runtime.update(gallery_bounds, doors, inventory_items, activated_targets, signaled_targets);
        }

        void advance_to_airlock()
        {
            update_mission_at_spawn();
            update_mission_at_airlock();
        }

        [[nodiscard]] auto pick_up_lab_keycard() -> RelayCore::gameplay::UseInteractionOutcome
        {
            const auto outcome = RelayGame::apply_usable_interaction(
                *find_pickup(),
                doors,
                usables,
                inventory_items,
                activated_targets,
                signaled_targets
            );
            update_mission_at_spawn();
            return outcome.kind;
        }

        [[nodiscard]] auto authorize_inner_bulkhead() -> RelayCore::gameplay::UseInteractionOutcome
        {
            const auto outcome = RelayGame::apply_usable_interaction(
                *find_reader(),
                doors,
                usables,
                inventory_items,
                activated_targets,
                signaled_targets
            );
            update_mission_at_spawn();
            return outcome.kind;
        }

        [[nodiscard]] auto destroy_turret() -> TurretKillSequence
        {
            TurretKillSequence sequence{};
            const glm::vec3 shot_origin = turrets[0].origin + glm::vec3{-1.8F, 0.0F, -1.2F};
            const glm::vec3 shot_direction = turrets[0].origin - shot_origin;

            sequence.first_shot = RelayCore::gameplay::apply_player_fire(
                session.level,
                doors,
                shot_origin,
                shot_direction,
                player_combat,
                turrets
            );

            RelayCore::gameplay::tick_combat_state(0.30F, player_combat, turrets);

            sequence.second_shot = RelayCore::gameplay::apply_player_fire(
                session.level,
                doors,
                shot_origin,
                shot_direction,
                player_combat,
                turrets
            );
            apply_combat_target_event(
                sequence.second_shot.target_event,
                doors,
                usables,
                activated_targets,
                signaled_targets
            );
            return sequence;
        }

        void progress_to_turret_unlock_state()
        {
            (void) open_outer_bulkhead();
            advance_to_airlock();
            (void) pick_up_lab_keycard();
            (void) authorize_inner_bulkhead();
            (void) destroy_turret();
        }

        [[nodiscard]] auto activate_terminal() -> RelayCore::gameplay::UseInteractionOutcome
        {
            return RelayGame::apply_usable_interaction(
                       *find_terminal(),
                       doors,
                       usables,
                       inventory_items,
                       activated_targets,
                       signaled_targets
            )
                .kind;
        }

        void complete_acceptance_slice()
        {
            progress_to_turret_unlock_state();
            (void) activate_terminal();
            update_mission_at_spawn();
            update_mission_at_gallery();
        }

        [[nodiscard]] auto capture_snapshot(const std::uint64_t simulation_tick) const
            -> RelayGame::RuntimeLevelSnapshot
        {
            return RelayGame::capture_runtime_level_snapshot(
                session.level_path.string(),
                session.runtime_registry,
                doors,
                triggers,
                usables,
                turrets,
                inventory_items,
                activated_targets,
                signaled_targets,
                player_combat,
                mission_runtime,
                simulation_tick
            );
        }

        [[nodiscard]] auto restore_snapshot(const RelayGame::RuntimeLevelSnapshot& snapshot)
            -> RelayGame::SnapshotRestoreResult
        {
            return RelayGame::restore_runtime_level_snapshot(
                snapshot,
                session.level_path.string(),
                session.runtime_registry,
                doors,
                triggers,
                usables,
                turrets,
                inventory_items,
                activated_targets,
                signaled_targets,
                player_combat,
                mission_runtime
            );
        }

        void corrupt_runtime_state_after_snapshot()
        {
            find_outer_door()->open_fraction = 0.0F;
            find_outer_door()->state = RelayCore::gameplay::DoorState::Closed;
            find_inner_door()->open_fraction = 0.0F;
            find_inner_door()->state = RelayCore::gameplay::DoorState::Closed;
            find_pickup()->consumed = false;
            find_reader()->consumed = false;
            find_terminal()->locked = true;
            inventory_items.clear();
            activated_targets.clear();
            signaled_targets.clear();
            turrets[0].health = turrets[0].max_health;
            turrets[0].destroyed = false;
            turrets[0].warming_up = false;
            player_combat.health = 5.0F;
            player_combat.alive = false;
            player_combat.respawn_remaining_seconds = 0.5F;
        }

    private:
        template<typename Predicate>
        [[nodiscard]] auto find_usable(Predicate predicate) -> RelayCore::gameplay::RuntimeUsable*
        {
            const auto it = std::find_if(usables.begin(), usables.end(), predicate);
            return it == usables.end() ? nullptr : &(*it);
        }

        template<typename Predicate>
        [[nodiscard]] auto find_door(Predicate predicate) -> RelayCore::gameplay::RuntimeDoor*
        {
            const auto it = std::find_if(doors.begin(), doors.end(), predicate);
            return it == doors.end() ? nullptr : &(*it);
        }
    };
} // namespace AcceptanceHarness

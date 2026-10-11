#pragma once

#include "relaycore/core/paths.h"
#include "relaycore/physics/player_motion.h"
#include "relaygame/level_session_builder.h"
#include "relaygame/usable_interaction.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <stdexcept>

namespace RelayCore::test
{
    struct ContainmentLoopRoute {
        RelayGame::LevelSession session{RelayGame::LevelSessionBuilder{{}
        }.build(core::asset_path("levels/containment_loop.toml"))};
        RelayGame::MissionRuntime mission;
        physics::PlayerMotionState motion{.position = session.spawn_position, .grounded = true};
        physics::PlayerMotionConfig config;
        std::vector<std::string> inventory, activated, signaled;
        std::vector<physics::WorldCollider> doors;
        std::vector<physics::WorldCollider> enemy_blockers;
        gameplay::PlayerCombatState player;
        unsigned enemy_windups = 0, enemy_shots = 0;
        bool fight = true;

        ContainmentLoopRoute()
        {
            config.max_speed = 3;
            mission.reset(session.mission_definition);
            gameplay::reset_player_combat_state(player);
        }

        auto world() -> physics::WorldQuery
        {
            gameplay::collect_blocking_doors(session.doors, doors);
            if (session.containment)
                doors.push_back(session.containment->collider());
            for (const auto& enemy : session.enemies)
                if (enemy.state.mode != RelayGame::EnemyMode::Dead)
                    doors.push_back(
                        {.bounds = RelayGame::enemy_bounds(enemy.state), .object = {physics::WorldObjectKind::Enemy}}
                    );
            return {session.world_colliders, doors};
        }

        void tick(physics::MoveInput input = {}, float yaw = 0)
        {
            constexpr float dt = 1.0F / 120;
            gameplay::tick_combat_state(dt, player, {});
            physics::update_grounded(motion, input, yaw, dt, world(), config);
            const auto bounds = physics::make_player_aabb(motion.position, .3F, 1.8F);
            static_cast<void>(gameplay::update_doors(dt, bounds, session.doors));
            gameplay::collect_blocking_doors(session.doors, doors);
            const auto guardian_before =
                session.containment ? session.containment->collider() : physics::WorldCollider{};
            const physics::WorldQuery combat_world{
                session.world_colliders,
                doors,
                session.containment ? std::span<const physics::WorldCollider>{&guardian_before, 1}
                                    : std::span<const physics::WorldCollider>{}
            };
            const auto eye = motion.position + glm::vec3{0, 1.65F, 0};
            if (fight)
                for (const auto& enemy : session.enemies)
                {
                    if (enemy.state.mode == RelayGame::EnemyMode::Dead)
                        continue;
                    const auto aim = enemy.state.position + glm::vec3{0, .6F, 0};
                    const auto delta = aim - eye;
                    const auto distance = glm::length(delta);
                    if (distance > 16 || combat_world.raycast(eye, delta / distance, distance))
                        continue;
                    const auto weapon =
                        distance < 2.5F && std::ranges::find(inventory, "weapon_shotgun") != inventory.end()
                            ? RelayGame::PlayerWeapon::Shotgun
                            : RelayGame::PlayerWeapon::Precise;
                    static_cast<void>(
                        RelayGame::fire_player_weapon(combat_world, eye, delta, player, {}, session.enemies, weapon)
                    );
                    break;
                }
            if (session.containment)
            {
                gameplay::collect_blocking_doors(session.doors, doors);
                enemy_blockers.clear();
                enemy_blockers.push_back({.bounds = bounds, .object = {physics::WorldObjectKind::Player}});
                for (const auto& enemy : session.enemies)
                    if (enemy.state.mode != RelayGame::EnemyMode::Dead)
                        enemy_blockers.push_back(
                            {.bounds = RelayGame::enemy_bounds(enemy.state), .object = {physics::WorldObjectKind::Enemy}
                            }
                        );
                session.containment->update(
                    {session.world_colliders, doors},
                    bounds,
                    motion.position + glm::vec3{0, 1.65F, 0},
                    player,
                    dt,
                    session.doors,
                    activated,
                    enemy_blockers
                );
                session.containment->sync_usables(session.usables, activated);
                if (!player.alive)
                    throw std::runtime_error(
                        "Containment route player died at " + std::to_string(motion.position.x) + ","
                        + std::to_string(motion.position.z)
                        + " guardian=" + std::to_string(session.containment->state.position.x) + ","
                        + std::to_string(session.containment->state.position.z)
                    );
            }
            gameplay::collect_blocking_doors(session.doors, doors);
            const auto guardian = session.containment ? session.containment->collider() : physics::WorldCollider{};
            const auto threat = RelayGame::update_enemies(
                {session.world_colliders, doors},
                bounds,
                eye,
                player,
                session.enemies,
                dt,
                enemy_blockers,
                session.containment ? std::span<const physics::WorldCollider>{&guardian, 1}
                                    : std::span<const physics::WorldCollider>{}
            );
            enemy_windups += threat.windups;
            enemy_shots += threat.shots;
            if (!player.alive)
                throw std::runtime_error("Containment route died in ordinary combat");
            mission.update(bounds, session.doors, inventory, activated, signaled);
        }

        void walk(glm::vec3 destination, int settle_ticks = 60)
        {
            for (int i = 0; i < 6000; ++i)
            {
                auto delta = destination - motion.position;
                delta.y = 0;
                if (glm::length(delta) < .07F)
                {
                    motion.velocity.x = motion.velocity.z = 0;
                    for (int settle = 0; settle < settle_ticks; ++settle)
                        tick();
                    if (std::abs(motion.position.y - destination.y) > .06F || !motion.grounded)
                        throw std::runtime_error(
                            "Containment route floor mismatch at " + std::to_string(motion.position.y)
                        );
                    return;
                }
                tick({.forward_axis = 1}, std::atan2(delta.z, delta.x));
            }
            throw std::runtime_error(
                "Containment route blocked at " + std::to_string(motion.position.x) + ","
                + std::to_string(motion.position.y) + "," + std::to_string(motion.position.z)
            );
        }

        auto use(std::string_view name) -> gameplay::UseInteractionOutcome
        {
            const auto found = std::ranges::find(session.usables, name, &gameplay::RuntimeUsable::name);
            if (found == session.usables.end())
                throw std::runtime_error("Containment usable missing: " + std::string{name});
            const auto eye = motion.position + glm::vec3{0, 1.65F, 0};
            const auto focus = gameplay::find_focused_usable(
                eye,
                glm::normalize(found->origin - eye),
                session.usables,
                2.25F,
                .75F,
                world()
            );
            if (focus.usable_index != found - session.usables.begin())
                throw std::runtime_error("Containment usable unreachable: " + std::string{name});
            const auto event = RelayGame::apply_usable_interaction(
                *found,
                session.doors,
                session.usables,
                inventory,
                activated,
                signaled
            );
            for (int i = 0; i < 300; ++i)
                tick();
            return event.kind;
        }

        void lure_and_capture()
        {
            walk({-7, .01F, 24});
            walk({0, .01F, 23});
            walk({2.5F, .01F, 23});
            walk({2.5F, .241F, 24.5F});
            for (int i = 0; i < 1800 && session.containment->state.mode != RelayGame::GuardianMode::Windup; ++i)
                tick();
            if (session.containment->state.mode != RelayGame::GuardianMode::Windup)
                throw std::runtime_error("Guardian did not follow the grounded player into the trap");
            walk({2.5F, .01F, 23}, 0);
            walk({-7, .01F, 24.5F}, 0);
            walk({-7, .01F, 26}, 0);
            if (use("containment_fire") != gameplay::UseInteractionOutcome::ActivatedTarget)
                throw std::runtime_error("Containment recovery window was not reachable from the lure position");
            if (session.containment->state.mode != RelayGame::GuardianMode::Contained)
                throw std::runtime_error("Containment did not complete");
        }

        void observe_and_enter_service()
        {
            walk({0, .01F, 9});
            walk({-8, .01F, 9});
            walk({-8, .01F, 14});
            walk({-16, .01F, 14});
        }

        void archive()
        {
            walk({-15, .01F, 24});
            walk({-15, .01F, 28});
            walk({-26, .01F, 28});
            walk({-26, .01F, 51});
            walk({-25.2F, .01F, 51});
        }

        void west_stairs_and_bridge()
        {
            walk({-26, .01F, 29});
            walk({-24, .01F, 29});
            walk({-24, 4.01F, 40.5F});
            walk({-10, 4.01F, 40.5F});
            walk({10, 4.01F, 40.5F});
            walk({21.5F, 4.01F, 40.5F});
            walk({21.5F, 4.01F, 32});
        }

        void east_stairs(bool ascending)
        {
            if (ascending)
            {
                walk({-8, .01F, 20});
                walk({14, .01F, 20});
                walk({14, .01F, 13.5F});
                walk({18, .01F, 13.5F});
                walk({18, 4.01F, 25});
                walk({21.5F, 4.01F, 25});
                walk({21.5F, 4.01F, 32});
            } else
            {
                walk({21.5F, 4.01F, 25});
                walk({18, 4.01F, 25});
                walk({18, .01F, 13.5F});
                walk({14, .01F, 13.5F});
                walk({14, .01F, 20});
                walk({-8, .01F, 20});
            }
        }

        void east_archive_return()
        {
            walk({14.8F, .01F, 20});
            walk({14.8F, .01F, 50});
            walk({8, .01F, 50});
            walk({14.8F, .01F, 50});
            walk({20, .01F, 52});
            walk({20, .01F, 57});
            walk({-20, .01F, 57});
            walk({-20, .01F, 52});
            walk({-26, .01F, 52});
            walk({-25.2F, .01F, 51});
        }
    };
} // namespace RelayCore::test

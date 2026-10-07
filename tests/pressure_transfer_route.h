#pragma once

#include "relaycore/core/paths.h"
#include "relaygame/game_world_runtime.h"
#include "relaygame/usable_interaction.h"

#include <algorithm>
#include <glm/geometric.hpp>
#include <stdexcept>

namespace RelayCore::test
{
    enum class PressureTactic {
        Distance,
        Cover
    };

    struct PressureRoute {
        RelayGame::LevelSession session;
        RelayGame::MissionRuntime mission;
        RelayCore::gameplay::PlayerCombatState player;
        std::vector<std::string> inventory, activated, signaled;
        std::vector<RelayCore::physics::WorldCollider> enemy_scratch, doors, actors;
        std::uint64_t ticks = 0;
        glm::vec3 position;
        unsigned precise_shots = 0, shotgun_shots = 0;
        std::optional<PressureTactic> mixed_tactic;
        unsigned mixed_exposed_ticks = 0, mixed_shotgun_shots = 0;
        float mixed_rover_hit_range = 0, closest_mixed_rover_z = 37;
        RelayGame::PlayerWeapon weapon = RelayGame::PlayerWeapon::Precise;
        bool pistol_only = false;

        explicit PressureRoute(const RelayCore::core::GameplayConfig& config) :
            session(RelayGame::LevelSessionBuilder{config}.build(core::asset_path("levels/pressure_transfer.toml"))),
            position(session.spawn_position)
        {
            mission.reset(session.mission_definition);
        }

        auto world() -> physics::WorldQuery
        {
            gameplay::collect_blocking_doors(session.doors, doors);
            if (session.physics_fixture)
                return {*session.physics_fixture, doors};
            return {session.world_colliders, doors};
        }

        void tick(bool fire = true)
        {
            constexpr float dt = 1.0F / 120;
            ++ticks;
            const auto actor = RelayCore::physics::make_player_aabb(position, .3F, 1.8F);
            const auto eye = position + glm::vec3{0, 1.65F, 0};
            RelayCore::gameplay::tick_combat_state(dt, player, session.combat.turrets);
            static_cast<void>(RelayCore::gameplay::update_doors(dt, actor, session.doors));
            static_cast<void>(RelayCore::gameplay::update_combat_threats(
                world(),
                actor,
                eye,
                player,
                session.combat.turrets,
                session.combat.hazards,
                dt
            ));
            static_cast<void>(RelayGame::update_enemies(world(), actor, eye, player, session.enemies, dt, enemy_scratch)
            );
            if (mixed_tactic)
            {
                const auto& turret = session.combat.turrets[1];
                const auto delta = turret.origin - eye;
                if (!turret.destroyed && glm::length(delta) < turret.attack_range
                    && !world().raycast(eye, glm::normalize(delta), glm::length(delta)))
                    ++mixed_exposed_ticks;
                closest_mixed_rover_z = std::min(closest_mixed_rover_z, session.enemies[1].state.position.z);
            }
            if (fire)
            {
                glm::vec3 target{};
                float nearest = 16;
                bool nearest_enemy = false;
                if (mixed_tactic)
                {
                    if (session.enemies[1].state.mode != RelayGame::EnemyMode::Dead)
                    {
                        target = session.enemies[1].state.position + glm::vec3{0, .45F, 0};
                        nearest = glm::distance(eye, target);
                        nearest_enemy = true;
                        if (*mixed_tactic == PressureTactic::Cover && session.enemies[1].state.position.z > 32.8F)
                            nearest = 16;
                    } else if (!session.combat.turrets[1].destroyed)
                    {
                        target = session.combat.turrets[1].origin;
                        nearest = glm::distance(eye, target);
                    }
                } else
                {
                    for (const auto& enemy : session.enemies)
                        if (enemy.state.mode != RelayGame::EnemyMode::Dead)
                        {
                            const auto center = enemy.state.position + glm::vec3{0, .45F, 0};
                            const float distance = glm::distance(eye, center);
                            if (distance < nearest)
                            {
                                nearest = distance;
                                target = center;
                                nearest_enemy = true;
                            }
                        }
                    for (const auto& turret : session.combat.turrets)
                        if (!turret.destroyed && glm::distance(eye, turret.origin) < nearest)
                        {
                            nearest = glm::distance(eye, turret.origin);
                            target = turret.origin;
                            nearest_enemy = false;
                        }
                }
                if (nearest < 16 && (!nearest_enemy || nearest < 4 || mixed_tactic == PressureTactic::Distance))
                {
                    weapon = mixed_tactic ? (nearest_enemy && *mixed_tactic == PressureTactic::Cover
                                                 ? RelayGame::PlayerWeapon::Shotgun
                                                 : RelayGame::PlayerWeapon::Precise)
                                          : (nearest < 4 && !inventory.empty() ? RelayGame::PlayerWeapon::Shotgun
                                                                               : RelayGame::PlayerWeapon::Precise);
                    if (pistol_only)
                        weapon = RelayGame::PlayerWeapon::Precise;
                    const auto shot = RelayGame::fire_player_weapon(
                        world(),
                        eye,
                        glm::normalize(target - eye),
                        player,
                        session.combat.turrets,
                        session.enemies,
                        weapon
                    );
                    if (shot.combat.fired)
                    {
                        weapon == RelayGame::PlayerWeapon::Shotgun ? ++shotgun_shots : ++precise_shots;
                        if (mixed_tactic && weapon == RelayGame::PlayerWeapon::Shotgun)
                            ++mixed_shotgun_shots;
                        if (mixed_tactic && shot.enemy_index == 1 && mixed_rover_hit_range == 0)
                            mixed_rover_hit_range = nearest;
                    }
                }
            }
            mission.update(actor, session.doors, inventory, activated, signaled);
        }

        auto use(std::string_view name) -> RelayCore::gameplay::UseInteractionOutcome
        {
            const auto found = std::ranges::find(session.usables, name, &RelayCore::gameplay::RuntimeUsable::name);
            if (found == session.usables.end())
                throw std::runtime_error("Pressure transfer usable missing: " + std::string{name});
            const auto eye = position + glm::vec3{0, 1.65F, 0};
            const auto focus = RelayCore::gameplay::find_focused_usable(
                eye,
                glm::normalize(found->origin - eye),
                session.usables,
                2.25F,
                .75F,
                world()
            );
            if (focus.usable_index != found - session.usables.begin())
                throw std::runtime_error("Pressure transfer usable unreachable: " + std::string{name});
            const auto event = RelayGame::apply_usable_interaction(
                *found,
                session.doors,
                session.usables,
                inventory,
                activated,
                signaled
            );
            tick();
            return event.kind;
        }

        void walk_to(glm::vec3 destination)
        {
            for (int i = 0;
                 i < 4000
                 && glm::distance(glm::vec2{position.x, position.z}, glm::vec2{destination.x, destination.z}) > .06F;
                 ++i)
            {
                actors.clear();
                for (const auto& enemy : session.enemies)
                    if (enemy.state.mode != RelayGame::EnemyMode::Dead)
                        actors.push_back(
                            {.bounds = RelayGame::enemy_bounds(enemy.state), .object = {physics::WorldObjectKind::Enemy}
                            }
                        );
                auto delta = destination - position;
                delta.y = 0;
                const auto velocity = glm::normalize(delta) * 3.0F + glm::vec3{0, -1, 0};
                position = RelayCore::physics::move_and_collide(
                               world().with_actors(actors),
                               position,
                               velocity,
                               1.0F / 120,
                               {},
                               true
                )
                               .position;
                tick();
                if (!player.alive)
                    throw std::runtime_error("Pressure transfer route died");
            }
            if (glm::distance(glm::vec2{position.x, position.z}, glm::vec2{destination.x, destination.z}) >= .07F)
                throw std::runtime_error(
                    "Pressure transfer route blocked at " + std::to_string(position.x) + ","
                    + std::to_string(position.z)
                );
            if (!physics::probe_ground(world(), position, {}))
                throw std::runtime_error("Pressure transfer route lost its floor");
        }

        void wait(int count)
        {
            for (int i = 0; i < count; ++i)
                tick();
        }

        void reach_mixed()
        {
            walk_to({4, .01F, 5.3F});
            walk_to({2, .01F, 5.3F});
            static_cast<void>(use("transfer_shotgun"));
            walk_to({6.1F, .01F, 8.5F});
            static_cast<void>(use("service_reader"));
            walk_to({4, .01F, 8});
            wait(270);
            walk_to({4, .01F, 22});
            walk_to({10, .01F, 22});
            walk_to({10, .01F, 25});
            walk_to({18, .01F, 25});
            walk_to({18, .01F, 19.5F});
            walk_to({20, .01F, 19.5F});
            static_cast<void>(use("gallery_reader"));
            walk_to({18, .01F, 19.5F});
            walk_to({18, .01F, 22});
            walk_to({24, .01F, 22});
            walk_to({30, .01F, 22});
            walk_to({28.5F, .01F, 22});
            walk_to({28.5F, .01F, 28});
            walk_to({28, .01F, 29});
        }

        void fight_mixed(PressureTactic tactic)
        {
            mixed_tactic = tactic;
            if (tactic == PressureTactic::Cover)
                walk_to({28, .01F, 30.3F});
            wait(600);
            if (session.enemies[1].state.mode != RelayGame::EnemyMode::Dead)
                throw std::runtime_error("Pressure transfer rover did not reach the native opening approach");
            if (tactic == PressureTactic::Distance)
            {
                walk_to({34, .01F, 29});
                wait(180);
                walk_to({34, .01F, 40});
            } else
            {
                walk_to({25.8F, .01F, 30.3F});
                walk_to({25.8F, .01F, 38});
                walk_to({30, .01F, 38});
                wait(180);
                walk_to({33, .01F, 40});
            }
            if (!session.combat.turrets[1].destroyed)
                throw std::runtime_error("Pressure transfer tactic left the relay turret alive");
        }

        void power_transfer()
        {
            walk_to({38, .01F, 40});
            static_cast<void>(use("transfer_control"));
        }

        void leave()
        {
            walk_to({31, .01F, 42.5F});
            static_cast<void>(use("exit_reader"));
            walk_to({28, .01F, 42});
            wait(270);
            walk_to({28, .01F, 49});
        }

        auto save() const -> RelayGame::SavedGame
        {
            RelayGame::SavedGame result;
            result.world = RelayGame::capture_runtime_level_snapshot(
                session.level_path.string(),
                session.runtime_registry,
                session.doors,
                session.triggers,
                session.usables,
                session.combat.turrets,
                inventory,
                activated,
                signaled,
                player,
                mission,
                ticks
            );
            result.world.selected_weapon = weapon;
            result.player.position = position;
            result.player.grounded = true;
            result.player.view_angles = {1.5707963268F, 0};
            result.level_fingerprint = RelayGame::fingerprint_level(session.level);
            result.played_seconds = static_cast<float>(ticks) / 120;
            for (const auto& enemy : session.enemies)
                result.enemies.push_back(enemy.state);
            return result;
        }
    };
} // namespace RelayCore::test

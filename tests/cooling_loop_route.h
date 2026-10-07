#pragma once

#include "relaycore/core/paths.h"
#include "relaygame/game_world_runtime.h"
#include "relaygame/usable_interaction.h"

#include <algorithm>
#include <glm/geometric.hpp>
#include <stdexcept>

namespace RelayCore::test
{
    struct CoolingLoopRoute {
        RelayGame::LevelSession session;
        RelayGame::MissionRuntime mission;
        gameplay::PlayerCombatState player;
        std::vector<std::string> inventory, activated, signaled;
        std::vector<physics::WorldCollider> doors, actors, scratch;
        glm::vec3 position;
        std::uint64_t ticks = 0;
        unsigned precise_shots = 0, shotgun_shots = 0, exposed_ticks = 0;
        RelayGame::PlayerWeapon weapon = RelayGame::PlayerWeapon::Precise;
        bool close_rover = false;

        explicit CoolingLoopRoute(const core::GameplayConfig& config) :
            session(RelayGame::LevelSessionBuilder{config}.build(core::asset_path("levels/cooling_loop.toml"))),
            position(session.spawn_position)
        {
            mission.reset(session.mission_definition);
        }

        auto world() -> physics::WorldQuery
        {
            gameplay::collect_blocking_doors(session.doors, doors);
            return {session.world_colliders, doors};
        }

        void tick(bool fire = true)
        {
            constexpr float dt = 1.0F / 120;
            ++ticks;
            const auto bounds = physics::make_player_aabb(position, .3F, 1.8F);
            const auto eye = position + glm::vec3{0, 1.65F, 0};
            gameplay::tick_combat_state(dt, player, session.combat.turrets);
            static_cast<void>(gameplay::update_doors(dt, bounds, session.doors));
            static_cast<void>(gameplay::update_combat_threats(
                world(),
                bounds,
                eye,
                player,
                session.combat.turrets,
                session.combat.hazards,
                dt
            ));
            static_cast<void>(RelayGame::update_enemies(world(), bounds, eye, player, session.enemies, dt, scratch));
            glm::vec3 target{};
            float nearest = player.weapon_range;
            bool rover = false;
            const auto visible = [&](glm::vec3 point) {
                const auto delta = point - eye;
                return !world().raycast(eye, glm::normalize(delta), glm::length(delta));
            };
            for (const auto& turret : session.combat.turrets)
                if (!turret.destroyed && visible(turret.origin))
                {
                    if (glm::distance(eye, turret.origin) < turret.attack_range)
                        ++exposed_ticks;
                    if (glm::distance(eye, turret.origin) < nearest)
                    {
                        nearest = glm::distance(eye, turret.origin);
                        target = turret.origin;
                    }
                }
            for (const auto& enemy : session.enemies)
            {
                const auto center = enemy.state.position + glm::vec3{0, .45F, 0};
                if (enemy.state.mode != RelayGame::EnemyMode::Dead && glm::distance(eye, center) < nearest
                    && visible(center))
                {
                    nearest = glm::distance(eye, center);
                    target = center;
                    rover = true;
                }
            }
            const bool shotgun = std::ranges::find(inventory, "weapon_shotgun") != inventory.end();
            if (fire && nearest < player.weapon_range && (!rover || !close_rover || (shotgun && nearest < 4)))
            {
                weapon = rover && shotgun ? RelayGame::PlayerWeapon::Shotgun : RelayGame::PlayerWeapon::Precise;
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
                    weapon == RelayGame::PlayerWeapon::Shotgun ? ++shotgun_shots : ++precise_shots;
            }
            mission.update(bounds, session.doors, inventory, activated, signaled);
            if (!player.alive)
                throw std::runtime_error("Cooling loop route died");
        }

        void wait(int count)
        {
            for (int i = 0; i < count; ++i)
                tick();
        }

        void walk(glm::vec3 destination)
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
                position = physics::move_and_collide(
                               world().with_actors(actors),
                               position,
                               glm::normalize(delta) * 3.0F + glm::vec3{0, -1, 0},
                               1.0F / 120,
                               {},
                               true
                )
                               .position;
                tick();
            }
            if (glm::distance(glm::vec2{position.x, position.z}, glm::vec2{destination.x, destination.z}) >= .07F)
                throw std::runtime_error(
                    "Cooling loop blocked at " + std::to_string(position.x) + "," + std::to_string(position.z)
                );
            if (!physics::probe_ground(world(), position, {}))
                throw std::runtime_error("Cooling loop lost its floor");
        }

        auto use(std::string_view name) -> gameplay::UseInteractionOutcome
        {
            const auto found = std::ranges::find(session.usables, name, &gameplay::RuntimeUsable::name);
            if (found == session.usables.end())
                throw std::runtime_error("Cooling loop usable missing: " + std::string{name});
            const auto eye = position + glm::vec3{0, 1.65F, 0};
            const auto focus = gameplay::find_focused_usable(
                eye,
                glm::normalize(found->origin - eye),
                session.usables,
                2.25F,
                .75F,
                world()
            );
            if (focus.usable_index != found - session.usables.begin())
                throw std::runtime_error("Cooling loop usable unreachable: " + std::string{name});
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

        void reach_isolator(bool service)
        {
            close_rover = service;
            if (service)
            {
                walk({-4.5F, .01F, 2});
                static_cast<void>(use("cooling_shotgun"));
                walk({-3, .01F, 2});
                walk({-3, .01F, 6});
                walk({-4, .01F, 6});
                walk({-4, .01F, 18.8F});
            } else
            {
                walk({6, .01F, 3});
                walk({6, .01F, 7});
                wait(180);
                walk({6, .01F, 19.2F});
                walk({-4, .01F, 19.2F});
            }
            walk({-5.3F, .01F, 18.5F});
        }

        void isolate()
        {
            static_cast<void>(use("cooling_isolator"));
            wait(240);
        }

        void reach_control()
        {
            walk({-4, .01F, 19.2F});
            walk({0, .01F, 19.2F});
            walk({0, .01F, 23});
            walk({5, .01F, 23});
        }

        void leave()
        {
            walk({3, .01F, 24.5F});
            static_cast<void>(use("cooling_exit_reader"));
            walk({0, .01F, 24.5F});
            wait(240);
            walk({0, .01F, 28.5F});
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

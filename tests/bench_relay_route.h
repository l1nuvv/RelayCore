#pragma once

#include "relaycore/core/paths.h"
#include "relaygame/game_world_runtime.h"
#include "relaygame/usable_interaction.h"

#include <algorithm>
#include <glm/geometric.hpp>
#include <stdexcept>

namespace RelayCore::test
{
    enum class BenchRelayTactic {
        Distance,
        Cover
    };

    struct BenchRelayRoute {
        RelayGame::LevelSession session;
        RelayGame::MissionRuntime mission;
        gameplay::PlayerCombatState player;
        std::vector<std::string> inventory, activated, signaled;
        std::vector<physics::WorldCollider> doors, enemy_scratch;
        glm::vec3 position;
        std::uint64_t ticks = 0;
        unsigned precise_shots = 0, shotgun_shots = 0;
        unsigned turret_exposed_ticks = 0;
        float first_rover_hit_range = 0;
        float closest_rover_z = 100;
        RelayGame::PlayerWeapon weapon = RelayGame::PlayerWeapon::Precise;
        BenchRelayTactic tactic;

        explicit BenchRelayRoute(
            const core::GameplayConfig& config,
            BenchRelayTactic selected_tactic = BenchRelayTactic::Cover
        ) :
            session(RelayGame::LevelSessionBuilder{config}.build(core::asset_path("levels/bench_relay.toml"))),
            position(session.spawn_position), tactic(selected_tactic)
        {
            mission.reset(session.mission_definition);
        }

        auto world() -> physics::WorldQuery
        {
            doors.clear();
            for (std::size_t i = 0; i < session.doors.size(); ++i)
                if (gameplay::is_door_blocking(session.doors[i]))
                    doors.push_back(
                        {gameplay::current_door_bounds(session.doors[i]),
                         SurfaceMaterial::Metal,
                         {physics::WorldObjectKind::Door, i, session.doors[i].authored_entity_index}}
                    );
            return {*session.physics_fixture, doors};
        }

        void tick(bool fire = true)
        {
            constexpr float dt = 1.0F / 120;
            ++ticks;
            const auto actor = physics::make_player_aabb(position, .3F, 1.8F);
            const auto eye = position + glm::vec3{0, 1.65F, 0};
            gameplay::tick_combat_state(dt, player, session.combat.turrets);
            static_cast<void>(gameplay::update_doors(dt, actor, session.doors));
            const auto query = world();
            const auto& turret = session.combat.turrets.front();
            const auto to_turret = turret.origin - eye;
            if (!turret.destroyed && glm::length(to_turret) < turret.attack_range
                && !query.raycast(eye, glm::normalize(to_turret), glm::length(to_turret)))
                ++turret_exposed_ticks;
            static_cast<void>(gameplay::update_combat_threats(
                query,
                actor,
                eye,
                player,
                session.combat.turrets,
                session.combat.hazards,
                dt
            ));
            static_cast<void>(RelayGame::update_enemies(query, actor, eye, player, session.enemies, dt, enemy_scratch));
            auto& rover = session.enemies.front();
            closest_rover_z = std::min(closest_rover_z, rover.state.position.z);
            if (fire)
            {
                std::optional<glm::vec3> target;
                bool aim_at_rover = false;
                if (rover.state.mode != RelayGame::EnemyMode::Dead
                    && ((tactic == BenchRelayTactic::Distance && turret.destroyed)
                        || (tactic == BenchRelayTactic::Cover && rover.state.position.z < 6.1F
                            && glm::distance(eye, rover.state.position) < 4)))
                {
                    target = rover.state.position + glm::vec3{0, .45F, 0};
                    weapon = tactic == BenchRelayTactic::Cover ? RelayGame::PlayerWeapon::Shotgun
                                                               : RelayGame::PlayerWeapon::Precise;
                    aim_at_rover = true;
                } else if (!turret.destroyed
                           && (tactic == BenchRelayTactic::Distance || rover.state.mode == RelayGame::EnemyMode::Dead))
                {
                    target = session.combat.turrets.front().origin;
                    weapon = RelayGame::PlayerWeapon::Precise;
                }
                if (target)
                {
                    const auto shot = RelayGame::fire_player_weapon(
                        query,
                        eye,
                        glm::normalize(*target - eye),
                        player,
                        session.combat.turrets,
                        session.enemies,
                        weapon
                    );
                    if (shot.combat.fired)
                    {
                        weapon == RelayGame::PlayerWeapon::Precise ? ++precise_shots : ++shotgun_shots;
                        if (aim_at_rover && shot.enemy_index >= 0 && first_rover_hit_range == 0)
                            first_rover_hit_range = glm::distance(eye, *target);
                    }
                }
            }
            mission.update(actor, session.doors, inventory, activated, signaled);
            if (!player.alive)
                throw std::runtime_error("Bench relay scripted route died");
        }

        void wait(int count)
        {
            for (int i = 0; i < count; ++i)
                tick();
        }

        void walk(glm::vec3 destination)
        {
            for (int i = 0;
                 i < 2400
                 && glm::distance(glm::vec2{position.x, position.z}, glm::vec2{destination.x, destination.z}) > .05F;
                 ++i)
            {
                auto delta = destination - position;
                delta.y = 0;
                position = physics::move_and_collide(
                               world(),
                               position,
                               glm::normalize(delta) * 3.0F + glm::vec3{0, -1, 0},
                               1.0F / 120,
                               {},
                               true
                )
                               .position;
                tick();
            }
            if (glm::distance(glm::vec2{position.x, position.z}, glm::vec2{destination.x, destination.z}) > .06F)
                throw std::runtime_error(
                    "Bench relay route blocked at " + std::to_string(position.x) + "," + std::to_string(position.z)
                );
        }

        auto use(std::string_view name) -> gameplay::UseInteractionOutcome
        {
            const auto found = std::ranges::find(session.usables, name, &gameplay::RuntimeUsable::name);
            if (found == session.usables.end())
                throw std::runtime_error("Bench relay usable missing: " + std::string{name});
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
                throw std::runtime_error("Bench relay usable is unreachable: " + std::string{name});
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

        void open_bay()
        {
            walk({-2.5F, .01F, -2.3F});
            static_cast<void>(use("bay_shotgun"));
            walk({2.3F, .01F, -.4F});
            static_cast<void>(use("bay_reader"));
            walk({0, .01F, -.4F});
        }

        void enter_bay()
        {
            open_bay();
            wait(200);
            walk({0, .01F, 2.2F});
        }

        void fight()
        {
            if (tactic == BenchRelayTactic::Cover)
            {
                walk({-2.6F, .01F, 3.1F});
                wait(600);
                if (session.enemies.front().state.mode != RelayGame::EnemyMode::Dead)
                    throw std::runtime_error("Bench relay rover did not reach the cover approach");
                walk({-1.3F, .01F, 3.1F});
            }
            wait(360);
            if (player.kills != 2)
                throw std::runtime_error("Bench relay tactic left a threat alive");
        }

        void power_relay()
        {
            fight();
            walk({1.6F, .01F, 2.2F});
            walk({1.6F, .01F, 9.5F});
            walk({-2.6F, .01F, 11.2F});
            static_cast<void>(use("relay_control"));
        }

        void leave_bay()
        {
            walk({2.3F, .01F, 11.2F});
            static_cast<void>(use("exit_reader"));
            walk({0, .01F, 11.2F});
            wait(200);
            walk({0, .01F, 14});
        }

        void finish()
        {
            power_relay();
            leave_bay();
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

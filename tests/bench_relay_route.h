#pragma once

#include "relaycore/core/paths.h"
#include "relaygame/game_world_runtime.h"
#include "relaygame/usable_interaction.h"

#include <algorithm>
#include <glm/geometric.hpp>
#include <stdexcept>

namespace RelayCore::test
{
    struct BenchRelayRoute {
        RelayGame::LevelSession session;
        RelayGame::MissionRuntime mission;
        gameplay::PlayerCombatState player;
        std::vector<std::string> inventory, activated, signaled;
        std::vector<physics::WorldCollider> doors, enemy_scratch;
        glm::vec3 position;
        std::uint64_t ticks = 0;
        unsigned precise_shots = 0, shotgun_shots = 0;
        float closest_rover_z = 100;
        RelayGame::PlayerWeapon weapon = RelayGame::PlayerWeapon::Precise;

        explicit BenchRelayRoute(const core::GameplayConfig& config) :
            session(RelayGame::LevelSessionBuilder{config}.build(core::asset_path("levels/bench_relay.toml"))),
            position(session.spawn_position)
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
                if (rover.state.mode != RelayGame::EnemyMode::Dead && rover.state.position.z < 6.1F
                    && glm::distance(eye, rover.state.position) < 4)
                {
                    target = rover.state.position + glm::vec3{0, .45F, 0};
                    weapon = RelayGame::PlayerWeapon::Shotgun;
                } else if (!session.combat.turrets.front().destroyed)
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
                        weapon == RelayGame::PlayerWeapon::Precise ? ++precise_shots : ++shotgun_shots;
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

        void enter_bay()
        {
            walk({-2.5F, .01F, -2.3F});
            static_cast<void>(use("bay_shotgun"));
            walk({2.3F, .01F, -.4F});
            static_cast<void>(use("bay_reader"));
            walk({0, .01F, -.4F});
            wait(200);
            walk({0, .01F, 2.2F});
        }

        void finish()
        {
            wait(360);
            walk({1.6F, .01F, 2.2F});
            walk({1.6F, .01F, 9.5F});
            walk({-2.6F, .01F, 11.2F});
            static_cast<void>(use("relay_control"));
            walk({2.3F, .01F, 11.2F});
            static_cast<void>(use("exit_reader"));
            walk({0, .01F, 11.2F});
            wait(200);
            walk({0, .01F, 14});
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

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

        ContainmentLoopRoute()
        {
            config.max_speed = 3;
            mission.reset(session.mission_definition);
        }

        auto world() -> physics::WorldQuery
        {
            gameplay::collect_blocking_doors(session.doors, doors);
            return {session.world_colliders, doors};
        }

        void tick(physics::MoveInput input = {}, float yaw = 0)
        {
            constexpr float dt = 1.0F / 120;
            physics::update_grounded(motion, input, yaw, dt, world(), config);
            const auto bounds = physics::make_player_aabb(motion.position, .3F, 1.8F);
            static_cast<void>(gameplay::update_doors(dt, bounds, session.doors));
            mission.update(bounds, session.doors, inventory, activated, signaled);
        }

        void walk(glm::vec3 destination)
        {
            for (int i = 0; i < 6000; ++i)
            {
                auto delta = destination - motion.position;
                delta.y = 0;
                if (glm::length(delta) < .07F)
                {
                    motion.velocity.x = motion.velocity.z = 0;
                    for (int settle = 0; settle < 60; ++settle)
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

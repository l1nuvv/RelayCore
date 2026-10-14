#pragma once

#include "relaygame/enemy_runtime.h"

namespace RelayGame
{
    // Rebuild after replacing static collision or authored points; doors and actors stay live.
    void prepare_enemy_navigation(RelayCore::physics::WorldQuery world, std::span<RuntimeEnemy> enemies);

    [[nodiscard]] auto enemy_route_velocity(
        RelayCore::physics::WorldQuery world,
        RuntimeEnemy& enemy,
        const glm::vec3& goal,
        float target_eye_height,
        float dt
    ) -> glm::vec3;
} // namespace RelayGame

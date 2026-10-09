#pragma once

#include "relaygame/enemy_runtime.h"

namespace RelayGame
{
    [[nodiscard]] auto enemy_route_velocity(
        RelayCore::physics::WorldQuery world,
        RuntimeEnemy& enemy,
        const glm::vec3& goal,
        float target_eye_height,
        float dt
    ) -> glm::vec3;
} // namespace RelayGame

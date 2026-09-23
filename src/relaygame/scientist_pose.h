#pragma once
#include <glm/vec3.hpp>
namespace RelayGame
{
    struct ScientistPose {
        glm::vec3 position{};
        glm::vec3 forward{0.0F, 0.0F, 1.0F};
        float stride_phase = 0.0F;
        float stride_amount = 0.0F;
        float landing = 0.0F;
        bool visible = false;
    };
} // namespace RelayGame

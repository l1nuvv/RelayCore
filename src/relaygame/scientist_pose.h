#pragma once
#include <array>
#include <glm/mat4x4.hpp>
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
        glm::vec3 travel_direction{0.0F, 0.0F, 1.0F};
    };

    struct ScientistBodyTransforms {
        glm::mat4 coat{1.0F};
        std::array<glm::mat4, 2> thighs{}, shins{}, shoes{};
    };

    [[nodiscard]] auto build_scientist_body_transforms(const ScientistPose& pose) -> ScientistBodyTransforms;
} // namespace RelayGame

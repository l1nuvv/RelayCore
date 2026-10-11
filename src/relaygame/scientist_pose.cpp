#include "relaygame/scientist_pose.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace RelayGame
{
    auto build_scientist_body_transforms(const ScientistPose& pose) -> ScientistBodyTransforms
    {
        ScientistBodyTransforms result;
        const float yaw = std::atan2(pose.forward.x, pose.forward.z);
        auto body = glm::translate(glm::mat4{1}, pose.position);
        body = glm::rotate(body, yaw, glm::vec3{0, 1, 0});
        body = glm::translate(body, glm::vec3{0, 0, -.22F});
        const glm::vec3 forward{std::sin(yaw), 0, std::cos(yaw)};
        const glm::vec3 right{forward.z, 0, -forward.x};
        const glm::vec2 travel{glm::dot(pose.travel_direction, right), glm::dot(pose.travel_direction, forward)};
        const float settle = pose.landing * .055F;
        result.coat = glm::translate(body, glm::vec3{0, -settle, 0});
        for (std::size_t leg = 0; leg < 2; ++leg)
        {
            const float side = leg == 0 ? -1.F : 1.F;
            const float phase = pose.stride_phase + (leg == 0 ? 3.14159265F : 0);
            const float step = -std::cos(phase) * pose.stride_amount;
            const float lift = std::max(std::sin(phase), 0.F) * pose.stride_amount;
            const glm::vec3 hip{side * .125F, .86F - settle - .024F * pose.stride_amount, -.055F};
            const glm::vec3 ankle{
                side * .125F + travel.x * step * .032F,
                .10F + lift * .065F,
                .025F + travel.y * step * .17F
            };
            const glm::vec3 axis = glm::normalize(ankle - hip);
            const float distance = glm::length(ankle - hip);
            constexpr float thigh_length = .40F, shin_length = .38F;
            const float along =
                (thigh_length * thigh_length - shin_length * shin_length + distance * distance) / (2 * distance);
            const float bend = std::sqrt(std::max(thigh_length * thigh_length - along * along, 0.F));
            // Keep the knee facing forward even while stepping sideways or backwards.
            const glm::vec3 pole = glm::normalize(glm::vec3{0, 0, 1} - axis * axis.z);
            const glm::vec3 knee = hip + axis * along + pole * bend;
            const auto bone = [&](glm::vec3 origin, glm::vec3 end) {
                const glm::vec3 y = glm::normalize(origin - end);
                const glm::vec3 x = glm::normalize(glm::cross(y, glm::vec3{0, 0, 1}));
                return body
                       * glm::mat4{
                           glm::vec4{x, 0},
                           glm::vec4{y, 0},
                           glm::vec4{glm::cross(x, y), 0},
                           glm::vec4{origin, 1}
                       };
            };
            result.thighs[leg] = bone(hip, knee);
            result.shins[leg] = bone(knee, ankle);
            result.shoes[leg] = glm::translate(body, ankle);
        }
        return result;
    }
} // namespace RelayGame

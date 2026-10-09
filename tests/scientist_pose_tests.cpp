#include "relaygame/scientist_pose.h"

#include <algorithm>
#include <cmath>
#include <doctest/doctest.h>
#include <glm/geometric.hpp>

TEST_CASE("player body keeps knees connected and support soles grounded throughout stride and landing")
{
    for (const glm::vec3 travel : {glm::vec3{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}})
        for (const float landing : {0.F, .5F, 1.F})
            for (const float stride : {0.F, .5F, 1.F})
                for (int phase = 0; phase < 32; ++phase)
                {
                    const RelayGame::ScientistPose pose{
                        .position = {2, 3, 4},
                        .stride_phase = phase * 6.2831853F / 32,
                        .stride_amount = stride,
                        .landing = landing,
                        .travel_direction = travel
                    };
                    const auto body = RelayGame::build_scientist_body_transforms(pose);
                    float lowest = 100;
                    for (std::size_t leg = 0; leg < 2; ++leg)
                    {
                        const glm::vec3 knee = body.thighs[leg] * glm::vec4{0, -.40F, 0, 1};
                        CHECK(glm::distance(knee, glm::vec3{body.shins[leg][3]}) < .00001F);
                        const glm::vec3 ankle = body.shins[leg] * glm::vec4{0, -.38F, 0, 1};
                        CHECK(glm::distance(ankle, glm::vec3{body.shoes[leg][3]}) < .00001F);
                        const glm::vec3 sole = body.shoes[leg] * glm::vec4{0, -.10F, 0, 1};
                        CHECK(sole.y >= pose.position.y - .00001F);
                        lowest = std::min(lowest, sole.y);
                        CHECK(doctest::Approx(glm::length(glm::vec3{body.thighs[leg][1]})) == 1);
                    }
                    CHECK(lowest == doctest::Approx(pose.position.y));
                    CHECK(body.shoes[1][3].x - body.shoes[0][3].x > .18F);
                }
}

TEST_CASE("player body turns about the feet without inheriting camera pitch")
{
    RelayGame::ScientistPose pose{.stride_phase = 1.2F, .stride_amount = 1, .landing = .5F};
    const auto original = RelayGame::build_scientist_body_transforms(pose);
    pose.forward = {1, -.99F, 0};
    pose.travel_direction = {1, 0, 0};
    const auto turned = RelayGame::build_scientist_body_transforms(pose);
    const auto rotate = [](glm::vec3 point) {
        return glm::vec3{point.z, point.y, -point.x};
    };
    for (std::size_t leg = 0; leg < 2; ++leg)
    {
        CHECK(glm::distance(rotate(glm::vec3{original.shoes[leg][3]}), glm::vec3{turned.shoes[leg][3]}) < .00001F);
        CHECK(glm::distance(rotate(glm::vec3{original.shins[leg][3]}), glm::vec3{turned.shins[leg][3]}) < .00001F);
    }
}

#include "relaycore/render/light_tiles.h"

#include <doctest/doctest.h>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>

TEST_CASE("light tiles conservatively contain surface and volume projections")
{
    using namespace RelayCore::render;
    const auto projection = glm::perspective(glm::radians(70.0F), 16.0F / 9, .05F, 100.0F);
    const std::array spheres{glm::vec4{1, .5F, -6, 1.5F}, glm::vec4{-2, -1, -9, 2}};
    const auto masks = light_tile_masks(projection, spheres);
    for (uint32_t light = 0; light < spheres.size(); ++light)
        for (int z = -8; z <= 8; ++z)
            for (int y = -8; y <= 8; ++y)
                for (int x = -8; x <= 8; ++x)
                {
                    const glm::vec3 offset = glm::vec3{x, y, z} / 8.0F;
                    if (glm::dot(offset, offset) > 1)
                        continue;
                    const auto clip = projection * glm::vec4{glm::vec3{spheres[light]} + offset * spheres[light].w, 1};
                    const auto uv = glm::vec2{clip} / clip.w * glm::vec2{.5F, -.5F} + .5F;
                    if (uv.x < 0 || uv.y < 0 || uv.x >= 1 || uv.y >= 1)
                        continue;
                    const auto tile =
                        uint32_t(uv.y * light_tile_rows) * light_tile_columns + uint32_t(uv.x * light_tile_columns);
                    CHECK((masks[tile / 4][tile % 4] & (uint32_t{1} << light)) != 0);
                }
    CHECK(masks.front().x == 0);
}

TEST_CASE("light tiles retain camera enclosing and near plane spheres in every tile")
{
    using namespace RelayCore::render;
    const std::array spheres{glm::vec4{0, 0, 0, 1}, glm::vec4{0, 0, -.1F, .1F}};
    const auto masks = light_tile_masks(glm::perspective(glm::radians(70.0F), 1.0F, .05F, 100.0F), spheres);
    for (const auto& mask : masks)
        CHECK(mask == glm::uvec4{3});
    CHECK_THROWS_AS(
        (static_cast<void>(light_tile_masks(glm::mat4{1}, std::array<glm::vec4, 33>{}))),
        std::invalid_argument
    );
}

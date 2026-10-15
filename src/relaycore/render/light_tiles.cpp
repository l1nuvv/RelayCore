#include "relaycore/render/light_tiles.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace RelayCore::render
{
    auto light_tile_masks(const glm::mat4& view_projection, const std::span<const glm::vec4> spheres) -> LightTileMasks
    {
        if (spheres.size() > 32)
            throw std::invalid_argument("Light tiles support at most 32 selected lights");
        LightTileMasks masks{};
        for (uint32_t light = 0; light < spheres.size(); ++light)
        {
            const auto sphere = spheres[light];
            glm::vec2 lower{1}, upper{0};
            bool full = false;
            // A projected enclosing box bounds the complete sphere, including every volume sample.
            for (uint32_t corner = 0; corner < 8; ++corner)
            {
                const glm::vec3 sign{corner & 1 ? 1 : -1, corner & 2 ? 1 : -1, corner & 4 ? 1 : -1};
                const auto clip =
                    view_projection * glm::vec4{glm::vec3(sphere) + sign * (std::max(sphere.w, .001F) + .001F), 1};
                if (!std::isfinite(clip.w) || clip.w <= .05F)
                {
                    full = true;
                    break;
                }
                const auto uv = glm::vec2{clip} / clip.w * glm::vec2{.5F, -.5F} + .5F;
                lower = glm::min(lower, uv);
                upper = glm::max(upper, uv);
            }
            if (full)
            {
                lower = glm::vec2{0};
                upper = glm::vec2{1};
            } else if (upper.x < 0 || upper.y < 0 || lower.x > 1 || lower.y > 1)
                continue;
            const auto first = glm::ivec2{
                glm::floor(glm::clamp(lower - .00001F, 0.0F, 1.0F) * glm::vec2{light_tile_columns, light_tile_rows})
            };
            const auto last = glm::ivec2{
                glm::floor(glm::clamp(upper + .00001F, 0.0F, 1.0F) * glm::vec2{light_tile_columns, light_tile_rows})
            };
            for (int y = std::min(first.y, int(light_tile_rows) - 1); y <= std::min(last.y, int(light_tile_rows) - 1);
                 ++y)
                for (int x = std::min(first.x, int(light_tile_columns) - 1);
                     x <= std::min(last.x, int(light_tile_columns) - 1);
                     ++x)
                {
                    const auto tile = uint32_t(y) * light_tile_columns + uint32_t(x);
                    masks[tile / 4][tile % 4] |= uint32_t{1} << light;
                }
        }
        return masks;
    }
} // namespace RelayCore::render

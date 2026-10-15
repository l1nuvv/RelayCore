#pragma once

#include <array>
#include <cstdint>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <span>

namespace RelayCore::render
{
    inline constexpr uint32_t light_tile_columns = 64, light_tile_rows = 36;
    using LightTileMasks = std::array<glm::uvec4, light_tile_columns * light_tile_rows / 4>;

    [[nodiscard]] auto light_tile_masks(const glm::mat4& view_projection, std::span<const glm::vec4> spheres)
        -> LightTileMasks;
} // namespace RelayCore::render

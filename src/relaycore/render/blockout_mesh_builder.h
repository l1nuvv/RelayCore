#pragma once

#include "relaycore/render/mesh.h"
#include "relaycore/world/blockout_level.h"

#include <cstdint>
#include <glm/vec3.hpp>
#include <vector>

namespace RelayCore::render
{
    struct BlockoutRenderChunkData {
        CpuColorMeshData mesh{};
        glm::vec3 mins{};
        glm::vec3 maxs{};
        std::uint32_t triangle_count = 0;
        bool transparent = false;
    };

    struct BlockoutRenderData {
        std::vector<BlockoutRenderChunkData> chunks;
        std::uint32_t total_triangles = 0;
    };

    [[nodiscard]] auto build_blockout_mesh(const BlockoutLevel& level) -> CpuColorMeshData;

    [[nodiscard]] auto build_blockout_render_data(const BlockoutLevel& level) -> BlockoutRenderData;
} // namespace RelayCore::render

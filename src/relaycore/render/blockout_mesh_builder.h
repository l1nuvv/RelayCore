#pragma once

#include "relaycore/render/mesh.h"
#include "relaycore/world/blockout_level.h"

namespace RelayCore::render
{
    [[nodiscard]] auto build_blockout_mesh(const BlockoutLevel& level) -> CpuColorMeshData;
}

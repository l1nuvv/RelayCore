#pragma once

#include <cstdint>
#include <glm/vec4.hpp>
#include <vector>

namespace RelayCore::render
{
    struct CpuColorMeshData;
    inline constexpr uint32_t kMeshletVertices = 64, kMeshletTriangles = 124;
    struct Meshlet {
        glm::uvec4 range{}; // vertex offset, triangle offset, vertex count, triangle count
        glm::vec4 mins{}, maxs{};
    };
    struct MeshletData {
        std::vector<Meshlet> clusters;
        std::vector<uint32_t> vertices, triangles;
    };
    // Retains the exact original triangle order and winding, including equal-depth surfaces.
    [[nodiscard]] auto build_meshlets(const CpuColorMeshData& mesh) -> MeshletData;
    void validate_meshlets(const CpuColorMeshData& mesh, const MeshletData& data);
} // namespace RelayCore::render

#include "relaycore/render/meshlets.h"

#include "relaycore/render/mesh.h"

#include <array>
#include <cmath>
#include <glm/common.hpp>
#include <limits>
#include <stdexcept>

namespace RelayCore::render
{
    static_assert(sizeof(Meshlet) == 48);
    namespace
    {
        void require_meshlet(bool condition, const char* message)
        {
            if (!condition)
                throw std::runtime_error(message);
        }
    } // namespace
    auto build_meshlets(const CpuColorMeshData& mesh) -> MeshletData
    {
        require_meshlet(
            !mesh.vertices.empty() && !mesh.indices.empty() && mesh.indices.size() % 3 == 0,
            "Meshlets require nonempty triangle-list geometry"
        );
        require_meshlet(
            mesh.indices.size() <= UINT32_MAX && mesh.vertices.size() <= UINT32_MAX,
            "Meshlets exceed 32-bit geometry limits"
        );
        MeshletData result;
        std::vector<uint32_t> local(mesh.vertices.size(), UINT32_MAX);
        Meshlet cluster;
        const auto flush = [&] {
            if (!cluster.range.w)
                return;
            result.clusters.push_back(cluster);
            for (uint32_t i = 0; i < cluster.range.z; ++i)
                local[result.vertices[cluster.range.x + i]] = UINT32_MAX;
            cluster = {};
            cluster.range.x = uint32_t(result.vertices.size());
            cluster.range.y = uint32_t(result.triangles.size());
        };
        for (size_t offset = 0; offset < mesh.indices.size(); offset += 3)
        {
            std::array<uint32_t, 3> triangle{mesh.indices[offset], mesh.indices[offset + 1], mesh.indices[offset + 2]};
            uint32_t added = 0;
            for (uint32_t i = 0; i < 3; ++i)
            {
                require_meshlet(triangle[i] < mesh.vertices.size(), "Meshlet index is outside its vertex buffer");
                bool duplicate = false;
                for (uint32_t j = 0; j < i; ++j)
                    duplicate |= triangle[j] == triangle[i];
                added += local[triangle[i]] == UINT32_MAX && !duplicate ? 1 : 0;
            }
            if (cluster.range.z + added > kMeshletVertices || cluster.range.w == kMeshletTriangles)
                flush();
            uint32_t packed = 0;
            for (uint32_t i = 0; i < 3; ++i)
            {
                const auto index = triangle[i];
                if (local[index] == UINT32_MAX)
                {
                    local[index] = cluster.range.z++;
                    const auto p = mesh.vertices[index].position;
                    require_meshlet(
                        std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z),
                        "Meshlet bounds require finite positions"
                    );
                    if (cluster.range.z == 1)
                        cluster.mins = cluster.maxs = glm::vec4{p, 0};
                    else
                    {
                        cluster.mins = glm::min(cluster.mins, glm::vec4{p, 0});
                        cluster.maxs = glm::max(cluster.maxs, glm::vec4{p, 0});
                    }
                    result.vertices.push_back(index);
                }
                packed |= local[index] << (i * 8);
            }
            result.triangles.push_back(packed);
            ++cluster.range.w;
        }
        flush();
        return result;
    }
    void validate_meshlets(const CpuColorMeshData& mesh, const MeshletData& data)
    {
        require_meshlet(
            !data.clusters.empty() && data.triangles.size() * 3 == mesh.indices.size(),
            "Invalid meshlet triangle coverage"
        );
        size_t vertex_offset = 0, triangle_offset = 0;
        for (const auto& cluster : data.clusters)
        {
            const auto range = cluster.range;
            require_meshlet(
                range.x == vertex_offset && range.y == triangle_offset && range.z > 0 && range.z <= kMeshletVertices
                    && range.w > 0 && range.w <= kMeshletTriangles && vertex_offset <= data.vertices.size()
                    && range.z <= data.vertices.size() - vertex_offset && triangle_offset <= data.triangles.size()
                    && range.w <= data.triangles.size() - triangle_offset,
                "Invalid meshlet range/capacity"
            );
            glm::vec3 mins{std::numeric_limits<float>::max()}, maxs{-std::numeric_limits<float>::max()};
            for (uint32_t i = 0; i < range.z; ++i)
            {
                const auto index = data.vertices[range.x + i];
                require_meshlet(index < mesh.vertices.size(), "Meshlet vertex reference is outside its vertex buffer");
                const auto p = mesh.vertices[index].position;
                require_meshlet(
                    std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z),
                    "Meshlet bounds require finite positions"
                );
                mins = glm::min(mins, p);
                maxs = glm::max(maxs, p);
            }
            require_meshlet(
                cluster.mins == glm::vec4{mins, 0} && cluster.maxs == glm::vec4{maxs, 0},
                "Invalid meshlet bounds"
            );
            for (uint32_t i = 0; i < range.w; ++i)
            {
                const auto triangle = data.triangles[range.y + i];
                require_meshlet((triangle >> 24) == 0, "Invalid packed meshlet triangle");
                for (uint32_t corner = 0; corner < 3; ++corner)
                {
                    const auto local = (triangle >> (corner * 8)) & 255;
                    require_meshlet(local < range.z, "Meshlet triangle references an invalid local vertex");
                    require_meshlet(
                        data.vertices[range.x + local] == mesh.indices[(range.y + i) * 3 + corner],
                        "Meshlets changed triangle order/winding"
                    );
                }
            }
            vertex_offset += range.z;
            triangle_offset += range.w;
        }
        require_meshlet(
            vertex_offset == data.vertices.size() && triangle_offset == data.triangles.size(),
            "Unexpected meshlet payload after final range"
        );
    }
} // namespace RelayCore::render

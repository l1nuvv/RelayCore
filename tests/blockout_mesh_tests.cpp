#include <doctest/doctest.h>

#include "relaycore/render/blockout_mesh_builder.h"

TEST_CASE("single blockout solid emits six quads")
{
    RelayCore::BlockoutLevel level{};
    level.solids.push_back({{-1.0F, 0.0F, -1.0F}, {1.0F, 2.0F, 1.0F}, RelayCore::SurfaceMaterial::Concrete});

    const RelayCore::render::CpuColorMeshData mesh = RelayCore::render::build_blockout_mesh(level);

    CHECK(mesh.vertices.size() == 24);
    CHECK(mesh.indices.size() == 36);
}

TEST_CASE("adjacent solids cull their shared face")
{
    RelayCore::BlockoutLevel level{};
    level.solids.push_back({{-1.0F, 0.0F, -1.0F}, {0.0F, 1.0F, 1.0F}, RelayCore::SurfaceMaterial::Concrete});
    level.solids.push_back({{0.0F, 0.0F, -1.0F}, {1.0F, 1.0F, 1.0F}, RelayCore::SurfaceMaterial::Concrete});

    const RelayCore::render::CpuColorMeshData mesh = RelayCore::render::build_blockout_mesh(level);

    CHECK(mesh.vertices.size() == 40);
    CHECK(mesh.indices.size() == 60);
}

TEST_CASE("partially covered face only emits visible remainder")
{
    RelayCore::BlockoutLevel level{};
    level.solids.push_back({{0.0F, 0.0F, 0.0F}, {1.0F, 2.0F, 1.0F}, RelayCore::SurfaceMaterial::Concrete});
    level.solids.push_back({{1.0F, 0.0F, 0.0F}, {2.0F, 1.0F, 1.0F}, RelayCore::SurfaceMaterial::Concrete});

    const RelayCore::render::CpuColorMeshData mesh = RelayCore::render::build_blockout_mesh(level);

    CHECK(mesh.vertices.size() == 44);
    CHECK(mesh.indices.size() == 66);
}

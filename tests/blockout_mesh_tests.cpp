#include "relaycore/render/blockout_mesh_builder.h"

#include <doctest/doctest.h>
#include <glm/vec3.hpp>

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

TEST_CASE("blockout solid tint and brightness affect emitted vertex colors")
{
    RelayCore::BlockoutLevel level{};

    RelayCore::BlockoutSolid solid{};
    solid.mins = {-1.0F, 0.0F, -1.0F};
    solid.maxs = {1.0F, 2.0F, 1.0F};
    solid.material = RelayCore::SurfaceMaterial::Wall;
    solid.tint = {0.5F, 1.0F, 0.5F};
    solid.brightness = 1.4F;
    level.solids.push_back(solid);

    const RelayCore::render::CpuColorMeshData mesh = RelayCore::render::build_blockout_mesh(level);

    REQUIRE_FALSE(mesh.vertices.empty());
    CHECK(mesh.vertices.front().color.x == doctest::Approx(0.301F).epsilon(0.01));
    CHECK(mesh.vertices.front().color.y == doctest::Approx(0.644F).epsilon(0.01));
    CHECK(mesh.vertices.front().color.z == doctest::Approx(0.350F).epsilon(0.01));
}

TEST_CASE("blockout mesh emits outward face normals")
{
    RelayCore::BlockoutLevel level{};
    level.solids.push_back({{-1.0F, 0.0F, -1.0F}, {1.0F, 2.0F, 1.0F}, RelayCore::SurfaceMaterial::Concrete});

    const RelayCore::render::CpuColorMeshData mesh = RelayCore::render::build_blockout_mesh(level);

    REQUIRE(mesh.vertices.size() >= 24);
    CHECK(mesh.vertices[0].normal.x == doctest::Approx(0.0F));
    CHECK(mesh.vertices[0].normal.y == doctest::Approx(0.0F));
    CHECK(mesh.vertices[0].normal.z == doctest::Approx(1.0F));
    CHECK(mesh.vertices[20].normal.y == doctest::Approx(-1.0F));
}

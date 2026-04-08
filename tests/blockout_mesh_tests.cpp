#include "relaycore/render/blockout_mesh_builder.h"

#include <algorithm>
#include <cmath>
#include <doctest/doctest.h>
#include <glm/vec3.hpp>

namespace
{
    [[nodiscard]] auto material_id(const RelayCore::SurfaceMaterial material) -> float
    {
        return static_cast<float>(static_cast<std::uint8_t>(material));
    }
} // namespace

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

TEST_CASE("facility materials emit beveled blockout geometry")
{
    RelayCore::BlockoutLevel level{};
    level.solids.push_back({{-1.0F, 0.0F, -1.0F}, {1.0F, 2.0F, 1.0F}, RelayCore::SurfaceMaterial::WallPanel});

    const RelayCore::render::CpuColorMeshData mesh = RelayCore::render::build_blockout_mesh(level);

    CHECK(mesh.vertices.size() > 24);
    CHECK(mesh.indices.size() > 36);
}

TEST_CASE("coplanar decorative overlay claims the visible front layer without z-fighting")
{
    RelayCore::BlockoutLevel level{};
    level.solids.push_back({{1.0F, 1.0F, 0.8F}, {3.0F, 2.0F, 1.0F}, RelayCore::SurfaceMaterial::Metal});
    level.solids.push_back({{0.0F, 0.0F, 0.0F}, {4.0F, 4.0F, 1.0F}, RelayCore::SurfaceMaterial::Concrete});

    const RelayCore::render::CpuColorMeshData mesh = RelayCore::render::build_blockout_mesh(level);
    const float concrete_id = material_id(RelayCore::SurfaceMaterial::Concrete);
    const float metal_id = material_id(RelayCore::SurfaceMaterial::Metal);

    const auto concrete_front_vertex_count =
        std::count_if(mesh.vertices.begin(), mesh.vertices.end(), [&](const auto& vertex) {
            return std::fabs(vertex.material_kind - concrete_id) <= 0.001F
                   && std::fabs(vertex.position.z - 1.0F) <= 0.001F && std::fabs(vertex.normal.z - 1.0F) <= 0.001F;
        });
    const auto metal_front_vertex_count =
        std::count_if(mesh.vertices.begin(), mesh.vertices.end(), [&](const auto& vertex) {
            return std::fabs(vertex.material_kind - metal_id) <= 0.001F && std::fabs(vertex.position.z - 1.0F) <= 0.001F
                   && std::fabs(vertex.normal.z - 1.0F) <= 0.001F;
        });

    CHECK(concrete_front_vertex_count == 16);
    CHECK(metal_front_vertex_count == 4);
}

TEST_CASE("partially covered facility face stays on the original plane")
{
    RelayCore::BlockoutLevel level{};
    level.solids.push_back({{0.0F, 0.0F, 0.0F}, {4.0F, 2.0F, 1.0F}, RelayCore::SurfaceMaterial::WallPanel});
    level.solids.push_back({{1.0F, 0.0F, 0.8F}, {3.0F, 1.0F, 1.4F}, RelayCore::SurfaceMaterial::Concrete});

    const RelayCore::render::CpuColorMeshData mesh = RelayCore::render::build_blockout_mesh(level);
    const float wall_panel_id = material_id(RelayCore::SurfaceMaterial::WallPanel);

    const auto has_recessed_front_vertex =
        std::any_of(mesh.vertices.begin(), mesh.vertices.end(), [&](const auto& vertex) {
            return std::fabs(vertex.material_kind - wall_panel_id) <= 0.001F && vertex.normal.z > 0.99F
                   && vertex.position.z < 0.999F;
        });

    CHECK_FALSE(has_recessed_front_vertex);
}

TEST_CASE("bulkhead frame overlays cut the wall face without leaving front-plane duplicates")
{
    RelayCore::BlockoutLevel level{};
    level.solids.push_back({{-5.20F, 0.00F, -0.10F}, {-1.30F, 3.40F, 0.10F}, RelayCore::SurfaceMaterial::WallPanel});
    level.solids.push_back({{-1.64F, 0.10F, -0.10F}, {-1.42F, 2.74F, 0.10F}, RelayCore::SurfaceMaterial::Post});
    level.solids.push_back({{-1.70F, 2.59F, -0.10F}, {1.14F, 2.77F, 0.10F}, RelayCore::SurfaceMaterial::Beam});

    const RelayCore::render::CpuColorMeshData mesh = RelayCore::render::build_blockout_mesh(level);
    const float wall_panel_id = material_id(RelayCore::SurfaceMaterial::WallPanel);
    const float post_id = material_id(RelayCore::SurfaceMaterial::Post);
    const float beam_id = material_id(RelayCore::SurfaceMaterial::Beam);

    const auto wall_front_overlap_with_post =
        std::any_of(mesh.vertices.begin(), mesh.vertices.end(), [&](const auto& vertex) {
            return std::fabs(vertex.material_kind - wall_panel_id) <= 0.001F && vertex.normal.z > 0.99F
                   && std::fabs(vertex.position.z - 0.10F) <= 0.001F && vertex.position.x > -1.63F
                   && vertex.position.x < -1.43F && vertex.position.y > 0.11F && vertex.position.y < 2.73F;
        });
    const auto wall_front_overlap_with_beam =
        std::any_of(mesh.vertices.begin(), mesh.vertices.end(), [&](const auto& vertex) {
            return std::fabs(vertex.material_kind - wall_panel_id) <= 0.001F && vertex.normal.z > 0.99F
                   && std::fabs(vertex.position.z - 0.10F) <= 0.001F && vertex.position.x > -1.69F
                   && vertex.position.x < -1.31F && vertex.position.y > 2.60F && vertex.position.y < 2.76F;
        });
    const auto wall_front_recessed_after_cut =
        std::any_of(mesh.vertices.begin(), mesh.vertices.end(), [&](const auto& vertex) {
            return std::fabs(vertex.material_kind - wall_panel_id) <= 0.001F && vertex.normal.z > 0.99F
                   && vertex.position.z < 0.099F;
        });
    const auto post_front_vertex_count =
        std::count_if(mesh.vertices.begin(), mesh.vertices.end(), [&](const auto& vertex) {
            return std::fabs(vertex.material_kind - post_id) <= 0.001F && vertex.normal.z > 0.99F
                   && std::fabs(vertex.position.z - 0.10F) <= 0.001F;
        });
    const auto beam_front_vertex_count =
        std::count_if(mesh.vertices.begin(), mesh.vertices.end(), [&](const auto& vertex) {
            return std::fabs(vertex.material_kind - beam_id) <= 0.001F && vertex.normal.z > 0.99F
                   && std::fabs(vertex.position.z - 0.10F) <= 0.001F;
        });

    CHECK_FALSE(wall_front_overlap_with_post);
    CHECK_FALSE(wall_front_overlap_with_beam);
    CHECK_FALSE(wall_front_recessed_after_cut);
    CHECK(post_front_vertex_count == 4);
    CHECK(beam_front_vertex_count == 4);
}

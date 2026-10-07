#include "relaycore/physics/collision_asset.h"
#include "relaygame/game_world_runtime.h"
#include "relaygame/level_session_builder.h"

#include <array>
#include <doctest/doctest.h>
#include <fstream>
#include <glm/ext/matrix_transform.hpp>

#include "../tools/physics_fixture_data.h"
#include "scoped_temp_directory.h"
#include "scoped_test_logger.h"

using namespace RelayCore;
using namespace RelayCore::physics;

namespace
{
    auto sample() -> CollisionAsset
    {
        CollisionAsset asset{"models/lab_bench.rcmodel", {}, tool::physics_fixture_doorway()};
        asset.bounds = collision_bounds(asset.triangles);
        return asset;
    }

    auto bytes(const std::filesystem::path& path) -> std::string
    {
        std::ifstream input{path, std::ios::binary};
        return {std::istreambuf_iterator<char>{input}, {}};
    }
} // namespace

TEST_CASE("collision product round trip is deterministic and preserves bounds surfaces and opening")
{
    test::ScopedTempDirectory temp;
    const auto first = temp.path() / "first.rccollision", second = temp.path() / "second.rccollision";
    write_collision_asset(first, sample());
    const auto asset = read_collision_asset(first, "models/lab_bench.rcmodel");
    write_collision_asset(second, asset);
    CHECK(bytes(first) == bytes(second));
    CHECK(asset.bounds.mins == glm::vec3{-3, 0, 3});
    CHECK(asset.bounds.maxs == glm::vec3{3, 3, 3});
    const std::array meshes{PhysicsMesh{asset.triangles, {WorldObjectKind::CollisionMesh, 0}}};
    const PhysicsWorld world{{}, meshes};
    CHECK_FALSE(world.raycast({0, 1, 0}, {0, 0, 1}, 6));
    CHECK(world.raycast({1, 1, 0}, {0, 0, 1}, 6)->surface == SurfaceMaterial::Metal);
    CHECK(world.raycast({0, 2.5F, 0}, {0, 0, 1}, 6)->surface == SurfaceMaterial::Tile);
}

TEST_CASE("collision product rejects corrupt versions counts geometry surfaces and mismatched models")
{
    test::ScopedTempDirectory temp;
    const auto path = temp.path() / "bad.rccollision";
    const auto asset = sample();
    write_collision_asset(path, asset);
    CHECK_THROWS_WITH(
        static_cast<void>(read_collision_asset(path, "models/missing.rcmodel")),
        doctest::Contains("pairing")
    );
    const auto original = bytes(path);
    for (const auto offset : {0u, 4u, 8u, 12u, 16u, 40u + static_cast<unsigned>(asset.model.size()) + 36u})
    {
        auto malformed = original;
        for (unsigned i = 0; i < 4; ++i)
            malformed[offset + i] = '\xff';
        std::ofstream output{path, std::ios::binary | std::ios::trunc};
        output.write(malformed.data(), static_cast<std::streamsize>(malformed.size()));
        output.close();
        CHECK_THROWS(static_cast<void>(read_collision_asset(path, asset.model)));
    }
    std::ofstream{path, std::ios::binary | std::ios::trunc} << original.substr(0, original.size() - 1);
    CHECK_THROWS(static_cast<void>(read_collision_asset(path, asset.model)));
    auto invalid = asset;
    invalid.triangles.front().b = invalid.triangles.front().a;
    CHECK_THROWS(write_collision_asset(path, invalid));
    invalid = asset;
    invalid.bounds.maxs.x += 1;
    CHECK_THROWS(write_collision_asset(path, invalid));
    CHECK_THROWS_WITH(
        static_cast<void>(read_collision_asset(temp.path() / "missing.rccollision", asset.model)),
        doctest::Contains("rebuild")
    );
}

TEST_CASE("collision transforms preserve outward winding under reflected nonuniform scale")
{
    auto asset = sample();
    auto transform = glm::translate(glm::mat4{1}, glm::vec3{5, 2, -1});
    transform = glm::scale(transform, glm::vec3{-2, 3, .5F});
    asset.triangles = transform_collision_triangles(asset.triangles, transform);
    asset.bounds = collision_bounds(asset.triangles);
    validate_collision_asset(asset);
    CHECK(asset.bounds.mins == glm::vec3{-1, 2, .5F});
    CHECK(asset.bounds.maxs == glm::vec3{11, 11, .5F});
    const std::array meshes{PhysicsMesh{asset.triangles, {WorldObjectKind::CollisionMesh, 0}}};
    const PhysicsWorld world{{}, meshes};
    const auto hit = world.raycast({3, 5, -1}, {0, 0, 1}, 4);
    REQUIRE(hit);
    CHECK(hit->surface == SurfaceMaterial::Metal);
    CHECK(hit->normal == glm::vec3{0, 0, -1});
    CHECK_THROWS(
        static_cast<void>(transform_collision_triangles(asset.triangles, glm::scale(glm::mat4{1}, glm::vec3{1, 0, 1})))
    );
    transform[0][3] = 1;
    CHECK_THROWS(static_cast<void>(transform_collision_triangles(asset.triangles, transform)));
}

TEST_CASE("staged collision product prepares a session and failed updates retain the active world")
{
    test::ScopedTempDirectory temp;
    test::ScopedTestLogger logger{temp.path() / "logs"};
    core::EngineConfig config;
    RelayGame::LevelSessionBuilder builder{config.gameplay};
    RelayGame::GameWorldRuntime runtime;
    const auto product = std::filesystem::path{RELAYCORE_TEST_COLLISION_PRODUCT};
    auto session = builder.build_collision_fixture(
        tool::physics_fixture_level(),
        "bench_fixture.toml",
        product,
        "models/lab_bench.rcmodel"
    );
    runtime.load_level_session(config, std::move(session));
    CHECK_FALSE(runtime.query_physics_fixture({0, .5F, 2}, {0, 0, -1}, 4));
    const auto hit = runtime.query_physics_fixture({1.05F, .5F, 2}, {0, 0, -1}, 4);
    REQUIRE(hit);
    CHECK(hit->surface == SurfaceMaterial::Metal);
    for (const auto& bad : {temp.path() / "missing.rccollision", temp.path() / "malformed.rccollision"})
    {
        if (bad.filename() == "malformed.rccollision")
            std::ofstream{bad} << "broken";
        CHECK_THROWS(static_cast<void>(builder.build_collision_fixture(
            tool::physics_fixture_level(),
            "bench_fixture.toml",
            bad,
            "models/lab_bench.rcmodel"
        )));
        CHECK(runtime.query_physics_fixture({1.05F, .5F, 2}, {0, 0, -1}, 4)->surface == SurfaceMaterial::Metal);
    }
}

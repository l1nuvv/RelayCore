#include "relaycore/render/reflection_probe.h"
#include "relaycore/world/blockout_level.h"
#include "relaygame/world_irradiance.h"

#include <doctest/doctest.h>
#include <fstream>
#include <glm/geometric.hpp>

#include "scoped_temp_directory.h"

TEST_CASE("reflection cube captures room faces and filters roughness reproducibly")
{
    using namespace RelayCore::render;
    IrradianceBakeScene scene;
    for (std::uint32_t face = 0; face < 6; ++face)
    {
        const auto a = reflection_face_direction(face, {0, 0}) * glm::vec3{2};
        const auto b = reflection_face_direction(face, {1, 0}) * glm::vec3{2};
        const auto c = reflection_face_direction(face, {1, 1}) * glm::vec3{2};
        const auto d = reflection_face_direction(face, {0, 1}) * glm::vec3{2};
        const glm::vec3 color = face == 4 ? glm::vec3{4.F / 5, .1F, .05F} : glm::vec3{.05F, .2F, .5F};
        scene.triangles.push_back({a, b, c, color});
        scene.triangles.push_back({a, c, d, color});
    }
    ReflectionProbeSettings settings{{}, {-2, -2, -2}, {2, 2, 2}, 16, 64};
    const auto p = bake_reflection_probe(scene, settings, glm::vec3{2});
    REQUIRE(p.mips.size() == 5);
    const auto sharp = sample_reflection_probe(p, {0, 0, 1}, 0);
    CHECK(sharp.x == doctest::Approx(1.6));
    CHECK(sharp.y == doctest::Approx(.2));
    const auto rough = sample_reflection_probe(p, {0, 0, 1}, 1);
    CHECK(rough.x < sharp.x);
    CHECK(rough.z > sharp.z);
    for (const auto d : {glm::vec3{1, .1F, 1.001F}, glm::vec3{1.001F, .1F, 1}})
        CHECK(glm::length(sample_reflection_probe(p, d, 1) - sample_reflection_probe(p, {1, .1F, 1}, 1)) < .02F);
    RelayCore::test::ScopedTempDirectory temp;
    const auto path = temp.path() / "room.rcreflection";
    write_reflection_probe(path, p);
    const auto loaded = read_reflection_probe(path);
    CHECK(loaded.source_signature == p.source_signature);
    CHECK(loaded.mips == p.mips);
    const auto repeat = bake_reflection_probe(scene, settings, glm::vec3{2});
    CHECK(repeat.mips == p.mips);
    settings.position.x = .1F;
    CHECK(reflection_source_signature(scene, settings, glm::vec3{2}) != p.source_signature);
    settings.position.x = 0;
    settings.samples = 128;
    CHECK(reflection_source_signature(scene, settings, glm::vec3{2}) != p.source_signature);
    settings.samples = 64;
    scene.triangles[0].diffuse.x = .7F;
    CHECK(reflection_source_signature(scene, settings, glm::vec3{2}) != p.source_signature);
    scene.lights.push_back({{0, 0, 0}, {1, 1, 1}, 5});
    CHECK(reflection_source_signature(scene, settings, glm::vec3{2}) != p.source_signature);
    CHECK_THROWS_WITH_AS(
        static_cast<void>(bake_reflection_probe(scene, {{3, 0, 0}, {-2, -2, -2}, {2, 2, 2}, 16, 64}, glm::vec3{1})),
        doctest::Contains("inside"),
        std::runtime_error
    );
    auto broken = p;
    broken.mips.pop_back();
    CHECK_THROWS_WITH_AS(validate_reflection_probe(broken), doctest::Contains("complete"), std::runtime_error);
    std::ofstream(path, std::ios::binary | std::ios::trunc) << "RREF";
    CHECK_THROWS_WITH_AS(
        static_cast<void>(read_reflection_probe(path)),
        doctest::Contains("room.rcreflection"),
        std::runtime_error
    );
}

TEST_CASE("reflection authoring rejects duplicate probes, invalid bounds and unsafe products")
{
    RelayCore::BlockoutLevel level;
    RelayCore::EntitySpawnData entity;
    entity.classname = "reflection_probe";
    entity.properties = {
        {"mins", "-2 -2 -2"},
        {"maxs", "2 2 2"},
        {"resolution", "16"},
        {"samples", "64"},
        {"product", "lighting/test.rcreflection"}
    };
    level.entities.push_back(entity);
    REQUIRE(RelayGame::world_reflection_source(level));
    level.entities.push_back(entity);
    CHECK_THROWS_WITH_AS(
        static_cast<void>(RelayGame::world_reflection_source(level)),
        doctest::Contains("only one"),
        std::runtime_error
    );
    level.entities.pop_back();
    for (const auto& [key, value] : std::vector<std::pair<std::string, std::string>>{
             {"product", "../escape.rcreflection"},
             {"resolution", "12"},
             {"samples", "64.5"},
             {"mins", "bad"}
         })
    {
        auto bad = level;
        bad.entities[0].properties[key] = value;
        CHECK_THROWS_AS(static_cast<void>(RelayGame::world_reflection_source(bad)), std::runtime_error);
    }
}

#include "relaycore/core/paths.h"
#include "relaycore/render/static_model.h"
#include "relaygame/level_session_builder.h"
#include "relaygame/player_feedback.h"
#include "relaygame/static_props.h"

#include <doctest/doctest.h>
#include <fstream>
#include <glm/common.hpp>

#include "scoped_temp_directory.h"

TEST_CASE("OBJ preserves separate UV/normal indices and rejects malformed geometry")
{
    RelayCore::test::ScopedTempDirectory directory;
    const auto path = directory.path() / "triangle.obj";
    const std::string prefix = "v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nvn 0 0 2\n";
    const auto write = [&](const std::string& face) {
        std::ofstream(path) << prefix << face;
    };
    write("f -3/3/1 -2/2/1 -1/1/1\n");
    const auto mesh = RelayCore::render::load_obj_mesh(path);
    REQUIRE(mesh.indices.size() == 3);
    CHECK(mesh.vertices[0].uv.y == 1);
    CHECK(mesh.vertices[0].normal.z == 1);
    for (const char* face :
         {"f 0/1/1 2/2/1 3/3/1",
          "f 1//1 2//1 3//1",
          "f 1/1/1 2/2/1 7/3/1",
          "f 1/1/1 1/1/1 3/3/1",
          "f 1/1/1 2/2/1 3/3/1 1/1/1"})
    {
        write(face);
        CHECK_THROWS_WITH_AS(
            static_cast<void>(RelayCore::render::load_obj_mesh(path)),
            doctest::Contains("triangle.obj:"),
            std::runtime_error
        );
    }
    CHECK_THROWS_WITH_AS(
        static_cast<void>(RelayCore::render::load_obj_mesh(directory.path() / "missing.obj")),
        doctest::Contains("missing.obj"),
        std::runtime_error
    );
}

TEST_CASE("shotgun body and pump preserve a shared metre scale and material UVs")
{
    glm::vec3 mins{100}, maxs{-100};
    std::size_t triangles = 0;
    for (const auto* path : {"models/shotgun_body.obj", "models/shotgun_pump.obj"})
    {
        const auto mesh = RelayCore::render::load_obj_mesh(RelayCore::core::asset_path(path));
        REQUIRE_FALSE(mesh.vertices.empty());
        triangles += mesh.indices.size() / 3;
        for (const auto& vertex : mesh.vertices)
        {
            mins = glm::min(mins, vertex.position);
            maxs = glm::max(maxs, vertex.position);
            CHECK(vertex.uv.x > 0);
            CHECK(vertex.uv.x < 1);
            CHECK(vertex.uv.y > 0);
            CHECK(vertex.uv.y < 1);
        }
    }
    CHECK(maxs.z - mins.z == doctest::Approx(1.02).epsilon(.001));
    CHECK(maxs.x - mins.x < .08F);
    CHECK(maxs.y - mins.y < .25F);
    CHECK(triangles > 1000);
    CHECK(triangles < 2000);
}

TEST_CASE("Static props keep transformed collision separate from visible geometry")
{
    RelayCore::BlockoutLevel level;
    RelayCore::EntitySpawnData entity;
    entity.classname = "prop_static";
    entity.origin = {3, 0, 5};
    entity.angles = {90, 0, 0};
    entity.properties = {
        {"model", "models/service_cylinder.obj"},
        {"texture", "models/service_cylinder.bmp"},
        {"scale", "2 1 1"},
        {"collision_mins", "-1 0 -0.5"},
        {"collision_maxs", "1 2 0.5"},
        {"collision_material", "Metal"}
    };
    level.entities.push_back(entity);
    const auto session = RelayGame::LevelSessionBuilder{RelayCore::core::GameplayConfig{}}.build(level, "props.toml");
    CHECK(session.level.solids.empty());
    REQUIRE(session.level.prop_colliders.size() == 1);
    const auto& bounds = session.level.prop_colliders.front();
    CHECK(bounds.mins.x == doctest::Approx(2.5));
    CHECK(bounds.maxs.z == doctest::Approx(7));
    CHECK(RelayGame::footstep_surface(session.level, {3, 2, 5}) == RelayGame::FootstepSurface::Metal);
    level.entities[0].properties["scale"] = "0 1 1";
    CHECK_THROWS(static_cast<void>(RelayGame::build_static_props(level)));
    level.entities[0].properties["scale"] = "1 1 1 trailing";
    CHECK_THROWS(static_cast<void>(RelayGame::build_static_props(level)));
    level.entities[0].properties.erase("scale");
    level.entities[0].properties.erase("collision_maxs");
    CHECK_THROWS(static_cast<void>(RelayGame::build_static_props(level)));
}

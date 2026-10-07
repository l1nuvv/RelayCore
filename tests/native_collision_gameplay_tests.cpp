#include "relaycore/debug/inspector_source_reference.h"
#include "relaycore/physics/physics_world.h"
#include "relaygame/enemy_runtime.h"
#include "relaygame/level_editor_document.h"
#include "relaygame/level_session_builder.h"
#include "relaygame/world_inspector.h"

#include <doctest/doctest.h>

#include "native_collision_fixture.h"
#include "scoped_temp_directory.h"
#include "scoped_test_logger.h"

using namespace RelayCore;
using namespace RelayCore::physics;

TEST_CASE("native collision inspector distinguishes opening surfaces shape hits and authored identity")
{
    test::ScopedTempDirectory temp;
    test::ScopedTestLogger logger{temp.path() / "logs"};
    const auto map = temp.path() / "inspect.toml";
    test::write_native_collision_fixture(map);
    core::EngineConfig config;
    const auto session = RelayGame::LevelSessionBuilder{config.gameplay}.build(map);
    const auto& world = *session.physics_fixture;
    CHECK_FALSE(world.raycast({0, 1, 0}, {0, 0, 1}, 4));
    const auto support = world.raycast({1.05F, 1, 0}, {0, 0, 1}, 4);
    REQUIRE(support);
    const auto bounds = world.object_bounds(support->object);
    REQUIRE(bounds);
    CHECK(bounds->mins.z == doctest::Approx(2.55F));
    CHECK(bounds->maxs.y == doctest::Approx(2.76F));
    const auto info = RelayGame::build_collision_inspector_diagnostics(session.level, *support, *bounds);
    CHECK(info.has_collision_hit);
    CHECK(info.object_kind == "collision_mesh");
    CHECK(info.material == "metal");
    CHECK(info.collision_layer == "Static");
    CHECK(info.hit_normal.z == doctest::Approx(-1));
    CHECK(info.authored_index == 2);
    CHECK(info.has_authored_location);
    CHECK(info.collision_product == "collision/lab_bench.rccollision");
    CHECK(info.collision_model == "models/lab_bench.rcmodel");
    const auto restored = RelayGame::find_world_inspector_diagnostics_by_authored_source(
        session.level,
        session.doors,
        session.triggers,
        session.usables,
        "collision_mesh",
        debug::build_primary_inspector_source_reference(info),
        {0, 1, 0},
        &world
    );
    CHECK(restored.has_pick);
    CHECK(restored.pinned);
    CHECK(restored.authored_index == 2);
    CHECK(restored.collision_product == info.collision_product);
    CHECK_FALSE(restored.has_collision_hit);
    const auto top = world.raycast({0, 2.6F, 0}, {0, 0, 1}, 4);
    REQUIRE(top);
    CHECK(RelayGame::build_collision_inspector_diagnostics(session.level, *top, *bounds).material == "tile");
    CHECK_FALSE(world.cast_box({0, 1, 0}, glm::vec3{.2F}, {0, 0, 4}));
    REQUIRE(world.cast_box({1.05F, 1, 0}, glm::vec3{.2F}, {0, 0, 4}));
    CHECK(world.cast_box({1.05F, 1, 0}, glm::vec3{.2F}, {0, 0, 4})->object == support->object);
    CHECK_FALSE(world.object_bounds({WorldObjectKind::CollisionMesh, 999, 2}));
}

TEST_CASE("editor switches collision products atomically and preserves references through undo save and reload")
{
    test::ScopedTempDirectory temp;
    test::ScopedTestLogger logger{temp.path() / "logs"};
    const auto map = temp.path() / "authoring.toml";
    test::write_native_collision_fixture(map);
    core::EngineConfig config;
    RelayGame::LevelSessionBuilder builder{config.gameplay};
    RelayGame::LevelEditorDocument document;
    document.reset(builder.build(map).level);
    document.select(RelayGame::EditorSelection{true, 2});
    document.set_model_collision(true);
    CHECK_FALSE(document.level().entities[2].properties.contains("collision_asset"));
    CHECK(document.level().entities[2].properties.contains("collision_mins"));
    document.undo();
    CHECK(document.level().entities[2].properties.contains("collision_asset"));
    document.redo();
    document.set_model_collision_asset("collision/lab_bench.rccollision");
    CHECK_FALSE(document.level().entities[2].properties.contains("collision_mins"));
    const auto revision = document.revision();
    CHECK_THROWS(document.set_model_collision_asset("collision/missing.rccollision"));
    CHECK_THROWS(document.set_model_collision_asset("../lab_bench.rccollision"));
    CHECK(document.revision() == revision);
    document.set_model_collision_asset({});
    CHECK_FALSE(document.level().entities[2].properties.contains("collision_asset"));
    document.undo();
    const auto edited = temp.path() / "saved.toml";
    document.save(edited);
    const auto reloaded = builder.build(edited);
    REQUIRE(reloaded.physics_fixture);
    CHECK(reloaded.level.entities[2].properties.at("collision_asset") == "collision/lab_bench.rccollision");
    CHECK_FALSE(reloaded.physics_fixture->raycast({0, 1, 0}, {0, 0, 1}, 4));
    document.set_property("model", "models/service_cylinder.rcmodel");
    CHECK_THROWS_WITH(
        document.set_model_collision_asset("collision/lab_bench.rccollision"),
        doctest::Contains("collision/model pairing differs; rebuild assets or fix reference")
    );
}

TEST_CASE("native collision session preserves transforms pairing editor data and safe spawns")
{
    test::ScopedTempDirectory temp;
    test::ScopedTestLogger logger{temp.path() / "logs"};
    const auto map = temp.path() / "native.toml";
    test::write_native_collision_fixture(map);
    core::EngineConfig config;
    RelayGame::LevelSessionBuilder builder{config.gameplay};
    auto session = builder.build(map);
    REQUIRE(session.physics_fixture);
    REQUIRE(session.level.prop_colliders.empty());
    CHECK_FALSE(session.physics_fixture->raycast({0, 1, 0}, {0, 0, 1}, 4));
    const auto support = session.physics_fixture->raycast({1.05F, 1, 0}, {0, 0, 1}, 4);
    REQUIRE(support);
    CHECK(support->surface == SurfaceMaterial::Metal);
    CHECK(support->object.authored_entity_index == 2);
    CHECK(support->distance == doctest::Approx(2.63F));
    CHECK(session.physics_fixture->raycast({0, 2.6F, 0}, {0, 0, 1}, 4)->surface == SurfaceMaterial::Tile);
    RelayGame::LevelEditorDocument document;
    document.reset(session.level);
    const auto edited = temp.path() / "edited.toml";
    document.save(edited);
    CHECK(builder.build(edited).physics_fixture != nullptr);
    auto bad = session.level;
    bad.entities[2].properties["model"] = "models/service_cylinder.rcmodel";
    CHECK_THROWS_WITH(static_cast<void>(builder.build(bad, map)), doctest::Contains("pairing"));
    for (const auto* reference :
         {"collision/missing.rccollision", "../lab_bench.rccollision", "collision/lab_bench.obj"})
    {
        bad = session.level;
        bad.entities[2].properties["collision_asset"] = reference;
        CHECK_THROWS(static_cast<void>(builder.build(bad, map)));
    }
    bad = session.level;
    bad.entities[0].origin = {1.05F, .001F, 3};
    CHECK_THROWS_WITH(static_cast<void>(builder.build(bad, map)), doctest::Contains("player spawn"));
    bad = session.level;
    bad.entities[1].origin = {1.05F, .001F, 3};
    document.reset(bad);
    document.save(edited);
    CHECK_THROWS_WITH(static_cast<void>(builder.build_at_entry(edited, "arrival")), doctest::Contains("level_entry"));
}

TEST_CASE("native opening drives shots LOS rover movement focus doors and trigger entry")
{
    test::ScopedTempDirectory temp;
    test::ScopedTestLogger logger{temp.path() / "logs"};
    const auto map = temp.path() / "native.toml";
    test::write_native_collision_fixture(map);
    core::EngineConfig config;
    auto session = RelayGame::LevelSessionBuilder{config.gameplay}.build(map);
    std::vector<WorldCollider> blockers;
    auto query = [&] {
        gameplay::collect_blocking_doors(session.doors, blockers);
        return WorldQuery{*session.physics_fixture, blockers};
    };
    CHECK(query().raycast({0, 1, 0}, {0, 0, 1}, 8)->object.kind == WorldObjectKind::Door);
    session.doors[0].state = gameplay::DoorState::Opening;
    static_cast<void>(gameplay::update_doors(1, session.doors));
    CHECK_FALSE(query().raycast({0, 1, 0}, {0, 0, 1}, 8));

    for (const float x : {0.0F, 1.05F})
    {
        std::vector<RelayGame::RuntimeEnemy> enemies{RelayGame::RuntimeEnemy{.state = {.position = {x, .001F, 4}}}};
        gameplay::PlayerCombatState player;
        const auto shot = RelayGame::fire_player_weapon(
            query(),
            {x, .5F, 0},
            {0, 0, 1},
            player,
            {},
            enemies,
            RelayGame::PlayerWeapon::Precise
        );
        CHECK((shot.enemy_index == 0) == (x == 0));
        CHECK(shot.combat.world_impact.has_value() == (x != 0));
        if (shot.combat.world_impact)
        {
            const auto surface = query().raycast({x, .5F, 0}, {0, 0, 1}, 8);
            REQUIRE(surface);
            CHECK(shot.combat.world_impact->object.kind == WorldObjectKind::CollisionMesh);
            CHECK(shot.combat.world_impact->surface == surface->surface);
            CHECK(shot.combat.world_impact->position == surface->position);
            CHECK(shot.combat.world_impact->normal == surface->normal);
        }
        enemies[0].state.mode = RelayGame::EnemyMode::Idle;
        enemies[0].state.last_seen = {};
        const auto actor = make_player_aabb({x, .001F, 0}, .3F, 1.8F);
        std::vector<WorldCollider> actors;
        for (int tick = 0; tick < 180; ++tick)
            static_cast<void>(RelayGame::update_enemies(query(), actor, {x, .5F, 0}, player, enemies, 1.F / 120, actors)
            );
        CHECK((enemies[0].state.position.z < 3) == (x == 0));
    }
    const auto focus = gameplay::find_focused_usable({0, 1.2F, 2.5F}, {0, 0, 1}, session.usables, 2.25F, .75F, query());
    CHECK(focus.usable_index == 0);
    session.doors[0].state = gameplay::DoorState::Closing;
    session.doors[0].open_fraction = .5F;
    const auto actor = make_player_aabb({1, .001F, 5}, .3F, 1.8F);
    static_cast<void>(gameplay::update_doors(1.F / 120, actor, session.doors));
    CHECK(session.doors[0].state == gameplay::DoorState::Opening);
    gameplay::RuntimeTrigger trigger;
    trigger.bounds = {{-.5F, 0, 3.5F}, {.5F, 2, 4.5F}};
    trigger.target = "fixture_door";
    std::vector<gameplay::RuntimeTrigger> triggers{trigger};
    std::vector<std::string> activated, signaled;
    gameplay::InteractionEvent event;
    CHECK(
        gameplay::update_trigger_overlaps(
            make_player_aabb({0, .001F, 4}, .3F, 1.8F),
            triggers,
            session.doors,
            session.usables,
            activated,
            signaled,
            event
        )
        == 1
    );
    CHECK(
        gameplay::update_trigger_overlaps(
            make_player_aabb({0, .001F, 4}, .3F, 1.8F),
            triggers,
            session.doors,
            session.usables,
            activated,
            signaled,
            event
        )
        == 0
    );
}

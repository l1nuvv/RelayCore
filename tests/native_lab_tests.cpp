#include "relaycore/core/paths.h"
#include "relaycore/physics/collision.h"
#include "relaycore/render/static_model.h"
#include "relaygame/game_world_runtime.h"
#include "relaygame/level_editor_document.h"
#include "relaygame/level_session_builder.h"
#include "relaygame/static_props.h"

#include <algorithm>
#include <doctest/doctest.h>

#include "scoped_temp_directory.h"
#include "scoped_test_logger.h"
#include "world_query_fixture.h"

TEST_CASE("native lab shares model resources and preserves transformed props through editor save")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const RelayCore::core::GameplayConfig config;
    const auto path = RelayCore::core::asset_path("levels/native_lab.toml");
    const auto session = RelayGame::LevelSessionBuilder{config}.build(path);
    REQUIRE(session.authoring_warnings.empty());
    const auto props = RelayGame::build_static_props(session.level);
    REQUIRE(props.size() == 8);
    REQUIRE(session.level.prop_colliders.size() == props.size());
    RelayCore::render::StaticModelCache cache;
    for (const auto& prop : props)
    {
        CAPTURE(prop.model.string());
        REQUIRE(prop.collider.has_value());
        CHECK(prop.texture.empty());
        const auto model = cache.load(prop.model, RelayCore::render::TextureEncoding::srgb);
        CHECK(cache.load(prop.model, RelayCore::render::TextureEncoding::srgb) == model);
        CHECK(model->materials.size() >= 2);
        for (const auto& part : model->parts)
            for (const auto& vertex : model->meshes[part.mesh].cpu_data()->vertices)
            {
                const auto world = glm::vec3{prop.transform * part.transform * glm::vec4{vertex.position, 1}};
                CHECK(glm::all(glm::greaterThanEqual(world, prop.collider->mins - glm::vec3{.002F})));
                CHECK(glm::all(glm::lessThanEqual(world, prop.collider->maxs + glm::vec3{.002F})));
            }
    }
    RelayGame::LevelEditorDocument editor;
    editor.reset(session.level);
    CHECK(editor.validation_messages(path) == std::vector<std::string>{"Нет задания или зоны выхода."});
    const auto saved = temp.path() / "native_lab.toml";
    editor.save(saved);
    const auto roundtrip = RelayGame::LevelSessionBuilder{config}.build(saved);
    const auto saved_props = RelayGame::build_static_props(roundtrip.level);
    REQUIRE(saved_props.size() == props.size());
    for (std::size_t i = 0; i < props.size(); ++i)
    {
        CHECK(saved_props[i].model == props[i].model);
        CHECK(saved_props[i].transform == props[i].transform);
        CHECK(saved_props[i].collider->mins == props[i].collider->mins);
        CHECK(saved_props[i].collider->maxs == props[i].collider->maxs);
    }
}

TEST_CASE("native lab cabinet controls lighting and a traversable storage door")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const RelayCore::core::EngineConfig config;
    const auto path = RelayCore::core::asset_path("levels/native_lab.toml");
    const RelayGame::LevelSessionBuilder builder{config.gameplay};
    auto session = builder.build(path);
    REQUIRE(session.doors.size() == 1);
    REQUIRE(session.usables.size() == 2);
    std::vector<RelayCore::physics::Aabb> world;
    for (const auto* solids : {&session.level.solids, &session.level.prop_colliders})
        for (const auto& solid : *solids)
            world.push_back({solid.mins, solid.maxs});
    for (const char* entry : {"entrance", "controls"})
        CHECK(RelayCore::physics::probe_ground(
            RelayCore::test::WorldQueryFixture{world},
            builder.build_at_entry(path, entry).spawn_position,
            {}
        ));
    const auto walk = [&](bool closed) {
        auto blockers = world;
        if (closed)
            blockers.push_back(RelayCore::gameplay::current_door_bounds(session.doors.front()));
        auto position = session.spawn_position;
        for (int tick = 0; tick < 600; ++tick)
            position = RelayCore::physics::move_and_collide(
                           RelayCore::test::WorldQueryFixture{blockers},
                           position,
                           {0, -.1F, 2},
                           1.0F / 120,
                           {},
                           true
            )
                           .position;
        return position;
    };
    CHECK(walk(true).z < 4.6F);
    CHECK(walk(false).z > 6.4F);
    RelayGame::GameWorldRuntime runtime;
    runtime.load_level_session(config, session);
    const auto power =
        std::ranges::find(runtime.usables(), "native_power_control", &RelayCore::gameplay::RuntimeUsable::name);
    REQUIRE(power != runtime.usables().end());
    static_cast<void>(runtime.debug_apply_usable_interaction(std::size_t(power - runtime.usables().begin())));
    CHECK(std::ranges::find(runtime.activated_target_ids(), "native_power") != runtime.activated_target_ids().end());
    const auto control =
        std::ranges::find(runtime.usables(), "storage_control", &RelayCore::gameplay::RuntimeUsable::name);
    REQUIRE(control != runtime.usables().end());
    static_cast<void>(runtime.debug_apply_usable_interaction(std::size_t(control - runtime.usables().begin())));
    CHECK(runtime.doors().front().state == RelayCore::gameplay::DoorState::Opening);
    const auto saved = runtime.capture_saved_game(0, false);
    RelayGame::GameWorldRuntime restored;
    restored.load_level_session(config, std::move(session));
    restored.restore_saved_game(saved);
    CHECK(restored.activated_target_ids() == runtime.activated_target_ids());
    CHECK(restored.doors().front().state == RelayCore::gameplay::DoorState::Opening);
}

TEST_CASE("B17 laboratory loads native graphics and keeps its inspection routes traversable")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    const RelayCore::core::EngineConfig config;
    const auto path = RelayCore::core::asset_path("levels/occult_lab_b17.toml");
    const RelayGame::LevelSessionBuilder builder{config.gameplay};
    auto session = builder.build(path);
    REQUIRE(session.authoring_warnings.empty());
    REQUIRE(session.physics_fixture);
    const auto& world = *session.physics_fixture;
    for (const char* entry : {"entrance", "observation", "containment"})
        CHECK(RelayCore::physics::probe_ground(world, builder.build_at_entry(path, entry).spawn_position, {}));
    auto position = session.spawn_position;
    for (int tick = 0; tick < 600; ++tick)
        position = RelayCore::physics::move_and_collide(world, position, {0, -.1F, 2}, 1.0F / 120, {}, true).position;
    CHECK(position.z > 1.5F);
    RelayCore::render::StaticModelCache cache;
    for (const auto& prop : RelayGame::build_static_props(session.level))
    {
        CAPTURE(prop.model.string());
        const auto model = cache.load(prop.model, RelayCore::render::TextureEncoding::srgb);
        REQUIRE_FALSE(model->meshes.empty());
        if (prop.model.stem() == "lab_notes_board" || prop.model.stem() == "safety_notice")
            CHECK(std::ranges::any_of(model->materials, [](const auto& material) {
                return material.base_color.cpu_data()->width > 1;
            }));
    }
    RelayGame::LevelEditorDocument editor;
    editor.reset(session.level);
    CHECK(editor.validation_messages(path).empty());
    RelayGame::GameWorldRuntime runtime;
    runtime.load_level_session(config, std::move(session));
    REQUIRE(runtime.usables().size() == 1);
    static_cast<void>(runtime.debug_apply_usable_interaction(0));
    CHECK(std::ranges::find(runtime.activated_target_ids(), "b17_isolated") != runtime.activated_target_ids().end());
}

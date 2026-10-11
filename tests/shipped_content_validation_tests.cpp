#include "relaycore/core/engine_config.h"
#include "relaycore/core/log.h"
#include "relaycore/gameplay/combat_runtime.h"
#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/physics/collision.h"
#include "relaycore/physics/player_motion.h"
#include "relaycore/world/blockout_loader.h"
#include "relaygame/game_world_runtime.h"
#include "relaygame/level_session_builder.h"
#include "relaygame/mission_runtime.h"
#include "relaygame/runtime_model.h"
#include "relaygame/usable_interaction.h"
#include "relaygame/world_audio_zones.h"

#include <algorithm>
#include <array>
#include <doctest/doctest.h>
#include <filesystem>
#include <glm/geometric.hpp>
#include <string_view>
#include <vector>

#include "acceptance_slice_harness.h"
#include "scoped_temp_directory.h"
#include "scoped_test_logger.h"
#include "world_query_fixture.h"

namespace
{
    using RelayCore::gameplay::CombatTargetActionKind;
    using RelayCore::gameplay::CombatTargetEvent;
    using RelayCore::gameplay::DoorState;
    using RelayCore::gameplay::InteractionEvent;
    using RelayCore::gameplay::PlayerCombatState;
    using RelayCore::gameplay::RuntimeDoor;
    using RelayCore::gameplay::RuntimeHazardVolume;
    using RelayCore::gameplay::RuntimeTrigger;
    using RelayCore::gameplay::RuntimeTurret;
    using RelayCore::gameplay::RuntimeUsable;
    using RelayCore::gameplay::UsableActionKind;
    using RelayCore::gameplay::UseInteractionOutcome;
    using RelayGame::MissionBuilder;
    using RelayGame::MissionDefinition;
    using RelayGame::MissionRuntime;

    struct ShippedMapExpectation {
        std::string_view file_name{};
        std::size_t min_solids = 0;
        std::size_t min_entities = 0;
        std::size_t door_count = 0;
        std::size_t trigger_count = 0;
        std::size_t pickup_count = 0;
        std::size_t turret_count = 0;
        std::size_t audio_zone_count = 0;
        std::size_t mission_step_count = 0;
    };

    constexpr std::array kShippedMaps{
        ShippedMapExpectation{
            .file_name = "power_restoration.toml",
            .min_solids = 90,
            .min_entities = 15,
            .door_count = 3,
            .trigger_count = 0,
            .pickup_count = 1,
            .turret_count = 2,
            .audio_zone_count = 2,
            .mission_step_count = 9,
        },
        ShippedMapExpectation{
            .file_name = "test_room.toml",
            .min_solids = 24,
            .min_entities = 12,
            .door_count = 2,
            .trigger_count = 1,
            .pickup_count = 1,
            .turret_count = 1,
            .audio_zone_count = 4,
            .mission_step_count = 6,
        },
        ShippedMapExpectation{
            .file_name = "stress_room_01.toml",
            .min_solids = 40,
            .min_entities = 11,
            .door_count = 4,
            .trigger_count = 1,
            .pickup_count = 0,
            .turret_count = 0,
            .audio_zone_count = 5,
            .mission_step_count = 0,
        },
        ShippedMapExpectation{
            .file_name = "stress_room_02.toml",
            .min_solids = 70,
            .min_entities = 20,
            .door_count = 2,
            .trigger_count = 1,
            .pickup_count = 1,
            .turret_count = 0,
            .audio_zone_count = 5,
            .mission_step_count = 6,
        },
    };

    auto repo_root() -> std::filesystem::path
    {
        return std::filesystem::path(__FILE__).parent_path().parent_path();
    }

    auto find_usable_index(const RelayGame::GameWorldRuntime& runtime, const auto& predicate) -> std::size_t
    {
        const auto& usables = runtime.usables();
        const auto it = std::find_if(usables.begin(), usables.end(), predicate);
        REQUIRE(it != usables.end());
        return static_cast<std::size_t>(std::distance(usables.begin(), it));
    }

    auto find_door_index(const RelayGame::GameWorldRuntime& runtime, const auto& predicate) -> std::size_t
    {
        const auto& doors = runtime.doors();
        const auto it = std::find_if(doors.begin(), doors.end(), predicate);
        REQUIRE(it != doors.end());
        return static_cast<std::size_t>(std::distance(doors.begin(), it));
    }

    auto find_farthest_hazard(const RelayGame::LevelSession& session) -> const RuntimeHazardVolume&
    {
        REQUIRE_FALSE(session.combat.hazards.empty());
        const auto it = std::max_element(
            session.combat.hazards.begin(),
            session.combat.hazards.end(),
            [&session](const RuntimeHazardVolume& lhs, const RuntimeHazardVolume& rhs) {
                const float lhs_distance =
                    glm::distance(AcceptanceHarness::bounds_center(lhs.bounds), session.spawn_position);
                const float rhs_distance =
                    glm::distance(AcceptanceHarness::bounds_center(rhs.bounds), session.spawn_position);
                return lhs_distance < rhs_distance;
            }
        );
        REQUIRE(it != session.combat.hazards.end());
        return *it;
    }

    void progress_test_room_runtime_to_turret_unlock(
        RelayGame::GameWorldRuntime& runtime,
        const RelayGame::LevelSession& session
    )
    {
        const std::size_t outer_panel_index = find_usable_index(runtime, [](const RuntimeUsable& usable) {
            return usable.label == "Открыть внешний шлюз";
        });
        const std::size_t pickup_index = find_usable_index(runtime, [](const RuntimeUsable& usable) {
            return usable.action_kind == UsableActionKind::PickupItem && usable.item_id == "lab_keycard";
        });
        const std::size_t reader_index = find_usable_index(runtime, [](const RuntimeUsable& usable) {
            return usable.label == "Подтвердить допуск";
        });

        const RelayCore::physics::Aabb spawn_bounds =
            AcceptanceHarness::make_actor_bounds({0.0F, 1.0F, -1.4F}, {0.3F, 0.9F, 0.3F});
        const RelayCore::physics::Aabb airlock_bounds = AcceptanceHarness::make_actor_bounds(
            AcceptanceHarness::bounds_center(session.mission_definition.steps[1].bounds),
            {0.3F, 0.9F, 0.3F}
        );

        CHECK(runtime.debug_apply_usable_interaction(outer_panel_index).kind == UseInteractionOutcome::ActivatedTarget);
        runtime.debug_update_mission(spawn_bounds);
        runtime.debug_update_mission(airlock_bounds);
        CHECK(runtime.debug_apply_usable_interaction(pickup_index).kind == UseInteractionOutcome::PickedUpItem);
        runtime.debug_update_mission(spawn_bounds);
        CHECK(runtime.debug_apply_usable_interaction(reader_index).kind == UseInteractionOutcome::ActivatedTarget);
        runtime.debug_update_mission(spawn_bounds);

        REQUIRE(runtime.turrets().size() == 1);
        const glm::vec3 shot_origin = runtime.turrets()[0].origin + glm::vec3{-1.8F, 0.0F, -1.2F};
        const glm::vec3 shot_direction = runtime.turrets()[0].origin - shot_origin;

        const auto first_shot = runtime.debug_apply_player_fire(shot_origin, shot_direction);
        CHECK(first_shot.fired);
        CHECK(first_shot.hit_turret);
        CHECK_FALSE(first_shot.killed_turret);

        runtime.debug_tick_combat(0.30F);

        const auto second_shot = runtime.debug_apply_player_fire(shot_origin, shot_direction);
        CHECK(second_shot.fired);
        CHECK(second_shot.hit_turret);
        CHECK(second_shot.killed_turret);
    }

} // namespace

TEST_CASE("power restoration requires a fuse and preserves powered presentation across snapshots")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::core::EngineConfig config{};
    config.gameplay.level_path = (repo_root() / "assets/levels/power_restoration.toml").string();
    const auto session = RelayGame::LevelSessionBuilder{config.gameplay}.build();
    REQUIRE(session.combat.hazards.size() == 1);
    RelayGame::GameWorldRuntime runtime{};
    runtime.load_level_session(config, session);
    REQUIRE(runtime.usables().size() == 7);
    CHECK(session.authoring_warnings.empty());
    const auto reader = find_usable_index(runtime, [](const auto& usable) {
        return usable.name == "airlock_reader";
    });
    const auto fuse = find_usable_index(runtime, [](const auto& usable) {
        return usable.item_id == "power_fuse";
    });
    const auto breaker = find_usable_index(runtime, [](const auto& usable) {
        return usable.name == "power_breaker";
    });
    const auto isolator = find_usable_index(runtime, [](const auto& usable) {
        return usable.name == "maintenance_isolator";
    });
    const auto bus_relay = find_usable_index(runtime, [](const auto& usable) {
        return usable.name == "bus_relay";
    });
    const auto cooling = find_usable_index(runtime, [](const auto& usable) {
        return usable.name == "cooling_control";
    });
    const auto filter = find_usable_index(runtime, [](const auto& usable) {
        return usable.name == "filter_purge";
    });
    const auto bounds = RelayCore::physics::make_player_aabb(session.spawn_position, 0.3F, 1.8F);
    const auto unpowered = runtime.capture_runtime_snapshot();
    CHECK(runtime.debug_apply_usable_interaction(reader).kind == UseInteractionOutcome::Locked);
    CHECK(runtime.debug_apply_usable_interaction(cooling).kind == UseInteractionOutcome::Locked);
    CHECK(runtime.debug_apply_usable_interaction(breaker).kind == UseInteractionOutcome::Locked);
    CHECK(runtime.activated_target_ids().empty());
    CHECK(RelayGame::find_active_audio_zone(session.audio_zones, bounds) == -1);
    CHECK(runtime.debug_apply_usable_interaction(fuse).kind == UseInteractionOutcome::PickedUpItem);
    runtime.debug_update_mission(bounds);
    CHECK(runtime.mission_status().title == "Открыть дверь к резервному питанию");
    CHECK(runtime.debug_apply_usable_interaction(isolator).kind == UseInteractionOutcome::ActivatedTarget);
    runtime.debug_update_mission(bounds);
    CHECK(runtime.mission_status().title == "Подключить резервное питание");
    CHECK(runtime.doors()[2].state == DoorState::Opening);
    const auto isolated_save_path = temp.path() / "isolated-progress.toml";
    RelayGame::write_saved_game(isolated_save_path, runtime.capture_saved_game(42, false));
    RelayGame::GameWorldRuntime isolated_runtime{};
    isolated_runtime.load_level_session(config, session);
    isolated_runtime.restore_saved_game(RelayGame::load_saved_game(isolated_save_path));
    CHECK(isolated_runtime.doors()[2].state == DoorState::Opening);
    CHECK(isolated_runtime.mission_status().title == "Подключить резервное питание");
    CHECK(isolated_runtime.usables()[breaker].locked);
    CHECK(runtime.debug_apply_usable_interaction(bus_relay).chain_applied);
    runtime.debug_update_mission(bounds);
    CHECK(runtime.mission_status().title == "Восстановить питание");
    CHECK_FALSE(runtime.usables()[breaker].locked);
    const auto power = runtime.debug_apply_usable_interaction(breaker);
    CHECK(power.kind == UseInteractionOutcome::ActivatedTarget);
    CHECK(power.chain_applied);
    runtime.debug_update_mission(bounds);
    CHECK(runtime.mission_status().title == "Промыть фильтр");
    CHECK(runtime.usables()[reader].locked);
    CHECK(runtime.usables()[cooling].locked);
    CHECK_FALSE(runtime.usables()[filter].locked);
    REQUIRE(session.audio_zones.size() == 2);
    CHECK(session.audio_zones[0].name == "powered_intake");
    CHECK(RelayGame::find_active_audio_zone(session.audio_zones, bounds, -1, runtime.activated_target_ids()) == 0);
    const auto power_only = runtime.capture_runtime_snapshot();
    CHECK(runtime.debug_apply_usable_interaction(filter).chain_applied);
    runtime.debug_update_mission(bounds);
    CHECK(runtime.mission_status().title == "Включить охлаждение");
    CHECK_FALSE(runtime.usables()[cooling].locked);
    CHECK(runtime.debug_apply_usable_interaction(cooling).chain_applied);
    runtime.debug_update_mission(RelayCore::physics::make_player_aabb({7, 0.01F, 8}, 0.3F, 1.8F));
    CHECK(runtime.mission_status().title == "Вернуться к входному залу");
    CHECK(runtime.doors()[1].state == DoorState::Opening);
    runtime.debug_update_mission(RelayCore::physics::make_player_aabb({3.5F, 0.1F, 7}, 0.3F, 1.8F));
    CHECK(runtime.mission_status().title == "Открыть выход");
    CHECK_FALSE(runtime.usables()[reader].locked);
    CHECK(RelayGame::find_active_audio_zone(session.audio_zones, bounds, -1, runtime.activated_target_ids()) == 0);
    const auto powered = runtime.capture_runtime_snapshot();
    REQUIRE(runtime.restore_runtime_snapshot(power_only).restored);
    CHECK(runtime.usables()[reader].locked);
    CHECK(runtime.usables()[cooling].locked);
    CHECK_FALSE(runtime.usables()[filter].locked);
    CHECK(RelayGame::find_active_audio_zone(session.audio_zones, bounds, 0, runtime.activated_target_ids()) == 0);
    REQUIRE(runtime.restore_runtime_snapshot(unpowered).restored);
    CHECK(runtime.usables()[reader].locked);
    CHECK(RelayGame::find_active_audio_zone(session.audio_zones, bounds, 0, runtime.activated_target_ids()) == -1);
    REQUIRE(runtime.restore_runtime_snapshot(powered).restored);
    CHECK_FALSE(runtime.usables()[reader].locked);
    CHECK(RelayGame::find_active_audio_zone(session.audio_zones, bounds, -1, runtime.activated_target_ids()) == 0);
    const auto corridor_bounds = RelayCore::physics::make_player_aabb({8.0F, 0.05F, 8.0F}, 0.3F, 1.8F);
    CHECK(
        RelayGame::find_active_audio_zone(session.audio_zones, corridor_bounds, -1, runtime.activated_target_ids()) == 1
    );
    CHECK(runtime.debug_apply_usable_interaction(reader).kind == UseInteractionOutcome::ActivatedTarget);
    runtime.debug_update_mission(bounds);
    CHECK_FALSE(runtime.mission_status().completed);
    runtime.debug_update_mission(RelayCore::physics::make_player_aabb({0.0F, 0.1F, 9.0F}, 0.3F, 1.8F));
    CHECK(runtime.mission_status().completed);
}

TEST_CASE("power restoration can finish after returning through the original corridor")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::core::EngineConfig config;
    config.gameplay.level_path = (repo_root() / "assets/levels/power_restoration.toml").string();
    const auto session = RelayGame::LevelSessionBuilder{config.gameplay}.build();
    RelayGame::GameWorldRuntime runtime;
    runtime.load_level_session(config, session);
    for (const auto name :
         {"spare_fuse", "maintenance_isolator", "bus_relay", "power_breaker", "filter_purge", "cooling_control"})
    {
        const auto index = find_usable_index(runtime, [name](const auto& usable) {
            return usable.name == name;
        });
        static_cast<void>(runtime.debug_apply_usable_interaction(index));
        runtime.debug_update_mission(RelayCore::physics::make_player_aabb({7, 0.01F, 8}, 0.3F, 1.8F));
    }
    CHECK(runtime.mission_status().current_step == 7);
    // Return to the intake from the original corridor, without visiting the shortcut zone.
    runtime.debug_update_mission(RelayCore::physics::make_player_aabb({4, 0.01F, 0}, 0.3F, 1.8F));
    CHECK(runtime.mission_status().current_step == 8);
    const auto reader = find_usable_index(runtime, [](const auto& usable) {
        return usable.name == "airlock_reader";
    });
    CHECK(runtime.debug_apply_usable_interaction(reader).kind == UseInteractionOutcome::ActivatedTarget);
    runtime.debug_update_mission(RelayCore::physics::make_player_aabb({2.1F, 0.01F, 3.5F}, 0.3F, 1.8F));
    CHECK(runtime.mission_status().current_step == 9);
    CHECK_FALSE(runtime.mission_status().completed);
    runtime.debug_update_mission(RelayCore::physics::make_player_aabb({0, 0.01F, 9}, 0.3F, 1.8F));
    CHECK(runtime.mission_status().completed);
}

TEST_CASE("power restoration repeated death and disk continue preserve progress at each power phase")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::core::EngineConfig config;
    config.gameplay.level_path = (repo_root() / "assets/levels/power_restoration.toml").string();
    const auto session = RelayGame::LevelSessionBuilder{config.gameplay}.build();
    const std::array
        actions{"spare_fuse", "maintenance_isolator", "bus_relay", "power_breaker", "filter_purge", "cooling_control"};
    for (const std::size_t phase : {0U, 4U, 6U})
    {
        CAPTURE(phase);
        RelayGame::GameWorldRuntime runtime;
        runtime.load_level_session(config, session);
        for (std::size_t i = 0; i < phase; ++i)
        {
            const auto index = find_usable_index(runtime, [&](const auto& usable) {
                return usable.name == actions[i];
            });
            static_cast<void>(runtime.debug_apply_usable_interaction(index));
            runtime.debug_update_mission(RelayCore::physics::make_player_aabb({7, .01F, 8}, .3F, 1.8F));
        }
        if (phase > 0)
        {
            const glm::vec3 eye{7.5F, 1.62F, 26};
            for (int shot = 0; shot < 3; ++shot)
            {
                runtime.debug_tick_combat(.3F);
                REQUIRE(
                    runtime.debug_apply_player_fire(eye, glm::normalize(runtime.turrets()[1].origin - eye)).hit_turret
                );
            }
            REQUIRE(runtime.turrets()[1].destroyed);
            runtime.debug_tick_combat(.3F);
            static_cast<void>(runtime.debug_apply_player_fire({20, .6F, 34}, {1, 0, 0}));
            REQUIRE(runtime.enemies()[0].state.health < session.enemies[0].max_health);
        }
        const auto before = runtime.capture_saved_game(60, true);
        for (int death = 1; death <= 2; ++death)
        {
            runtime.debug_set_player_position({6.8F, .01F, 18});
            const auto bounds = RelayCore::physics::make_player_aabb(runtime.debug_player_position(), .3F, 1.8F);
            REQUIRE(runtime.debug_apply_combat_threats(bounds, runtime.debug_player_eye_position(), 100).player_died);
            CHECK_FALSE(runtime.debug_advance_player_respawn(0));
            REQUIRE(runtime.debug_advance_player_respawn(2));
            CHECK(runtime.player_combat_state().alive);
            CHECK(runtime.player_combat_state().health == runtime.player_combat_state().max_health);
            CHECK(runtime.player_combat_state().deaths == death);
            CHECK(runtime.debug_player_position() == session.spawn_position);
            const auto after = runtime.capture_saved_game(60, true);
            CHECK(after.world.inventory_item_ids == before.world.inventory_item_ids);
            CHECK(after.world.activated_target_ids == before.world.activated_target_ids);
            CHECK(after.world.mission.current_step == before.world.mission.current_step);
            for (std::size_t i = 0; i < after.world.doors.size(); ++i)
            {
                if (before.world.doors[i].state == DoorState::Closed)
                    CHECK(after.world.doors[i].state == DoorState::Closed);
                else
                    CHECK(
                        (after.world.doors[i].state == DoorState::Opening
                         || after.world.doors[i].state == DoorState::Open)
                    );
                CHECK(after.world.doors[i].open_fraction >= before.world.doors[i].open_fraction);
            }
            for (std::size_t i = 0; i < after.world.turrets.size(); ++i)
            {
                CHECK(after.world.turrets[i].destroyed == before.world.turrets[i].destroyed);
                CHECK(after.world.turrets[i].health == before.world.turrets[i].health);
            }
            REQUIRE(after.world.usables.size() == before.world.usables.size());
            for (std::size_t i = 0; i < after.world.usables.size(); ++i)
            {
                CHECK(after.world.usables[i].consumed == before.world.usables[i].consumed);
                CHECK(after.world.usables[i].locked == before.world.usables[i].locked);
            }
            REQUIRE(after.enemies.size() == before.enemies.size());
            for (std::size_t i = 0; i < after.enemies.size(); ++i)
                CHECK(after.enemies[i].health == before.enemies[i].health);
            const auto path = temp.path() / "phase.toml";
            RelayGame::write_saved_game(path, after);
            RelayGame::GameWorldRuntime resumed;
            resumed.load_level_session(config, session);
            resumed.restore_saved_game(RelayGame::load_saved_game(path));
            CHECK(resumed.player_combat_state().alive);
            CHECK(resumed.player_combat_state().deaths == death);
            CHECK(resumed.debug_player_position() == session.spawn_position);
            CHECK(resumed.mission_status().current_step == runtime.mission_status().current_step);
            runtime = std::move(resumed);
        }
    }
}

TEST_CASE("power restoration return passage opens with cooling and remains walkable without jumping")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::core::EngineConfig config;
    config.gameplay.level_path = (repo_root() / "assets/levels/power_restoration.toml").string();
    const auto session = RelayGame::LevelSessionBuilder{config.gameplay}.build();
    RelayGame::GameWorldRuntime runtime;
    runtime.load_level_session(config, session);
    std::vector<RelayCore::physics::Aabb> blockers;
    for (const auto& solid : session.level.solids)
        blockers.push_back({solid.mins, solid.maxs});
    const auto static_count = blockers.size();
    blockers.push_back(RelayCore::gameplay::current_door_bounds(runtime.doors()[1]));
    auto position = glm::vec3{7, 0.01F, 8};
    for (int i = 0; i < 120; ++i)
        position = RelayCore::physics::move_and_collide(
                       RelayCore::test::WorldQueryFixture{blockers},
                       position,
                       {-2, 0, 0},
                       1.0F / 60,
                       {},
                       true
        )
                       .position;
    CHECK(position.x > 5.3F);
    for (const auto name :
         {"spare_fuse", "maintenance_isolator", "bus_relay", "power_breaker", "filter_purge", "cooling_control"})
    {
        const auto index = find_usable_index(runtime, [name](const auto& usable) {
            return usable.name == name;
        });
        static_cast<void>(runtime.debug_apply_usable_interaction(index));
        runtime.debug_update_mission(RelayCore::physics::make_player_aabb({7, 0.01F, 8}, 0.3F, 1.8F));
    }
    auto doors = runtime.doors();
    CHECK(doors[1].state == DoorState::Opening);
    static_cast<void>(RelayCore::gameplay::update_doors(5, {}, doors));
    CHECK(doors[1].state == DoorState::Open);
    blockers.resize(static_count);
    for (const auto& door : doors)
        if (RelayCore::gameplay::is_door_blocking(door))
            blockers.push_back(RelayCore::gameplay::current_door_bounds(door));
    const std::array route{glm::vec3{7, 0.01F, 8}, glm::vec3{3.5F, 0.01F, 8}, glm::vec3{3.5F, 0.01F, 3.5F}};
    for (std::size_t i = 1; i < route.size(); ++i)
    {
        position = route[i - 1];
        const auto velocity = (route[i] - position) * 0.5F;
        for (int frame = 0; frame < 120; ++frame)
            position = RelayCore::physics::move_and_collide(
                           RelayCore::test::WorldQueryFixture{blockers},
                           position,
                           velocity,
                           1.0F / 60,
                           {},
                           true
            )
                           .position;
        CHECK(glm::distance(position, route[i]) < 0.03F);
    }
    runtime.debug_set_player_position({3.5F, 0.01F, 7});
    runtime.debug_update_mission(RelayCore::physics::make_player_aabb(runtime.debug_player_position(), 0.3F, 1.8F));
    const auto path = temp.path() / "progress.toml";
    RelayGame::write_saved_game(path, runtime.capture_saved_game(123, true));
    RelayGame::GameWorldRuntime resumed;
    resumed.load_level_session(config, session);
    const auto save = RelayGame::load_saved_game(path);
    resumed.restore_saved_game(save);
    CHECK(resumed.debug_player_position() == runtime.debug_player_position());
    CHECK(resumed.inventory_item_ids() == runtime.inventory_item_ids());
    CHECK(resumed.activated_target_ids() == runtime.activated_target_ids());
    CHECK(resumed.mission_status().title == "Открыть выход");
    REQUIRE(resumed.ending());
    CHECK(resumed.ending()->closing_door == 0);
    auto edited = session;
    edited.level.solids[0].maxs.z += 0.2F;
    RelayGame::GameWorldRuntime incompatible;
    incompatible.load_level_session(config, edited);
    CHECK_THROWS(incompatible.restore_saved_game(save));
    CHECK(incompatible.inventory_item_ids().empty());
}

TEST_CASE("power restoration return turret threatens the player only after the cooling door opens")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::core::GameplayConfig config{};
    config.level_path = (repo_root() / "assets/levels/power_restoration.toml").string();
    const auto session = RelayGame::LevelSessionBuilder{config}.build();
    REQUIRE(session.combat.turrets.size() == 2);
    CHECK(session.combat.turrets[0].yaw == doctest::Approx(90.0F));

    auto doors = session.doors;
    auto turrets = session.combat.turrets;
    PlayerCombatState player{};
    const auto bounds = RelayCore::physics::make_player_aabb({6.4F, 0.01F, 8.5F}, 0.3F, 1.8F);
    const glm::vec3 eye{6.4F, 1.6F, 8.5F};
    const auto closed = RelayCore::gameplay::update_combat_threats(
        RelayCore::test::WorldQueryFixture{session.level, doors},
        bounds,
        eye,
        player,
        turrets,
        {},
        0.1F
    );
    CHECK(closed.turret_shots == 0);
    CHECK_FALSE(turrets[0].warming_up);

    doors[1].state = DoorState::Open;
    doors[1].open_fraction = 1.0F;
    const auto sighted = RelayCore::gameplay::update_combat_threats(
        RelayCore::test::WorldQueryFixture{session.level, doors},
        bounds,
        eye,
        player,
        turrets,
        {},
        0.1F
    );
    CHECK(sighted.turret_shots == 0);
    CHECK(turrets[0].warming_up);
    RelayCore::gameplay::tick_combat_state(0.8F, player, turrets);
    const auto exposed = RelayCore::gameplay::update_combat_threats(
        RelayCore::test::WorldQueryFixture{session.level, doors},
        bounds,
        eye,
        player,
        turrets,
        {},
        0.1F
    );
    CHECK(exposed.turret_shots == 1);
    CHECK(exposed.damage_taken == doctest::Approx(8.0F));
    const auto shot = RelayCore::gameplay::apply_player_fire(
        RelayCore::test::WorldQueryFixture{session.level, doors},
        eye,
        glm::normalize(turrets[0].origin - eye),
        player,
        turrets
    );
    CHECK(shot.fired);
    CHECK(shot.hit_turret);
    const auto exit_bounds = RelayCore::physics::make_player_aabb({0, .01F, 3.5F}, .3F, 1.8F);
    RelayCore::gameplay::tick_combat_state(2, player, turrets);
    const auto escaped = RelayCore::gameplay::update_combat_threats(
        RelayCore::test::WorldQueryFixture{session.level, doors},
        exit_bounds,
        {0, 1.6F, 3.5F},
        player,
        turrets,
        {},
        .1F
    );
    CHECK_FALSE(turrets[0].destroyed);
    CHECK_FALSE(turrets[0].warming_up);
    CHECK(escaped.damage_taken == 0);
    CHECK(escaped.turret_shots == 0);
}

TEST_CASE("power restoration service route is walkable in both directions")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::core::GameplayConfig config{};
    config.level_path = (repo_root() / "assets/levels/power_restoration.toml").string();
    const auto session = RelayGame::LevelSessionBuilder{config}.build();
    std::vector<RelayCore::physics::Aabb> blockers;
    for (const auto& solid : session.level.solids)
    {
        blockers.push_back({solid.mins, solid.maxs});
    }
    blockers.push_back(RelayCore::gameplay::current_door_bounds(session.doors.front()));
    for (const float x : {11.5F, 12.5F, 13.5F})
    {
        auto position = glm::vec3{x, 0.01F, 0};
        for (int frame = 0; frame < 120; ++frame)
            position = RelayCore::physics::move_and_collide(
                           RelayCore::test::WorldQueryFixture{blockers},
                           position,
                           {0, 0, -3.0F},
                           1.0F / 60.0F,
                           {},
                           true
            )
                           .position;
        CAPTURE(x);
        CHECK(position.z > -1.1F);
        CHECK(position.z < -0.9F);
    }
    const std::array route{
        session.spawn_position,         glm::vec3{-2.3F, 0.01F, -0.5F}, glm::vec3{0.0F, 0.01F, 0.0F},
        glm::vec3{12.5F, 0.01F, 0.0F},  glm::vec3{12.5F, 0.01F, 20.5F}, glm::vec3{13.0F, 0.01F, 27.0F},
        glm::vec3{13.0F, 0.01F, 35.0F}, glm::vec3{15.5F, 0.01F, 37.0F}, glm::vec3{13.0F, 0.01F, 35.0F},
        glm::vec3{12.5F, 0.01F, 20.5F}, glm::vec3{14.3F, 0.01F, 20.5F}, glm::vec3{12.5F, 0.01F, 20.5F},
        glm::vec3{12.5F, 0.01F, 8.0F},  glm::vec3{7.0F, 0.01F, 8.0F},   glm::vec3{8.3F, 0.01F, 8.0F},
        glm::vec3{8.3F, 0.01F, 12.0F},  glm::vec3{8.2F, 0.01F, 20.0F},  glm::vec3{7.5F, 0.01F, 27.0F},
        glm::vec3{7.5F, 0.01F, 29.3F},  glm::vec3{3.2F, 0.01F, 29.3F},  glm::vec3{7.5F, 0.01F, 29.3F},
        glm::vec3{7.5F, 0.01F, 27.0F},  glm::vec3{8.2F, 0.01F, 20.0F},  glm::vec3{8.3F, 0.01F, 12.0F},
        glm::vec3{8.3F, 0.01F, 8.0F},   glm::vec3{7.0F, 0.01F, 8.0F},   glm::vec3{7.0F, 0.01F, 6.6F}
    };
    for (const bool reverse : {false, true})
    {
        for (std::size_t index = 1; index < route.size(); ++index)
        {
            const auto start = reverse ? route[route.size() - index] : route[index - 1];
            const auto finish = reverse ? route[route.size() - index - 1] : route[index];
            auto position = start;
            const auto velocity = (finish - start) * 0.5F;
            for (int frame = 0; frame < 120; ++frame)
            {
                position = RelayCore::physics::move_and_collide(
                               RelayCore::test::WorldQueryFixture{blockers},
                               position,
                               velocity,
                               1.0F / 60.0F,
                               {},
                               true
                )
                               .position;
            }
            CAPTURE(index);
            CAPTURE(reverse);
            CHECK(glm::distance(glm::vec2{position.x, position.z}, glm::vec2{finish.x, finish.z}) < 0.03F);
        }
    }
}

TEST_CASE("power restoration corridor edges keep the player grounded")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::core::GameplayConfig config{};
    config.level_path = (repo_root() / "assets/levels/power_restoration.toml").string();
    const auto session = RelayGame::LevelSessionBuilder{config}.build();
    std::vector<RelayCore::physics::Aabb> blockers;
    for (const auto& solid : session.level.solids)
        blockers.push_back({solid.mins, solid.maxs});

    for (const glm::vec2 point : {glm::vec2{13.0F, 27.0F}, glm::vec2{9.72F, -0.99F}})
    {
        RelayCore::physics::PlayerMotionState state{};
        state.position = {point.x, 0.01F, point.y};
        for (int frame = 0; frame < 120; ++frame)
            RelayCore::physics::update_grounded(
                state,
                {},
                0,
                1.0F / 60.0F,
                RelayCore::test::WorldQueryFixture{blockers},
                {}
            );
        CAPTURE(point.x);
        CAPTURE(point.y);
        CHECK(state.grounded);
        CHECK(state.position.y >= -0.001F);
    }
}

TEST_CASE("power restoration filtration threatens the player past the spill")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::core::GameplayConfig config{};
    config.level_path = (repo_root() / "assets/levels/power_restoration.toml").string();
    const auto session = RelayGame::LevelSessionBuilder{config}.build();
    REQUIRE(session.combat.hazards.size() == 1);
    REQUIRE(session.combat.turrets.size() == 2);
    auto turrets = session.combat.turrets;
    PlayerCombatState player{};
    const auto spill = RelayCore::physics::make_player_aabb({6.8F, 0.01F, 18}, 0.3F, 1.8F);
    const auto hazard = RelayCore::gameplay::update_combat_threats(
        RelayCore::test::WorldQueryFixture{session.level, session.doors},
        spill,
        {6.8F, 1.62F, 18},
        player,
        turrets,
        session.combat.hazards,
        0.5F
    );
    CHECK(hazard.hazard_contacts == 1);
    CHECK(hazard.damage_taken > 0);
    CHECK(hazard.turret_shots == 0);

    const auto approach = RelayCore::physics::make_player_aabb({7.5F, 0.01F, 26}, 0.3F, 1.8F);
    const auto sighted = RelayCore::gameplay::update_combat_threats(
        RelayCore::test::WorldQueryFixture{session.level, session.doors},
        approach,
        {7.5F, 1.62F, 26},
        player,
        turrets,
        {},
        0.1F
    );
    CHECK(sighted.turret_shots == 0);
    CHECK(turrets[1].warming_up);
    CHECK(sighted.turret_windups == 1);
    const auto retreat = RelayCore::physics::make_player_aabb({7.5F, .01F, 22}, .3F, 1.8F);
    RelayCore::gameplay::tick_combat_state(1, player, turrets);
    const auto safe = RelayCore::gameplay::update_combat_threats(
        RelayCore::test::WorldQueryFixture{session.level, session.doors},
        retreat,
        {7.5F, 1.62F, 22},
        player,
        turrets,
        {},
        .1F
    );
    CHECK(safe.turret_shots == 0);
    CHECK_FALSE(turrets[1].warming_up);
    for (int i = 0; i < 3; ++i)
    {
        RelayCore::gameplay::tick_combat_state(.3F, player, turrets);
        const glm::vec3 eye{7.5F, 1.62F, 26};
        const auto hit = RelayCore::gameplay::apply_player_fire(
            RelayCore::test::WorldQueryFixture{session.level, session.doors},
            eye,
            glm::normalize(turrets[1].origin - eye),
            player,
            turrets
        );
        CHECK(hit.hit_turret);
    }
    CHECK(turrets[1].destroyed);
    CHECK(
        RelayCore::gameplay::update_combat_threats(
            RelayCore::test::WorldQueryFixture{session.level, session.doors},
            approach,
            {7.5F, 1.62F, 26},
            player,
            turrets,
            {},
            1
        )
            .turret_shots
        == 0
    );
}

TEST_CASE("power restoration reserve room stays sealed until the isolator opens it")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::core::GameplayConfig config{};
    config.level_path = (repo_root() / "assets/levels/power_restoration.toml").string();
    const auto session = RelayGame::LevelSessionBuilder{config}.build();
    std::vector<RelayCore::physics::Aabb> blockers;
    for (const auto& solid : session.level.solids)
        blockers.push_back({solid.mins, solid.maxs});
    auto doors = session.doors;
    const auto walk_to_relay = [&] {
        auto position = glm::vec3{15.5F, 0.01F, 34.0F};
        for (int frame = 0; frame < 180; ++frame)
            position = RelayCore::physics::move_and_collide(
                           RelayCore::test::WorldQueryFixture{blockers},
                           position,
                           {3, 0, 0},
                           1.0F / 60,
                           {},
                           true
            )
                           .position;
        return position;
    };
    for (const auto& door : doors)
        blockers.push_back(RelayCore::gameplay::current_door_bounds(door));
    CHECK(walk_to_relay().x < 16.8F);

    std::vector<std::string> inventory, activated, signaled;
    auto usables = session.usables;
    const auto use = [&](std::string_view name) {
        const auto it = std::ranges::find(usables, name, &RuntimeUsable::name);
        REQUIRE(it != usables.end());
        return RelayGame::apply_usable_interaction(*it, doors, usables, inventory, activated, signaled);
    };
    CHECK(use("spare_fuse").kind == UseInteractionOutcome::PickedUpItem);
    CHECK(use("maintenance_isolator").kind == UseInteractionOutcome::ActivatedTarget);
    static_cast<void>(RelayCore::gameplay::update_doors(5, {}, doors));
    blockers.resize(session.level.solids.size());
    for (const auto& door : doors)
        if (RelayCore::gameplay::is_door_blocking(door))
            blockers.push_back(RelayCore::gameplay::current_door_bounds(door));
    CHECK(walk_to_relay().x > 23.0F);
}

TEST_CASE("power restoration final threshold needs no jump at walking speed")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};
    RelayCore::core::GameplayConfig config;
    config.level_path = (repo_root() / "assets/levels/power_restoration.toml").string();
    auto session = RelayGame::LevelSessionBuilder{config}.build();
    auto& door = session.doors.front();
    door.state = RelayCore::gameplay::DoorState::Open;
    door.open_fraction = 1;
    std::vector<RelayCore::physics::Aabb> blockers;
    for (const auto& solid : session.level.solids)
        blockers.push_back({solid.mins, solid.maxs});
    if (RelayCore::gameplay::is_door_blocking(door))
        blockers.push_back(RelayCore::gameplay::current_door_bounds(door));
    for (float x : {-0.7F, 0.0F, 0.7F})
    {
        glm::vec3 position{x, 0.001F, 3.5F};
        for (int tick = 0; tick < 180; ++tick)
            position = RelayCore::physics::move_and_collide(
                           RelayCore::test::WorldQueryFixture{blockers},
                           position,
                           {0, -0.1F, 3.5F},
                           1.0F / 120,
                           {},
                           true
            )
                           .position;
        CAPTURE(x);
        CHECK(position.z > 8.5F);
        CHECK(position.y < 0.03F);
    }
}

TEST_CASE("all shipped maps build clean runtime content")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};

    const std::filesystem::path root = repo_root();
    const std::filesystem::path asset_root = root / "assets";
    const std::filesystem::path levels_dir = asset_root / "levels";

    for (const ShippedMapExpectation& expectation : kShippedMaps)
    {
        INFO(expectation.file_name);

        const std::filesystem::path level_path = levels_dir / expectation.file_name;
        REQUIRE(std::filesystem::exists(level_path));

        const RelayCore::BlockoutLevel level = RelayCore::load_blockout_level(level_path);
        CHECK(level.solids.size() >= expectation.min_solids);
        CHECK(level.entities.size() >= expectation.min_entities);
        CHECK(
            std::count_if(
                level.entities.begin(),
                level.entities.end(),
                [](const RelayCore::EntitySpawnData& entity) {
                    return entity.classname == "player_spawn";
                }
            )
            == 1
        );
        CHECK(
            std::count_if(
                level.entities.begin(),
                level.entities.end(),
                [](const RelayCore::EntitySpawnData& entity) {
                    return entity.classname == "render_light";
                }
            )
            > 0
        );

        std::vector<RuntimeDoor> doors;
        std::vector<RuntimeTrigger> triggers;
        std::vector<RuntimeUsable> usables;
        RelayCore::gameplay::build_runtime_interactions(level, doors, triggers, usables);
        const auto combat = RelayCore::gameplay::build_runtime_combat(level);

        CHECK(doors.size() == expectation.door_count);
        CHECK(triggers.size() == expectation.trigger_count);
        CHECK(
            std::count_if(
                usables.begin(),
                usables.end(),
                [](const RuntimeUsable& usable) {
                    return usable.action_kind == UsableActionKind::PickupItem;
                }
            )
            == expectation.pickup_count
        );
        CHECK(combat.turrets.size() == expectation.turret_count);

        const auto audio_zones = RelayGame::build_audio_zones(level, asset_root);
        CHECK(audio_zones.size() == expectation.audio_zone_count);
        for (const RelayGame::RuntimeAudioZone& zone : audio_zones)
        {
            CHECK(std::filesystem::exists(zone.loop_path));
        }

        const MissionDefinition definition = MissionBuilder{level}.build();
        CHECK(definition.steps.size() == expectation.mission_step_count);

        RelayCore::core::GameplayConfig config{};
        config.level_path = level_path.string();
        const RelayGame::LevelSession session = RelayGame::LevelSessionBuilder{config}.build();
        CHECK(session.doors.size() == expectation.door_count);
        CHECK(session.triggers.size() == expectation.trigger_count);
        CHECK(session.authoring_warnings.empty());
        CHECK_FALSE(session.runtime_registry.empty());
        CHECK(session.combat.turrets.size() == expectation.turret_count);
        CHECK(session.mission_definition.steps.size() == expectation.mission_step_count);
        CHECK(session.audio_zones.size() == expectation.audio_zone_count);
        for (const RelayGame::RuntimeAudioZone& zone : session.audio_zones)
        {
            CHECK(std::filesystem::exists(zone.loop_path));
        }
    }

    RelayCore::log::shutdown();
}

TEST_CASE("stress_room_02 preserves the second acceptance flow")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};

    const std::filesystem::path level_path = repo_root() / "assets" / "levels" / "stress_room_02.toml";
    const RelayCore::BlockoutLevel level = RelayCore::load_blockout_level(level_path);

    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeTrigger> triggers;
    std::vector<RuntimeUsable> usables;
    RelayCore::gameplay::build_runtime_interactions(level, doors, triggers, usables);

    const MissionDefinition definition = MissionBuilder{level}.build();
    REQUIRE(definition.steps.size() == 6);
    CHECK(definition.steps[0].target == "south_bulkhead");
    CHECK(definition.steps[2].target == "turbine_badge");
    CHECK(definition.steps[3].target == "spine_checkpoint_event");
    CHECK(definition.steps[4].target == "north_bulkhead");
    CHECK(definition.steps[5].target == "turbine_sync_console");

    auto south_panel = std::find_if(usables.begin(), usables.end(), [](const RuntimeUsable& usable) {
        return usable.label == "Spine access";
    });
    auto pickup = std::find_if(usables.begin(), usables.end(), [](const RuntimeUsable& usable) {
        return usable.action_kind == UsableActionKind::PickupItem && usable.item_id == "turbine_badge";
    });
    auto reader = std::find_if(usables.begin(), usables.end(), [](const RuntimeUsable& usable) {
        return usable.label == "Authorize turbine access";
    });
    auto terminal = std::find_if(usables.begin(), usables.end(), [](const RuntimeUsable& usable) {
        return usable.action_kind == UsableActionKind::ActivateTarget && usable.target == "turbine_sync_console";
    });
    auto south_door = std::find_if(doors.begin(), doors.end(), [](const RuntimeDoor& door) {
        return door.name == "south_bulkhead";
    });
    auto north_door = std::find_if(doors.begin(), doors.end(), [](const RuntimeDoor& door) {
        return door.name == "north_bulkhead";
    });
    auto checkpoint_trigger = std::find_if(triggers.begin(), triggers.end(), [](const RuntimeTrigger& trigger) {
        return trigger.target == "spine_checkpoint_event";
    });

    REQUIRE(south_panel != usables.end());
    REQUIRE(pickup != usables.end());
    REQUIRE(reader != usables.end());
    REQUIRE(terminal != usables.end());
    REQUIRE(south_door != doors.end());
    REQUIRE(north_door != doors.end());
    REQUIRE(checkpoint_trigger != triggers.end());

    std::vector<std::string> inventory_items;
    std::vector<std::string> activated_targets;
    std::vector<std::string> signaled_targets;
    MissionRuntime mission_runtime{};
    mission_runtime.reset(definition);

    const RelayCore::physics::Aabb spawn_bounds =
        AcceptanceHarness::make_actor_bounds({0.0F, 1.0F, -36.0F}, {0.3F, 0.9F, 0.3F});
    CHECK(mission_runtime.status().title == "Open the south bulkhead");

    CHECK(
        RelayGame::apply_usable_interaction(
            *south_panel,
            doors,
            usables,
            inventory_items,
            activated_targets,
            signaled_targets
        )
            .kind
        == UseInteractionOutcome::ActivatedTarget
    );
    CHECK(south_door->state == DoorState::Opening);

    mission_runtime.update(spawn_bounds, doors, inventory_items, activated_targets, signaled_targets);
    CHECK(mission_runtime.status().title == "Reach the cargo junction");

    const glm::vec3 zone_center = AcceptanceHarness::bounds_center(definition.steps[1].bounds);
    mission_runtime.update(
        AcceptanceHarness::make_actor_bounds(zone_center, {0.3F, 0.9F, 0.3F}),
        doors,
        inventory_items,
        activated_targets,
        signaled_targets
    );
    CHECK(mission_runtime.status().title == "Pick up the turbine badge");

    CHECK(
        RelayGame::apply_usable_interaction(
            *pickup,
            doors,
            usables,
            inventory_items,
            activated_targets,
            signaled_targets
        )
            .kind
        == UseInteractionOutcome::PickedUpItem
    );
    CHECK(pickup->consumed);
    mission_runtime.update(spawn_bounds, doors, inventory_items, activated_targets, signaled_targets);
    CHECK(mission_runtime.status().title == "Cross the north checkpoint");

    InteractionEvent last_event{};
    const RelayCore::physics::Aabb checkpoint_bounds =
        AcceptanceHarness::make_actor_bounds({0.0F, 1.0F, 74.0F}, {0.3F, 0.9F, 0.3F});
    CHECK(
        RelayCore::gameplay::update_trigger_overlaps(
            checkpoint_bounds,
            triggers,
            doors,
            usables,
            activated_targets,
            signaled_targets,
            last_event
        )
        == 1
    );
    CHECK(checkpoint_trigger->fired);
    CHECK(last_event.target == "spine_checkpoint_event");
    CHECK(
        RelayCore::gameplay::update_trigger_overlaps(
            checkpoint_bounds,
            triggers,
            doors,
            usables,
            activated_targets,
            signaled_targets,
            last_event
        )
        == 0
    );

    mission_runtime.update(checkpoint_bounds, doors, inventory_items, activated_targets, signaled_targets);
    CHECK(mission_runtime.status().title == "Authorize turbine hall access");

    CHECK(
        RelayGame::apply_usable_interaction(
            *reader,
            doors,
            usables,
            inventory_items,
            activated_targets,
            signaled_targets
        )
            .kind
        == UseInteractionOutcome::ActivatedTarget
    );
    CHECK(reader->consumed);
    CHECK(north_door->state == DoorState::Opening);

    mission_runtime.update(checkpoint_bounds, doors, inventory_items, activated_targets, signaled_targets);
    CHECK(mission_runtime.status().title == "Stabilize the turbine relay");

    CHECK(
        RelayGame::apply_usable_interaction(
            *terminal,
            doors,
            usables,
            inventory_items,
            activated_targets,
            signaled_targets
        )
            .kind
        == UseInteractionOutcome::ActivatedTarget
    );
    mission_runtime.update(checkpoint_bounds, doors, inventory_items, activated_targets, signaled_targets);

    const auto status = mission_runtime.status();
    CHECK(status.completed);
    CHECK(status.current_step == definition.steps.size());
    CHECK(status.title == "Turbine hall stabilized");

    RelayCore::log::shutdown();
}

TEST_CASE("test_room preserves the combat acceptance slice")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};

    const std::filesystem::path level_path = repo_root() / "assets" / "levels" / "test_room.toml";
    auto scenario = AcceptanceHarness::TestRoomScenario::build(level_path);

    REQUIRE(scenario.find_outer_panel() != nullptr);
    REQUIRE(scenario.find_pickup() != nullptr);
    REQUIRE(scenario.find_reader() != nullptr);
    REQUIRE(scenario.find_terminal() != nullptr);
    REQUIRE(scenario.find_outer_door() != nullptr);
    REQUIRE(scenario.find_inner_door() != nullptr);
    REQUIRE(scenario.turrets.size() == 1);
    CHECK(scenario.find_terminal()->locked);

    CHECK(scenario.mission_runtime.status().title == "Открыть внешний шлюз");
    CHECK(scenario.open_outer_bulkhead() == UseInteractionOutcome::ActivatedTarget);
    CHECK(scenario.find_outer_door()->state == DoorState::Opening);

    scenario.update_mission_at_spawn();
    CHECK(scenario.mission_runtime.status().title == "Войти в шлюзовую камеру");

    scenario.update_mission_at_airlock();
    CHECK(scenario.mission_runtime.status().title == "Забрать ключ-карту лаборатории");

    CHECK(scenario.pick_up_lab_keycard() == UseInteractionOutcome::PickedUpItem);
    CHECK(scenario.find_pickup()->consumed);
    CHECK(scenario.mission_runtime.status().title == "Разблокировать внутренний шлюз");

    CHECK(scenario.authorize_inner_bulkhead() == UseInteractionOutcome::ActivatedTarget);
    CHECK(scenario.find_reader()->consumed);
    CHECK(scenario.find_inner_door()->state == DoorState::Opening);
    CHECK(scenario.mission_runtime.status().title == "Стабилизировать ядро ретранслятора");

    CHECK(scenario.activate_terminal() == UseInteractionOutcome::Locked);
    CHECK(scenario.find_terminal()->locked);

    const AcceptanceHarness::TurretKillSequence shots = scenario.destroy_turret();
    CHECK(shots.first_shot.fired);
    CHECK(shots.first_shot.hit_turret);
    CHECK_FALSE(shots.first_shot.killed_turret);
    CHECK(shots.second_shot.fired);
    CHECK(shots.second_shot.hit_turret);
    CHECK(shots.second_shot.killed_turret);
    CHECK(scenario.turrets[0].destroyed);
    CHECK_FALSE(scenario.find_terminal()->locked);
    CHECK(
        std::find(scenario.signaled_targets.begin(), scenario.signaled_targets.end(), "lab_console_lock")
        != scenario.signaled_targets.end()
    );

    CHECK(scenario.activate_terminal() == UseInteractionOutcome::ActivatedTarget);
    scenario.update_mission_at_spawn();

    const auto post_console_status = scenario.mission_runtime.status();
    CHECK_FALSE(post_console_status.completed);
    CHECK(post_console_status.current_step == 6);
    CHECK(post_console_status.title == "Выйти к реакторной галерее");

    scenario.update_mission_at_gallery();

    const auto status = scenario.mission_runtime.status();
    CHECK(status.completed);
    CHECK(status.current_step == scenario.session.mission_definition.steps.size());
    CHECK(status.title == "Проход к реакторной галерее открыт");

    RelayCore::log::shutdown();
}

TEST_CASE("test_room level session preserves the typed acceptance contract")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};

    const std::filesystem::path level_path = repo_root() / "assets" / "levels" / "test_room.toml";
    RelayCore::core::GameplayConfig config{};
    config.level_path = level_path.string();

    const RelayGame::LevelSession session = RelayGame::LevelSessionBuilder{config}.build();

    CHECK(session.level_path == level_path);
    CHECK(session.spawn_view_angles.has_value());
    CHECK(session.doors.size() == 2);
    CHECK(session.triggers.size() == 1);
    CHECK(session.usables.size() >= 4);
    CHECK(session.combat.turrets.size() == 1);
    CHECK_FALSE(session.combat.hazards.empty());
    CHECK(session.mission_definition.steps.size() == 6);
    CHECK(session.mission_definition.steps[0].target == "intake/outer_bulkhead");
    CHECK(session.mission_definition.steps[4].target == "lab_relay_console");
    CHECK(session.audio_zones.size() == 4);
    CHECK(session.authoring_warnings.empty());
    CHECK_FALSE(session.runtime_registry.empty());

    const auto console = std::find_if(session.usables.begin(), session.usables.end(), [](const RuntimeUsable& usable) {
        return usable.name == "lab_console_lock";
    });
    REQUIRE(console != session.usables.end());
    CHECK(console->locked);

    for (const RelayGame::RuntimeAudioZone& zone : session.audio_zones)
    {
        CHECK(std::filesystem::exists(zone.loop_path));
    }

    RelayCore::log::shutdown();
}

TEST_CASE("test_room snapshot restore preserves mid-slice runtime progress")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};

    const std::filesystem::path level_path = repo_root() / "assets" / "levels" / "test_room.toml";
    auto scenario = AcceptanceHarness::TestRoomScenario::build(level_path);

    REQUIRE(scenario.find_outer_panel() != nullptr);
    REQUIRE(scenario.find_pickup() != nullptr);
    REQUIRE(scenario.find_reader() != nullptr);
    REQUIRE(scenario.find_terminal() != nullptr);
    REQUIRE(scenario.find_outer_door() != nullptr);
    REQUIRE(scenario.find_inner_door() != nullptr);
    REQUIRE(scenario.turrets.size() == 1);

    scenario.progress_to_turret_unlock_state();
    CHECK(scenario.mission_runtime.status().title == "Стабилизировать ядро ретранслятора");
    CHECK(scenario.turrets[0].destroyed);
    CHECK_FALSE(scenario.find_terminal()->locked);

    const auto snapshot = scenario.capture_snapshot(77);
    scenario.corrupt_runtime_state_after_snapshot();

    const auto restore = scenario.restore_snapshot(snapshot);

    CHECK(restore.restored);
    CHECK(restore.restored_doors == 2);
    CHECK(restore.restored_usables >= 3);
    CHECK(restore.restored_turrets == 1);
    REQUIRE(scenario.find_outer_door() != nullptr);
    REQUIRE(scenario.find_inner_door() != nullptr);
    REQUIRE(scenario.find_pickup() != nullptr);
    REQUIRE(scenario.find_reader() != nullptr);
    REQUIRE(scenario.find_terminal() != nullptr);
    CHECK(scenario.find_outer_door()->state == DoorState::Opening);
    CHECK(scenario.find_inner_door()->state == DoorState::Opening);
    CHECK(scenario.find_pickup()->consumed);
    CHECK(scenario.find_reader()->consumed);
    CHECK_FALSE(scenario.find_terminal()->locked);
    REQUIRE(scenario.inventory_items.size() == 1);
    CHECK(scenario.inventory_items[0] == "lab_keycard");
    CHECK(
        std::find(scenario.signaled_targets.begin(), scenario.signaled_targets.end(), "lab_console_lock")
        != scenario.signaled_targets.end()
    );
    CHECK(scenario.turrets[0].destroyed);
    CHECK(scenario.player_combat.alive);
    CHECK(scenario.player_combat.kills == 1);
    CHECK(scenario.player_combat.shots_fired == 2);
    CHECK(scenario.mission_runtime.status().title == "Стабилизировать ядро ретранслятора");
    CHECK(scenario.mission_runtime.status().current_step == 5);

    RelayCore::log::shutdown();
}

TEST_CASE("test_room snapshot restore preserves the completed acceptance slice")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};

    const std::filesystem::path level_path = repo_root() / "assets" / "levels" / "test_room.toml";
    auto scenario = AcceptanceHarness::TestRoomScenario::build(level_path);

    scenario.complete_acceptance_slice();
    const auto completed_status = scenario.mission_runtime.status();
    REQUIRE(completed_status.completed);
    REQUIRE(completed_status.current_step == scenario.session.mission_definition.steps.size());

    const auto snapshot = scenario.capture_snapshot(128);
    scenario.corrupt_runtime_state_after_snapshot();
    scenario.mission_runtime.reset(scenario.session.mission_definition);
    scenario.player_combat.kills = 0;
    scenario.player_combat.shots_fired = 0;

    const auto restore = scenario.restore_snapshot(snapshot);

    CHECK(restore.restored);
    CHECK(scenario.mission_runtime.status().completed);
    CHECK(scenario.mission_runtime.status().current_step == scenario.session.mission_definition.steps.size());
    CHECK(scenario.mission_runtime.status().title == "Проход к реакторной галерее открыт");
    CHECK_FALSE(scenario.find_terminal()->locked);
    CHECK(scenario.turrets[0].destroyed);
    CHECK(scenario.player_combat.alive);
    CHECK(scenario.player_combat.kills == 1);
    CHECK(scenario.player_combat.shots_fired == 2);
    REQUIRE(scenario.inventory_items.size() == 1);
    CHECK(scenario.inventory_items[0] == "lab_keycard");

    RelayCore::log::shutdown();
}

TEST_CASE("test_room game world runtime preserves the golden path progression")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};

    const std::filesystem::path level_path = repo_root() / "assets" / "levels" / "test_room.toml";
    RelayCore::core::EngineConfig config{};
    config.gameplay.level_path = level_path.string();

    const RelayGame::LevelSession session = RelayGame::LevelSessionBuilder{config.gameplay}.build();
    RelayGame::GameWorldRuntime runtime{};
    runtime.load_level_session(config, session);

    const std::size_t outer_door_index = find_door_index(runtime, [](const RuntimeDoor& door) {
        return door.name == "intake/outer_bulkhead";
    });
    const std::size_t inner_door_index = find_door_index(runtime, [](const RuntimeDoor& door) {
        return door.name == "inner_bulkhead";
    });
    const std::size_t terminal_index = find_usable_index(runtime, [](const RuntimeUsable& usable) {
        return usable.name == "lab_console_lock";
    });

    const RelayCore::physics::Aabb spawn_bounds =
        AcceptanceHarness::make_actor_bounds({0.0F, 1.0F, -1.4F}, {0.3F, 0.9F, 0.3F});
    const RelayCore::physics::Aabb gallery_bounds = AcceptanceHarness::make_actor_bounds(
        AcceptanceHarness::bounds_center(session.mission_definition.steps.back().bounds),
        {0.3F, 0.9F, 0.3F}
    );

    CHECK(runtime.mission_status().title == "Открыть внешний шлюз");
    progress_test_room_runtime_to_turret_unlock(runtime, session);

    CHECK(runtime.doors()[outer_door_index].state == DoorState::Opening);
    CHECK(runtime.doors()[inner_door_index].state == DoorState::Opening);
    CHECK(runtime.mission_status().title == "Стабилизировать ядро ретранслятора");
    CHECK(runtime.turrets()[0].destroyed);
    CHECK_FALSE(runtime.usables()[terminal_index].locked);
    CHECK(runtime.player_combat_state().shots_fired == 2);
    CHECK(runtime.player_combat_state().kills == 1);
    CHECK(
        std::find(runtime.inventory_item_ids().begin(), runtime.inventory_item_ids().end(), "lab_keycard")
        != runtime.inventory_item_ids().end()
    );
    CHECK(
        std::find(runtime.signaled_target_ids().begin(), runtime.signaled_target_ids().end(), "lab_console_lock")
        != runtime.signaled_target_ids().end()
    );

    CHECK(runtime.debug_apply_usable_interaction(terminal_index).kind == UseInteractionOutcome::ActivatedTarget);
    runtime.debug_update_mission(spawn_bounds);

    const auto post_console_status = runtime.mission_status();
    CHECK_FALSE(post_console_status.completed);
    CHECK(post_console_status.current_step == 6);
    CHECK(post_console_status.title == "Выйти к реакторной галерее");

    runtime.debug_update_mission(gallery_bounds);

    const auto status = runtime.mission_status();
    CHECK(status.completed);
    CHECK(status.current_step == session.mission_definition.steps.size());
    CHECK(status.title == "Проход к реакторной галерее открыт");

    RelayCore::log::shutdown();
}

TEST_CASE("test_room game world runtime snapshot restore preserves mid-slice progression")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};

    const std::filesystem::path level_path = repo_root() / "assets" / "levels" / "test_room.toml";
    RelayCore::core::EngineConfig config{};
    config.gameplay.level_path = level_path.string();

    const RelayGame::LevelSession session = RelayGame::LevelSessionBuilder{config.gameplay}.build();
    RelayGame::GameWorldRuntime runtime{};
    runtime.load_level_session(config, session);
    progress_test_room_runtime_to_turret_unlock(runtime, session);

    const auto snapshot = runtime.capture_runtime_snapshot();

    RelayGame::GameWorldRuntime restored_runtime{};
    restored_runtime.load_level_session(config, RelayGame::LevelSessionBuilder{config.gameplay}.build());
    const auto restore = restored_runtime.restore_runtime_snapshot(snapshot);

    const std::size_t outer_door_index = find_door_index(restored_runtime, [](const RuntimeDoor& door) {
        return door.name == "intake/outer_bulkhead";
    });
    const std::size_t inner_door_index = find_door_index(restored_runtime, [](const RuntimeDoor& door) {
        return door.name == "inner_bulkhead";
    });
    const std::size_t pickup_index = find_usable_index(restored_runtime, [](const RuntimeUsable& usable) {
        return usable.action_kind == UsableActionKind::PickupItem && usable.item_id == "lab_keycard";
    });
    const std::size_t reader_index = find_usable_index(restored_runtime, [](const RuntimeUsable& usable) {
        return usable.label == "Подтвердить допуск";
    });
    const std::size_t terminal_index = find_usable_index(restored_runtime, [](const RuntimeUsable& usable) {
        return usable.name == "lab_console_lock";
    });

    CHECK(restore.restored);
    CHECK(restore.restored_doors == 2);
    CHECK(restore.restored_usables >= 3);
    CHECK(restore.restored_turrets == 1);
    CHECK(restored_runtime.doors()[outer_door_index].state == DoorState::Opening);
    CHECK(restored_runtime.doors()[inner_door_index].state == DoorState::Opening);
    CHECK(restored_runtime.usables()[pickup_index].consumed);
    CHECK(restored_runtime.usables()[reader_index].consumed);
    CHECK_FALSE(restored_runtime.usables()[terminal_index].locked);
    CHECK(restored_runtime.turrets()[0].destroyed);
    CHECK(restored_runtime.player_combat_state().alive);
    CHECK(restored_runtime.player_combat_state().kills == 1);
    CHECK(restored_runtime.player_combat_state().shots_fired == 2);
    CHECK(restored_runtime.mission_status().title == "Стабилизировать ядро ретранслятора");
    CHECK(restored_runtime.mission_status().current_step == 5);
    CHECK(
        std::find(
            restored_runtime.inventory_item_ids().begin(),
            restored_runtime.inventory_item_ids().end(),
            "lab_keycard"
        )
        != restored_runtime.inventory_item_ids().end()
    );
    CHECK(
        std::find(
            restored_runtime.signaled_target_ids().begin(),
            restored_runtime.signaled_target_ids().end(),
            "lab_console_lock"
        )
        != restored_runtime.signaled_target_ids().end()
    );

    RelayCore::log::shutdown();
}

TEST_CASE("test_room game world runtime respawn preserves mid-slice progression")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};

    const std::filesystem::path level_path = repo_root() / "assets" / "levels" / "test_room.toml";
    RelayCore::core::EngineConfig config{};
    config.gameplay.level_path = level_path.string();

    const RelayGame::LevelSession session = RelayGame::LevelSessionBuilder{config.gameplay}.build();
    RelayGame::GameWorldRuntime runtime{};
    runtime.load_level_session(config, session);
    progress_test_room_runtime_to_turret_unlock(runtime, session);

    const std::size_t terminal_index = find_usable_index(runtime, [](const RuntimeUsable& usable) {
        return usable.name == "lab_console_lock";
    });
    const RuntimeHazardVolume& hazard = find_farthest_hazard(session);
    const glm::vec3 hazard_center = AcceptanceHarness::bounds_center(hazard.bounds);
    const glm::vec3 hazard_position{hazard_center.x, session.spawn_position.y, hazard_center.z};
    const RelayCore::physics::Aabb hazard_bounds =
        AcceptanceHarness::make_actor_bounds(hazard_position, {0.3F, 0.9F, 0.3F});

    runtime.debug_set_player_position(hazard_position);
    const glm::vec3 position_before_death = runtime.debug_player_position();
    CHECK(position_before_death.x == doctest::Approx(hazard_position.x));
    CHECK(position_before_death.y == doctest::Approx(hazard_position.y));
    CHECK(position_before_death.z == doctest::Approx(hazard_position.z));

    const auto threat = runtime.debug_apply_combat_threats(hazard_bounds, runtime.debug_player_eye_position(), 6.0F);
    CHECK(threat.player_died);
    CHECK(threat.damage_taken >= runtime.player_combat_state().max_health);
    CHECK(threat.hazard_contacts >= 1);
    CHECK_FALSE(runtime.player_combat_state().alive);
    CHECK(runtime.player_combat_state().deaths == 1);
    CHECK(runtime.player_combat_state().last_damage_source == hazard.label);
    CHECK(runtime.mission_status().title == "Стабилизировать ядро ретранслятора");
    CHECK(runtime.mission_status().current_step == 5);
    CHECK(runtime.turrets()[0].destroyed);
    CHECK_FALSE(runtime.usables()[terminal_index].locked);
    CHECK(
        std::find(runtime.inventory_item_ids().begin(), runtime.inventory_item_ids().end(), "lab_keycard")
        != runtime.inventory_item_ids().end()
    );
    CHECK(
        std::find(runtime.signaled_target_ids().begin(), runtime.signaled_target_ids().end(), "lab_console_lock")
        != runtime.signaled_target_ids().end()
    );

    CHECK_FALSE(runtime.debug_advance_player_respawn(0.60F));
    CHECK_FALSE(runtime.player_combat_state().alive);
    CHECK(runtime.player_combat_state().respawn_remaining_seconds == doctest::Approx(0.60F));

    CHECK(runtime.debug_advance_player_respawn(0.60F));
    CHECK(runtime.player_combat_state().alive);
    CHECK(runtime.player_combat_state().health == doctest::Approx(runtime.player_combat_state().max_health));
    CHECK(runtime.player_combat_state().deaths == 1);
    CHECK(runtime.player_combat_state().last_damage_source.empty());

    const glm::vec3 respawn_position = runtime.debug_player_position();
    CHECK(respawn_position.x == doctest::Approx(session.spawn_position.x));
    CHECK(respawn_position.y == doctest::Approx(session.spawn_position.y));
    CHECK(respawn_position.z == doctest::Approx(session.spawn_position.z));
    CHECK(runtime.mission_status().title == "Стабилизировать ядро ретранслятора");
    CHECK(runtime.mission_status().current_step == 5);
    CHECK(runtime.turrets()[0].destroyed);
    CHECK_FALSE(runtime.usables()[terminal_index].locked);
    CHECK(
        std::find(runtime.inventory_item_ids().begin(), runtime.inventory_item_ids().end(), "lab_keycard")
        != runtime.inventory_item_ids().end()
    );

    RelayCore::log::shutdown();
}

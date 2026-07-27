#include "relaycore/core/engine_config.h"
#include "relaycore/core/log.h"
#include "relaycore/gameplay/combat_runtime.h"
#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/gameplay/mission_runtime.h"
#include "relaycore/gameplay/runtime_model.h"
#include "relaycore/physics/collision.h"
#include "relaycore/world/blockout_loader.h"
#include "relaygame/game_world_runtime.h"
#include "relaygame/level_session_builder.h"
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

namespace
{
    using RelayCore::gameplay::CombatTargetActionKind;
    using RelayCore::gameplay::CombatTargetEvent;
    using RelayCore::gameplay::DoorState;
    using RelayCore::gameplay::InteractionEvent;
    using RelayCore::gameplay::MissionBuilder;
    using RelayCore::gameplay::MissionDefinition;
    using RelayCore::gameplay::MissionRuntime;
    using RelayCore::gameplay::PlayerCombatState;
    using RelayCore::gameplay::RuntimeDoor;
    using RelayCore::gameplay::RuntimeHazardVolume;
    using RelayCore::gameplay::RuntimeTrigger;
    using RelayCore::gameplay::RuntimeTurret;
    using RelayCore::gameplay::RuntimeUsable;
    using RelayCore::gameplay::UsableActionKind;
    using RelayCore::gameplay::UseInteractionOutcome;

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
        RelayCore::gameplay::apply_usable_interaction(
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
        RelayCore::gameplay::apply_usable_interaction(
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
        RelayCore::gameplay::apply_usable_interaction(
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
        RelayCore::gameplay::apply_usable_interaction(
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

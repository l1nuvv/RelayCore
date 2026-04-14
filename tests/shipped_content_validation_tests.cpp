#include "relaycore/core/engine_config.h"
#include "relaycore/core/log.h"
#include "relaycore/gameplay/combat_runtime.h"
#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/gameplay/mission_runtime.h"
#include "relaycore/physics/collision.h"
#include "relaycore/world/blockout_loader.h"
#include "relaygame/level_session_builder.h"
#include "relaygame/world_audio_zones.h"

#include <algorithm>
#include <array>
#include <doctest/doctest.h>
#include <filesystem>
#include <string_view>
#include <vector>

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
            .mission_step_count = 5,
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

    void init_test_log()
    {
        RelayCore::log::shutdown();
        RelayCore::log::Config log_config{};
        log_config.enable_console_sink = false;
        log_config.enable_timestamped_log = false;
        log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
        RelayCore::log::init(log_config);
    }

    auto make_actor_bounds(const glm::vec3& center, const glm::vec3& half_extents) -> RelayCore::physics::Aabb
    {
        return {center - half_extents, center + half_extents};
    }

    auto bounds_center(const RelayCore::physics::Aabb& bounds) -> glm::vec3
    {
        return (bounds.mins + bounds.maxs) * 0.5F;
    }

    void apply_combat_target_event(
        const CombatTargetEvent& event,
        std::vector<RuntimeDoor>& doors,
        std::vector<RuntimeUsable>& usables,
        std::vector<std::string>& activated_targets,
        std::vector<std::string>& signaled_targets
    )
    {
        if (event.target.empty())
        {
            return;
        }

        switch (event.action)
        {
            case CombatTargetActionKind::ActivateTarget:
                if (RelayCore::gameplay::activate_target(event.target, doors))
                {
                    activated_targets.push_back(event.target);
                }
                break;
            case CombatTargetActionKind::SignalTarget:
                if (RelayCore::gameplay::signal_target(event.target, doors, usables))
                {
                    signaled_targets.push_back(event.target);
                }
                break;
            case CombatTargetActionKind::None:
            default:
                break;
        }
    }
} // namespace

TEST_CASE("all shipped maps build clean runtime content")
{
    init_test_log();

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
    }

    RelayCore::log::shutdown();
}

TEST_CASE("stress_room_02 preserves the second acceptance flow")
{
    init_test_log();

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

    const RelayCore::physics::Aabb spawn_bounds = make_actor_bounds({0.0F, 1.0F, -36.0F}, {0.3F, 0.9F, 0.3F});
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

    const glm::vec3 zone_center = bounds_center(definition.steps[1].bounds);
    mission_runtime.update(
        make_actor_bounds(zone_center, {0.3F, 0.9F, 0.3F}),
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
    const RelayCore::physics::Aabb checkpoint_bounds = make_actor_bounds({0.0F, 1.0F, 74.0F}, {0.3F, 0.9F, 0.3F});
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
    init_test_log();

    const std::filesystem::path level_path = repo_root() / "assets" / "levels" / "test_room.toml";
    const RelayCore::BlockoutLevel level = RelayCore::load_blockout_level(level_path);

    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeTrigger> triggers;
    std::vector<RuntimeUsable> usables;
    RelayCore::gameplay::build_runtime_interactions(level, doors, triggers, usables);
    std::vector<RuntimeTurret> turrets = RelayCore::gameplay::build_runtime_combat(level).turrets;

    const MissionDefinition definition = MissionBuilder{level}.build();
    REQUIRE(definition.steps.size() == 5);
    CHECK(definition.steps[0].target == "intake/outer_bulkhead");
    CHECK(definition.steps[2].target == "lab_keycard");
    CHECK(definition.steps[3].target == "inner_bulkhead");
    CHECK(definition.steps[4].target == "lab_relay_console");

    auto outer_panel = std::find_if(usables.begin(), usables.end(), [](const RuntimeUsable& usable) {
        return usable.label == "Открыть внешний шлюз";
    });
    auto pickup = std::find_if(usables.begin(), usables.end(), [](const RuntimeUsable& usable) {
        return usable.action_kind == UsableActionKind::PickupItem && usable.item_id == "lab_keycard";
    });
    auto reader = std::find_if(usables.begin(), usables.end(), [](const RuntimeUsable& usable) {
        return usable.label == "Подтвердить допуск";
    });
    auto terminal = std::find_if(usables.begin(), usables.end(), [](const RuntimeUsable& usable) {
        return usable.name == "lab_console_lock";
    });
    auto outer_door = std::find_if(doors.begin(), doors.end(), [](const RuntimeDoor& door) {
        return door.name == "intake/outer_bulkhead";
    });
    auto inner_door = std::find_if(doors.begin(), doors.end(), [](const RuntimeDoor& door) {
        return door.name == "inner_bulkhead";
    });

    REQUIRE(outer_panel != usables.end());
    REQUIRE(pickup != usables.end());
    REQUIRE(reader != usables.end());
    REQUIRE(terminal != usables.end());
    REQUIRE(outer_door != doors.end());
    REQUIRE(inner_door != doors.end());
    REQUIRE(turrets.size() == 1);
    CHECK(terminal->locked);

    std::vector<std::string> inventory_items;
    std::vector<std::string> activated_targets;
    std::vector<std::string> signaled_targets;
    MissionRuntime mission_runtime{};
    mission_runtime.reset(definition);

    const RelayCore::physics::Aabb spawn_bounds = make_actor_bounds({0.0F, 1.0F, -1.4F}, {0.3F, 0.9F, 0.3F});
    CHECK(mission_runtime.status().title == "Открыть внешний шлюз");

    CHECK(
        RelayCore::gameplay::apply_usable_interaction(
            *outer_panel,
            doors,
            usables,
            inventory_items,
            activated_targets,
            signaled_targets
        )
            .kind
        == UseInteractionOutcome::ActivatedTarget
    );
    CHECK(outer_door->state == DoorState::Opening);

    mission_runtime.update(spawn_bounds, doors, inventory_items, activated_targets, signaled_targets);
    CHECK(mission_runtime.status().title == "Войти в шлюзовую камеру");

    const glm::vec3 airlock_center = bounds_center(definition.steps[1].bounds);
    mission_runtime.update(
        make_actor_bounds(airlock_center, {0.3F, 0.9F, 0.3F}),
        doors,
        inventory_items,
        activated_targets,
        signaled_targets
    );
    CHECK(mission_runtime.status().title == "Забрать ключ-карту лаборатории");

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
    mission_runtime.update(spawn_bounds, doors, inventory_items, activated_targets, signaled_targets);
    CHECK(mission_runtime.status().title == "Разблокировать внутренний шлюз");

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
    CHECK(inner_door->state == DoorState::Opening);

    mission_runtime.update(spawn_bounds, doors, inventory_items, activated_targets, signaled_targets);
    CHECK(mission_runtime.status().title == "Стабилизировать ядро ретранслятора");

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
        == UseInteractionOutcome::Locked
    );
    CHECK(terminal->locked);

    PlayerCombatState player_combat{};
    RelayCore::gameplay::reset_player_combat_state(player_combat);
    player_combat.weapon_range = 16.0F;
    player_combat.weapon_damage = 25.0F;

    const glm::vec3 shot_origin = turrets[0].origin + glm::vec3{-1.8F, 0.0F, -1.2F};
    const glm::vec3 shot_direction = turrets[0].origin - shot_origin;

    const RelayCore::gameplay::CombatFireResult first_shot =
        RelayCore::gameplay::apply_player_fire(level, doors, shot_origin, shot_direction, player_combat, turrets);
    CHECK(first_shot.fired);
    CHECK(first_shot.hit_turret);
    CHECK_FALSE(first_shot.killed_turret);

    RelayCore::gameplay::tick_combat_state(0.30F, player_combat, turrets);

    const RelayCore::gameplay::CombatFireResult second_shot =
        RelayCore::gameplay::apply_player_fire(level, doors, shot_origin, shot_direction, player_combat, turrets);
    CHECK(second_shot.fired);
    CHECK(second_shot.hit_turret);
    CHECK(second_shot.killed_turret);
    apply_combat_target_event(second_shot.target_event, doors, usables, activated_targets, signaled_targets);

    CHECK(turrets[0].destroyed);
    CHECK_FALSE(terminal->locked);
    CHECK(std::find(signaled_targets.begin(), signaled_targets.end(), "lab_console_lock") != signaled_targets.end());

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
    mission_runtime.update(spawn_bounds, doors, inventory_items, activated_targets, signaled_targets);

    const auto status = mission_runtime.status();
    CHECK(status.completed);
    CHECK(status.current_step == definition.steps.size());
    CHECK(status.title == "Лаборатория наблюдения защищена");

    RelayCore::log::shutdown();
}

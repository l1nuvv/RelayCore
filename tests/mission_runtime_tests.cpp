#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/gameplay/mission_runtime.h"
#include "relaycore/world/blockout_level.h"
#include "relaycore/world/blockout_loader.h"

#include <algorithm>
#include <doctest/doctest.h>
#include <filesystem>

namespace
{
    using RelayCore::gameplay::DoorState;
    using RelayCore::gameplay::MissionBuilder;
    using RelayCore::gameplay::MissionDefinition;
    using RelayCore::gameplay::MissionRuntime;
    using RelayCore::gameplay::MissionStep;
    using RelayCore::gameplay::MissionStepKind;
    using RelayCore::gameplay::RuntimeDoor;
    using RelayCore::gameplay::RuntimeTrigger;
    using RelayCore::gameplay::RuntimeUsable;
    using RelayCore::gameplay::UsableActionKind;
    using RelayCore::physics::Aabb;

    [[nodiscard]] auto make_mission_bounds(const glm::vec3& center, const glm::vec3& half_extents) -> Aabb
    {
        return {center - half_extents, center + half_extents};
    }

    [[nodiscard]] auto bounds_center(const Aabb& bounds) -> glm::vec3
    {
        return (bounds.mins + bounds.maxs) * 0.5F;
    }
} // namespace

TEST_CASE("mission builder parses shipped-style step entities")
{
    RelayCore::BlockoutLevel level{};
    level.entities.push_back(
        RelayCore::EntitySpawnData{
            .classname = "mission_settings",
            .properties = {
                {"completion_title", "Observation lab secured"},
                {"completion_detail", "Route complete."},
            },
        }
    );
    level.entities.push_back(
        RelayCore::EntitySpawnData{
            .classname = "mission_step",
            .properties = {
                {"kind", "door_open"},
                {"target", "outer_bulkhead"},
                {"title", "Cycle outer bulkhead"},
                {"detail", "Use the intake wall panel."},
            },
        }
    );
    level.entities.push_back(
        RelayCore::EntitySpawnData{
            .classname = "mission_step",
            .origin = {0.0F, 0.1F, 6.5F},
            .properties = {
                {"kind", "zone"},
                {"size", "2.6 2.4 2.8"},
                {"title", "Enter the airlock"},
            },
        }
    );
    level.entities.push_back(
        RelayCore::EntitySpawnData{
            .classname = "mission_step",
            .properties = {
                {"kind", "pickup_item"},
                {"target", "lab_keycard"},
                {"title", "Secure the keycard"},
            },
        }
    );
    level.entities.push_back(
        RelayCore::EntitySpawnData{
            .classname = "mission_step",
            .properties = {
                {"kind", "signal"},
                {"target", "reader_lock"},
                {"title", "Ping the reader interlock"},
            },
        }
    );
    level.entities.push_back(
        RelayCore::EntitySpawnData{
            .classname = "mission_step",
            .properties = {
                {"kind", "activate_target"},
                {"target", "lab_relay_console"},
                {"title", "Stabilize the relay core"},
            },
        }
    );

    const MissionDefinition definition = MissionBuilder{level}.build();

    REQUIRE(definition.steps.size() == 5);
    CHECK(definition.completion_title == "Observation lab secured");
    CHECK(definition.completion_detail == "Route complete.");
    CHECK(definition.steps[0].kind == MissionStepKind::DoorOpened);
    CHECK(definition.steps[0].target == "outer_bulkhead");
    CHECK(definition.steps[1].kind == MissionStepKind::EnterZone);
    CHECK(definition.steps[1].bounds.mins.x == doctest::Approx(-1.3F));
    CHECK(definition.steps[1].bounds.maxs.y == doctest::Approx(2.5F));
    CHECK(definition.steps[1].bounds.maxs.z == doctest::Approx(7.9F));
    CHECK(definition.steps[2].kind == MissionStepKind::AcquireItem);
    CHECK(definition.steps[2].target == "lab_keycard");
    CHECK(definition.steps[3].kind == MissionStepKind::SignalTarget);
    CHECK(definition.steps[3].target == "reader_lock");
    CHECK(definition.steps[4].kind == MissionStepKind::ActivateTarget);
    CHECK(definition.steps[4].target == "lab_relay_console");
}

TEST_CASE("mission runtime advances through door and zone objectives")
{
    MissionDefinition definition{};
    definition.completion_title = "Observation lab secured";
    definition.completion_detail = "Route complete.";
    definition.steps = {
        MissionStep{
            .kind = MissionStepKind::DoorOpened,
            .title = "Cycle outer bulkhead",
            .detail = "Use the intake panel.",
            .target = "outer_bulkhead",
        },
        MissionStep{
            .kind = MissionStepKind::EnterZone,
            .title = "Enter the airlock",
            .detail = "Move into the chamber.",
            .bounds = make_mission_bounds({0.0F, 1.2F, 6.5F}, {1.3F, 1.2F, 1.4F}),
        },
    };

    MissionRuntime runtime{};
    runtime.reset(definition);

    std::vector<RuntimeDoor> doors{
        RuntimeDoor{.name = "outer_bulkhead", .open_fraction = 0.0F, .state = DoorState::Closed},
    };
    std::vector<std::string> inventory_items;
    std::vector<std::string> signaled_targets;

    auto status = runtime.status();
    CHECK(status.active);
    CHECK_FALSE(status.completed);
    CHECK(status.current_step == 1);
    CHECK(status.total_steps == 2);
    CHECK(status.title == "Cycle outer bulkhead");

    std::vector<std::string> activated_targets;

    runtime.update(
        make_mission_bounds({0.0F, 1.0F, 0.0F}, {0.3F, 0.9F, 0.3F}),
        doors,
        inventory_items,
        activated_targets,
        signaled_targets
    );
    CHECK(runtime.status().current_step == 1);

    doors[0].state = DoorState::Opening;
    doors[0].open_fraction = 0.15F;
    runtime.update(
        make_mission_bounds({0.0F, 1.0F, 0.0F}, {0.3F, 0.9F, 0.3F}),
        doors,
        inventory_items,
        activated_targets,
        signaled_targets
    );

    status = runtime.status();
    CHECK_FALSE(status.completed);
    CHECK(status.current_step == 2);
    CHECK(status.title == "Enter the airlock");

    runtime.update(
        make_mission_bounds({0.0F, 1.2F, 6.5F}, {0.3F, 0.9F, 0.3F}),
        doors,
        inventory_items,
        activated_targets,
        signaled_targets
    );

    status = runtime.status();
    CHECK(status.completed);
    CHECK(status.current_step == 2);
    CHECK(status.title == "Observation lab secured");
    CHECK(status.detail == "Route complete.");
}

TEST_CASE("mission runtime advances when required item is collected")
{
    MissionDefinition definition{};
    definition.steps = {
        MissionStep{
            .kind = MissionStepKind::AcquireItem,
            .title = "Secure the keycard",
            .detail = "Grab the access card from the airlock rack.",
            .target = "lab_keycard",
        },
    };

    MissionRuntime runtime{};
    runtime.reset(definition);

    std::vector<RuntimeDoor> doors;
    std::vector<std::string> inventory_items;
    std::vector<std::string> activated_targets;
    std::vector<std::string> signaled_targets;
    const Aabb player_bounds = make_mission_bounds({0.0F, 1.0F, 0.0F}, {0.3F, 0.9F, 0.3F});

    runtime.update(player_bounds, doors, inventory_items, activated_targets, signaled_targets);
    CHECK_FALSE(runtime.completed());

    inventory_items.push_back("lab_keycard");
    runtime.update(player_bounds, doors, inventory_items, activated_targets, signaled_targets);

    const auto status = runtime.status();
    CHECK(status.completed);
    CHECK(status.current_step == 1);
}

TEST_CASE("mission runtime advances when required target is activated")
{
    MissionDefinition definition{};
    definition.steps = {
        MissionStep{
            .kind = MissionStepKind::ActivateTarget,
            .title = "Stabilize the relay core",
            .detail = "Use the lab console.",
            .target = "lab_relay_console",
        },
    };

    MissionRuntime runtime{};
    runtime.reset(definition);

    std::vector<RuntimeDoor> doors;
    std::vector<std::string> inventory_items;
    std::vector<std::string> activated_targets;
    std::vector<std::string> signaled_targets;
    const Aabb player_bounds = make_mission_bounds({0.0F, 1.0F, 0.0F}, {0.3F, 0.9F, 0.3F});

    runtime.update(player_bounds, doors, inventory_items, activated_targets, signaled_targets);
    CHECK_FALSE(runtime.completed());

    activated_targets.push_back("lab_relay_console");
    runtime.update(player_bounds, doors, inventory_items, activated_targets, signaled_targets);

    const auto status = runtime.status();
    CHECK(status.completed);
    CHECK(status.current_step == 1);
}

TEST_CASE("mission runtime advances when required target is signaled")
{
    MissionDefinition definition{};
    definition.steps = {
        MissionStep{
            .kind = MissionStepKind::SignalTarget,
            .title = "Unlock the reader interlock",
            .detail = "Send a signal to the reader lock.",
            .target = "reader_lock",
        },
    };

    MissionRuntime runtime{};
    runtime.reset(definition);

    std::vector<RuntimeDoor> doors;
    std::vector<std::string> inventory_items;
    std::vector<std::string> activated_targets;
    std::vector<std::string> signaled_targets;
    const Aabb player_bounds = make_mission_bounds({0.0F, 1.0F, 0.0F}, {0.3F, 0.9F, 0.3F});

    runtime.update(player_bounds, doors, inventory_items, activated_targets, signaled_targets);
    CHECK_FALSE(runtime.completed());

    signaled_targets.push_back("reader_lock");
    runtime.update(player_bounds, doors, inventory_items, activated_targets, signaled_targets);

    const auto status = runtime.status();
    CHECK(status.completed);
    CHECK(status.current_step == 1);
}

TEST_CASE("shipped test_room preserves the authored bulkhead to terminal mission flow")
{
    RelayCore::log::shutdown();
    RelayCore::log::Config log_config{};
    log_config.enable_console_sink = false;
    log_config.enable_timestamped_log = false;
    log_config.logs_directory = (std::filesystem::temp_directory_path() / "relaycore_test_logs").string();
    RelayCore::log::init(log_config);

    const std::filesystem::path repo_root = std::filesystem::path(__FILE__).parent_path().parent_path();
    const RelayCore::BlockoutLevel level =
        RelayCore::load_blockout_level(repo_root / "assets" / "levels" / "test_room.toml");

    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeTrigger> triggers;
    std::vector<RuntimeUsable> usables;
    RelayCore::gameplay::build_runtime_interactions(level, doors, triggers, usables);

    const MissionDefinition definition = MissionBuilder{level}.build();
    REQUIRE(definition.steps.size() == 5);
    CHECK(definition.steps[0].target == "intake/outer_bulkhead");
    CHECK(definition.steps[1].kind == MissionStepKind::EnterZone);
    CHECK(definition.steps[2].target == "lab_keycard");
    CHECK(definition.steps[3].target == "inner_bulkhead");
    CHECK(definition.steps[4].target == "lab_relay_console");

    const auto outer_door = std::find_if(doors.begin(), doors.end(), [](const RuntimeDoor& door) {
        return door.name == "intake/outer_bulkhead";
    });
    REQUIRE(outer_door != doors.end());
    CHECK(outer_door->move_speed == doctest::Approx(1.75F));

    const auto inner_door = std::find_if(doors.begin(), doors.end(), [](const RuntimeDoor& door) {
        return door.name == "inner_bulkhead";
    });
    REQUIRE(inner_door != doors.end());
    CHECK(inner_door->move_speed == doctest::Approx(1.90F));

    const auto keycard_pickup = std::find_if(usables.begin(), usables.end(), [](const RuntimeUsable& usable) {
        return usable.action_kind == UsableActionKind::PickupItem && usable.item_id == "lab_keycard";
    });
    REQUIRE(keycard_pickup != usables.end());
    CHECK(keycard_pickup->label == "Взять: Ключ-карта лаборатории");

    const auto inner_reader = std::find_if(usables.begin(), usables.end(), [](const RuntimeUsable& usable) {
        return usable.label == "Подтвердить допуск";
    });
    REQUIRE(inner_reader != usables.end());
    CHECK(inner_reader->single_use);
    CHECK(inner_reader->required_item == "lab_keycard");
    CHECK(inner_reader->denied_message == "Нужна ключ-карта лаборатории");

    const auto terminal = std::find_if(usables.begin(), usables.end(), [](const RuntimeUsable& usable) {
        return usable.action_kind == UsableActionKind::ActivateTarget && usable.target == "lab_relay_console";
    });
    REQUIRE(terminal != usables.end());
    CHECK(terminal->success_message == "Ядро ретранслятора стабилизировано");

    MissionRuntime runtime{};
    runtime.reset(definition);

    std::vector<std::string> inventory_items;
    std::vector<std::string> activated_targets;
    std::vector<std::string> signaled_targets;
    const Aabb player_bounds = make_mission_bounds({0.0F, 1.0F, 0.0F}, {0.3F, 0.9F, 0.3F});

    CHECK(runtime.status().title == "Открыть внешний шлюз");

    outer_door->state = DoorState::Opening;
    outer_door->open_fraction = 0.2F;
    runtime.update(player_bounds, doors, inventory_items, activated_targets, signaled_targets);
    CHECK(runtime.status().title == "Войти в шлюзовую камеру");

    const glm::vec3 zone_center = bounds_center(definition.steps[1].bounds);
    runtime.update(
        make_mission_bounds(zone_center, {0.3F, 0.9F, 0.3F}),
        doors,
        inventory_items,
        activated_targets,
        signaled_targets
    );
    CHECK(runtime.status().title == "Забрать ключ-карту лаборатории");

    inventory_items.push_back("lab_keycard");
    runtime.update(player_bounds, doors, inventory_items, activated_targets, signaled_targets);
    CHECK(runtime.status().title == "Разблокировать внутренний шлюз");

    inner_door->state = DoorState::Opening;
    inner_door->open_fraction = 0.2F;
    runtime.update(player_bounds, doors, inventory_items, activated_targets, signaled_targets);
    CHECK(runtime.status().title == "Стабилизировать ядро ретранслятора");

    activated_targets.push_back("lab_relay_console");
    runtime.update(player_bounds, doors, inventory_items, activated_targets, signaled_targets);

    const auto status = runtime.status();
    CHECK(status.completed);
    CHECK(status.title == "Лаборатория наблюдения защищена");
    CHECK(status.detail == "Маршрут завершен. Сектор комплекса зачищен.");

    RelayCore::log::shutdown();
}

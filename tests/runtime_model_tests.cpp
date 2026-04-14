#include "relaycore/gameplay/mission_runtime.h"
#include "relaycore/gameplay/runtime_model.h"
#include "relaycore/world/blockout_level.h"

#include <doctest/doctest.h>

namespace
{
    using RelayCore::AuthoredSourceLocation;
    using RelayCore::BlockoutLevel;
    using RelayCore::EntitySpawnData;
    using RelayCore::gameplay::DoorState;
    using RelayCore::gameplay::MissionDefinition;
    using RelayCore::gameplay::MissionRuntime;
    using RelayCore::gameplay::MissionStep;
    using RelayCore::gameplay::MissionStepKind;
    using RelayCore::gameplay::RuntimeDoor;
    using RelayCore::gameplay::RuntimeObjectKind;
    using RelayCore::gameplay::RuntimeObjectRegistry;
    using RelayCore::gameplay::RuntimeTrigger;
    using RelayCore::gameplay::RuntimeTurret;
    using RelayCore::gameplay::RuntimeUsable;
    using RelayCore::gameplay::TriggerActionKind;
    using RelayCore::gameplay::UsableActionKind;
    using RelayCore::gameplay::UsableSignalAction;
    using RelayCore::physics::Aabb;

    [[nodiscard]] auto make_bounds(const glm::vec3& center, const glm::vec3& half_extents) -> Aabb
    {
        return {center - half_extents, center + half_extents};
    }
} // namespace

TEST_CASE("runtime registry builds stable ids and resolved bindings")
{
    BlockoutLevel level{};
    level.entities = {
        EntitySpawnData{.classname = "func_door"},
        EntitySpawnData{.classname = "trigger_volume"},
        EntitySpawnData{.classname = "logic_usable"},
    };
    level.entity_sources = {
        AuthoredSourceLocation{.file_path = "tests/data/registry_door.toml", .line = 5, .column = 1},
        AuthoredSourceLocation{
            .file_path = "tests/data/registry_trigger_template.toml",
            .line = 8,
            .column = 1,
            .source_kind = "module_instance",
            .template_name = "security_block",
            .instance_name = "entry_a",
            .placement_file_path = "tests/data/registry_level.toml",
            .placement_line = 22,
            .placement_column = 1,
        },
        AuthoredSourceLocation{.file_path = "tests/data/registry_usable.toml", .line = 12, .column = 1},
    };

    const std::vector<RuntimeDoor> doors{
        RuntimeDoor{.authored_entity_index = 0, .name = "bulkhead_alpha"},
    };
    const std::vector<RuntimeTrigger> triggers{
        RuntimeTrigger{
            .authored_entity_index = 1,
            .name = "airlock_trigger",
            .target = "bulkhead_alpha",
            .bounds = make_bounds({0.0F, 1.0F, 0.0F}, {1.0F, 1.0F, 1.0F}),
            .action_kind = TriggerActionKind::ActivateTarget,
        },
    };
    const std::vector<RuntimeUsable> usables{
        RuntimeUsable{
            .authored_entity_index = 2,
            .name = "reader_panel",
            .label = "Reader",
            .target = "bulkhead_alpha",
            .required_item = "lab_keycard",
            .action_kind = UsableActionKind::ActivateTarget,
        },
        RuntimeUsable{
            .generated_from_entity_index = 0,
            .name = "pickup_keycard",
            .label = "Take keycard",
            .chain_target = "reader_panel",
            .item_id = "lab_keycard",
            .action_kind = UsableActionKind::PickupItem,
            .signal_action = UsableSignalAction::Unlock,
        },
    };

    RuntimeObjectRegistry first_registry{};
    first_registry.build(level, doors, triggers, usables);

    RuntimeObjectRegistry second_registry{};
    second_registry.build(level, doors, triggers, usables);

    REQUIRE(first_registry.objects().size() == 4);
    REQUIRE(second_registry.objects().size() == 4);
    CHECK(first_registry.signature() == second_registry.signature());

    const auto* door_handle = first_registry.find_door(0);
    const auto* trigger_handle = first_registry.find_trigger(0);
    const auto* panel_handle = first_registry.find_usable(0);
    const auto* pickup_handle = first_registry.find_usable(1);
    REQUIRE(door_handle != nullptr);
    REQUIRE(trigger_handle != nullptr);
    REQUIRE(panel_handle != nullptr);
    REQUIRE(pickup_handle != nullptr);

    CHECK(door_handle->kind == RuntimeObjectKind::Door);
    CHECK(trigger_handle->source_bucket == "instanced");
    CHECK(panel_handle->source_bucket == "direct");
    CHECK(pickup_handle->kind == RuntimeObjectKind::Pickup);
    CHECK(pickup_handle->source_bucket == "generated");

    CHECK(second_registry.find_door(0)->id == door_handle->id);
    CHECK(second_registry.find_trigger(0)->id == trigger_handle->id);
    CHECK(second_registry.find_usable(0)->id == panel_handle->id);
    CHECK(second_registry.find_usable(1)->id == pickup_handle->id);

    REQUIRE(first_registry.bindings().size() == 5);
    CHECK(first_registry.bindings()[0].resolved_target_count == 1);
    CHECK(first_registry.bindings()[0].resolved_targets_summary.find("bulkhead_alpha") != std::string::npos);
    CHECK(first_registry.bindings()[1].kind == RelayCore::gameplay::RuntimeBindingKind::RequireItem);
    CHECK(first_registry.bindings()[2].kind == RelayCore::gameplay::RuntimeBindingKind::ActivateTarget);
    CHECK(first_registry.bindings()[3].kind == RelayCore::gameplay::RuntimeBindingKind::PickupItem);
    CHECK(first_registry.bindings()[4].kind == RelayCore::gameplay::RuntimeBindingKind::ChainTarget);
}

TEST_CASE("runtime level snapshot restores mutable gameplay state by stable ids")
{
    BlockoutLevel level{};
    level.entities = {
        EntitySpawnData{.classname = "func_door"},
        EntitySpawnData{.classname = "trigger_volume"},
        EntitySpawnData{.classname = "logic_usable"},
    };
    level.entity_sources = {
        AuthoredSourceLocation{.file_path = "tests/data/snapshot_door.toml", .line = 5, .column = 1},
        AuthoredSourceLocation{.file_path = "tests/data/snapshot_trigger.toml", .line = 9, .column = 1},
        AuthoredSourceLocation{.file_path = "tests/data/snapshot_panel.toml", .line = 14, .column = 1},
    };

    std::vector<RuntimeDoor> doors{
        RuntimeDoor{
            .authored_entity_index = 0,
            .name = "bulkhead_alpha",
            .open_fraction = 0.35F,
            .state = DoorState::Opening,
        },
    };
    std::vector<RuntimeTrigger> triggers{
        RuntimeTrigger{
            .authored_entity_index = 1,
            .name = "airlock_trigger",
            .target = "bulkhead_alpha",
            .bounds = make_bounds({0.0F, 1.0F, 0.0F}, {1.0F, 1.0F, 1.0F}),
            .fired = true,
        },
    };
    std::vector<RuntimeUsable> usables{
        RuntimeUsable{
            .authored_entity_index = 2,
            .name = "reader_panel",
            .label = "Reader",
            .target = "bulkhead_alpha",
            .locked = false,
            .consumed = false,
            .action_kind = UsableActionKind::ActivateTarget,
        },
        RuntimeUsable{
            .generated_from_entity_index = 0,
            .name = "pickup_keycard",
            .label = "Take keycard",
            .item_id = "lab_keycard",
            .locked = false,
            .consumed = true,
            .action_kind = UsableActionKind::PickupItem,
        },
    };

    RuntimeObjectRegistry registry{};
    registry.build(level, doors, triggers, usables);

    MissionDefinition mission_definition{};
    mission_definition.steps = {
        MissionStep{
            .kind = MissionStepKind::DoorOpened,
            .title = "Open bulkhead",
            .target = "bulkhead_alpha",
        },
        MissionStep{
            .kind = MissionStepKind::ActivateTarget,
            .title = "Use reader",
            .target = "bulkhead_alpha",
        },
    };

    MissionRuntime mission_runtime{};
    mission_runtime.reset(mission_definition);
    std::vector<std::string> inventory_item_ids{"lab_keycard"};
    std::vector<std::string> activated_target_ids{"bulkhead_alpha"};
    std::vector<std::string> signaled_target_ids;
    std::vector<RuntimeTurret> turrets{
        RuntimeTurret{
            .authored_entity_index = 7,
            .name = "security_turret",
            .label = "Security turret",
            .health = 22.0F,
            .cooldown_remaining_seconds = 0.35F,
            .warmup_remaining_seconds = 0.10F,
            .warming_up = true,
        },
    };
    RelayCore::gameplay::PlayerCombatState player_combat{};
    RelayCore::gameplay::reset_player_combat_state(player_combat);
    player_combat.health = 72.0F;
    player_combat.weapon_cooldown_remaining_seconds = 0.18F;
    player_combat.hurt_flash_remaining_seconds = 0.12F;
    player_combat.shots_fired = 4;
    player_combat.shots_hit = 3;
    player_combat.kills = 1;
    player_combat.damage_dealt = 50.0F;
    player_combat.last_damage_source = "security_turret";
    mission_runtime.update(
        make_bounds({0.0F, 1.0F, 0.0F}, {0.3F, 0.9F, 0.3F}),
        doors,
        inventory_item_ids,
        activated_target_ids,
        signaled_target_ids
    );

    const auto snapshot = RelayCore::gameplay::capture_runtime_level_snapshot(
        "levels/test_room.toml",
        registry,
        doors,
        triggers,
        usables,
        turrets,
        inventory_item_ids,
        activated_target_ids,
        signaled_target_ids,
        player_combat,
        mission_runtime,
        42
    );

    doors[0].open_fraction = 0.0F;
    doors[0].state = DoorState::Closed;
    triggers[0].fired = false;
    usables[0].locked = true;
    usables[1].consumed = false;
    inventory_item_ids.clear();
    activated_target_ids.clear();
    turrets[0].health = 50.0F;
    turrets[0].destroyed = false;
    turrets[0].warming_up = false;
    player_combat.health = 19.0F;
    player_combat.alive = false;
    player_combat.respawn_remaining_seconds = 0.7F;

    const auto restore = RelayCore::gameplay::restore_runtime_level_snapshot(
        snapshot,
        "levels/test_room.toml",
        registry,
        doors,
        triggers,
        usables,
        turrets,
        inventory_item_ids,
        activated_target_ids,
        signaled_target_ids,
        player_combat,
        mission_runtime
    );

    CHECK(restore.restored);
    CHECK(doors[0].open_fraction == doctest::Approx(0.35F));
    CHECK(doors[0].state == DoorState::Opening);
    CHECK(triggers[0].fired);
    CHECK_FALSE(usables[0].locked);
    CHECK(usables[1].consumed);
    REQUIRE(inventory_item_ids.size() == 1);
    CHECK(inventory_item_ids[0] == "lab_keycard");
    REQUIRE(activated_target_ids.size() == 1);
    CHECK(activated_target_ids[0] == "bulkhead_alpha");
    REQUIRE(turrets.size() == 1);
    CHECK(turrets[0].health == doctest::Approx(22.0F));
    CHECK(turrets[0].cooldown_remaining_seconds == doctest::Approx(0.35F));
    CHECK(turrets[0].warmup_remaining_seconds == doctest::Approx(0.10F));
    CHECK(turrets[0].warming_up);
    CHECK(player_combat.health == doctest::Approx(72.0F));
    CHECK(player_combat.alive);
    CHECK(player_combat.weapon_cooldown_remaining_seconds == doctest::Approx(0.18F));
    CHECK(player_combat.shots_fired == 4);
    CHECK(player_combat.kills == 1);
    CHECK(player_combat.last_damage_source == "security_turret");
    CHECK(restore.restored_turrets == 1);
    CHECK(mission_runtime.status().current_step == 2);
}

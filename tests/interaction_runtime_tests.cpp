#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/gameplay/combat_runtime.h"
#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/world/blockout_level.h"
#include "relaycore/world/blockout_loader.h"
#include "relaygame/usable_interaction.h"

#include <algorithm>
#include <doctest/doctest.h>
#include <filesystem>
#include <vector>

#include "scoped_temp_directory.h"
#include "scoped_test_logger.h"
#include "world_query_fixture.h"

namespace
{
    using RelayCore::gameplay::DoorActivationPolicy;
    using RelayCore::gameplay::DoorState;
    using RelayCore::gameplay::InteractionEvent;
    using RelayCore::gameplay::InteractionEventSourceKind;
    using RelayCore::gameplay::InteractionQueryResult;
    using RelayCore::gameplay::PanelMountAxis;
    using RelayCore::gameplay::PlayerCombatState;
    using RelayCore::gameplay::RuntimeDoor;
    using RelayCore::gameplay::RuntimeTrigger;
    using RelayCore::gameplay::RuntimeTurret;
    using RelayCore::gameplay::RuntimeUsable;
    using RelayCore::gameplay::UsableActionKind;
    using RelayCore::gameplay::UsableVisualKind;
    using RelayCore::gameplay::UseInteractionOutcome;
    using RelayCore::physics::Aabb;

    [[nodiscard]] auto make_bounds(const glm::vec3& center, const glm::vec3& half_extents) -> Aabb
    {
        return {center - half_extents, center + half_extents};
    }
} // namespace

TEST_CASE("nearest usable is selected within range")
{
    std::vector<RuntimeUsable> usables{
        RuntimeUsable{.label = "far", .target = "door_0", .origin = {0.0F, 1.0F, -2.0F}, .use_radius = 3.0F},
        RuntimeUsable{.label = "near", .target = "door_1", .origin = {0.0F, 1.0F, -1.0F}, .use_radius = 3.0F},
    };

    const InteractionQueryResult result =
        RelayCore::gameplay::find_focused_usable({0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, -1.0F}, usables);

    CHECK(result.usable_index == 1);
    CHECK(result.distance < 1.1F);
}

TEST_CASE("usable outside distance is ignored")
{
    std::vector<RuntimeUsable> usables{
        RuntimeUsable{.label = "far", .target = "door_0", .origin = {0.0F, 1.0F, -5.0F}, .use_radius = 2.25F},
    };

    const InteractionQueryResult result =
        RelayCore::gameplay::find_focused_usable({0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, -1.0F}, usables);

    CHECK(result.usable_index == -1);
}

TEST_CASE("usable behind player is ignored")
{
    std::vector<RuntimeUsable> usables{
        RuntimeUsable{.label = "behind", .target = "door_0", .origin = {0.0F, 1.0F, 1.0F}, .use_radius = 3.0F},
    };

    const InteractionQueryResult result =
        RelayCore::gameplay::find_focused_usable({0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, -1.0F}, usables);

    CHECK(result.usable_index == -1);
}

TEST_CASE("activating target opens matching door")
{
    std::vector<RuntimeDoor> doors{
        RuntimeDoor{.name = "door_0"},
        RuntimeDoor{.name = "door_1"},
    };

    CHECK(RelayCore::gameplay::activate_target("door_1", doors));
    CHECK(doors[0].state == DoorState::Closed);
    CHECK(doors[1].state == DoorState::Opening);
}

TEST_CASE("trigger overlap fires once and opens door")
{
    std::vector<RuntimeDoor> doors{
        RuntimeDoor{.name = "door_0"},
    };
    std::vector<RuntimeUsable> usables;
    std::vector<RuntimeTrigger> triggers{
        RuntimeTrigger{.target = "door_0", .bounds = make_bounds({0.0F, 1.0F, 0.0F}, {0.5F, 1.0F, 0.5F}), .once = true},
    };
    std::vector<std::string> activated_target_ids;
    std::vector<std::string> signaled_target_ids;
    InteractionEvent last_event;
    const Aabb player = make_bounds({0.0F, 1.0F, 0.0F}, {0.3F, 0.9F, 0.3F});
    const Aabb outside = make_bounds({3.0F, 1.0F, 0.0F}, {0.3F, 0.9F, 0.3F});

    CHECK(
        RelayCore::gameplay::update_trigger_overlaps(
            player,
            triggers,
            doors,
            usables,
            activated_target_ids,
            signaled_target_ids,
            last_event
        )
        == 1
    );
    CHECK(doors[0].state == DoorState::Opening);
    CHECK(triggers[0].fired);
    CHECK(last_event.target == "door_0");
    CHECK(last_event.source_kind == InteractionEventSourceKind::Trigger);
    CHECK(last_event.kind == UseInteractionOutcome::ActivatedTarget);
    REQUIRE(activated_target_ids.size() == 1);
    CHECK(activated_target_ids[0] == "door_0");
    CHECK(signaled_target_ids.empty());
    CHECK(
        RelayCore::gameplay::update_trigger_overlaps(
            player,
            triggers,
            doors,
            usables,
            activated_target_ids,
            signaled_target_ids,
            last_event
        )
        == 0
    );
    CHECK(
        RelayCore::gameplay::update_trigger_overlaps(
            outside,
            triggers,
            doors,
            usables,
            activated_target_ids,
            signaled_target_ids,
            last_event
        )
        == 0
    );
    CHECK(
        RelayCore::gameplay::update_trigger_overlaps(
            player,
            triggers,
            doors,
            usables,
            activated_target_ids,
            signaled_target_ids,
            last_event
        )
        == 0
    );
}

TEST_CASE("reusable trigger fires on entry, including the initial overlap")
{
    std::vector<RuntimeDoor> doors{
        RuntimeDoor{.name = "door_0"},
    };
    std::vector<RuntimeUsable> usables;
    std::vector<RuntimeTrigger> triggers{
        RuntimeTrigger{
            .target = "door_0",
            .bounds = make_bounds({0.0F, 1.0F, 0.0F}, {0.5F, 1.0F, 0.5F}),
            .once = false,
        },
    };
    std::vector<std::string> activated_target_ids;
    std::vector<std::string> signaled_target_ids;
    InteractionEvent last_event;
    const Aabb inside = make_bounds({0.0F, 1.0F, 0.0F}, {0.3F, 0.9F, 0.3F});
    const Aabb outside = make_bounds({3.0F, 1.0F, 0.0F}, {0.3F, 0.9F, 0.3F});

    CHECK(
        RelayCore::gameplay::update_trigger_overlaps(
            inside,
            triggers,
            doors,
            usables,
            activated_target_ids,
            signaled_target_ids,
            last_event
        )
        == 1
    );
    CHECK(doors[0].state == DoorState::Opening);

    for (int tick = 0; tick < 3; ++tick)
    {
        CHECK(
            RelayCore::gameplay::update_trigger_overlaps(
                inside,
                triggers,
                doors,
                usables,
                activated_target_ids,
                signaled_target_ids,
                last_event
            )
            == 0
        );
    }
    CHECK(doors[0].state == DoorState::Opening);

    CHECK(
        RelayCore::gameplay::update_trigger_overlaps(
            outside,
            triggers,
            doors,
            usables,
            activated_target_ids,
            signaled_target_ids,
            last_event
        )
        == 0
    );
    CHECK(
        RelayCore::gameplay::update_trigger_overlaps(
            inside,
            triggers,
            doors,
            usables,
            activated_target_ids,
            signaled_target_ids,
            last_event
        )
        == 1
    );
    CHECK(doors[0].state == DoorState::Closing);
}

TEST_CASE("trigger signal action records signaled targets and updates receivers")
{
    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeUsable> usables{
        RuntimeUsable{
            .name = "reader_lock",
            .locked = true,
            .enabled = true,
            .signal_action = RelayCore::gameplay::UsableSignalAction::Unlock,
        },
    };
    std::vector<RuntimeTrigger> triggers{
        RuntimeTrigger{
            .target = "reader_lock",
            .bounds = make_bounds({0.0F, 1.0F, 0.0F}, {0.5F, 1.0F, 0.5F}),
            .once = true,
            .action_kind = RelayCore::gameplay::TriggerActionKind::SignalTarget,
        },
    };
    std::vector<std::string> activated_target_ids;
    std::vector<std::string> signaled_target_ids;
    InteractionEvent last_event;
    const Aabb player = make_bounds({0.0F, 1.0F, 0.0F}, {0.3F, 0.9F, 0.3F});

    CHECK(
        RelayCore::gameplay::update_trigger_overlaps(
            player,
            triggers,
            doors,
            usables,
            activated_target_ids,
            signaled_target_ids,
            last_event
        )
        == 1
    );
    CHECK_FALSE(usables[0].locked);
    CHECK(activated_target_ids.empty());
    REQUIRE(signaled_target_ids.size() == 1);
    CHECK(signaled_target_ids[0] == "reader_lock");
    CHECK(last_event.target == "reader_lock");
    CHECK(last_event.source_kind == InteractionEventSourceKind::Trigger);
    CHECK(last_event.kind == UseInteractionOutcome::SignaledTarget);
}

TEST_CASE("trigger activate_target records event-only activations")
{
    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeUsable> usables;
    std::vector<RuntimeTrigger> triggers{
        RuntimeTrigger{
            .target = "airlock_cycle_event",
            .bounds = make_bounds({0.0F, 1.0F, 0.0F}, {0.5F, 1.0F, 0.5F}),
            .once = true,
            .action_kind = RelayCore::gameplay::TriggerActionKind::ActivateTarget,
        },
    };
    std::vector<std::string> activated_target_ids;
    std::vector<std::string> signaled_target_ids;
    InteractionEvent last_event;
    const Aabb player = make_bounds({0.0F, 1.0F, 0.0F}, {0.3F, 0.9F, 0.3F});

    CHECK(
        RelayCore::gameplay::update_trigger_overlaps(
            player,
            triggers,
            doors,
            usables,
            activated_target_ids,
            signaled_target_ids,
            last_event
        )
        == 1
    );
    REQUIRE(activated_target_ids.size() == 1);
    CHECK(activated_target_ids[0] == "airlock_cycle_event");
    CHECK(signaled_target_ids.empty());
    CHECK(last_event.target == "airlock_cycle_event");
    CHECK(last_event.source_kind == InteractionEventSourceKind::Trigger);
    CHECK(last_event.kind == UseInteractionOutcome::ActivatedTarget);
}

TEST_CASE("usable interaction returns structured pickup event with chained signal")
{
    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeUsable> usables{
        RuntimeUsable{
            .name = "pickup_keycard",
            .label = "Take keycard",
            .chain_target = "reader_lock",
            .item_id = "lab_keycard",
            .item_name = "Lab Keycard",
            .enabled = true,
            .action_kind = UsableActionKind::PickupItem,
        },
        RuntimeUsable{
            .name = "reader_lock",
            .locked = true,
            .enabled = true,
            .signal_action = RelayCore::gameplay::UsableSignalAction::Unlock,
        },
    };
    std::vector<std::string> inventory_item_ids;
    std::vector<std::string> activated_target_ids;
    std::vector<std::string> signaled_target_ids;

    const InteractionEvent event = RelayGame::apply_usable_interaction(
        usables[0],
        doors,
        usables,
        inventory_item_ids,
        activated_target_ids,
        signaled_target_ids
    );

    CHECK(event.kind == UseInteractionOutcome::PickedUpItem);
    CHECK(event.source_kind == InteractionEventSourceKind::Usable);
    CHECK(event.source_name == "pickup_keycard");
    CHECK(event.source_label == "Take keycard");
    CHECK(event.item_id == "lab_keycard");
    CHECK(event.chain_target == "reader_lock");
    CHECK(event.primary_applied);
    CHECK(event.chain_applied);
    CHECK(event.source_consumed);
    REQUIRE(inventory_item_ids.size() == 1);
    CHECK(inventory_item_ids[0] == "lab_keycard");
    REQUIRE(signaled_target_ids.size() == 1);
    CHECK(signaled_target_ids[0] == "reader_lock");
    CHECK_FALSE(usables[1].locked);
}

TEST_CASE("usable interaction returns denial events for locked and missing item states")
{
    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeUsable> usables{
        RuntimeUsable{
            .name = "locked_panel",
            .label = "Locked panel",
            .target = "door_a",
            .locked = true,
            .enabled = true,
        },
        RuntimeUsable{
            .name = "reader_panel",
            .label = "Reader",
            .target = "door_b",
            .required_item = "lab_keycard",
            .required_item_name = "Lab Keycard",
            .enabled = true,
        },
    };
    std::vector<std::string> inventory_item_ids;
    std::vector<std::string> activated_target_ids;
    std::vector<std::string> signaled_target_ids;

    const InteractionEvent locked_event = RelayGame::apply_usable_interaction(
        usables[0],
        doors,
        usables,
        inventory_item_ids,
        activated_target_ids,
        signaled_target_ids
    );
    const InteractionEvent missing_item_event = RelayGame::apply_usable_interaction(
        usables[1],
        doors,
        usables,
        inventory_item_ids,
        activated_target_ids,
        signaled_target_ids
    );

    CHECK(locked_event.kind == UseInteractionOutcome::Locked);
    CHECK(locked_event.source_kind == InteractionEventSourceKind::Usable);
    CHECK(locked_event.target == "door_a");
    CHECK_FALSE(locked_event.primary_applied);

    CHECK(missing_item_event.kind == UseInteractionOutcome::MissingRequiredItem);
    CHECK(missing_item_event.source_kind == InteractionEventSourceKind::Usable);
    CHECK(missing_item_event.target == "door_b");
    CHECK_FALSE(missing_item_event.primary_applied);
    CHECK_FALSE(usables[1].requirement_met);
}

TEST_CASE("single-use usable returns activation event and consumes itself")
{
    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeUsable> usables{
        RuntimeUsable{
            .name = "relay_console",
            .label = "Console",
            .target = "console_event",
            .chain_target = "reader_lock",
            .single_use = true,
            .enabled = true,
            .action_kind = UsableActionKind::ActivateTarget,
        },
        RuntimeUsable{
            .name = "reader_lock",
            .locked = true,
            .enabled = true,
            .signal_action = RelayCore::gameplay::UsableSignalAction::Unlock,
        },
    };
    std::vector<std::string> inventory_item_ids;
    std::vector<std::string> activated_target_ids;
    std::vector<std::string> signaled_target_ids;

    const InteractionEvent event = RelayGame::apply_usable_interaction(
        usables[0],
        doors,
        usables,
        inventory_item_ids,
        activated_target_ids,
        signaled_target_ids
    );

    CHECK(event.kind == UseInteractionOutcome::ActivatedTarget);
    CHECK(event.source_kind == InteractionEventSourceKind::Usable);
    CHECK(event.target == "console_event");
    CHECK(event.chain_target == "reader_lock");
    CHECK(event.primary_applied);
    CHECK(event.chain_applied);
    CHECK(event.source_consumed);
    REQUIRE(activated_target_ids.size() == 1);
    CHECK(activated_target_ids[0] == "console_event");
    REQUIRE(signaled_target_ids.size() == 1);
    CHECK(signaled_target_ids[0] == "reader_lock");
    CHECK(usables[0].consumed);
    CHECK_FALSE(usables[0].enabled);
    CHECK_FALSE(usables[1].locked);
}

TEST_CASE("trigger spatial index narrows overlap candidates")
{
    std::vector<RuntimeTrigger> triggers{
        RuntimeTrigger{.target = "door_0", .bounds = make_bounds({0.0F, 1.0F, 0.0F}, {0.5F, 1.0F, 0.5F})},
        RuntimeTrigger{.target = "door_1", .bounds = make_bounds({24.0F, 1.0F, 24.0F}, {0.5F, 1.0F, 0.5F})},
    };
    RelayCore::gameplay::InteractionSpatialIndex index{};
    std::vector<std::uint32_t> candidates;

    RelayCore::gameplay::rebuild_trigger_spatial_index(triggers, index);
    RelayCore::gameplay::query_trigger_candidates(
        index,
        make_bounds({0.0F, 1.0F, 0.0F}, {0.3F, 0.9F, 0.3F}),
        candidates
    );

    REQUIRE(candidates.size() == 1);
    CHECK(candidates[0] == 0);
}

TEST_CASE("door reaches fully open state")
{
    RuntimeDoor door{};
    door.name = "door_0";
    door.move_speed = 2.0F;
    door.state = DoorState::Opening;
    std::vector<RuntimeDoor> doors{door};

    for (int i = 0; i < 60; ++i)
    {
        (void) RelayCore::gameplay::update_doors(1.0F / 60.0F, doors);
    }

    CHECK(doors[0].state == DoorState::Open);
    CHECK(doors[0].open_fraction == doctest::Approx(1.0F).epsilon(0.001));
}

TEST_CASE("activating open door starts closing")
{
    RuntimeDoor door{};
    door.name = "door_0";
    door.open_fraction = 1.0F;
    door.state = DoorState::Open;
    std::vector<RuntimeDoor> doors{door};

    CHECK(RelayCore::gameplay::activate_target("door_0", doors));
    CHECK(doors[0].state == DoorState::Closing);
}

TEST_CASE("door reaches fully closed state")
{
    RuntimeDoor door{};
    door.name = "door_0";
    door.open_fraction = 1.0F;
    door.move_speed = 2.0F;
    door.state = DoorState::Closing;
    std::vector<RuntimeDoor> doors{door};

    for (int i = 0; i < 60; ++i)
    {
        (void) RelayCore::gameplay::update_doors(1.0F / 60.0F, doors);
    }

    CHECK(doors[0].state == DoorState::Closed);
    CHECK(doors[0].open_fraction == doctest::Approx(0.0F).epsilon(0.001));
    CHECK(RelayCore::gameplay::is_door_blocking(doors[0]));
}

TEST_CASE("open-only door strategy never starts closing on repeated activation")
{
    RuntimeDoor door{};
    door.name = "door_0";
    door.open_fraction = 1.0F;
    door.state = DoorState::Open;
    door.activation_policy = DoorActivationPolicy::OpenOnly;
    std::vector<RuntimeDoor> doors{door};

    CHECK(RelayCore::gameplay::activate_target("door_0", doors));
    CHECK(doors[0].state == DoorState::Open);
}

TEST_CASE("door becomes usable even without explicit use entity")
{
    RelayCore::BlockoutLevel level{};
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "gameplay_entity",
        .origin = {0.0F, 0.0F, 4.0F},
        .properties =
            {
                {"kind", "door"},
                {"targetname", "door_auto_use"},
                {"width", "2.0"},
                {"height", "2.0"},
            },
    });

    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeTrigger> triggers;
    std::vector<RuntimeUsable> usables;

    RelayCore::gameplay::build_runtime_interactions(level, doors, triggers, usables);

    REQUIRE(doors.size() == 1);
    REQUIRE(usables.size() == 1);
    CHECK(doors[0].authored_entity_index == 0);
    CHECK(usables[0].authored_entity_index == -1);
    CHECK(usables[0].generated_from_entity_index == 0);
    CHECK(usables[0].target == "door_auto_use");
    CHECK(usables[0].use_radius >= 2.0F);
    CHECK(usables[0].origin.z == doctest::Approx(4.0F));
    CHECK(usables[0].tracks_target_door);
    CHECK(usables[0].enabled);
}

TEST_CASE("fully open door no longer blocks")
{
    RuntimeDoor door{};
    door.open_fraction = 1.0F;
    CHECK_FALSE(RelayCore::gameplay::is_door_blocking(door));
}

TEST_CASE("closing door that overlaps actor reopens instead of forcing penetration")
{
    RuntimeDoor door{};
    door.name = "door_0";
    door.closed_bounds = make_bounds({0.0F, 1.3F, 0.0F}, {1.0F, 1.3F, 0.1F});
    door.open_offset = {2.0F, 0.0F, 0.0F};
    door.open_fraction = 0.35F;
    door.move_speed = 4.0F;
    door.state = DoorState::Closing;
    std::vector<RuntimeDoor> doors{door};

    const Aabb actor_bounds = make_bounds({0.55F, 0.9F, 0.0F}, {0.30F, 0.90F, 0.30F});

    CHECK(RelayCore::gameplay::update_doors(1.0F / 60.0F, actor_bounds, doors) == 1);
    CHECK(doors[0].state == DoorState::Opening);
    CHECK(doors[0].open_fraction > 0.0F);
}

TEST_CASE("closing door overlapping actor is excluded from dynamic blockers")
{
    RuntimeDoor door{};
    door.closed_bounds = make_bounds({0.0F, 1.3F, 0.0F}, {1.0F, 1.3F, 0.1F});
    door.open_offset = {2.0F, 0.0F, 0.0F};
    door.open_fraction = 0.20F;
    door.state = DoorState::Closing;
    std::vector<RuntimeDoor> doors{door};

    const Aabb actor_bounds = make_bounds({0.65F, 0.9F, 0.0F}, {0.30F, 0.90F, 0.30F});
    std::vector<RelayCore::physics::WorldCollider> blockers;

    RelayCore::gameplay::collect_blocking_doors(doors, actor_bounds, blockers);

    CHECK(blockers.empty());
}

TEST_CASE("closed door overlapping actor remains in dynamic blockers")
{
    RuntimeDoor door{};
    door.closed_bounds = make_bounds({0.0F, 1.3F, 0.0F}, {1.0F, 1.3F, 0.1F});
    door.open_fraction = 0.0F;
    door.state = DoorState::Closed;
    std::vector<RuntimeDoor> doors{door};

    const Aabb actor_bounds = make_bounds({0.65F, 0.9F, 0.0F}, {0.30F, 0.90F, 0.30F});
    std::vector<RelayCore::physics::WorldCollider> blockers;

    RelayCore::gameplay::collect_blocking_doors(doors, actor_bounds, blockers);

    REQUIRE(blockers.size() == 1);
    CHECK(blockers[0].bounds.mins.x == doctest::Approx(door.closed_bounds.mins.x));
    CHECK(blockers[0].bounds.maxs.z == doctest::Approx(door.closed_bounds.maxs.z));
}

TEST_CASE("runtime interactions read target links from entity properties")
{
    RelayCore::BlockoutLevel level{};
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "gameplay_entity",
        .origin = {0.0F, 0.0F, 0.0F},
        .properties =
            {
                {"kind", "door"},
                {"targetname", "airlock_main"},
                {"move_speed", "2.5"},
                {"travel_distance", "3.0"},
            },
    });
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "gameplay_entity",
        .origin = {0.0F, 1.0F, -1.0F},
        .properties =
            {
                {"kind", "usable"},
                {"target", "airlock_main"},
                {"label", "Open airlock"},
                {"use_radius", "3.5"},
            },
    });
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "gameplay_entity",
        .origin = {0.0F, 0.0F, 2.0F},
        .properties =
            {
                {"kind", "trigger"},
                {"target", "airlock_main"},
                {"targetname", "airlock_trigger"},
                {"once", "false"},
            },
    });

    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeTrigger> triggers;
    std::vector<RuntimeUsable> usables;

    RelayCore::gameplay::build_runtime_interactions(level, doors, triggers, usables);

    REQUIRE(doors.size() == 1);
    REQUIRE(usables.size() == 1);
    REQUIRE(triggers.size() == 1);

    CHECK(doors[0].authored_entity_index == 0);
    CHECK(usables[0].authored_entity_index == 1);
    CHECK(usables[0].generated_from_entity_index == -1);
    CHECK(triggers[0].authored_entity_index == 2);
    CHECK(doors[0].name == "airlock_main");
    CHECK(doors[0].move_speed == doctest::Approx(2.5F));
    CHECK(doors[0].open_offset.y == doctest::Approx(3.0F));
    CHECK(doors[0].closed_bounds.mins.x == doctest::Approx(-0.5F));
    CHECK(doors[0].closed_bounds.maxs.y == doctest::Approx(2.0F));
    CHECK(usables[0].target == "airlock_main");
    CHECK(usables[0].label == "Open airlock");
    CHECK(usables[0].use_radius == doctest::Approx(3.5F));
    CHECK(usables[0].tracks_target_door);
    CHECK(usables[0].tracked_offset.z == doctest::Approx(-1.0F));
    CHECK(triggers[0].name == "airlock_trigger");
    CHECK(triggers[0].target == "airlock_main");
    CHECK_FALSE(triggers[0].once);
}

TEST_CASE("door bounds and slide offset can be configured from entity properties")
{
    RelayCore::BlockoutLevel level{};
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "gameplay_entity",
        .origin = {0.0F, 0.0F, 4.0F},
        .properties =
            {
                {"kind", "door"},
                {"width", "2.0"},
                {"height", "2.6"},
                {"thickness", "0.18"},
                {"move_offset_x", "2.25"},
                {"move_offset_y", "0.0"},
                {"move_offset_z", "0.0"},
            },
    });

    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeTrigger> triggers;
    std::vector<RuntimeUsable> usables;

    RelayCore::gameplay::build_runtime_interactions(level, doors, triggers, usables);

    REQUIRE(doors.size() == 1);
    CHECK(doors[0].closed_bounds.mins.x == doctest::Approx(-1.0F));
    CHECK(doors[0].closed_bounds.maxs.x == doctest::Approx(1.0F));
    CHECK(doors[0].closed_bounds.maxs.y == doctest::Approx(2.6F));
    CHECK(doors[0].closed_bounds.mins.z == doctest::Approx(3.91F));
    CHECK(doors[0].closed_bounds.maxs.z == doctest::Approx(4.09F));
    CHECK(doors[0].open_offset.x == doctest::Approx(2.25F));
    CHECK(doors[0].open_offset.y == doctest::Approx(0.0F));
}

TEST_CASE("compact size and move_offset properties are supported by interaction runtime")
{
    RelayCore::BlockoutLevel level{};
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "gameplay_entity",
        .origin = {0.0F, 0.0F, 4.0F},
        .properties =
            {
                {"kind", "door"},
                {"targetname", "door_compact"},
                {"size", "2.0 2.6 0.18"},
                {"move_offset", "2.25 0.0 0.0"},
            },
    });
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "gameplay_entity",
        .origin = {-2.9F, 1.35F, 3.2F},
        .properties =
            {
                {"kind", "panel"},
                {"target", "door_compact"},
                {"label", "Open airlock"},
                {"size", "0.18 0.45 0.28"},
            },
    });

    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeTrigger> triggers;
    std::vector<RuntimeUsable> usables;

    RelayCore::gameplay::build_runtime_interactions(level, doors, triggers, usables);

    REQUIRE(doors.size() == 1);
    REQUIRE(usables.size() == 1);
    CHECK(doors[0].closed_bounds.mins.x == doctest::Approx(-1.0F));
    CHECK(doors[0].closed_bounds.maxs.y == doctest::Approx(2.6F));
    CHECK(doors[0].closed_bounds.maxs.z == doctest::Approx(4.09F));
    CHECK(doors[0].open_offset.x == doctest::Approx(2.25F));
    CHECK(doors[0].open_offset.y == doctest::Approx(0.0F));
    CHECK(usables[0].visual_kind == UsableVisualKind::Panel);
    CHECK(usables[0].has_visual_bounds);
    CHECK(usables[0].visual_bounds.mins.x == doctest::Approx(-2.99F));
    CHECK(usables[0].visual_bounds.maxs.z == doctest::Approx(3.34F));
}

TEST_CASE("panel mount_axis builds wall-mounted bounds")
{
    RelayCore::BlockoutLevel level{};
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "gameplay_entity",
        .origin = {-5.0F, 1.35F, 2.8F},
        .properties =
            {
                {"kind", "panel"},
                {"target", "outer_bulkhead"},
                {"label", "Outer bulkhead"},
                {"size", "0.34 0.52 0.12"},
                {"mount_axis", "+x"},
            },
    });

    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeTrigger> triggers;
    std::vector<RuntimeUsable> usables;

    RelayCore::gameplay::build_runtime_interactions(level, doors, triggers, usables);

    REQUIRE(usables.size() == 1);
    CHECK(usables[0].visual_kind == UsableVisualKind::Panel);
    CHECK(usables[0].has_visual_bounds);
    CHECK(usables[0].panel_mount_axis == PanelMountAxis::PositiveX);
    CHECK(usables[0].visual_bounds.mins.x == doctest::Approx(-4.994F));
    CHECK(usables[0].visual_bounds.maxs.x == doctest::Approx(-4.874F));
    CHECK(usables[0].visual_bounds.mins.z == doctest::Approx(2.63F));
    CHECK(usables[0].visual_bounds.maxs.z == doctest::Approx(2.97F));
    CHECK(usables[0].visual_bounds.mins.y == doctest::Approx(1.09F));
    CHECK(usables[0].visual_bounds.maxs.y == doctest::Approx(1.61F));
}

TEST_CASE("runtime interactions can be defined by kind property without dev classnames")
{
    RelayCore::BlockoutLevel level{};
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "marker",
        .origin = {0.0F, 0.0F, 4.0F},
        .properties =
            {
                {"kind", "door"},
                {"targetname", "door_schema"},
            },
    });
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "marker",
        .origin = {0.0F, 1.0F, 3.0F},
        .properties =
            {
                {"kind", "usable"},
                {"target", "door_schema"},
            },
    });
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "marker",
        .origin = {0.0F, 0.0F, 6.0F},
        .properties =
            {
                {"kind", "trigger"},
                {"target", "door_schema"},
            },
    });

    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeTrigger> triggers;
    std::vector<RuntimeUsable> usables;

    RelayCore::gameplay::build_runtime_interactions(level, doors, triggers, usables);

    CHECK(doors.size() == 1);
    CHECK(triggers.size() == 1);
    CHECK(usables.size() == 1);
    CHECK(doors[0].name == "door_schema");
    CHECK(usables[0].target == "door_schema");
    CHECK(triggers[0].target == "door_schema");
}

TEST_CASE("entities without kind are ignored by interaction runtime")
{
    RelayCore::BlockoutLevel level{};
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "func_door",
        .origin = {0.0F, 0.0F, 4.0F},
    });
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "dev_use",
        .origin = {0.0F, 1.0F, 3.0F},
        .properties =
            {
                {"target", "legacy_door"},
            },
    });
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "trigger_once",
        .origin = {0.0F, 0.0F, 6.0F},
        .properties =
            {
                {"target", "legacy_door"},
            },
    });

    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeTrigger> triggers;
    std::vector<RuntimeUsable> usables;

    RelayCore::gameplay::build_runtime_interactions(level, doors, triggers, usables);

    CHECK(doors.empty());
    CHECK(triggers.empty());
    CHECK(usables.empty());
}

TEST_CASE("panel entities build visual bounds without adding a second auto-usable")
{
    RelayCore::BlockoutLevel level{};
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "gameplay_entity",
        .origin = {0.0F, 0.0F, 4.0F},
        .properties =
            {
                {"kind", "door"},
                {"targetname", "airlock_main"},
            },
    });
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "gameplay_entity",
        .origin = {-2.9F, 1.35F, 3.2F},
        .properties =
            {
                {"kind", "panel"},
                {"target", "airlock_main"},
                {"label", "Open airlock"},
                {"width", "0.18"},
                {"height", "0.45"},
                {"depth", "0.28"},
            },
    });

    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeTrigger> triggers;
    std::vector<RuntimeUsable> usables;

    RelayCore::gameplay::build_runtime_interactions(level, doors, triggers, usables);

    REQUIRE(doors.size() == 1);
    REQUIRE(usables.size() == 1);
    CHECK(usables[0].target == "airlock_main");
    CHECK(usables[0].visual_kind == UsableVisualKind::Panel);
    CHECK(usables[0].has_visual_bounds);
    CHECK(usables[0].has_focus_bounds);
    CHECK_FALSE(usables[0].tracks_target_door);
    CHECK(usables[0].visual_bounds.mins.x == doctest::Approx(-2.99F));
    CHECK(usables[0].visual_bounds.maxs.x == doctest::Approx(-2.81F));
    CHECK(usables[0].visual_bounds.mins.y == doctest::Approx(1.125F));
    CHECK(usables[0].visual_bounds.maxs.z == doctest::Approx(3.34F));
}

TEST_CASE("panel focus uses panel bounds")
{
    const Aabb panel_bounds = make_bounds({-2.9F, 1.35F, 3.2F}, {0.09F, 0.225F, 0.14F});
    std::vector<RuntimeUsable> usables{
        RuntimeUsable{
            .label = "Open airlock",
            .target = "airlock_main",
            .origin = {-2.9F, 1.35F, 3.2F},
            .focus_bounds = panel_bounds,
            .visual_bounds = panel_bounds,
            .use_radius = 1.75F,
            .has_focus_bounds = true,
            .has_visual_bounds = true,
            .enabled = true,
            .tracks_target_door = false,
            .visual_kind = UsableVisualKind::Panel,
        },
    };

    const InteractionQueryResult result =
        RelayCore::gameplay::find_focused_usable({-2.9F, 1.35F, 2.2F}, {0.0F, 0.0F, 1.0F}, usables);

    CHECK(result.usable_index == 0);
    CHECK(result.distance < 1.1F);
}

TEST_CASE("panel focus tolerates slight off-center aim")
{
    const Aabb panel_bounds = make_bounds({-2.9F, 1.35F, 3.2F}, {0.09F, 0.225F, 0.14F});
    std::vector<RuntimeUsable> usables{
        RuntimeUsable{
            .label = "Open airlock",
            .target = "airlock_main",
            .origin = {-2.9F, 1.35F, 3.2F},
            .focus_bounds = panel_bounds,
            .visual_bounds = panel_bounds,
            .use_radius = 1.75F,
            .has_focus_bounds = true,
            .has_visual_bounds = true,
            .enabled = true,
            .tracks_target_door = false,
            .visual_kind = UsableVisualKind::Panel,
        },
    };

    const InteractionQueryResult result =
        RelayCore::gameplay::find_focused_usable({-2.55F, 1.35F, 2.2F}, {-0.28F, 0.0F, 1.0F}, usables);

    CHECK(result.usable_index == 0);
    CHECK(result.distance < 1.3F);
}

TEST_CASE("pickup entities build collectible visual usables")
{
    RelayCore::BlockoutLevel level{};
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "gameplay_entity",
        .origin = {-1.1F, 1.15F, 7.45F},
        .properties =
            {
                {"kind", "pickup"},
                {"item_id", "lab_keycard"},
                {"item_name", "Lab keycard"},
                {"size", "0.22 0.12 0.02"},
            },
    });

    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeTrigger> triggers;
    std::vector<RuntimeUsable> usables;

    RelayCore::gameplay::build_runtime_interactions(level, doors, triggers, usables);

    REQUIRE(usables.size() == 1);
    CHECK(usables[0].action_kind == UsableActionKind::PickupItem);
    CHECK(usables[0].visual_kind == UsableVisualKind::Pickup);
    CHECK(usables[0].item_id == "lab_keycard");
    CHECK(usables[0].item_name == "Lab keycard");
    CHECK(usables[0].label == "Взять: Lab keycard");
    CHECK(usables[0].has_visual_bounds);
    CHECK(usables[0].has_focus_bounds);
    CHECK(usables[0].visual_bounds.mins.x == doctest::Approx(-1.21F));
    CHECK(usables[0].visual_bounds.maxs.y == doctest::Approx(1.21F));
}

TEST_CASE("required_item metadata is preserved on panels")
{
    RelayCore::BlockoutLevel level{};
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "gameplay_entity",
        .origin = {0.0F, 0.0F, 8.7F},
        .properties =
            {
                {"kind", "door"},
                {"targetname", "inner_bulkhead"},
            },
    });
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "gameplay_entity",
        .origin = {1.6F, 1.35F, 7.4F},
        .properties =
            {
                {"kind", "panel"},
                {"target", "inner_bulkhead"},
                {"label", "Unlock inner bulkhead"},
                {"required_item", "lab_keycard"},
                {"required_item_name", "Lab keycard"},
                {"success_message", "Inner bulkhead command accepted"},
                {"denied_message", "Lab keycard required"},
            },
    });

    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeTrigger> triggers;
    std::vector<RuntimeUsable> usables;

    RelayCore::gameplay::build_runtime_interactions(level, doors, triggers, usables);

    REQUIRE(usables.size() == 1);
    CHECK(usables[0].action_kind == UsableActionKind::ActivateTarget);
    CHECK(usables[0].required_item == "lab_keycard");
    CHECK(usables[0].required_item_name == "Lab keycard");
    CHECK(usables[0].success_message == "Inner bulkhead command accepted");
    CHECK(usables[0].denied_message == "Lab keycard required");
}

TEST_CASE("signal action metadata is preserved on terminals")
{
    RelayCore::BlockoutLevel level{};
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "gameplay_entity",
        .origin = {0.0F, 1.55F, 12.5F},
        .properties =
            {
                {"kind", "terminal"},
                {"target", "lab_relay_console"},
                {"label", "Stabilize relay core"},
                {"action", "signal"},
                {"mount_axis", "-z"},
                {"success_message", "Relay core stabilized"},
            },
    });

    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeTrigger> triggers;
    std::vector<RuntimeUsable> usables;

    RelayCore::gameplay::build_runtime_interactions(level, doors, triggers, usables);

    REQUIRE(usables.size() == 1);
    CHECK(usables[0].action_kind == UsableActionKind::SignalTarget);
    CHECK(usables[0].visual_kind == UsableVisualKind::Panel);
    CHECK(usables[0].target == "lab_relay_console");
    CHECK(usables[0].success_message == "Relay core stabilized");
}

TEST_CASE("locked single-use usable metadata and signal response are preserved")
{
    RelayCore::BlockoutLevel level{};
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "gameplay_entity",
        .origin = {0.0F, 0.0F, 6.0F},
        .properties =
            {
                {"kind", "door"},
                {"targetname", "inner_door"},
            },
    });
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "gameplay_entity",
        .origin = {-1.5F, 1.2F, 5.0F},
        .properties =
            {
                {"kind", "panel"},
                {"target", "inner_door"},
                {"label", "Prime relay"},
                {"single_use", "true"},
                {"chain_target", "reader_lock"},
            },
    });
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "gameplay_entity",
        .origin = {1.5F, 1.2F, 5.0F},
        .properties =
            {
                {"kind", "panel"},
                {"targetname", "reader_lock"},
                {"label", "Unlock reader"},
                {"starts_locked", "true"},
                {"locked_message", "Reader is locked"},
                {"signal_action", "unlock"},
            },
    });

    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeTrigger> triggers;
    std::vector<RuntimeUsable> usables;

    RelayCore::gameplay::build_runtime_interactions(level, doors, triggers, usables);

    REQUIRE(usables.size() == 2);
    const auto prime_panel = std::find_if(usables.begin(), usables.end(), [](const RuntimeUsable& usable) {
        return usable.label == "Prime relay";
    });
    REQUIRE(prime_panel != usables.end());
    CHECK(prime_panel->single_use);
    CHECK(prime_panel->chain_target == "reader_lock");

    const auto locked_reader = std::find_if(usables.begin(), usables.end(), [](const RuntimeUsable& usable) {
        return usable.name == "reader_lock";
    });
    REQUIRE(locked_reader != usables.end());
    CHECK(locked_reader->locked);
    CHECK(locked_reader->locked_message == "Reader is locked");
    CHECK(locked_reader->signal_action == RelayCore::gameplay::UsableSignalAction::Unlock);
}

TEST_CASE("signal target can unlock named usable and activate named door")
{
    std::vector<RuntimeDoor> doors{
        RuntimeDoor{.name = "door_a"},
    };
    std::vector<RuntimeUsable> usables{
        RuntimeUsable{
            .name = "reader_lock",
            .label = "Reader",
            .locked = true,
            .signal_action = RelayCore::gameplay::UsableSignalAction::Unlock,
        },
        RuntimeUsable{
            .name = "reader_toggle",
            .label = "Toggle reader",
            .locked = false,
            .signal_action = RelayCore::gameplay::UsableSignalAction::ToggleLock,
        },
    };

    CHECK(RelayCore::gameplay::signal_target("reader_lock", doors, usables));
    CHECK_FALSE(usables[0].locked);

    CHECK(RelayCore::gameplay::signal_target("reader_toggle", doors, usables));
    CHECK(usables[1].locked);

    CHECK(RelayCore::gameplay::signal_target("door_a", doors, usables));
    CHECK(doors[0].state == DoorState::Opening);
}

TEST_CASE("signal target reports failure for plain named usables")
{
    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeUsable> usables{
        RuntimeUsable{
            .name = "plain_panel",
            .label = "Plain panel",
            .signal_action = RelayCore::gameplay::UsableSignalAction::None,
        },
        RuntimeUsable{
            .name = "signal_reader",
            .label = "Signal reader",
            .locked = true,
            .signal_action = RelayCore::gameplay::UsableSignalAction::Unlock,
        },
    };

    CHECK_FALSE(RelayCore::gameplay::signal_target("plain_panel", doors, usables));
    CHECK_FALSE(usables[0].locked);
    CHECK(RelayCore::gameplay::signal_target("signal_reader", doors, usables));
    CHECK_FALSE(usables[1].locked);
}

TEST_CASE("perform usable action supports door targets and event activations")
{
    std::vector<RuntimeDoor> doors{
        RuntimeDoor{.name = "door_a"},
    };
    std::vector<RuntimeUsable> usables{
        RuntimeUsable{
            .name = "reader_lock",
            .locked = true,
            .enabled = true,
            .signal_action = RelayCore::gameplay::UsableSignalAction::Unlock,
        },
    };

    const RuntimeUsable door_panel{
        .target = "door_a",
        .action_kind = UsableActionKind::ActivateTarget,
    };
    CHECK(RelayCore::gameplay::perform_usable_action(door_panel, doors, usables));
    CHECK(doors[0].state == DoorState::Opening);

    doors[0].state = DoorState::Closed;
    const RuntimeUsable terminal{
        .target = "lab_relay_console",
        .action_kind = UsableActionKind::ActivateTarget,
    };
    CHECK(RelayCore::gameplay::perform_usable_action(terminal, doors, usables));
    CHECK(doors[0].state == DoorState::Closed);

    const RuntimeUsable signal_panel{
        .target = "reader_lock",
        .action_kind = UsableActionKind::SignalTarget,
    };
    CHECK(RelayCore::gameplay::perform_usable_action(signal_panel, doors, usables));
    CHECK_FALSE(usables[0].locked);

    const RuntimeUsable empty_target{
        .action_kind = UsableActionKind::ActivateTarget,
    };
    CHECK_FALSE(RelayCore::gameplay::perform_usable_action(empty_target, doors, usables));
}

TEST_CASE("apply usable interaction rejects locked and missing-item states")
{
    std::vector<RuntimeDoor> doors{
        RuntimeDoor{.name = "inner_bulkhead"},
    };
    std::vector<RuntimeUsable> usables{
        RuntimeUsable{
            .name = "locked_reader",
            .target = "inner_bulkhead",
            .locked = true,
            .enabled = true,
            .action_kind = UsableActionKind::ActivateTarget,
        },
        RuntimeUsable{
            .name = "key_reader",
            .target = "inner_bulkhead",
            .required_item = "lab_keycard",
            .enabled = true,
            .action_kind = UsableActionKind::ActivateTarget,
        },
    };
    std::vector<std::string> inventory_item_ids;
    std::vector<std::string> activated_target_ids;
    std::vector<std::string> signaled_target_ids;

    CHECK(
        RelayGame::apply_usable_interaction(
            usables[0],
            doors,
            usables,
            inventory_item_ids,
            activated_target_ids,
            signaled_target_ids
        )
            .kind
        == UseInteractionOutcome::Locked
    );
    CHECK(doors[0].state == DoorState::Closed);
    CHECK(inventory_item_ids.empty());
    CHECK(activated_target_ids.empty());
    CHECK(signaled_target_ids.empty());

    CHECK(
        RelayGame::apply_usable_interaction(
            usables[1],
            doors,
            usables,
            inventory_item_ids,
            activated_target_ids,
            signaled_target_ids
        )
            .kind
        == UseInteractionOutcome::MissingRequiredItem
    );
    CHECK_FALSE(usables[1].requirement_met);
    CHECK(doors[0].state == DoorState::Closed);
    CHECK(activated_target_ids.empty());
    CHECK(signaled_target_ids.empty());
}

TEST_CASE("apply usable interaction collects pickups and chains signal targets")
{
    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeUsable> usables{
        RuntimeUsable{
            .label = "Pickup keycard",
            .chain_target = "reader_lock",
            .item_id = "lab_keycard",
            .item_name = "Lab keycard",
            .enabled = true,
            .action_kind = UsableActionKind::PickupItem,
        },
        RuntimeUsable{
            .name = "reader_lock",
            .label = "Reader lock",
            .locked = true,
            .enabled = true,
            .signal_action = RelayCore::gameplay::UsableSignalAction::Unlock,
        },
    };
    std::vector<std::string> inventory_item_ids;
    std::vector<std::string> activated_target_ids;
    std::vector<std::string> signaled_target_ids;

    CHECK(
        RelayGame::apply_usable_interaction(
            usables[0],
            doors,
            usables,
            inventory_item_ids,
            activated_target_ids,
            signaled_target_ids
        )
            .kind
        == UseInteractionOutcome::PickedUpItem
    );

    REQUIRE(inventory_item_ids.size() == 1);
    CHECK(inventory_item_ids[0] == "lab_keycard");
    CHECK(usables[0].consumed);
    CHECK_FALSE(usables[0].enabled);
    CHECK_FALSE(usables[1].locked);
    CHECK(activated_target_ids.empty());
    REQUIRE(signaled_target_ids.size() == 1);
    CHECK(signaled_target_ids[0] == "reader_lock");

    CHECK(
        RelayGame::apply_usable_interaction(
            usables[0],
            doors,
            usables,
            inventory_item_ids,
            activated_target_ids,
            signaled_target_ids
        )
            .kind
        == UseInteractionOutcome::None
    );
    CHECK(inventory_item_ids.size() == 1);
}

TEST_CASE("apply usable interaction records activated targets and respects single-use")
{
    std::vector<RuntimeDoor> doors{
        RuntimeDoor{.name = "door_a"},
    };
    std::vector<RuntimeUsable> usables{
        RuntimeUsable{
            .label = "Reusable panel",
            .target = "door_a",
            .enabled = true,
            .action_kind = UsableActionKind::ActivateTarget,
        },
        RuntimeUsable{
            .label = "One-shot terminal",
            .target = "lab_relay_console",
            .single_use = true,
            .enabled = true,
            .action_kind = UsableActionKind::ActivateTarget,
        },
    };
    std::vector<std::string> inventory_item_ids;
    std::vector<std::string> activated_target_ids;
    std::vector<std::string> signaled_target_ids;

    CHECK(
        RelayGame::apply_usable_interaction(
            usables[0],
            doors,
            usables,
            inventory_item_ids,
            activated_target_ids,
            signaled_target_ids
        )
            .kind
        == UseInteractionOutcome::ActivatedTarget
    );
    CHECK(doors[0].state == DoorState::Opening);
    CHECK_FALSE(usables[0].consumed);
    CHECK(usables[0].enabled);

    REQUIRE(activated_target_ids.size() == 1);
    CHECK(activated_target_ids[0] == "door_a");
    CHECK(signaled_target_ids.empty());

    CHECK(
        RelayGame::apply_usable_interaction(
            usables[1],
            doors,
            usables,
            inventory_item_ids,
            activated_target_ids,
            signaled_target_ids
        )
            .kind
        == UseInteractionOutcome::ActivatedTarget
    );
    CHECK(usables[1].consumed);
    CHECK_FALSE(usables[1].enabled);
    REQUIRE(activated_target_ids.size() == 2);
    CHECK(activated_target_ids[1] == "lab_relay_console");

    CHECK(
        RelayGame::apply_usable_interaction(
            usables[1],
            doors,
            usables,
            inventory_item_ids,
            activated_target_ids,
            signaled_target_ids
        )
            .kind
        == UseInteractionOutcome::None
    );
    CHECK(activated_target_ids.size() == 2);
}

TEST_CASE("apply usable interaction records primary and chained signal targets")
{
    std::vector<RuntimeDoor> doors{
        RuntimeDoor{.name = "door_a"},
    };
    std::vector<RuntimeUsable> usables{
        RuntimeUsable{
            .label = "Signal opener",
            .target = "door_a",
            .chain_target = "reader_lock",
            .enabled = true,
            .action_kind = UsableActionKind::SignalTarget,
        },
        RuntimeUsable{
            .name = "reader_lock",
            .locked = true,
            .enabled = true,
            .signal_action = RelayCore::gameplay::UsableSignalAction::Unlock,
        },
    };
    std::vector<std::string> inventory_item_ids;
    std::vector<std::string> activated_target_ids;
    std::vector<std::string> signaled_target_ids;

    CHECK(
        RelayGame::apply_usable_interaction(
            usables[0],
            doors,
            usables,
            inventory_item_ids,
            activated_target_ids,
            signaled_target_ids
        )
            .kind
        == UseInteractionOutcome::SignaledTarget
    );
    CHECK(doors[0].state == DoorState::Opening);
    CHECK_FALSE(usables[1].locked);
    CHECK(activated_target_ids.empty());
    REQUIRE(signaled_target_ids.size() == 2);
    CHECK(signaled_target_ids[0] == "door_a");
    CHECK(signaled_target_ids[1] == "reader_lock");
}

TEST_CASE("shipped test_room use flow preserves required item and terminal activation")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};

    const std::filesystem::path repo_root = std::filesystem::path(__FILE__).parent_path().parent_path();
    const RelayCore::BlockoutLevel level =
        RelayCore::load_blockout_level(repo_root / "assets" / "levels" / "test_room.toml");

    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeTrigger> triggers;
    std::vector<RuntimeUsable> usables;
    RelayCore::gameplay::build_runtime_interactions(level, doors, triggers, usables);

    auto reader_panel = std::find_if(usables.begin(), usables.end(), [](const RuntimeUsable& usable) {
        return usable.label == "Подтвердить допуск";
    });
    auto keycard_pickup = std::find_if(usables.begin(), usables.end(), [](const RuntimeUsable& usable) {
        return usable.action_kind == UsableActionKind::PickupItem && usable.item_id == "lab_keycard";
    });
    auto terminal = std::find_if(usables.begin(), usables.end(), [](const RuntimeUsable& usable) {
        return usable.target == "lab_relay_console" && usable.action_kind == UsableActionKind::ActivateTarget;
    });
    auto inner_door = std::find_if(doors.begin(), doors.end(), [](const RuntimeDoor& door) {
        return door.name == "inner_bulkhead";
    });

    REQUIRE(reader_panel != usables.end());
    REQUIRE(keycard_pickup != usables.end());
    REQUIRE(terminal != usables.end());
    REQUIRE(inner_door != doors.end());
    std::vector<RuntimeTurret> turrets = RelayCore::gameplay::build_runtime_combat(level).turrets;
    REQUIRE(turrets.size() == 1);

    std::vector<std::string> inventory_item_ids;
    std::vector<std::string> activated_target_ids;
    std::vector<std::string> signaled_target_ids;

    CHECK(
        RelayGame::apply_usable_interaction(
            *reader_panel,
            doors,
            usables,
            inventory_item_ids,
            activated_target_ids,
            signaled_target_ids
        )
            .kind
        == UseInteractionOutcome::MissingRequiredItem
    );
    CHECK(inner_door->state == DoorState::Closed);

    CHECK(
        RelayGame::apply_usable_interaction(
            *keycard_pickup,
            doors,
            usables,
            inventory_item_ids,
            activated_target_ids,
            signaled_target_ids
        )
            .kind
        == UseInteractionOutcome::PickedUpItem
    );
    REQUIRE(inventory_item_ids.size() == 1);
    CHECK(inventory_item_ids[0] == "lab_keycard");
    CHECK(keycard_pickup->consumed);
    CHECK_FALSE(keycard_pickup->enabled);

    CHECK(
        RelayGame::apply_usable_interaction(
            *reader_panel,
            doors,
            usables,
            inventory_item_ids,
            activated_target_ids,
            signaled_target_ids
        )
            .kind
        == UseInteractionOutcome::ActivatedTarget
    );
    CHECK(reader_panel->consumed);
    CHECK_FALSE(reader_panel->enabled);
    CHECK(inner_door->state == DoorState::Opening);

    CHECK(
        RelayGame::apply_usable_interaction(
            *terminal,
            doors,
            usables,
            inventory_item_ids,
            activated_target_ids,
            signaled_target_ids
        )
            .kind
        == UseInteractionOutcome::Locked
    );

    PlayerCombatState player_combat{};
    RelayCore::gameplay::reset_player_combat_state(player_combat);
    player_combat.weapon_range = 16.0F;
    player_combat.weapon_damage = 25.0F;
    const glm::vec3 shot_origin = turrets[0].origin + glm::vec3{-1.8F, 0.0F, -1.2F};
    const glm::vec3 shot_direction = turrets[0].origin - shot_origin;

    const auto first_shot = RelayCore::gameplay::apply_player_fire(
        RelayCore::test::WorldQueryFixture{level, doors},
        shot_origin,
        shot_direction,
        player_combat,
        turrets
    );
    CHECK(first_shot.fired);
    CHECK(first_shot.hit_turret);
    CHECK_FALSE(first_shot.killed_turret);

    RelayCore::gameplay::tick_combat_state(0.30F, player_combat, turrets);
    const auto second_shot = RelayCore::gameplay::apply_player_fire(
        RelayCore::test::WorldQueryFixture{level, doors},
        shot_origin,
        shot_direction,
        player_combat,
        turrets
    );
    CHECK(second_shot.fired);
    CHECK(second_shot.hit_turret);
    CHECK(second_shot.killed_turret);
    CHECK(RelayCore::gameplay::signal_target(second_shot.target_event.target, doors, usables));
    signaled_target_ids.push_back(second_shot.target_event.target);

    CHECK(
        RelayGame::apply_usable_interaction(
            *terminal,
            doors,
            usables,
            inventory_item_ids,
            activated_target_ids,
            signaled_target_ids
        )
            .kind
        == UseInteractionOutcome::ActivatedTarget
    );
    CHECK(terminal->enabled);
    CHECK_FALSE(terminal->consumed);
    CHECK(
        std::find(activated_target_ids.begin(), activated_target_ids.end(), std::string{"lab_relay_console"})
        != activated_target_ids.end()
    );
    CHECK(
        std::find(signaled_target_ids.begin(), signaled_target_ids.end(), std::string{"lab_console_lock"})
        != signaled_target_ids.end()
    );

    RelayCore::log::shutdown();
}

TEST_CASE("signal target ignores consumed or disabled signal receivers")
{
    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeUsable> usables{
        RuntimeUsable{
            .name = "reader_lock",
            .label = "Consumed reader",
            .locked = true,
            .enabled = false,
            .consumed = true,
            .signal_action = RelayCore::gameplay::UsableSignalAction::Unlock,
        },
        RuntimeUsable{
            .name = "reader_lock",
            .label = "Active reader",
            .locked = true,
            .enabled = true,
            .consumed = false,
            .signal_action = RelayCore::gameplay::UsableSignalAction::Unlock,
        },
    };

    CHECK(RelayCore::gameplay::signal_target("reader_lock", doors, usables));
    CHECK(usables[0].locked);
    CHECK_FALSE(usables[1].locked);

    usables[1].enabled = false;
    usables[1].consumed = true;
    usables[1].locked = true;

    CHECK_FALSE(RelayCore::gameplay::signal_target("reader_lock", doors, usables));
    CHECK(usables[0].locked);
    CHECK(usables[1].locked);
}

TEST_CASE("auto-generated usable follows moved door bounds")
{
    std::vector<RuntimeDoor> doors{
        RuntimeDoor{
            .name = "door_auto_use",
            .closed_bounds = make_bounds({0.0F, 1.0F, 4.0F}, {1.0F, 1.0F, 0.1F}),
            .open_offset = {2.25F, 0.0F, 0.0F},
            .open_fraction = 0.5F,
        },
    };
    std::vector<RuntimeUsable> usables{
        RuntimeUsable{
            .label = "Использовать",
            .target = "door_auto_use",
            .origin = {0.0F, 1.0F, 4.0F},
            .use_radius = 2.25F,
            .enabled = true,
            .tracks_target_door = true,
        },
    };

    RelayCore::gameplay::update_runtime_usables(doors, usables);

    REQUIRE(usables[0].enabled);
    CHECK(usables[0].origin.x == doctest::Approx(1.125F));
    CHECK(usables[0].origin.z == doctest::Approx(4.0F));
    CHECK(usables[0].has_focus_bounds);
    CHECK(usables[0].focus_bounds.mins.x == doctest::Approx(0.125F));
    CHECK(usables[0].focus_bounds.maxs.x == doctest::Approx(2.125F));

    doors[0].open_fraction = 1.0F;
    doors[0].state = DoorState::Open;

    RelayCore::gameplay::update_runtime_usables(doors, usables);

    CHECK(usables[0].enabled);
    CHECK(usables[0].focus_bounds.mins.x == doctest::Approx(1.25F));
    CHECK(usables[0].focus_bounds.maxs.x == doctest::Approx(3.25F));
}

TEST_CASE("opened door remains focusable by its moved bounds")
{
    RuntimeDoor door{};
    door.name = "airlock_main";
    door.closed_bounds = make_bounds({0.0F, 1.3F, 3.7F}, {1.0F, 1.3F, 0.09F});
    door.open_offset = {2.25F, 0.0F, 0.0F};
    door.open_fraction = 1.0F;
    door.state = DoorState::Open;
    std::vector<RuntimeDoor> doors{door};
    std::vector<RuntimeUsable> usables{
        RuntimeUsable{
            .label = "Open airlock",
            .target = "airlock_main",
            .origin = {0.0F, 1.35F, 2.9F},
            .tracked_offset = {0.0F, 0.05F, -0.8F},
            .use_radius = 2.1F,
            .has_focus_bounds = true,
            .enabled = true,
            .tracks_target_door = true,
        },
    };

    RelayCore::gameplay::update_runtime_usables(doors, usables);

    const InteractionQueryResult result =
        RelayCore::gameplay::find_focused_usable({1.6F, 1.35F, 2.2F}, {0.0F, 0.0F, 1.0F}, usables);

    CHECK(result.usable_index == 0);
}

TEST_CASE("consumed pickup stays disabled after runtime refresh")
{
    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeUsable> usables{
        RuntimeUsable{
            .label = "Взять: Lab keycard",
            .item_id = "lab_keycard",
            .item_name = "Lab keycard",
            .origin = {-1.1F, 1.15F, 7.45F},
            .visual_bounds = make_bounds({-1.1F, 1.15F, 7.45F}, {0.11F, 0.06F, 0.01F}),
            .use_radius = 1.4F,
            .has_focus_bounds = true,
            .has_visual_bounds = true,
            .enabled = true,
            .consumed = true,
            .tracks_target_door = false,
            .visual_kind = UsableVisualKind::Pickup,
            .action_kind = UsableActionKind::PickupItem,
        },
    };

    RelayCore::gameplay::update_runtime_usables(doors, usables);

    CHECK_FALSE(usables[0].enabled);
}

TEST_CASE("usable spatial index narrows focus candidates")
{
    std::vector<RuntimeUsable> usables{
        RuntimeUsable{.label = "near", .target = "door_0", .origin = {0.0F, 1.0F, -1.0F}, .use_radius = 3.0F},
        RuntimeUsable{.label = "far", .target = "door_1", .origin = {32.0F, 1.0F, -1.0F}, .use_radius = 3.0F},
    };
    RelayCore::gameplay::UsableSpatialIndex index{};
    std::vector<std::uint32_t> candidates;

    RelayCore::gameplay::rebuild_usable_spatial_index(usables, index);
    RelayCore::gameplay::query_usable_candidates(index, {0.0F, 1.0F, 0.0F}, 2.25F, candidates);

    REQUIRE(candidates.size() == 1);
    CHECK(candidates[0] == 0);

    const InteractionQueryResult result =
        RelayCore::gameplay::find_focused_usable({0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, -1.0F}, usables, candidates);

    CHECK(result.usable_index == 0);
}

TEST_CASE("usable spatial index invalidates bounds and membership without changing focus rules")
{
    std::vector<RuntimeUsable> usables{{.origin = {0, 1, 0}, .use_radius = .5F}};
    RelayCore::gameplay::UsableSpatialIndex index;
    std::vector<std::uint32_t> candidates;
    const auto query = [&](float x) {
        RelayCore::gameplay::rebuild_usable_spatial_index(usables, index);
        RelayCore::gameplay::query_usable_candidates(index, {x, 1, 0}, .1F, candidates);
    };
    for (int repeat = 0; repeat < 8; ++repeat)
    {
        query(0);
        REQUIRE(candidates == std::vector<std::uint32_t>{0});
    }

    usables[0].origin.x = 20;
    query(0);
    CHECK(candidates.empty());
    query(20);
    REQUIRE(candidates == std::vector<std::uint32_t>{0});
    usables[0].enabled = false;
    query(20);
    CHECK(candidates.empty());
    usables[0].enabled = true;
    usables[0].use_radius = 10;
    query(29);
    REQUIRE(candidates == std::vector<std::uint32_t>{0});

    usables[0].has_focus_bounds = true;
    usables[0].focus_bounds = {{40, 0, -1}, {42, 2, 1}};
    query(20);
    CHECK(candidates.empty());
    query(41);
    REQUIRE(candidates == std::vector<std::uint32_t>{0});
    usables.insert(usables.begin(), {.origin = {-20, 1, 0}, .use_radius = .5F});
    query(41);
    REQUIRE(candidates == std::vector<std::uint32_t>{1});

    usables.clear();
    query(41);
    CHECK(candidates.empty());
    usables.push_back({.origin = {0, 1, 0}, .use_radius = .5F});
    query(0);
    CHECK(candidates == std::vector<std::uint32_t>{0});
}

TEST_CASE("shipped test_room preserves authored interaction metadata")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};

    const std::filesystem::path repo_root = std::filesystem::path(__FILE__).parent_path().parent_path();
    const RelayCore::BlockoutLevel level =
        RelayCore::load_blockout_level(repo_root / "assets" / "levels" / "test_room.toml");

    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeTrigger> triggers;
    std::vector<RuntimeUsable> usables;
    RelayCore::gameplay::build_runtime_interactions(level, doors, triggers, usables);

    CHECK(doors.size() == 2);
    CHECK(triggers.size() == 1);
    CHECK(usables.size() == 6);

    const auto outer_panel = std::find_if(usables.begin(), usables.end(), [](const RuntimeUsable& usable) {
        return usable.target == "intake/outer_bulkhead" && usable.label == "Открыть внешний шлюз";
    });
    REQUIRE(outer_panel != usables.end());
    CHECK(outer_panel->authored_entity_index >= 0);
    CHECK(outer_panel->generated_from_entity_index == -1);
    CHECK(outer_panel->use_radius == doctest::Approx(2.45F));
    CHECK(outer_panel->success_message == "Команда на внешний шлюз принята");

    const auto keycard_pickup = std::find_if(usables.begin(), usables.end(), [](const RuntimeUsable& usable) {
        return usable.action_kind == UsableActionKind::PickupItem && usable.item_id == "lab_keycard";
    });
    REQUIRE(keycard_pickup != usables.end());
    CHECK(keycard_pickup->authored_entity_index >= 0);
    CHECK(keycard_pickup->generated_from_entity_index == -1);
    CHECK(keycard_pickup->use_radius == doctest::Approx(1.65F));
    CHECK(keycard_pickup->item_name == "Ключ-карта лаборатории");
    CHECK(keycard_pickup->chain_target.empty());
    CHECK(
        keycard_pickup->success_message == "Ключ-карта получена. Подтвердите допуск на считывателе у внутреннего шлюза."
    );
    CHECK(keycard_pickup->has_visual_bounds);
    CHECK(keycard_pickup->visual_kind == UsableVisualKind::Pickup);

    const auto reader_panel = std::find_if(usables.begin(), usables.end(), [](const RuntimeUsable& usable) {
        return usable.label == "Подтвердить допуск";
    });
    REQUIRE(reader_panel != usables.end());
    CHECK(reader_panel->authored_entity_index >= 0);
    CHECK(reader_panel->generated_from_entity_index == -1);
    CHECK_FALSE(reader_panel->locked);
    CHECK(reader_panel->single_use);
    CHECK(reader_panel->signal_action == RelayCore::gameplay::UsableSignalAction::None);
    CHECK(reader_panel->target == "inner_bulkhead");
    CHECK(reader_panel->required_item == "lab_keycard");
    CHECK(reader_panel->required_item_name == "ключ-карта лаборатории");
    CHECK(reader_panel->locked_message.empty());
    CHECK(reader_panel->denied_message == "Нужна ключ-карта лаборатории");
    CHECK(reader_panel->success_message == "Допуск подтвержден. Внутренний шлюз открыт");
    CHECK(reader_panel->has_visual_bounds);
    CHECK(reader_panel->visual_kind == UsableVisualKind::Panel);

    const auto inner_panel = std::find_if(usables.begin(), usables.end(), [](const RuntimeUsable& usable) {
        return usable.target == "inner_bulkhead" && usable.label == "Открыть внутренний шлюз";
    });
    REQUIRE(inner_panel != usables.end());
    CHECK_FALSE(inner_panel->locked);
    CHECK(inner_panel->signal_action == RelayCore::gameplay::UsableSignalAction::None);
    CHECK(inner_panel->target == "inner_bulkhead");
    CHECK(inner_panel->locked_message.empty());
    CHECK(inner_panel->use_radius == doctest::Approx(2.35F));

    const auto terminal = std::find_if(usables.begin(), usables.end(), [](const RuntimeUsable& usable) {
        return usable.action_kind == UsableActionKind::ActivateTarget && usable.target == "lab_relay_console";
    });
    REQUIRE(terminal != usables.end());
    CHECK(terminal->authored_entity_index >= 0);
    CHECK(terminal->generated_from_entity_index == -1);
    CHECK(terminal->label == "Стабилизировать ядро ретранслятора");
    CHECK(terminal->success_message == "Ядро ретранслятора стабилизировано");
    CHECK(terminal->use_radius == doctest::Approx(2.20F));

    RelayCore::log::shutdown();
}

TEST_CASE("shipped test_room reader panel focus works from authored approach")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};

    const std::filesystem::path repo_root = std::filesystem::path(__FILE__).parent_path().parent_path();
    const RelayCore::BlockoutLevel level =
        RelayCore::load_blockout_level(repo_root / "assets" / "levels" / "test_room.toml");

    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeTrigger> triggers;
    std::vector<RuntimeUsable> usables;
    RelayCore::gameplay::build_runtime_interactions(level, doors, triggers, usables);

    const auto reader_panel = std::find_if(usables.begin(), usables.end(), [](const RuntimeUsable& usable) {
        return usable.label == "Подтвердить допуск";
    });
    REQUIRE(reader_panel != usables.end());

    const InteractionQueryResult result =
        RelayCore::gameplay::find_focused_usable({3.10F, 1.35F, 7.40F}, {-1.0F, 0.0F, 0.0F}, usables);

    REQUIRE(result.usable_index >= 0);
    CHECK(&usables[static_cast<std::size_t>(result.usable_index)] == &(*reader_panel));
    CHECK(result.distance < 1.6F);

    RelayCore::log::shutdown();
}

TEST_CASE("shipped test_room airlock trigger fires once and reseals the bulkhead")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};

    const std::filesystem::path repo_root = std::filesystem::path(__FILE__).parent_path().parent_path();
    const RelayCore::BlockoutLevel level =
        RelayCore::load_blockout_level(repo_root / "assets" / "levels" / "test_room.toml");

    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeTrigger> triggers;
    std::vector<RuntimeUsable> usables;
    RelayCore::gameplay::build_runtime_interactions(level, doors, triggers, usables);

    REQUIRE(triggers.size() == 1);
    auto outer_door = std::find_if(doors.begin(), doors.end(), [](const RuntimeDoor& door) {
        return door.name == "intake/outer_bulkhead";
    });
    REQUIRE(outer_door != doors.end());
    outer_door->state = DoorState::Open;
    outer_door->open_fraction = 1.0F;

    const glm::vec3 trigger_center = (triggers[0].bounds.mins + triggers[0].bounds.maxs) * 0.5F;
    const Aabb player_bounds = make_bounds(trigger_center, {0.3F, 0.9F, 0.3F});
    std::vector<std::string> activated_target_ids;
    std::vector<std::string> signaled_target_ids;
    InteractionEvent last_event;

    CHECK(
        RelayCore::gameplay::update_trigger_overlaps(
            player_bounds,
            triggers,
            doors,
            usables,
            activated_target_ids,
            signaled_target_ids,
            last_event
        )
        == 1
    );
    CHECK(triggers[0].fired);
    CHECK(last_event.target == "intake/outer_bulkhead");
    CHECK(outer_door->state == DoorState::Closing);
    REQUIRE(activated_target_ids.size() == 1);
    CHECK(activated_target_ids[0] == "intake/outer_bulkhead");
    CHECK(signaled_target_ids.empty());
    CHECK(
        RelayCore::gameplay::update_trigger_overlaps(
            player_bounds,
            triggers,
            doors,
            usables,
            activated_target_ids,
            signaled_target_ids,
            last_event
        )
        == 0
    );

    RelayCore::log::shutdown();
}

TEST_CASE("shipped test_room fully open bulkheads are excluded from blockers")
{
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::test::ScopedTestLogger logger{temp.path() / "logs"};

    const std::filesystem::path repo_root = std::filesystem::path(__FILE__).parent_path().parent_path();
    const RelayCore::BlockoutLevel level =
        RelayCore::load_blockout_level(repo_root / "assets" / "levels" / "test_room.toml");

    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeTrigger> triggers;
    std::vector<RuntimeUsable> usables;
    RelayCore::gameplay::build_runtime_interactions(level, doors, triggers, usables);

    for (RuntimeDoor& door : doors)
    {
        door.state = DoorState::Open;
        door.open_fraction = 1.0F;
    }

    std::vector<RelayCore::physics::WorldCollider> blockers;
    RelayCore::gameplay::collect_blocking_doors(doors, make_bounds({0.0F, 1.0F, 0.0F}, {0.3F, 0.9F, 0.3F}), blockers);

    CHECK(blockers.empty());

    RelayCore::log::shutdown();
}

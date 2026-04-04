#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/world/blockout_level.h"

#include <doctest/doctest.h>
#include <vector>

namespace
{
    using RelayCore::gameplay::DoorActivationPolicy;
    using RelayCore::gameplay::DoorState;
    using RelayCore::gameplay::InteractionQueryResult;
    using RelayCore::gameplay::RuntimeDoor;
    using RelayCore::gameplay::RuntimeTrigger;
    using RelayCore::gameplay::RuntimeUsable;
    using RelayCore::gameplay::UsableVisualKind;
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
    std::vector<RuntimeTrigger> triggers{
        RuntimeTrigger{.target = "door_0", .bounds = make_bounds({0.0F, 1.0F, 0.0F}, {0.5F, 1.0F, 0.5F}), .once = true},
    };
    std::string last_target;
    const Aabb player = make_bounds({0.0F, 1.0F, 0.0F}, {0.3F, 0.9F, 0.3F});

    CHECK(RelayCore::gameplay::update_trigger_overlaps(player, triggers, doors, last_target) == 1);
    CHECK(doors[0].state == DoorState::Opening);
    CHECK(triggers[0].fired);
    CHECK(last_target == "door_0");
    CHECK(RelayCore::gameplay::update_trigger_overlaps(player, triggers, doors, last_target) == 0);
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
    level.entities.push_back(
        RelayCore::EntitySpawnData{
            .classname = "gameplay_entity",
            .origin = {0.0F, 0.0F, 4.0F},
            .properties = {
                {"kind", "door"},
                {"targetname", "door_auto_use"},
                {"width", "2.0"},
                {"height", "2.0"},
            },
        }
    );

    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeTrigger> triggers;
    std::vector<RuntimeUsable> usables;

    RelayCore::gameplay::build_runtime_interactions(level, doors, triggers, usables);

    REQUIRE(doors.size() == 1);
    REQUIRE(usables.size() == 1);
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

TEST_CASE("runtime interactions read target links from entity properties")
{
    RelayCore::BlockoutLevel level{};
    level.entities.push_back(
        RelayCore::EntitySpawnData{
            .classname = "gameplay_entity",
            .origin = {0.0F, 0.0F, 0.0F},
            .properties = {
                {"kind", "door"},
                {"targetname", "airlock_main"},
                {"move_speed", "2.5"},
                {"travel_distance", "3.0"},
            },
        }
    );
    level.entities.push_back(
        RelayCore::EntitySpawnData{
            .classname = "gameplay_entity",
            .origin = {0.0F, 1.0F, -1.0F},
            .properties = {
                {"kind", "usable"},
                {"target", "airlock_main"},
                {"label", "Open airlock"},
                {"use_radius", "3.5"},
            },
        }
    );
    level.entities.push_back(
        RelayCore::EntitySpawnData{
            .classname = "gameplay_entity",
            .origin = {0.0F, 0.0F, 2.0F},
            .properties = {
                {"kind", "trigger"},
                {"target", "airlock_main"},
                {"targetname", "airlock_trigger"},
                {"once", "false"},
            },
        }
    );

    std::vector<RuntimeDoor> doors;
    std::vector<RuntimeTrigger> triggers;
    std::vector<RuntimeUsable> usables;

    RelayCore::gameplay::build_runtime_interactions(level, doors, triggers, usables);

    REQUIRE(doors.size() == 1);
    REQUIRE(usables.size() == 1);
    REQUIRE(triggers.size() == 1);

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
    level.entities.push_back(
        RelayCore::EntitySpawnData{
            .classname = "gameplay_entity",
            .origin = {0.0F, 0.0F, 4.0F},
            .properties = {
                {"kind", "door"},
                {"width", "2.0"},
                {"height", "2.6"},
                {"thickness", "0.18"},
                {"move_offset_x", "2.25"},
                {"move_offset_y", "0.0"},
                {"move_offset_z", "0.0"},
            },
        }
    );

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

TEST_CASE("runtime interactions can be defined by kind property without dev classnames")
{
    RelayCore::BlockoutLevel level{};
    level.entities.push_back(
        RelayCore::EntitySpawnData{
            .classname = "marker",
            .origin = {0.0F, 0.0F, 4.0F},
            .properties = {
                {"kind", "door"},
                {"targetname", "door_schema"},
            },
        }
    );
    level.entities.push_back(
        RelayCore::EntitySpawnData{
            .classname = "marker",
            .origin = {0.0F, 1.0F, 3.0F},
            .properties = {
                {"kind", "usable"},
                {"target", "door_schema"},
            },
        }
    );
    level.entities.push_back(
        RelayCore::EntitySpawnData{
            .classname = "marker",
            .origin = {0.0F, 0.0F, 6.0F},
            .properties = {
                {"kind", "trigger"},
                {"target", "door_schema"},
            },
        }
    );

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
    level.entities.push_back(
        RelayCore::EntitySpawnData{
            .classname = "func_door",
            .origin = {0.0F, 0.0F, 4.0F},
        }
    );
    level.entities.push_back(
        RelayCore::EntitySpawnData{
            .classname = "dev_use",
            .origin = {0.0F, 1.0F, 3.0F},
            .properties = {
                {"target", "legacy_door"},
            },
        }
    );
    level.entities.push_back(
        RelayCore::EntitySpawnData{
            .classname = "trigger_once",
            .origin = {0.0F, 0.0F, 6.0F},
            .properties = {
                {"target", "legacy_door"},
            },
        }
    );

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
    level.entities.push_back(
        RelayCore::EntitySpawnData{
            .classname = "gameplay_entity",
            .origin = {0.0F, 0.0F, 4.0F},
            .properties = {
                {"kind", "door"},
                {"targetname", "airlock_main"},
            },
        }
    );
    level.entities.push_back(
        RelayCore::EntitySpawnData{
            .classname = "gameplay_entity",
            .origin = {-2.9F, 1.35F, 3.2F},
            .properties = {
                {"kind", "panel"},
                {"target", "airlock_main"},
                {"label", "Open airlock"},
                {"width", "0.18"},
                {"height", "0.45"},
                {"depth", "0.28"},
            },
        }
    );

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
            .label = "Use",
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

TEST_CASE("usable spatial index narrows focus candidates")
{
    std::vector<RuntimeUsable> usables{
        RuntimeUsable{.label = "near", .target = "door_0", .origin = {0.0F, 1.0F, -1.0F}, .use_radius = 3.0F},
        RuntimeUsable{.label = "far", .target = "door_1", .origin = {32.0F, 1.0F, -1.0F}, .use_radius = 3.0F},
    };
    RelayCore::gameplay::InteractionSpatialIndex index{};
    std::vector<std::uint32_t> candidates;

    RelayCore::gameplay::rebuild_usable_spatial_index(usables, index);
    RelayCore::gameplay::query_usable_candidates(index, {0.0F, 1.0F, 0.0F}, 2.25F, candidates);

    REQUIRE(candidates.size() == 1);
    CHECK(candidates[0] == 0);

    const InteractionQueryResult result =
        RelayCore::gameplay::find_focused_usable({0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, -1.0F}, usables, candidates);

    CHECK(result.usable_index == 0);
}

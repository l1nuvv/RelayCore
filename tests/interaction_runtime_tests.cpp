#include <doctest/doctest.h>
#include <vector>

#include "relaycore/gameplay/interaction_runtime.h"

namespace
{
    using RelayCore::gameplay::InteractionQueryResult;
    using RelayCore::gameplay::RuntimeDoor;
    using RelayCore::gameplay::RuntimeTrigger;
    using RelayCore::gameplay::RuntimeUsable;
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
    CHECK_FALSE(doors[0].opening);
    CHECK(doors[1].opening);
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
    const Aabb  player = make_bounds({0.0F, 1.0F, 0.0F}, {0.3F, 0.9F, 0.3F});

    CHECK(RelayCore::gameplay::update_trigger_overlaps(player, triggers, doors, last_target) == 1);
    CHECK(doors[0].opening);
    CHECK(triggers[0].fired);
    CHECK(last_target == "door_0");
    CHECK(RelayCore::gameplay::update_trigger_overlaps(player, triggers, doors, last_target) == 0);
}

TEST_CASE("door reaches fully open state")
{
    std::vector<RuntimeDoor> doors{
        RuntimeDoor{.name = "door_0", .move_speed = 2.0F, .opening = true},
    };

    for (int i = 0; i < 60; ++i)
    {
        (void) RelayCore::gameplay::update_doors(1.0F / 60.0F, doors);
    }

    CHECK(doors[0].open);
    CHECK_FALSE(doors[0].opening);
    CHECK(doors[0].open_fraction == doctest::Approx(1.0F).epsilon(0.001));
}

TEST_CASE("fully open door no longer blocks")
{
    RuntimeDoor door{};
    door.open_fraction = 1.0F;
    CHECK_FALSE(RelayCore::gameplay::is_door_blocking(door));
}

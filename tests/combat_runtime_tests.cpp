#include "relaycore/gameplay/combat_runtime.h"
#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/world/blockout_level.h"

#include <doctest/doctest.h>

namespace
{
    using RelayCore::BlockoutLevel;
    using RelayCore::BlockoutSolid;
    using RelayCore::EntitySpawnData;
    using RelayCore::SurfaceMaterial;
    using RelayCore::gameplay::CombatTargetActionKind;
    using RelayCore::gameplay::PlayerCombatState;
    using RelayCore::gameplay::RuntimeDoor;
    using RelayCore::gameplay::RuntimeHazardVolume;
    using RelayCore::gameplay::RuntimeTurret;
    using RelayCore::physics::Aabb;

    [[nodiscard]] auto make_combat_bounds(const glm::vec3& center, const glm::vec3& half_extents) -> Aabb
    {
        return {center - half_extents, center + half_extents};
    }
} // namespace

TEST_CASE("combat runtime builds authored turrets and hazard volumes")
{
    BlockoutLevel level{};
    level.solids = {
        BlockoutSolid{
            .mins = {-1.0F, 0.0F, -1.0F},
            .maxs = {1.0F, 0.2F, 1.0F},
            .material = SurfaceMaterial::Hazard,
        },
        BlockoutSolid{
            .mins = {3.0F, 0.0F, 3.0F},
            .maxs = {4.0F, 1.0F, 4.0F},
            .material = SurfaceMaterial::Wall,
        },
    };

    EntitySpawnData turret{};
    turret.classname = "gameplay_entity";
    turret.origin = {4.0F, 1.4F, 6.0F};
    turret.properties["kind"] = "turret";
    turret.properties["targetname"] = "lab_turret";
    turret.properties["label"] = "Lab turret";
    turret.properties["health"] = "60";
    turret.properties["damage"] = "18";
    turret.properties["attack_range"] = "15";
    turret.properties["fire_interval"] = "0.75";
    turret.properties["warmup"] = "0.30";
    turret.properties["destroy_target"] = "console_lock";
    turret.properties["destroy_action"] = "signal_target";
    level.entities.push_back(std::move(turret));

    const RelayCore::gameplay::CombatBuildResult result = RelayCore::gameplay::build_runtime_combat(level);

    REQUIRE(result.hazards.size() == 1);
    CHECK(result.hazards[0].authored_solid_index == 0);
    CHECK(result.hazards[0].bounds.maxs.y > level.solids[0].maxs.y);

    REQUIRE(result.turrets.size() == 1);
    const RuntimeTurret& built_turret = result.turrets[0];
    CHECK(built_turret.name == "lab_turret");
    CHECK(built_turret.label == "Lab turret");
    CHECK(built_turret.max_health == doctest::Approx(60.0F));
    CHECK(built_turret.damage == doctest::Approx(18.0F));
    CHECK(built_turret.attack_range == doctest::Approx(15.0F));
    CHECK(built_turret.fire_interval_seconds == doctest::Approx(0.75F));
    CHECK(built_turret.warmup_seconds == doctest::Approx(0.30F));
    CHECK(built_turret.destroy_target == "console_lock");
    CHECK(built_turret.destroy_action == CombatTargetActionKind::SignalTarget);
}

TEST_CASE("player fire respects blocking doors and emits turret destroy events")
{
    BlockoutLevel level{};
    PlayerCombatState player{};
    RelayCore::gameplay::reset_player_combat_state(player);
    player.weapon_damage = 25.0F;
    player.weapon_range = 12.0F;

    std::vector<RuntimeDoor> doors{
        RuntimeDoor{
            .name = "blast_door",
            .closed_bounds = make_combat_bounds({0.0F, 1.0F, 3.0F}, {0.8F, 1.0F, 0.15F}),
        },
    };
    std::vector<RuntimeTurret> turrets{
        RuntimeTurret{
            .authored_entity_index = 4,
            .name = "blast_turret",
            .label = "Blast turret",
            .destroy_target = "console_lock",
            .bounds = make_combat_bounds({0.0F, 1.0F, 6.0F}, {0.35F, 0.45F, 0.35F}),
            .origin = {0.0F, 1.0F, 6.0F},
            .max_health = 50.0F,
            .health = 50.0F,
            .destroy_action = CombatTargetActionKind::SignalTarget,
        },
    };

    const RelayCore::gameplay::CombatFireResult blocked_shot =
        RelayCore::gameplay::apply_player_fire(level, doors, {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, player, turrets);
    CHECK(blocked_shot.fired);
    CHECK_FALSE(blocked_shot.hit_turret);
    CHECK(turrets[0].health == doctest::Approx(50.0F));

    doors[0].open_fraction = 1.0F;
    doors[0].state = RelayCore::gameplay::DoorState::Open;
    RelayCore::gameplay::tick_combat_state(0.30F, player, turrets);

    const RelayCore::gameplay::CombatFireResult first_hit =
        RelayCore::gameplay::apply_player_fire(level, doors, {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, player, turrets);
    CHECK(first_hit.fired);
    CHECK(first_hit.hit_turret);
    CHECK_FALSE(first_hit.killed_turret);
    CHECK(turrets[0].health == doctest::Approx(25.0F));

    RelayCore::gameplay::tick_combat_state(0.30F, player, turrets);
    const RelayCore::gameplay::CombatFireResult second_hit =
        RelayCore::gameplay::apply_player_fire(level, doors, {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, player, turrets);
    CHECK(second_hit.fired);
    CHECK(second_hit.hit_turret);
    CHECK(second_hit.killed_turret);
    CHECK(second_hit.target_event.action == CombatTargetActionKind::SignalTarget);
    CHECK(second_hit.target_event.target == "console_lock");
    CHECK(turrets[0].destroyed);
    CHECK(player.kills == 1);
    CHECK(player.shots_fired == 3);
    CHECK(player.shots_hit == 2);
}

TEST_CASE("combat threats apply hazard damage, turret warmup, and lethal respawn state")
{
    BlockoutLevel level{};
    PlayerCombatState player{};
    RelayCore::gameplay::reset_player_combat_state(player);
    player.health = 21.0F;
    player.respawn_delay_seconds = 1.50F;

    std::vector<RuntimeTurret> turrets{
        RuntimeTurret{
            .authored_entity_index = 2,
            .name = "hall_turret",
            .label = "Hall turret",
            .bounds = make_combat_bounds({0.0F, 1.0F, 0.0F}, {0.35F, 0.45F, 0.35F}),
            .origin = {0.0F, 1.0F, 0.0F},
            .attack_range = 10.0F,
            .fire_interval_seconds = 0.40F,
            .warmup_seconds = 0.10F,
            .damage = 20.0F,
        },
    };
    const std::vector<RuntimeHazardVolume> hazards{
        RuntimeHazardVolume{
            .authored_solid_index = 0,
            .label = "steam_trench",
            .bounds = make_combat_bounds({0.0F, 1.0F, 4.0F}, {0.8F, 1.0F, 0.8F}),
            .damage_per_second = 10.0F,
        },
    };

    const RelayCore::physics::Aabb player_bounds = make_combat_bounds({0.0F, 1.0F, 4.0F}, {0.3F, 0.9F, 0.3F});
    const glm::vec3 player_eye_position{0.0F, 1.6F, 4.0F};

    const auto first_tick = RelayCore::gameplay::update_combat_threats(
        level,
        {},
        player_bounds,
        player_eye_position,
        player,
        turrets,
        hazards,
        0.10F
    );
    CHECK(first_tick.hazard_contacts == 1);
    CHECK(first_tick.turret_shots == 0);
    CHECK_FALSE(first_tick.player_died);
    CHECK(turrets[0].warming_up);
    CHECK(player.health == doctest::Approx(20.0F));
    CHECK(player.last_damage_source == "steam_trench");

    RelayCore::gameplay::tick_combat_state(0.10F, player, turrets);

    const auto second_tick = RelayCore::gameplay::update_combat_threats(
        level,
        {},
        player_bounds,
        player_eye_position,
        player,
        turrets,
        {},
        0.10F
    );
    CHECK(second_tick.turret_shots == 1);
    CHECK(second_tick.player_died);
    CHECK_FALSE(player.alive);
    CHECK(player.health == doctest::Approx(0.0F));

    RelayCore::gameplay::tick_combat_state(0.60F, player, turrets);
    CHECK(player.respawn_remaining_seconds == doctest::Approx(0.90F));
}

#include "relaycore/gameplay/combat_runtime.h"
#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/world/blockout_level.h"

#include <doctest/doctest.h>

#include "world_query_fixture.h"

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

TEST_CASE("player bullets stop at actor geometry before a turret behind it")
{
    PlayerCombatState player;
    RelayCore::gameplay::reset_player_combat_state(player);
    RuntimeTurret turret;
    turret.bounds = {{-1, 0, 8}, {1, 3, 9}};
    turret.health = turret.max_health = 60;
    const RelayCore::physics::WorldCollider actor{
        {{-1, 0, 4}, {1, 3, 5}},
        SurfaceMaterial::Rubber,
        {RelayCore::physics::WorldObjectKind::Enemy, 0, 42}
    };
    const auto result =
        RelayCore::gameplay::apply_player_fire({{}, {}, {&actor, 1}}, {0, 1.65F, 0}, {0, 0, 1}, player, {&turret, 1});
    REQUIRE(result.fired);
    REQUIRE(result.world_impact);
    CHECK(result.world_impact->object == actor.object);
    CHECK(result.world_impact->distance == doctest::Approx(4));
    CHECK_FALSE(result.hit_turret);
    CHECK(turret.health == 60);
}

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

TEST_CASE("combat runtime ignores thin hazard trim strips when building damaging volumes")
{
    BlockoutLevel level{};
    level.solids = {
        BlockoutSolid{
            .mins = {-1.4F, 0.0F, -0.12F},
            .maxs = {1.4F, 0.08F, 0.12F},
            .material = SurfaceMaterial::Hazard,
        },
        BlockoutSolid{
            .mins = {-1.5F, 0.0F, 0.50F},
            .maxs = {1.5F, 0.08F, 0.88F},
            .material = SurfaceMaterial::Hazard,
        },
        BlockoutSolid{
            .mins = {-1.0F, 0.0F, 2.5F},
            .maxs = {1.0F, 0.2F, 4.0F},
            .material = SurfaceMaterial::Hazard,
        },
    };

    const RelayCore::gameplay::CombatBuildResult result = RelayCore::gameplay::build_runtime_combat(level);

    REQUIRE(result.hazards.size() == 1);
    CHECK(result.hazards[0].authored_solid_index == 2);
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

    const RelayCore::gameplay::CombatFireResult blocked_shot = RelayCore::gameplay::apply_player_fire(
        RelayCore::test::WorldQueryFixture{level, doors},
        {0.0F, 1.0F, 0.0F},
        {0.0F, 0.0F, 1.0F},
        player,
        turrets
    );
    CHECK(blocked_shot.fired);
    CHECK_FALSE(blocked_shot.hit_turret);
    REQUIRE(blocked_shot.world_impact);
    CHECK(blocked_shot.world_impact->surface == RelayCore::SurfaceMaterial::Metal);
    CHECK(blocked_shot.world_impact->object.kind == RelayCore::physics::WorldObjectKind::Door);
    CHECK(blocked_shot.world_impact->position == player.shot_trace.end);
    REQUIRE(player.shot_trace.impact);
    CHECK(player.shot_trace.impact->normal == blocked_shot.world_impact->normal);
    CHECK(player.shot_trace.impact->surface == blocked_shot.world_impact->surface);
    CHECK(player.hit_confirmation_remaining_seconds == 0.0F);
    CHECK(player.shot_trace.end.z == doctest::Approx(2.85F));
    CHECK(turrets[0].health == doctest::Approx(50.0F));

    doors[0].open_fraction = 1.0F;
    doors[0].state = RelayCore::gameplay::DoorState::Open;
    RelayCore::gameplay::tick_combat_state(0.30F, player, turrets);

    const RelayCore::gameplay::CombatFireResult first_hit = RelayCore::gameplay::apply_player_fire(
        RelayCore::test::WorldQueryFixture{level, doors},
        {0.0F, 1.0F, 0.0F},
        {0.0F, 0.0F, 1.0F},
        player,
        turrets
    );
    CHECK(first_hit.fired);
    CHECK(first_hit.hit_turret);
    CHECK_FALSE(first_hit.world_impact);
    REQUIRE(player.shot_trace.impact);
    CHECK(player.shot_trace.impact->surface == RelayCore::SurfaceMaterial::Metal);
    CHECK(player.shot_trace.impact->normal == glm::vec3{0, 0, -1});
    CHECK(player.hit_confirmation_remaining_seconds > 0.0F);
    CHECK(player.shot_trace.end.z == doctest::Approx(first_hit.hit_distance));
    CHECK_FALSE(first_hit.killed_turret);
    CHECK(turrets[0].health == doctest::Approx(25.0F));

    RelayCore::gameplay::tick_combat_state(0.30F, player, turrets);
    const RelayCore::gameplay::CombatFireResult second_hit = RelayCore::gameplay::apply_player_fire(
        RelayCore::test::WorldQueryFixture{level, doors},
        {0.0F, 1.0F, 0.0F},
        {0.0F, 0.0F, 1.0F},
        player,
        turrets
    );
    CHECK(second_hit.fired);
    CHECK(second_hit.hit_turret);
    CHECK(second_hit.killed_turret);
    CHECK(second_hit.target_event.action == CombatTargetActionKind::SignalTarget);
    CHECK(second_hit.target_event.target == "console_lock");
    CHECK(turrets[0].destroyed);
    CHECK(player.shot_trace.remaining_seconds > 0.0F);
    CHECK(player.kills == 1);
    CHECK(player.shots_fired == 3);
    CHECK(player.shots_hit == 2);
    RelayCore::gameplay::tick_combat_state(0.17F, player, turrets);
    CHECK(player.hit_confirmation_remaining_seconds == 0.0F);
}

TEST_CASE("a missed shot clears the previous surface impact")
{
    RelayCore::gameplay::PlayerCombatState player;
    const std::array colliders{RelayCore::physics::WorldCollider{
        .bounds = {{-1, -1, 2}, {1, 1, 3}},
        .surface = RelayCore::SurfaceMaterial::Concrete
    }};
    (void) RelayCore::gameplay::apply_player_fire(RelayCore::physics::WorldQuery{colliders}, {}, {0, 0, 1}, player, {});
    REQUIRE(player.shot_trace.impact);
    CHECK(player.shot_trace.impact->surface == RelayCore::SurfaceMaterial::Concrete);
    CHECK(player.shot_trace.impact->normal == glm::vec3{0, 0, -1});
    RelayCore::gameplay::tick_combat_state(.3F, player, {});
    (void) RelayCore::gameplay::apply_player_fire({}, {}, {1, 0, 0}, player, {});
    CHECK_FALSE(player.shot_trace.impact);
}

TEST_CASE("activated equipment stops a turret before its pending shot without awarding a kill")
{
    std::array<RuntimeTurret, 1> turrets;
    auto& turret = turrets[0];
    turret.stop_target = "transfer_running";
    turret.origin = {0, 1, 0};
    turret.bounds = {{-.4F, .5F, -.4F}, {.4F, 1.5F, .4F}};
    turret.warming_up = true;
    turret.warmup_remaining_seconds = 0;
    turret.shot_trace.remaining_seconds = .08F;
    PlayerCombatState player;
    const std::array<std::string, 1> activated{"transfer_running"};
    const auto result = RelayCore::gameplay::update_combat_threats(
        {},
        {{-.3F, 0, 3}, {.3F, 1.8F, 3.6F}},
        {0, 1.6F, 3.3F},
        player,
        turrets,
        {},
        .1F,
        activated
    );
    CHECK(result.turret_shots == 0);
    CHECK(player.health == 100);
    CHECK(player.kills == 0);
    CHECK_FALSE(turret.enabled);
    CHECK_FALSE(turret.destroyed);
    CHECK_FALSE(turret.warming_up);
    CHECK(turret.shot_trace.remaining_seconds == 0);
    CHECK(turret.health == turret.max_health);
}

TEST_CASE("turret stop target requires an authored activation producer")
{
    BlockoutLevel level;
    RelayCore::EntitySpawnData turret;
    turret.classname = "gameplay_entity";
    turret.properties = {{"kind", "turret"}, {"targetname", "sentry"}, {"stop_target", "typo"}};
    level.entities.push_back(turret);
    CHECK_THROWS_WITH(
        static_cast<void>(RelayCore::gameplay::build_runtime_combat(level)),
        "turret 'sentry': stop_target 'typo' has no activate_target producer"
    );
    RelayCore::EntitySpawnData control;
    control.classname = "gameplay_entity";
    control.properties = {{"kind", "terminal"}, {"target", "typo"}, {"action", "activate_target"}};
    level.entities.push_back(control);
    CHECK(RelayCore::gameplay::build_runtime_combat(level).turrets[0].stop_target == "typo");
    level.entities.back().properties["kind"] = "door";
    CHECK_THROWS(static_cast<void>(RelayCore::gameplay::build_runtime_combat(level)));
    level.entities.back().properties["kind"] = "trigger";
    CHECK(RelayCore::gameplay::build_runtime_combat(level).turrets[0].stop_target == "typo");
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
        RelayCore::test::WorldQueryFixture{level, {}},
        player_bounds,
        player_eye_position,
        player,
        turrets,
        hazards,
        0.10F
    );
    CHECK(first_tick.hazard_contacts == 1);
    CHECK(first_tick.turret_shots == 0);
    CHECK(first_tick.turret_windups == 1);
    CHECK(turrets[0].shot_trace.remaining_seconds == 0);
    const auto waiting = RelayCore::gameplay::update_combat_threats(
        RelayCore::test::WorldQueryFixture{level, {}},
        player_bounds,
        player_eye_position,
        player,
        turrets,
        {},
        0.01F
    );
    CHECK(waiting.turret_windups == 0);
    CHECK(waiting.turret_shots == 0);
    CHECK_FALSE(first_tick.player_died);
    CHECK(turrets[0].warming_up);
    CHECK(player.health == doctest::Approx(20.0F));
    CHECK(player.last_damage_source == "steam_trench");

    RelayCore::gameplay::tick_combat_state(0.10F, player, turrets);

    const auto second_tick = RelayCore::gameplay::update_combat_threats(
        RelayCore::test::WorldQueryFixture{level, {}},
        player_bounds,
        player_eye_position,
        player,
        turrets,
        {},
        0.10F
    );
    CHECK(second_tick.turret_shots == 1);
    CHECK(second_tick.turret_windups == 0);
    CHECK(turrets[0].shot_trace.remaining_seconds > 0);
    CHECK(turrets[0].shot_trace.remaining_seconds <= 0.08F);
    CHECK(turrets[0].shot_trace.start.z > 0.0F);
    CHECK(turrets[0].shot_trace.end.z == doctest::Approx(player_eye_position.z));
    CHECK(second_tick.player_died);
    CHECK_FALSE(player.alive);
    CHECK(player.health == doctest::Approx(0.0F));

    RelayCore::gameplay::tick_combat_state(0.60F, player, turrets);
    CHECK(turrets[0].shot_trace.remaining_seconds == 0.0F);
    CHECK(player.respawn_remaining_seconds == doctest::Approx(0.90F));
}

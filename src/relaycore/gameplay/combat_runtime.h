#pragma once

#include "relaycore/physics/world_query.h"

#include <cstdint>
#include <glm/vec3.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace RelayCore
{
    struct BlockoutLevel;
}

namespace RelayCore::gameplay
{
    enum class CombatTargetActionKind : std::uint8_t {
        None = 0,
        ActivateTarget,
        SignalTarget,
    };

    struct CombatTargetEvent {
        CombatTargetActionKind action = CombatTargetActionKind::None;
        std::string target{};
    };

    struct CombatShotTrace {
        glm::vec3 start{};
        glm::vec3 end{};
        float remaining_seconds = 0.0F;
    };

    struct RuntimeTurret {
        int authored_entity_index = -1;
        std::string name{};
        std::string label{};
        std::string destroy_target{};
        physics::Aabb bounds{};
        glm::vec3 origin{};
        float yaw = 0.0F;
        float max_health = 45.0F;
        float health = 45.0F;
        float attack_range = 18.0F;
        float fire_interval_seconds = 0.85F;
        float warmup_seconds = 0.20F;
        float cooldown_remaining_seconds = 0.0F;
        float warmup_remaining_seconds = 0.0F;
        float hit_flash_remaining_seconds = 0.0F;
        float fire_flash_remaining_seconds = 0.0F;
        CombatShotTrace shot_trace{};
        float damage = 16.0F;
        bool enabled = true;
        bool destroyed = false;
        bool warming_up = false;
        CombatTargetActionKind destroy_action = CombatTargetActionKind::SignalTarget;
    };

    struct RuntimeHazardVolume {
        int authored_solid_index = -1;
        std::string label{};
        physics::Aabb bounds{};
        float damage_per_second = 18.0F;
    };

    struct PlayerCombatState {
        float max_health = 100.0F;
        float health = 100.0F;
        float weapon_refire_seconds = 0.24F;
        float weapon_cooldown_remaining_seconds = 0.0F;
        float weapon_range = 24.0F;
        float weapon_damage = 25.0F;
        float hurt_flash_remaining_seconds = 0.0F;
        float hit_confirmation_remaining_seconds = 0.0F;
        CombatShotTrace shot_trace{};
        float respawn_delay_seconds = 1.20F;
        float respawn_remaining_seconds = 0.0F;
        bool alive = true;
        std::uint32_t shots_fired = 0;
        std::uint32_t shots_hit = 0;
        std::uint32_t kills = 0;
        std::uint32_t deaths = 0;
        float damage_dealt = 0.0F;
        float damage_taken = 0.0F;
        std::string last_damage_source{};
    };

    struct CombatBuildResult {
        std::vector<RuntimeTurret> turrets{};
        std::vector<RuntimeHazardVolume> hazards{};
    };

    struct CombatFireResult {
        bool fired = false;
        bool hit_turret = false;
        bool killed_turret = false;
        int turret_index = -1;
        float hit_distance = 0.0F;
        CombatTargetEvent target_event{};
    };

    struct CombatThreatResult {
        float damage_taken = 0.0F;
        std::uint32_t hazard_contacts = 0;
        std::uint32_t turret_shots = 0;
        std::uint32_t turret_windups = 0;
        bool player_died = false;
    };

    [[nodiscard]] auto build_runtime_combat(const BlockoutLevel& level) -> CombatBuildResult;
    [[nodiscard]] auto apply_player_damage(PlayerCombatState& player, float amount, std::string_view source) -> float;

    void reset_player_combat_state(PlayerCombatState& state) noexcept;

    void respawn_player_combat_state(PlayerCombatState& state) noexcept;

    void tick_combat_state(float dt, PlayerCombatState& player, std::span<RuntimeTurret> turrets) noexcept;

    [[nodiscard]] auto apply_player_fire(
        const physics::WorldQuery world,
        const glm::vec3& origin,
        const glm::vec3& direction,
        PlayerCombatState& player,
        std::span<RuntimeTurret> turrets,
        float max_hit_distance = 1.0e9F
    ) -> CombatFireResult;

    [[nodiscard]] auto update_combat_threats(
        const physics::WorldQuery world,
        const physics::Aabb& player_bounds,
        const glm::vec3& player_eye_position,
        PlayerCombatState& player,
        std::span<RuntimeTurret> turrets,
        std::span<const RuntimeHazardVolume> hazards,
        float dt
    ) -> CombatThreatResult;
} // namespace RelayCore::gameplay

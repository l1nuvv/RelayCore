#pragma once

#include "relaycore/gameplay/combat_runtime.h"
#include "relaygame/player_weapon.h"

#include <array>

namespace RelayGame
{
    inline constexpr float mobile_shooter_range = 12;

    enum class EnemyMode : std::uint8_t {
        Idle,
        Chasing,
        Windup,
        Recovery,
        Dead,
        Lunging
    };

    struct EnemyState {
        int entity_index = -1;
        glm::vec3 position{};
        glm::vec3 last_seen{};
        float yaw = 0;
        float health = 60;
        float timer = 0;
        float vertical_speed = 0;
        EnemyMode mode = EnemyMode::Idle;
    };

    struct RuntimeEnemy {
        EnemyState state;
        float detection_range = 10;
        float move_speed = 1.6F;
        float max_health = 60;
        bool lunge_attack = false;
        bool ranged_attack = false;
        std::vector<glm::vec3> route_points;
        std::array<glm::vec3, 18> route{};
        std::size_t route_count = 0;
        std::size_t route_cursor = 0;
        glm::vec3 route_goal{};
        float route_replan_seconds = 0;
        bool seeking_last_seen = false;
        RelayCore::gameplay::CombatShotTrace shot_trace;
    };

    [[nodiscard]] auto build_enemies(const RelayCore::BlockoutLevel& level) -> std::vector<RuntimeEnemy>;
    [[nodiscard]] auto enemy_bounds(const EnemyState& state) -> RelayCore::physics::Aabb;
    struct EnemyThreatResult {
        std::uint32_t windups = 0;
        std::uint32_t strikes = 0;
        std::uint32_t shots = 0;
    };

    auto update_enemies(
        const RelayCore::physics::WorldQuery world,
        const RelayCore::physics::Aabb& player_bounds,
        const glm::vec3& player_eye,
        RelayCore::gameplay::PlayerCombatState& player,
        std::span<RuntimeEnemy> enemies,
        float dt,
        std::vector<RelayCore::physics::WorldCollider>& blockers,
        std::span<const RelayCore::physics::WorldCollider> additional_blockers = {}
    ) -> EnemyThreatResult;

    struct PlayerAttackResult {
        RelayCore::gameplay::CombatFireResult combat;
        int enemy_index = -1;
        bool killed_enemy = false;
        std::array<RelayCore::gameplay::CombatFireResult, 7> rays{};
        std::array<RelayCore::gameplay::CombatShotTrace, 7> pellet_traces{};
        std::size_t ray_count = 0;
    };
    [[nodiscard]] auto fire_player_weapon(
        const RelayCore::physics::WorldQuery world,
        const glm::vec3& origin,
        const glm::vec3& direction,
        RelayCore::gameplay::PlayerCombatState& player,
        std::span<RelayCore::gameplay::RuntimeTurret> turrets,
        std::span<RuntimeEnemy> enemies,
        PlayerWeapon weapon = PlayerWeapon::Precise
    ) -> PlayerAttackResult;
} // namespace RelayGame

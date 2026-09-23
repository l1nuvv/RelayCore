#pragma once

#include "relaycore/gameplay/combat_runtime.h"

namespace RelayGame
{
    enum class EnemyMode : std::uint8_t {
        Idle,
        Chasing,
        Windup,
        Recovery,
        Dead
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
    };

    [[nodiscard]] auto build_enemies(const RelayCore::BlockoutLevel& level) -> std::vector<RuntimeEnemy>;
    [[nodiscard]] auto enemy_bounds(const EnemyState& state) -> RelayCore::physics::Aabb;
    void update_enemies(
        const RelayCore::BlockoutLevel& level,
        std::span<const RelayCore::gameplay::RuntimeDoor> doors,
        const RelayCore::physics::Aabb& player_bounds,
        const glm::vec3& player_eye,
        RelayCore::gameplay::PlayerCombatState& player,
        std::span<RuntimeEnemy> enemies,
        float dt,
        std::vector<RelayCore::physics::Aabb>& blockers
    );

    struct PlayerAttackResult {
        RelayCore::gameplay::CombatFireResult combat;
        int enemy_index = -1;
        bool killed_enemy = false;
    };
    [[nodiscard]] auto fire_player_weapon(
        const RelayCore::BlockoutLevel& level,
        std::span<const RelayCore::gameplay::RuntimeDoor> doors,
        const glm::vec3& origin,
        const glm::vec3& direction,
        RelayCore::gameplay::PlayerCombatState& player,
        std::span<RelayCore::gameplay::RuntimeTurret> turrets,
        std::span<RuntimeEnemy> enemies
    ) -> PlayerAttackResult;
} // namespace RelayGame

#include "relaygame/enemy_runtime.h"

#include "relaycore/world/blockout_level.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <stdexcept>

namespace RelayGame
{
    namespace
    {
        auto blocked(
            const RelayCore::BlockoutLevel& level,
            std::span<const RelayCore::gameplay::RuntimeDoor> doors,
            glm::vec3 origin,
            glm::vec3 target
        ) -> bool
        {
            const auto delta = target - origin;
            const auto distance = glm::length(delta);
            if (distance < .001F)
                return false;
            const auto hit = [&](const RelayCore::physics::Aabb& box) {
                return RelayCore::physics::ray_intersection_distance(origin, delta / distance, box, distance)
                    .has_value();
            };
            for (const auto* solids : {&level.solids, &level.prop_colliders})
                for (const auto& solid : *solids)
                    if (hit({solid.mins, solid.maxs}))
                        return true;
            for (const auto& door : doors)
                if (RelayCore::gameplay::is_door_blocking(door) && hit(RelayCore::gameplay::current_door_bounds(door)))
                    return true;
            return false;
        }
    } // namespace

    auto enemy_bounds(const EnemyState& state) -> RelayCore::physics::Aabb
    {
        return RelayCore::physics::make_player_aabb(state.position, .42F, .95F);
    }

    auto build_enemies(const RelayCore::BlockoutLevel& level) -> std::vector<RuntimeEnemy>
    {
        std::vector<RuntimeEnemy> result;
        for (std::size_t i = 0; i < level.entities.size(); ++i)
        {
            const auto& entity = level.entities[i];
            if (entity.classname != "enemy_rover")
                continue;
            RuntimeEnemy enemy;
            enemy.state.entity_index = static_cast<int>(i);
            enemy.state.position = entity.origin;
            enemy.state.last_seen = entity.origin;
            enemy.state.yaw = entity.angles.x;
            const auto number = [&](const char* key, float fallback) {
                const auto found = entity.properties.find(key);
                if (found == entity.properties.end())
                    return fallback;
                float value = 0;
                const auto& text = found->second;
                const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
                if (error != std::errc{} || end != text.data() + text.size() || !std::isfinite(value) || value <= 0
                    || value > 1000)
                    throw std::runtime_error("enemy_rover " + std::to_string(i) + ": invalid " + key);
                return value;
            };
            enemy.max_health = number("health", enemy.max_health);
            enemy.state.health = enemy.max_health;
            enemy.move_speed = number("speed", enemy.move_speed);
            enemy.detection_range = number("detection_range", enemy.detection_range);
            result.push_back(enemy);
        }
        return result;
    }

    void update_enemies(
        const RelayCore::BlockoutLevel& level,
        std::span<const RelayCore::gameplay::RuntimeDoor> doors,
        const RelayCore::physics::Aabb& player_bounds,
        const glm::vec3& player_eye,
        RelayCore::gameplay::PlayerCombatState& player,
        std::span<RuntimeEnemy> enemies,
        float dt,
        std::vector<RelayCore::physics::Aabb>& blockers
    )
    {
        if (!player.alive || dt <= 0 || enemies.empty())
            return;
        blockers.clear();
        for (const auto* solids : {&level.solids, &level.prop_colliders})
            for (const auto& solid : *solids)
                blockers.push_back({solid.mins, solid.maxs});
        for (const auto& door : doors)
            if (RelayCore::gameplay::is_door_blocking(door))
                blockers.push_back(RelayCore::gameplay::current_door_bounds(door));
        blockers.push_back(player_bounds);
        const auto static_count = blockers.size();
        for (auto& enemy : enemies)
        {
            auto& state = enemy.state;
            if (state.mode == EnemyMode::Dead)
                continue;
            blockers.resize(static_count);
            for (const auto& other : enemies)
                if (&other != &enemy && other.state.mode != EnemyMode::Dead)
                    blockers.push_back(enemy_bounds(other.state));
            const auto eye = state.position + glm::vec3{0, .75F, 0};
            const bool sees =
                glm::distance(eye, player_eye) <= enemy.detection_range && !blocked(level, doors, eye, player_eye);
            if (sees)
                state.last_seen = {player_eye.x, player_bounds.mins.y, player_eye.z};
            state.timer = std::max(0.0F, state.timer - dt);
            const auto to_player = glm::vec2{player_eye.x - state.position.x, player_eye.z - state.position.z};
            const bool in_range =
                glm::length(to_player) < 1.15F && std::abs(player_bounds.mins.y - state.position.y) < .7F;
            glm::vec3 velocity{};
            if (state.mode == EnemyMode::Windup && state.timer == 0)
            {
                if (sees && in_range)
                    static_cast<void>(RelayCore::gameplay::apply_player_damage(player, 14, "Охранный робот"));
                state.mode = EnemyMode::Recovery;
                state.timer = .95F;
            } else if (state.mode != EnemyMode::Windup && state.mode != EnemyMode::Recovery)
            {
                if (sees)
                    state.mode = EnemyMode::Chasing;
                if (sees && in_range)
                {
                    state.mode = EnemyMode::Windup;
                    state.timer = .45F;
                } else if (state.mode == EnemyMode::Chasing)
                {
                    auto delta = state.last_seen - state.position;
                    delta.y = 0;
                    const auto distance = glm::length(delta);
                    if (distance > .12F)
                    {
                        velocity = delta / distance * std::min(enemy.move_speed, distance / dt);
                        state.yaw = glm::degrees(std::atan2(delta.x, delta.z));
                    } else
                        state.mode = EnemyMode::Idle;
                }
            } else if (state.mode == EnemyMode::Recovery && state.timer == 0)
                state.mode = EnemyMode::Chasing;
            state.vertical_speed = std::max(-20.0F, state.vertical_speed - 18.0F * dt);
            velocity.y = state.vertical_speed;
            const RelayCore::physics::CollisionConfig collision{.radius = .42F, .height = .95F, .step_height = .15F};
            const auto moved =
                RelayCore::physics::move_and_collide(blockers, state.position, velocity, dt, collision, true);
            state.position = moved.position;
            if (moved.grounded || moved.hit_y)
                state.vertical_speed = 0;
        }
    }

    auto fire_player_weapon(
        const RelayCore::BlockoutLevel& level,
        std::span<const RelayCore::gameplay::RuntimeDoor> doors,
        const glm::vec3& origin,
        const glm::vec3& direction,
        RelayCore::gameplay::PlayerCombatState& player,
        std::span<RelayCore::gameplay::RuntimeTurret> turrets,
        std::span<RuntimeEnemy> enemies
    ) -> PlayerAttackResult
    {
        PlayerAttackResult result;
        float distance = player.weapon_range;
        const float length = glm::length(direction);
        if (length < .001F)
            return result;
        const auto ray = direction / length;
        int index = -1;
        for (std::size_t i = 0; i < enemies.size(); ++i)
        {
            if (enemies[i].state.mode == EnemyMode::Dead)
                continue;
            const auto hit =
                RelayCore::physics::ray_intersection_distance(origin, ray, enemy_bounds(enemies[i].state), distance);
            if (hit && !blocked(level, doors, origin, origin + ray * *hit))
            {
                distance = *hit;
                index = static_cast<int>(i);
            }
        }
        result.combat = RelayCore::gameplay::apply_player_fire(level, doors, origin, ray, player, turrets, distance);
        if (index >= 0 && result.combat.fired && !result.combat.hit_turret)
        {
            result.enemy_index = index;
            auto& state = enemies[static_cast<std::size_t>(index)].state;
            player.damage_dealt += std::min(state.health, player.weapon_damage);
            state.health = std::max(0.0F, state.health - player.weapon_damage);
            ++player.shots_hit;
            state.mode = state.health == 0 ? EnemyMode::Dead : EnemyMode::Chasing;
            state.last_seen = origin;
            state.timer = 0;
            result.killed_enemy = state.health == 0;
            if (result.killed_enemy)
                ++player.kills;
        }
        return result;
    }
} // namespace RelayGame

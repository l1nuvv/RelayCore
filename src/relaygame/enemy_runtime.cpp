#include "relaygame/enemy_runtime.h"

#include "relaycore/world/blockout_level.h"
#include "relaygame/enemy_navigation.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <limits>
#include <stdexcept>
#include <utility>

namespace RelayGame
{
    namespace
    {
        auto blocked(const RelayCore::physics::WorldQuery world, glm::vec3 origin, glm::vec3 target) -> bool
        {
            const auto delta = target - origin;
            const auto distance = glm::length(delta);
            if (distance < .001F)
                return false;
            return world.raycast(origin, delta / distance, distance).has_value();
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
            if (entity.classname != "enemy_rover" && entity.classname != "enemy_shooter")
                continue;
            RuntimeEnemy enemy;
            enemy.ranged_attack = entity.classname == "enemy_shooter";
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
                    throw std::runtime_error(entity.classname + " " + std::to_string(i) + ": invalid " + key);
                return value;
            };
            enemy.max_health = number("health", enemy.max_health);
            enemy.state.health = enemy.max_health;
            enemy.move_speed = number("speed", enemy.move_speed);
            enemy.detection_range = number("detection_range", enemy.detection_range);
            if (const auto style = entity.properties.find("attack_style"); style != entity.properties.end())
            {
                if (enemy.ranged_attack)
                    throw std::runtime_error("enemy_shooter " + std::to_string(i) + ": attack_style is not supported");
                if (style->second != "melee" && style->second != "lunge")
                    throw std::runtime_error(
                        "enemy_rover " + std::to_string(i) + ": attack_style must be melee or lunge"
                    );
                enemy.lunge_attack = style->second == "lunge";
            }
            if (const auto route = entity.properties.find("route"); route != entity.properties.end())
            {
                if (route->second.empty())
                    throw std::runtime_error(entity.classname + " " + std::to_string(i) + ": empty route");
                for (const auto& point : level.entities)
                    if (point.classname == "enemy_route_point" && point.properties.contains("route")
                        && point.properties.at("route") == route->second)
                    {
                        if (std::abs(point.origin.y - entity.origin.y) > .08F)
                            throw std::runtime_error(
                                entity.classname + " " + std::to_string(i) + ": route must be flat"
                            );
                        enemy.route_points.push_back(point.origin);
                    }
                if (enemy.route_points.empty() || enemy.route_points.size() > 16)
                    throw std::runtime_error(
                        entity.classname + " " + std::to_string(i) + ": route needs 1 to 16 points"
                    );
            }
            if (enemy.ranged_attack && enemy.route_points.empty())
                throw std::runtime_error("enemy_shooter " + std::to_string(i) + ": route is required");
            result.push_back(enemy);
        }
        return result;
    }

    auto update_enemies(
        const RelayCore::physics::WorldQuery world,
        const RelayCore::physics::Aabb& player_bounds,
        const glm::vec3& player_eye,
        RelayCore::gameplay::PlayerCombatState& player,
        std::span<RuntimeEnemy> enemies,
        float dt,
        std::vector<RelayCore::physics::WorldCollider>& blockers,
        std::span<const RelayCore::physics::WorldCollider> additional_blockers
    ) -> EnemyThreatResult
    {
        EnemyThreatResult result{};
        if (!player.alive || dt <= 0 || enemies.empty())
            return result;
        blockers.clear();
        blockers.push_back({.bounds = player_bounds, .object = {RelayCore::physics::WorldObjectKind::Player}});
        blockers.insert(blockers.end(), additional_blockers.begin(), additional_blockers.end());
        const auto static_count = blockers.size();
        for (auto& enemy : enemies)
        {
            if (!player.alive)
                break;
            auto& state = enemy.state;
            enemy.shot_trace.remaining_seconds = std::max(0.0F, enemy.shot_trace.remaining_seconds - dt);
            enemy.hit_feedback.remaining_seconds = std::max(0.0F, enemy.hit_feedback.remaining_seconds - dt);
            if (enemy.hit_feedback.remaining_seconds == 0 || state.mode == EnemyMode::Dead)
                enemy.hit_feedback = {};
            if (state.mode == EnemyMode::Dead)
                continue;
            blockers.resize(static_count);
            for (const auto& other : enemies)
                if (&other != &enemy && other.state.mode != EnemyMode::Dead)
                    blockers.push_back(
                        {.bounds = enemy_bounds(other.state),
                         .object =
                             {RelayCore::physics::WorldObjectKind::Enemy,
                              static_cast<std::size_t>(&other - enemies.data()),
                              other.state.entity_index}}
                    );
            const auto eye = state.position + glm::vec3{0, .75F, 0};
            const bool sees =
                glm::distance(eye, player_eye) <= enemy.detection_range && !blocked(world, eye, player_eye);
            const bool committed = (enemy.lunge_attack || enemy.ranged_attack)
                                   && (state.mode == EnemyMode::Windup || state.mode == EnemyMode::Lunging);
            if (sees && !committed)
            {
                state.last_seen = {player_eye.x, player_bounds.mins.y, player_eye.z};
                enemy.seeking_last_seen = false;
            }
            state.timer = std::max(0.0F, state.timer - dt);
            const auto to_player = glm::vec2{player_eye.x - state.position.x, player_eye.z - state.position.z};
            const bool in_range =
                glm::length(to_player) < 1.15F && std::abs(player_bounds.mins.y - state.position.y) < .7F;
            glm::vec3 velocity{};
            if (state.mode == EnemyMode::Windup && state.timer == 0)
            {
                if (enemy.ranged_attack)
                {
                    const auto target = state.last_seen + glm::vec3{0, 1.0F, 0};
                    const auto delta = target - eye;
                    const auto range = glm::length(delta);
                    if (range > .001F)
                    {
                        const auto ray = delta / range;
                        float distance = mobile_shooter_range;
                        if (const auto hit = world.raycast(eye, ray, distance, RelayCore::physics::WorldQueryMask::All))
                            distance = hit->distance;
                        for (const auto& blocker : blockers)
                            if (const auto hit =
                                    RelayCore::physics::ray_intersection_distance(eye, ray, blocker.bounds, distance))
                            {
                                distance = *hit;
                            }
                        const auto player_hit =
                            RelayCore::physics::ray_intersection_distance(eye, ray, player_bounds, distance);
                        if (player_hit && *player_hit <= distance)
                            static_cast<void>(RelayCore::gameplay::apply_player_damage(
                                player,
                                12,
                                "Мобильный стрелок",
                                enemy.state.position
                            ));
                        enemy.shot_trace = {.start = eye, .end = eye + ray * distance, .remaining_seconds = .08F};
                        ++result.shots;
                    }
                    state.mode = EnemyMode::Recovery;
                    state.timer = .95F;
                } else if (enemy.lunge_attack)
                {
                    state.mode = EnemyMode::Lunging;
                    state.timer = .22F;
                } else
                {
                    ++result.strikes;
                    if (sees && in_range)
                        static_cast<void>(
                            RelayCore::gameplay::apply_player_damage(player, 14, "Охранный робот", enemy.state.position)
                        );
                    state.mode = EnemyMode::Recovery;
                    state.timer = .95F;
                }
            } else if (state.mode != EnemyMode::Windup && state.mode != EnemyMode::Recovery
                       && state.mode != EnemyMode::Lunging)
            {
                if (sees)
                    state.mode = EnemyMode::Chasing;
                const bool can_attack =
                    enemy.ranged_attack ? glm::distance(eye, player_eye) <= mobile_shooter_range
                    : enemy.lunge_attack
                        ? glm::length(to_player) < 2.4F && std::abs(player_bounds.mins.y - state.position.y) < .7F
                        : in_range;
                if (sees && can_attack)
                {
                    state.mode = EnemyMode::Windup;
                    state.timer = enemy.ranged_attack ? .7F : .45F;
                    if (enemy.lunge_attack || enemy.ranged_attack)
                        state.yaw = glm::degrees(std::atan2(to_player.x, to_player.y));
                    ++result.windups;
                } else if (state.mode == EnemyMode::Chasing)
                {
                    auto delta = state.last_seen - state.position;
                    delta.y = 0;
                    const auto distance = glm::length(delta);
                    if (distance > .12F)
                    {
                        velocity = enemy.route_points.empty()
                                       ? delta / distance * std::min(enemy.move_speed, distance / dt)
                                       : enemy_route_velocity(
                                             world,
                                             enemy,
                                             state.last_seen,
                                             player_eye.y - player_bounds.mins.y,
                                             dt
                                         );
                        if (glm::length(velocity) > .001F)
                            state.yaw = glm::degrees(std::atan2(velocity.x, velocity.z));
                    } else
                        state.mode = EnemyMode::Idle;
                }
            } else if (state.mode == EnemyMode::Recovery && state.timer == 0)
                state.mode = EnemyMode::Chasing;
            if (state.mode == EnemyMode::Lunging)
            {
                if (state.timer == 0)
                {
                    ++result.strikes;
                    if (sees && in_range)
                        static_cast<void>(
                            RelayCore::gameplay::apply_player_damage(player, 14, "Охранный робот", enemy.state.position)
                        );
                    state.mode = EnemyMode::Recovery;
                    state.timer = .95F;
                } else
                {
                    auto delta = state.last_seen - state.position;
                    delta.y = 0;
                    const auto distance = glm::length(delta);
                    if (distance > .12F)
                        velocity = delta / distance * std::min(enemy.move_speed * 3, distance / dt);
                }
            }
            state.vertical_speed = std::max(-20.0F, state.vertical_speed - 18.0F * dt);
            velocity.y = state.vertical_speed;
            const RelayCore::physics::CollisionConfig collision{.radius = .42F, .height = .95F, .step_height = .15F};
            const auto moved = RelayCore::physics::move_and_collide(
                world.with_actors(blockers),
                state.position,
                velocity,
                dt,
                collision,
                true
            );
            state.position = moved.position;
            if (moved.grounded || moved.hit_y)
                state.vertical_speed = 0;
        }
        return result;
    }

    static auto fire_player_ray(
        const RelayCore::physics::WorldQuery world,
        const glm::vec3& origin,
        const glm::vec3& direction,
        RelayCore::gameplay::PlayerCombatState& player,
        std::span<RelayCore::gameplay::RuntimeTurret> turrets,
        std::span<RuntimeEnemy> enemies,
        float max_distance = std::numeric_limits<float>::max()
    ) -> PlayerAttackResult
    {
        PlayerAttackResult result;
        float distance = std::min(player.weapon_range, max_distance);
        const float length = glm::length(direction);
        if (length < .001F)
            return result;
        const auto ray = direction / length;
        if (const auto blocker = world.raycast(origin, ray, distance, RelayCore::physics::WorldQueryMask::All))
            distance = blocker->distance;
        int index = -1;
        glm::vec3 hit_normal{};
        for (std::size_t i = 0; i < enemies.size(); ++i)
        {
            if (enemies[i].state.mode == EnemyMode::Dead)
                continue;
            const auto hit =
                RelayCore::physics::ray_intersection(origin, ray, enemy_bounds(enemies[i].state), distance);
            if (hit && !blocked(world, origin, origin + ray * hit->distance))
            {
                distance = hit->distance;
                hit_normal = hit->normal;
                index = static_cast<int>(i);
            }
        }
        result.combat = RelayCore::gameplay::apply_player_fire(world, origin, ray, player, turrets, distance);
        if (index >= 0 && result.combat.fired && !result.combat.hit_turret)
        {
            result.combat.world_impact.reset();
            player.shot_trace.impact = RelayCore::physics::WorldQueryHit{
                .distance = distance,
                .position = player.shot_trace.end,
                .normal = hit_normal,
                .surface = RelayCore::SurfaceMaterial::Metal,
            };
            result.enemy_index = index;
            auto& enemy = enemies[static_cast<std::size_t>(index)];
            auto& state = enemy.state;
            const float damage = std::min(state.health, player.weapon_damage);
            result.combat.hit_distance = distance;
            result.combat.actor_contact = RelayCore::gameplay::CombatActorContact{
                .kind = RelayCore::gameplay::CombatActorKind::Enemy,
                .index = index,
                .position = player.shot_trace.end,
                .normal = hit_normal,
                .direction = ray,
                .damage = damage,
            };
            player.damage_dealt += damage;
            state.health = std::max(0.0F, state.health - player.weapon_damage);
            ++player.shots_hit;
            player.hit_confirmation_remaining_seconds = 0.16F;
            if ((!enemy.lunge_attack && !enemy.ranged_attack)
                || (state.mode != EnemyMode::Windup && state.mode != EnemyMode::Lunging))
            {
                state.last_seen = origin;
                if (!enemy.route_points.empty())
                    state.last_seen.y -= 1.65F;
                enemy.seeking_last_seen = false;
                enemy.route_replan_seconds = 0;
            }
            result.killed_enemy = state.health == 0;
            if (result.killed_enemy)
            {
                state.mode = EnemyMode::Dead;
                state.timer = 0;
                ++player.kills;
            } else if (state.mode != EnemyMode::Windup && state.mode != EnemyMode::Recovery
                       && state.mode != EnemyMode::Lunging)
            {
                state.mode = EnemyMode::Chasing;
                state.timer = 0;
            }
        }
        return result;
    }

    auto fire_player_weapon(
        const RelayCore::physics::WorldQuery world,
        const glm::vec3& origin,
        const glm::vec3& direction,
        RelayCore::gameplay::PlayerCombatState& player,
        std::span<RelayCore::gameplay::RuntimeTurret> turrets,
        std::span<RuntimeEnemy> enemies,
        PlayerWeapon weapon
    ) -> PlayerAttackResult
    {
        const auto apply_feedback = [&](PlayerAttackResult& attack) {
            for (std::size_t i = 0; i < attack.ray_count; ++i)
            {
                const auto& contact = attack.rays[i].actor_contact;
                if (!contact)
                    continue;
                bool already_hit = false;
                for (std::size_t j = 0; j < i; ++j)
                    if (const auto& previous = attack.rays[j].actor_contact;
                        previous && previous->kind == contact->kind && previous->index == contact->index)
                        already_hit = true;
                const bool mobile = contact->kind == RelayCore::gameplay::CombatActorKind::Enemy;
                const bool dead =
                    mobile ? enemies[contact->index].state.mode == EnemyMode::Dead : turrets[contact->index].destroyed;
                // Live targets own their burst; keep one trace burst for a destroyed mobile actor.
                if (!mobile || !dead || already_hit)
                {
                    if (weapon == PlayerWeapon::Shotgun)
                        attack.pellet_traces[i].impact.reset();
                    else
                        player.shot_trace.impact.reset();
                }
                if (already_hit)
                    continue;
                auto& feedback = mobile ? enemies[contact->index].hit_feedback : turrets[contact->index].hit_feedback;
                feedback = dead ? RelayCore::gameplay::CombatHitFeedback{}
                                : RelayCore::gameplay::CombatHitFeedback{
                                      contact->position,
                                      contact->normal,
                                      RelayCore::gameplay::actor_hit_seconds
                                  };
            }
        };
        if (weapon == PlayerWeapon::Precise)
        {
            auto result = fire_player_ray(world, origin, direction, player, turrets, enemies);
            if (result.combat.fired)
            {
                result.rays[0] = result.combat;
                result.ray_count = 1;
            }
            apply_feedback(result);
            return result;
        }
        if (weapon != PlayerWeapon::Shotgun)
            throw std::runtime_error("Unknown player weapon");
        PlayerAttackResult result;
        const auto length = glm::length(direction);
        if (!player.alive || player.weapon_cooldown_remaining_seconds > 0 || !std::isfinite(length) || length < .001F)
            return result;
        const auto forward = direction / length;
        const auto reference = std::abs(forward.y) > .99F ? glm::vec3{1, 0, 0} : glm::vec3{0, 1, 0};
        const auto right = glm::normalize(glm::cross(forward, reference));
        const auto up = glm::cross(right, forward);
        constexpr std::array<glm::vec2, 7> spread{
            {{0, 0}, {.16F, 0}, {-.16F, 0}, {.08F, .13856F}, {-.08F, .13856F}, {.08F, -.13856F}, {-.08F, -.13856F}}
        };
        auto shot = player;
        shot.weapon_damage = 14;
        shot.weapon_range = 14;
        shot.weapon_refire_seconds = .85F;
        std::array<RelayCore::physics::Aabb, 7> killed_bounds{};
        std::size_t killed_count = 0;
        for (std::size_t i = 0; i < spread.size(); ++i)
        {
            shot.weapon_cooldown_remaining_seconds = 0;
            const auto pellet_direction = glm::normalize(forward + right * spread[i].x + up * spread[i].y);
            float distance = shot.weapon_range;
            for (std::size_t killed = 0; killed < killed_count; ++killed)
                if (const auto hit = RelayCore::physics::ray_intersection_distance(
                        origin,
                        pellet_direction,
                        killed_bounds[killed],
                        distance
                    ))
                    distance = *hit;
            auto ray = fire_player_ray(world, origin, pellet_direction, shot, turrets, enemies, distance);
            // Every pellet belongs to the same discharge: an early kill cannot expose a target behind it.
            if (ray.killed_enemy)
                killed_bounds[killed_count++] = enemy_bounds(enemies[static_cast<std::size_t>(ray.enemy_index)].state);
            else if (ray.combat.killed_turret)
                killed_bounds[killed_count++] = turrets[static_cast<std::size_t>(ray.combat.turret_index)].bounds;
            result.rays[i] = ray.combat;
            result.pellet_traces[i] = shot.shot_trace;
            result.pellet_traces[i].remaining_seconds = .06F;
            ++result.ray_count;
            if (ray.enemy_index >= 0 && result.enemy_index < 0)
                result.enemy_index = ray.enemy_index;
            result.killed_enemy = result.killed_enemy || ray.killed_enemy;
            if (ray.combat.hit_turret
                && (!result.combat.hit_turret || (ray.combat.killed_turret && !result.combat.killed_turret)))
                result.combat = ray.combat;
        }
        result.combat.fired = true;
        shot.shot_trace = {};
        shot.shots_fired = player.shots_fired + 1;
        shot.shots_hit = player.shots_hit + (result.enemy_index >= 0 || result.combat.hit_turret ? 1 : 0);
        shot.weapon_damage = player.weapon_damage;
        shot.weapon_range = player.weapon_range;
        shot.weapon_refire_seconds = player.weapon_refire_seconds;
        player = std::move(shot);
        apply_feedback(result);
        return result;
    }
} // namespace RelayGame

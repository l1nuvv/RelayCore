#include "relaycore/gameplay/combat_runtime.h"

#include "relaycore/gameplay/entity_spawn_parsing.h"
#include "relaycore/gameplay/interaction_descriptors.h"
#include "relaycore/world/blockout_level.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <stdexcept>
#include <string_view>

namespace RelayCore::gameplay
{
    namespace
    {
        using detail::find_property;
        using detail::lowercase_copy;
        using detail::parse_bool_property;
        using detail::parse_centered_bounds;
        using detail::parse_float_property;
        using detail::property_or;

        constexpr float kMinHazardDamageSpan = 0.50F;

        [[nodiscard]] auto is_turret_entity(const EntitySpawnData& entity) -> bool
        {
            if (entity.classname != "gameplay_entity")
            {
                return false;
            }

            const std::string* kind = find_property(entity, "kind");
            if (kind == nullptr || kind->empty())
            {
                return false;
            }

            const std::string normalized = lowercase_copy(*kind);
            return normalized == "turret" || normalized == "sentry";
        }

        [[nodiscard]] auto parse_target_action(const EntitySpawnData& entity) -> CombatTargetActionKind
        {
            const std::string* value = find_property(entity, "destroy_action");
            if (value == nullptr || value->empty())
            {
                value = find_property(entity, "on_destroy");
            }
            if (value == nullptr || value->empty())
            {
                return CombatTargetActionKind::SignalTarget;
            }

            const std::string normalized = lowercase_copy(*value);
            if (normalized == "activate" || normalized == "activate_target" || normalized == "activate-target")
            {
                return CombatTargetActionKind::ActivateTarget;
            }
            if (normalized == "signal" || normalized == "signal_target" || normalized == "signal-target")
            {
                return CombatTargetActionKind::SignalTarget;
            }
            return CombatTargetActionKind::None;
        }

        [[nodiscard]] auto turret_eye_position(const RuntimeTurret& turret) -> glm::vec3
        {
            return glm::vec3{
                turret.origin.x,
                turret.bounds.maxs.y - 0.12F,
                turret.origin.z,
            };
        }

        [[nodiscard]] auto is_damage_hazard_solid(const BlockoutSolid& solid) -> bool
        {
            const glm::vec3 extents = solid.maxs - solid.mins;
            return extents.x >= kMinHazardDamageSpan && extents.z >= kMinHazardDamageSpan;
        }

    } // namespace

    [[nodiscard]] auto apply_player_damage(PlayerCombatState& player, const float amount, const std::string_view source)
        -> float
    {
        if (!player.alive || amount <= 0.0F)
        {
            return 0.0F;
        }

        const float applied_damage = std::min(amount, player.health);
        player.health = std::max(0.0F, player.health - amount);
        player.damage_taken += applied_damage;
        player.hurt_flash_remaining_seconds = std::max(player.hurt_flash_remaining_seconds, 0.30F);
        player.last_damage_source = std::string{source};
        if (player.health <= 0.0F)
        {
            player.alive = false;
            player.health = 0.0F;
            player.respawn_remaining_seconds = player.respawn_delay_seconds;
            ++player.deaths;
        }

        return applied_damage;
    }

    namespace
    {
        [[nodiscard]] auto apply_turret_damage(RuntimeTurret& turret, const float amount, CombatFireResult& result)
            -> float
        {
            if (!turret.enabled || turret.destroyed || amount <= 0.0F)
            {
                return 0.0F;
            }

            const float applied_damage = std::min(amount, turret.health);
            turret.health = std::max(0.0F, turret.health - amount);
            turret.hit_flash_remaining_seconds = 0.18F;
            if (turret.health <= 0.0F)
            {
                turret.destroyed = true;
                turret.enabled = false;
                turret.cooldown_remaining_seconds = 0.0F;
                turret.warmup_remaining_seconds = 0.0F;
                turret.hit_flash_remaining_seconds = 0.32F;
                result.killed_turret = true;
                result.target_event = CombatTargetEvent{
                    .action = turret.destroy_action,
                    .target = turret.destroy_target,
                };
            }

            return applied_damage;
        }
    } // namespace

    auto build_runtime_combat(const BlockoutLevel& level) -> CombatBuildResult
    {
        CombatBuildResult result{};
        result.hazards.reserve(level.solids.size());

        for (std::size_t solid_index = 0; solid_index < level.solids.size(); ++solid_index)
        {
            const BlockoutSolid& solid = level.solids[solid_index];
            if (solid.material != SurfaceMaterial::Hazard || !is_damage_hazard_solid(solid))
            {
                continue;
            }

            RuntimeHazardVolume hazard{};
            hazard.authored_solid_index = static_cast<int>(solid_index);
            hazard.label = "hazard_surface_" + std::to_string(solid_index);
            hazard.bounds = physics::Aabb{
                .mins = solid.mins,
                .maxs = {solid.maxs.x, solid.maxs.y + 1.10F, solid.maxs.z},
            };
            hazard.damage_per_second = 18.0F;
            result.hazards.push_back(std::move(hazard));
        }

        result.turrets.reserve(level.entities.size());
        for (std::size_t entity_index = 0; entity_index < level.entities.size(); ++entity_index)
        {
            const EntitySpawnData& entity = level.entities[entity_index];
            if (!is_turret_entity(entity))
            {
                continue;
            }

            RuntimeTurret turret{};
            turret.authored_entity_index = static_cast<int>(entity_index);
            turret.bounds = parse_centered_bounds(entity, {0.70F, 0.90F, 0.70F}, {0.30F, 0.45F, 0.30F});
            turret.origin = (turret.bounds.mins + turret.bounds.maxs) * 0.5F;
            turret.yaw = entity.angles.x;
            turret.name = property_or(entity, "targetname", "turret_" + std::to_string(entity_index));
            turret.label = property_or(entity, "label", "Защитная турель");
            turret.max_health = std::max(parse_float_property(entity, "health").value_or(turret.max_health), 1.0F);
            turret.health = turret.max_health;
            turret.attack_range =
                std::max(parse_float_property(entity, "attack_range").value_or(turret.attack_range), 1.0F);
            turret.fire_interval_seconds =
                std::max(parse_float_property(entity, "fire_interval").value_or(turret.fire_interval_seconds), 0.05F);
            turret.warmup_seconds =
                std::max(parse_float_property(entity, "warmup").value_or(turret.warmup_seconds), 0.0F);
            turret.damage = std::max(parse_float_property(entity, "damage").value_or(turret.damage), 1.0F);
            turret.enabled = parse_bool_property(entity, "enabled", true);
            turret.destroy_target = property_or(entity, "destroy_target", {});
            turret.destroy_action = parse_target_action(entity);
            turret.stop_target = property_or(entity, "stop_target", {});
            result.turrets.push_back(std::move(turret));
        }

        if (std::ranges::any_of(result.turrets, [](const auto& turret) {
                return !turret.stop_target.empty();
            }))
        {
            const auto interactions = InteractionDescriptorBuilder{level}.build();
            for (const auto& turret : result.turrets)
            {
                if (turret.stop_target.empty())
                    continue;
                const bool produced = std::ranges::any_of(
                                          interactions.usables,
                                          [&](const auto& usable) {
                                              return usable.action_kind == UsableActionKind::ActivateTarget
                                                     && usable.target == turret.stop_target;
                                          }
                                      )
                                      || std::ranges::any_of(
                                          interactions.triggers,
                                          [&](const auto& trigger) {
                                              return trigger.action_kind == TriggerActionKind::ActivateTarget
                                                     && trigger.target == turret.stop_target;
                                          }
                                      )
                                      || std::ranges::any_of(result.turrets, [&](const auto& source) {
                                             return source.destroy_action == CombatTargetActionKind::ActivateTarget
                                                    && source.destroy_target == turret.stop_target;
                                         });
                if (!produced)
                    throw std::runtime_error(
                        "turret '" + turret.name + "': stop_target '" + turret.stop_target
                        + "' has no activate_target producer"
                    );
            }
        }

        return result;
    }

    void reset_player_combat_state(PlayerCombatState& state) noexcept
    {
        state.health = state.max_health;
        state.weapon_cooldown_remaining_seconds = 0.0F;
        state.hurt_flash_remaining_seconds = 0.0F;
        state.hit_confirmation_remaining_seconds = 0.0F;
        state.shot_trace = {};
        state.respawn_remaining_seconds = 0.0F;
        state.alive = true;
        state.shots_fired = 0;
        state.shots_hit = 0;
        state.kills = 0;
        state.deaths = 0;
        state.damage_dealt = 0.0F;
        state.damage_taken = 0.0F;
        state.last_damage_source.clear();
    }

    void respawn_player_combat_state(PlayerCombatState& state) noexcept
    {
        state.health = state.max_health;
        state.weapon_cooldown_remaining_seconds = 0.0F;
        state.hurt_flash_remaining_seconds = 0.0F;
        state.hit_confirmation_remaining_seconds = 0.0F;
        state.shot_trace = {};
        state.respawn_remaining_seconds = 0.0F;
        state.alive = true;
        state.last_damage_source.clear();
    }

    void tick_combat_state(const float dt, PlayerCombatState& player, const std::span<RuntimeTurret> turrets) noexcept
    {
        const float step = std::max(dt, 0.0F);
        player.weapon_cooldown_remaining_seconds = std::max(0.0F, player.weapon_cooldown_remaining_seconds - step);
        player.hurt_flash_remaining_seconds = std::max(0.0F, player.hurt_flash_remaining_seconds - step);
        player.hit_confirmation_remaining_seconds = std::max(0.0F, player.hit_confirmation_remaining_seconds - step);
        player.shot_trace.remaining_seconds = std::max(0.0F, player.shot_trace.remaining_seconds - step);
        if (!player.alive)
        {
            player.respawn_remaining_seconds = std::max(0.0F, player.respawn_remaining_seconds - step);
        }

        for (RuntimeTurret& turret : turrets)
        {
            turret.cooldown_remaining_seconds = std::max(0.0F, turret.cooldown_remaining_seconds - step);
            turret.warmup_remaining_seconds = std::max(0.0F, turret.warmup_remaining_seconds - step);
            turret.hit_flash_remaining_seconds = std::max(0.0F, turret.hit_flash_remaining_seconds - step);
            turret.fire_flash_remaining_seconds = std::max(0.0F, turret.fire_flash_remaining_seconds - step);
            turret.shot_trace.remaining_seconds = std::max(0.0F, turret.shot_trace.remaining_seconds - step);

            if (turret.destroyed || !turret.enabled)
            {
                turret.warmup_remaining_seconds = 0.0F;
                turret.cooldown_remaining_seconds = 0.0F;
                turret.warming_up = false;
            }
        }
    }

    auto apply_player_fire(
        const physics::WorldQuery world,
        const glm::vec3& origin,
        const glm::vec3& direction,
        PlayerCombatState& player,
        const std::span<RuntimeTurret> turrets,
        const float max_hit_distance
    ) -> CombatFireResult
    {
        CombatFireResult result{};
        if (!player.alive || player.weapon_cooldown_remaining_seconds > 0.0F)
        {
            return result;
        }

        result.fired = true;
        player.weapon_cooldown_remaining_seconds = player.weapon_refire_seconds;
        ++player.shots_fired;

        const glm::vec3 shot_direction = glm::normalize(direction);
        const float range = std::min(player.weapon_range, max_hit_distance);
        const auto blocker = world.raycast(origin, shot_direction, range, physics::WorldQueryMask::All);
        const float blocker_distance = blocker ? blocker->distance : range;

        float best_hit_distance = blocker_distance;
        int best_turret_index = -1;
        for (std::size_t turret_index = 0; turret_index < turrets.size(); ++turret_index)
        {
            const RuntimeTurret& turret = turrets[turret_index];
            if (!turret.enabled || turret.destroyed)
            {
                continue;
            }

            const auto hit =
                physics::ray_intersection_distance(origin, shot_direction, turret.bounds, best_hit_distance);
            if (!hit.has_value())
            {
                continue;
            }

            best_hit_distance = *hit;
            best_turret_index = static_cast<int>(turret_index);
        }

        player.shot_trace = {origin, origin + shot_direction * best_hit_distance, 0.25F};

        if (best_turret_index < 0)
        {
            result.world_impact = blocker;
            player.shot_trace.impact = blocker;
            return result;
        }

        RuntimeTurret& turret = turrets[static_cast<std::size_t>(best_turret_index)];
        player.shot_trace.impact = physics::WorldQueryHit{
            .distance = best_hit_distance,
            .position = player.shot_trace.end,
            .normal = -shot_direction,
            .surface = SurfaceMaterial::Metal,
        };
        result.hit_turret = true;
        result.turret_index = best_turret_index;
        result.hit_distance = best_hit_distance;
        ++player.shots_hit;
        player.hit_confirmation_remaining_seconds = 0.16F;
        player.damage_dealt += apply_turret_damage(turret, player.weapon_damage, result);
        if (result.killed_turret)
        {
            ++player.kills;
        }

        return result;
    }

    auto update_combat_threats(
        const physics::WorldQuery world,
        const physics::Aabb& player_bounds,
        const glm::vec3& player_eye_position,
        PlayerCombatState& player,
        const std::span<RuntimeTurret> turrets,
        const std::span<const RuntimeHazardVolume> hazards,
        const float dt,
        const std::span<const std::string> activated_targets
    ) -> CombatThreatResult
    {
        CombatThreatResult result{};
        for (auto& turret : turrets)
            if (!turret.stop_target.empty()
                && std::ranges::find(activated_targets, turret.stop_target) != activated_targets.end())
            {
                turret.enabled = false;
                turret.warming_up = false;
                turret.warmup_remaining_seconds = 0;
                turret.cooldown_remaining_seconds = 0;
                turret.fire_flash_remaining_seconds = 0;
                turret.shot_trace = {};
            }
        if (!player.alive)
        {
            return result;
        }

        for (const RuntimeHazardVolume& hazard : hazards)
        {
            if (!physics::overlaps(player_bounds, hazard.bounds, 0.001F))
            {
                continue;
            }

            ++result.hazard_contacts;
            result.damage_taken +=
                apply_player_damage(player, hazard.damage_per_second * std::max(dt, 0.0F), hazard.label);
            if (!player.alive)
            {
                result.player_died = true;
                return result;
            }
        }

        for (RuntimeTurret& turret : turrets)
        {
            if (!turret.enabled || turret.destroyed)
            {
                continue;
            }

            const glm::vec3 to_player = player_eye_position - turret_eye_position(turret);
            const float distance = glm::length(to_player);
            if (distance <= 0.001F || distance > turret.attack_range)
            {
                turret.warming_up = false;
                turret.warmup_remaining_seconds = turret.warmup_seconds;
                continue;
            }

            const glm::vec3 aim_direction = to_player / distance;
            const auto blocker = world.raycast(turret_eye_position(turret), aim_direction, distance);
            const float blocker_distance = blocker ? blocker->distance : distance;
            if (blocker_distance + 0.02F < distance)
            {
                turret.warming_up = false;
                turret.warmup_remaining_seconds = turret.warmup_seconds;
                continue;
            }

            if (turret.cooldown_remaining_seconds > 0.0F)
            {
                continue;
            }

            if (turret.warmup_seconds > 0.0F)
            {
                if (!turret.warming_up)
                {
                    turret.warming_up = true;
                    turret.warmup_remaining_seconds = turret.warmup_seconds;
                    ++result.turret_windups;
                    continue;
                }
                if (turret.warmup_remaining_seconds > 0.0F)
                {
                    continue;
                }
            }

            turret.cooldown_remaining_seconds = turret.fire_interval_seconds;
            turret.fire_flash_remaining_seconds = 0.16F;
            const glm::vec3 size = turret.bounds.maxs - turret.bounds.mins;
            const float yaw = turret.yaw * 0.0174532925F;
            const glm::vec3 muzzle =
                turret.origin
                + glm::vec3{std::sin(yaw) * size.z * 0.49F, size.y * 0.01F, std::cos(yaw) * size.z * 0.49F};
            turret.shot_trace = {muzzle, player_eye_position, 0.08F};
            turret.warmup_remaining_seconds = turret.warmup_seconds;
            turret.warming_up = false;
            ++result.turret_shots;
            result.damage_taken +=
                apply_player_damage(player, turret.damage, turret.label.empty() ? turret.name : turret.label);
            if (!player.alive)
            {
                result.player_died = true;
                return result;
            }
        }

        return result;
    }
} // namespace RelayCore::gameplay

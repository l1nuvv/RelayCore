#include "relaygame/containment_encounter.h"

#include "relaycore/gameplay/entity_spawn_parsing.h"
#include "relaycore/gameplay/interaction_runtime_detail.h"
#include "relaycore/world/blockout_level.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <stdexcept>

namespace RelayGame
{
    namespace
    {
        auto has(std::span<const std::string> values, std::string_view name) -> bool
        {
            return std::ranges::find(values, name) != values.end();
        }

        auto visible(RelayCore::physics::WorldQuery world, glm::vec3 from, glm::vec3 to) -> bool
        {
            const auto delta = to - from;
            const auto distance = glm::length(delta);
            return distance < .01F || !world.raycast(from, delta / distance, distance - .01F);
        }
    } // namespace

    auto ContainmentEncounter::bounds() const -> RelayCore::physics::Aabb
    {
        return {state.position + glm::vec3{-1.05F, 0, -1.05F}, state.position + glm::vec3{1.05F, 3.6F, 1.05F}};
    }

    auto ContainmentEncounter::collider() const -> RelayCore::physics::WorldCollider
    {
        return {
            bounds(),
            RelayCore::SurfaceMaterial::Rubber,
            {RelayCore::physics::WorldObjectKind::Enemy, 0, entity_index}
        };
    }

    auto ContainmentEncounter::capture_available(std::span<const std::string> activated) const -> bool
    {
        const auto& p = state.position;
        return state.mode == GuardianMode::Recovery && state.timer > 0 && state.grip_fraction == 1
               && has(activated, "limiter_ready") && p.x > trap.mins.x + 1.05F && p.x < trap.maxs.x - 1.05F
               && p.z > trap.mins.z + 1.05F && p.z < trap.maxs.z - 1.05F;
    }

    void ContainmentEncounter::sync_usables(
        std::span<RelayCore::gameplay::RuntimeUsable> usables,
        std::span<const std::string> activated
    ) const
    {
        for (auto& usable : usables)
            if (usable.name == "containment_fire")
            {
                usable.locked = !capture_available(activated);
                usable.locked_message = status_text(activated);
            }
    }

    void ContainmentEncounter::update(
        RelayCore::physics::WorldQuery world,
        const RelayCore::physics::Aabb& player_bounds,
        const glm::vec3& eye,
        RelayCore::gameplay::PlayerCombatState& player,
        float dt,
        std::vector<RelayCore::gameplay::RuntimeDoor>& doors,
        std::vector<std::string>& activated,
        std::span<const RelayCore::physics::WorldCollider> actors
    )
    {
        if (!player.alive || dt <= 0)
            return;
        if (has(activated, "grip_ready"))
            state.grip_fraction = std::min(1.0F, state.grip_fraction + dt / 3);
        if (has(activated, "containment_engaged") && state.mode != GuardianMode::Contained
            && state.mode != GuardianMode::Capturing)
        {
            state.mode = GuardianMode::Capturing;
            state.timer = 0;
        }
        if (state.mode == GuardianMode::Capturing)
        {
            state.capture_fraction = std::min(1.0F, state.capture_fraction + dt / 2);
            if (state.capture_fraction == 1)
            {
                state.mode = GuardianMode::Contained;
                static_cast<void>(RelayCore::gameplay::activate_target("containment_exit", doors));
                RelayCore::gameplay::detail::append_unique_string(activated, "guardian_contained");
            }
            return;
        }
        if (state.mode == GuardianMode::Contained)
            return;

        state.timer = std::max(0.0F, state.timer - dt);
        const auto chest = state.position + glm::vec3{0, 2, 0};
        const bool sees = visible(world, chest, eye);
        const auto target = (player_bounds.mins + player_bounds.maxs) * .5F;
        auto delta = target - state.position;
        delta.y = 0;
        const auto distance = glm::length(delta);
        if (state.mode == GuardianMode::Windup)
        {
            if (state.timer == 0)
            {
                auto strike_delta = target - state.strike_target;
                strike_delta.y = 0;
                if (sees && distance < 3.6F && glm::length(strike_delta) < 1.5F
                    && player_bounds.mins.y < state.position.y + 2)
                    static_cast<void>(RelayCore::gameplay::apply_player_damage(player, 32, "Хранитель"));
                state.mode = GuardianMode::Recovery;
                state.timer = 4;
            }
            return;
        }
        if (state.mode == GuardianMode::Recovery || state.mode == GuardianMode::Suppressed)
        {
            if (state.timer > 0)
                return;
            state.mode = GuardianMode::Watching;
        }
        const bool in_hall =
            target.x > -8 && target.x < 8 && target.z > 23 && target.z < 38 && player_bounds.mins.y < 2;
        if (!sees || !in_hall)
        {
            state.mode = GuardianMode::Watching;
            return;
        }
        if (distance < 2.7F)
        {
            state.mode = GuardianMode::Windup;
            state.strike_target = target;
            state.timer = .9F;
            return;
        }
        state.mode = GuardianMode::Pursuing;
        const auto velocity = delta / distance * std::min(2.1F, (distance - 2.4F) / dt);
        const auto blocker = RelayCore::physics::WorldCollider{player_bounds};
        const RelayCore::physics::CollisionConfig collision{.radius = 1.05F, .height = 3.6F, .step_height = .3F};
        const auto moved = RelayCore::physics::move_and_collide(
            world.with_actors(
                actors.empty() ? std::span<const RelayCore::physics::WorldCollider>{&blocker, 1} : actors
            ),
            state.position,
            velocity + glm::vec3{0, -3, 0},
            dt,
            collision,
            true
        );
        state.position = moved.position;
    }

    void ContainmentEncounter::suppress()
    {
        if (state.mode == GuardianMode::Watching || state.mode == GuardianMode::Pursuing)
        {
            state.mode = GuardianMode::Suppressed;
            state.timer = .65F;
        }
    }

    void ContainmentEncounter::reset_threat_after_death()
    {
        if (state.mode == GuardianMode::Capturing || state.mode == GuardianMode::Contained)
            return;
        state.position = home;
        state.strike_target = {};
        state.timer = 0;
        state.mode = GuardianMode::Watching;
    }

    auto
    ContainmentEncounter::validate_state(const ContainmentState& saved, std::span<const std::string> activated) const
        -> bool
    {
        const auto finite = [](glm::vec3 v) {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        };
        const bool timed = saved.mode == GuardianMode::Windup || saved.mode == GuardianMode::Recovery
                           || saved.mode == GuardianMode::Suppressed;
        const bool engaged = saved.mode == GuardianMode::Capturing || saved.mode == GuardianMode::Contained;
        const bool in_trap = saved.position.x > trap.mins.x + 1.05F && saved.position.x < trap.maxs.x - 1.05F
                             && saved.position.z > trap.mins.z + 1.05F && saved.position.z < trap.maxs.z - 1.05F;
        return finite(saved.position) && finite(saved.strike_target) && saved.position.x >= -9 && saved.position.x <= 9
               && saved.position.z >= 16 && saved.position.z <= 39 && saved.position.y >= -.01F
               && saved.position.y <= .31F
               && static_cast<unsigned>(saved.mode) <= static_cast<unsigned>(GuardianMode::Contained)
               && std::isfinite(saved.timer) && saved.timer >= 0 && saved.timer <= 4 && (timed || saved.timer == 0)
               && (saved.mode != GuardianMode::Windup || saved.timer <= .9F)
               && (saved.mode != GuardianMode::Suppressed || saved.timer <= .65F) && std::isfinite(saved.grip_fraction)
               && saved.grip_fraction >= 0 && saved.grip_fraction <= 1
               && (has(activated, "grip_ready") || saved.grip_fraction == 0) && std::isfinite(saved.capture_fraction)
               && saved.capture_fraction >= 0 && saved.capture_fraction <= 1 && (engaged || saved.capture_fraction == 0)
               && (saved.mode != GuardianMode::Capturing || saved.capture_fraction < 1)
               && (saved.mode != GuardianMode::Contained || saved.capture_fraction == 1)
               && engaged == has(activated, "containment_engaged")
               && (saved.mode == GuardianMode::Contained) == has(activated, "guardian_contained")
               && (!engaged || (in_trap && saved.grip_fraction == 1 && has(activated, "limiter_ready")));
    }

    auto ContainmentEncounter::status_text(std::span<const std::string> activated) const -> std::string
    {
        if (state.mode == GuardianMode::Contained)
            return "Хранитель сдержан. Главный выход открыт.";
        if (state.mode == GuardianMode::Capturing)
            return "Захват опускается. Сдерживание выполняется.";
        if (!has(activated, "limiter_ready"))
            return "Установите кассету в гнездо у рамы. Это можно сделать до ремонта захвата.";
        if (!has(activated, "grip_ready"))
            return "Восстановите привод захвата на верхнем сервисном посту.";
        if (state.grip_fraction < 1)
            return "Захват выходит в рабочее положение.";
        if (capture_available(activated))
            return "ОКНО СДЕРЖИВАНИЯ: нажмите пуск у рамы, пока Хранитель восстанавливается!";
        if (state.mode == GuardianMode::Windup)
            return "Хранитель готовит удар: уйдите с отмеченного места!";
        if (state.mode == GuardianMode::Suppressed)
            return "Выстрел задержал Хранителя. Оружие не заменяет сдерживание.";
        return "Заведите Хранителя на жёлтую платформу. Уклонитесь от удара и включите пуск у рамы.";
    }

    auto build_containment_encounter(
        const RelayCore::BlockoutLevel& level,
        std::span<const RelayCore::gameplay::RuntimeUsable> usables,
        std::span<const RelayCore::gameplay::RuntimeDoor> doors
    ) -> std::optional<ContainmentEncounter>
    {
        std::optional<ContainmentEncounter> result;
        for (std::size_t i = 0; i < level.entities.size(); ++i)
        {
            const auto& entity = level.entities[i];
            if (entity.classname != "containment_apparatus")
                continue;
            const auto trap_origin = RelayCore::gameplay::detail::parse_vec3_property(entity, "trap_origin");
            const auto trap_size = RelayCore::gameplay::detail::parse_vec3_property(entity, "trap_size");
            if (result || !trap_origin || !trap_size || !std::isfinite(trap_size->x) || !std::isfinite(trap_size->y)
                || !std::isfinite(trap_size->z) || trap_size->x <= 2.1F || trap_size->y <= 0 || trap_size->z <= 2.1F
                || !std::isfinite(trap_origin->x) || !std::isfinite(trap_origin->y) || !std::isfinite(trap_origin->z))
                throw std::runtime_error("containment_apparatus requires one marker, finite trap_origin and positive "
                                         "trap_size wider than the Guardian");
            result = ContainmentEncounter{
                static_cast<int>(i),
                entity.origin,
                {*trap_origin - *trap_size * .5F, *trap_origin + *trap_size * .5F},
                {.position = entity.origin}
            };
            if (result->trap.mins.x < -8 || result->trap.maxs.x > 8 || result->trap.mins.z < 23
                || result->trap.maxs.z > 38 || result->trap.mins.y < 0 || trap_size->y < 3.6F)
                throw std::runtime_error("containment_apparatus trap must fit the central hall and the Guardian height"
                );
            for (const auto& [name, target] :
                 {std::pair{"grip_service", "grip_ready"},
                  {"limiter_socket", "limiter_ready"},
                  {"containment_fire", "containment_engaged"}})
            {
                const auto found = std::ranges::find(usables, name, &RelayCore::gameplay::RuntimeUsable::name);
                if (found == usables.end() || found->target != target
                    || found->action_kind != RelayCore::gameplay::UsableActionKind::ActivateTarget || !found->single_use
                    || (std::string_view{name} == "limiter_socket" && found->required_item != "limiter_cassette"))
                    throw std::runtime_error(
                        "containment_apparatus requires single-use activate_target binding: " + std::string{name}
                    );
            }
            const auto exit = std::ranges::find(doors, "containment_exit", &RelayCore::gameplay::RuntimeDoor::name);
            if (exit == doors.end() || !result->validate_state(result->state, {}))
                throw std::runtime_error("containment_apparatus has an invalid home or missing containment_exit");
        }
        return result;
    }
} // namespace RelayGame

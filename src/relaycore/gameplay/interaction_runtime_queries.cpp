#include "relaycore/core/profiling.h"
#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/gameplay/interaction_runtime_detail.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <limits>

namespace RelayCore::gameplay
{
    namespace
    {
        template<typename IndexProvider>
        auto find_focused_usable_impl(
            const glm::vec3& eye_position,
            const glm::vec3& view_forward,
            const std::vector<RuntimeUsable>& usables,
            const std::size_t candidate_count,
            IndexProvider&& index_provider,
            const float max_distance,
            const float min_facing_dot
        ) -> InteractionQueryResult
        {
            RELAYCORE_PROFILE_FUNCTION();
            InteractionQueryResult result{};
            float best_distance = std::numeric_limits<float>::max();

            const float forward_len_sq = glm::dot(view_forward, view_forward);
            const glm::vec3 forward =
                forward_len_sq > 0.0F ? glm::normalize(view_forward) : glm::vec3{0.0F, 0.0F, -1.0F};

            for (std::size_t candidate = 0; candidate < candidate_count; ++candidate)
            {
                const std::size_t raw_index = static_cast<std::size_t>(index_provider(candidate));
                if (raw_index >= usables.size())
                {
                    continue;
                }
                const RuntimeUsable& usable = usables[raw_index];
                if (!usable.enabled)
                {
                    continue;
                }

                const glm::vec3 focus_origin =
                    usable.has_focus_bounds ? detail::aabb_center(usable.focus_bounds) : usable.origin;
                const glm::vec3 to_focus = focus_origin - eye_position;
                const float forward_distance = glm::dot(to_focus, forward);
                if (forward_distance <= 0.0F)
                {
                    continue;
                }

                const glm::vec3 probe_point = eye_position + forward * forward_distance;
                const glm::vec3 target_point = usable.has_focus_bounds
                                                   ? detail::closest_point_on_aabb(usable.focus_bounds, probe_point)
                                                   : usable.origin;
                const glm::vec3 eye_target = usable.has_focus_bounds
                                                 ? detail::closest_point_on_aabb(usable.focus_bounds, eye_position)
                                                 : usable.origin;
                const float distance = glm::length(eye_target - eye_position);
                if (distance <= 0.0001F || distance > std::min(max_distance, usable.use_radius))
                {
                    continue;
                }

                const glm::vec3 to_target = target_point - eye_position;
                const float to_target_length = glm::length(to_target);
                const glm::vec3 direction = to_target_length > 0.0001F ? (to_target / to_target_length) : forward;
                const float facing_dot = glm::dot(forward, direction);
                if (facing_dot < min_facing_dot)
                {
                    continue;
                }

                const float lateral_distance = glm::length(target_point - probe_point);
                float focus_radius = glm::clamp(usable.use_radius * 0.45F, 0.35F, 0.90F);
                if (usable.has_focus_bounds)
                {
                    const glm::vec3 focus_size = usable.focus_bounds.maxs - usable.focus_bounds.mins;
                    const float panel_radius = std::max({focus_size.x, focus_size.y, focus_size.z}) * 0.75F;
                    focus_radius = std::max(0.24F, panel_radius);
                }
                if (lateral_distance > focus_radius)
                {
                    continue;
                }

                if (distance < best_distance)
                {
                    best_distance = distance;
                    result.usable_index = static_cast<int>(raw_index);
                    result.distance = distance;
                }
            }

            return result;
        }

        template<typename IndexProvider>
        auto update_trigger_overlaps_impl(
            const physics::Aabb& player_bounds,
            const std::size_t candidate_count,
            IndexProvider&& index_provider,
            std::vector<RuntimeTrigger>& triggers,
            std::vector<RuntimeDoor>& doors,
            std::vector<RuntimeUsable>& usables,
            std::vector<std::string>& activated_target_ids,
            std::vector<std::string>& signaled_target_ids,
            InteractionEvent& last_event
        ) -> std::uint32_t
        {
            RELAYCORE_PROFILE_FUNCTION();
            std::uint32_t fired_count = 0;

            for (std::size_t candidate = 0; candidate < candidate_count; ++candidate)
            {
                const std::size_t index = static_cast<std::size_t>(index_provider(candidate));
                if (index >= triggers.size())
                {
                    continue;
                }

                RuntimeTrigger& trigger = triggers[index];
                if (trigger.once && trigger.fired)
                {
                    continue;
                }
                if (!detail::aabb_overlaps(player_bounds, trigger.bounds))
                {
                    continue;
                }

                bool action_applied = false;
                if (trigger.action_kind == TriggerActionKind::SignalTarget)
                {
                    action_applied = !trigger.target.empty() && signal_target(trigger.target, doors, usables);
                    if (action_applied)
                    {
                        detail::append_unique_string(signaled_target_ids, trigger.target);
                    }
                } else if (!trigger.target.empty())
                {
                    (void) activate_target(trigger.target, doors);
                    detail::append_unique_string(activated_target_ids, trigger.target);
                    action_applied = true;
                }

                if (action_applied)
                {
                    last_event = InteractionEvent{
                        .kind = trigger.action_kind == TriggerActionKind::SignalTarget
                                    ? UseInteractionOutcome::SignaledTarget
                                    : UseInteractionOutcome::ActivatedTarget,
                        .source_kind = InteractionEventSourceKind::Trigger,
                        .source_name = trigger.name,
                        .target = trigger.target,
                        .primary_applied = true,
                    };
                }

                trigger.fired = true;
                ++fired_count;
            }

            return fired_count;
        }
    } // namespace

    void update_runtime_usables(const std::vector<RuntimeDoor>& doors, std::vector<RuntimeUsable>& usables)
    {
        RELAYCORE_PROFILE_FUNCTION();
        for (RuntimeUsable& usable : usables)
        {
            usable.enabled = !usable.consumed;
            if (!usable.tracks_target_door)
            {
                continue;
            }

            const RuntimeDoor* door = detail::find_target_door(doors, usable.target);
            if (door == nullptr)
            {
                usable.enabled = false;
                continue;
            }

            const physics::Aabb bounds = current_door_bounds(*door);
            usable.origin = detail::aabb_center(bounds) + usable.tracked_offset;
            usable.focus_bounds = bounds;
            usable.has_focus_bounds = true;
        }
    }

    void rebuild_trigger_spatial_index(const std::vector<RuntimeTrigger>& triggers, InteractionSpatialIndex& index)
    {
        RELAYCORE_PROFILE_FUNCTION();
        index.reset(triggers.size());
        for (std::size_t i = 0; i < triggers.size(); ++i)
        {
            index.insert(triggers[i].bounds, static_cast<std::uint32_t>(i));
        }
    }

    void rebuild_usable_spatial_index(const std::vector<RuntimeUsable>& usables, InteractionSpatialIndex& index)
    {
        RELAYCORE_PROFILE_FUNCTION();
        index.reset(usables.size());
        for (std::size_t i = 0; i < usables.size(); ++i)
        {
            if (!usables[i].enabled)
            {
                continue;
            }
            index.insert(detail::usable_spatial_bounds(usables[i]), static_cast<std::uint32_t>(i));
        }
    }

    void query_trigger_candidates(
        InteractionSpatialIndex& index,
        const physics::Aabb& bounds,
        std::vector<std::uint32_t>& out_indices
    )
    {
        RELAYCORE_PROFILE_FUNCTION();
        index.query(bounds, out_indices);
    }

    void query_usable_candidates(
        InteractionSpatialIndex& index,
        const glm::vec3& eye_position,
        const float max_distance,
        std::vector<std::uint32_t>& out_indices
    )
    {
        RELAYCORE_PROFILE_FUNCTION();
        const glm::vec3 half_extents{max_distance, std::max(max_distance * 0.75F, 1.0F), max_distance};
        index.query({eye_position - half_extents, eye_position + half_extents}, out_indices);
    }

    auto find_focused_usable(
        const glm::vec3& eye_position,
        const glm::vec3& view_forward,
        const std::vector<RuntimeUsable>& usables,
        const std::span<const std::uint32_t> candidate_indices,
        const float max_distance,
        const float min_facing_dot
    ) -> InteractionQueryResult
    {
        RELAYCORE_PROFILE_FUNCTION();
        return find_focused_usable_impl(
            eye_position,
            view_forward,
            usables,
            candidate_indices.size(),
            [&candidate_indices](const std::size_t candidate) {
                return candidate_indices[candidate];
            },
            max_distance,
            min_facing_dot
        );
    }

    auto find_focused_usable(
        const glm::vec3& eye_position,
        const glm::vec3& view_forward,
        const std::vector<RuntimeUsable>& usables,
        const float max_distance,
        const float min_facing_dot
    ) -> InteractionQueryResult
    {
        RELAYCORE_PROFILE_FUNCTION();
        return find_focused_usable_impl(
            eye_position,
            view_forward,
            usables,
            usables.size(),
            [](const std::size_t candidate) {
                return candidate;
            },
            max_distance,
            min_facing_dot
        );
    }

    auto update_trigger_overlaps(
        const physics::Aabb& player_bounds,
        const std::span<const std::uint32_t> candidate_indices,
        std::vector<RuntimeTrigger>& triggers,
        std::vector<RuntimeDoor>& doors,
        std::vector<RuntimeUsable>& usables,
        std::vector<std::string>& activated_target_ids,
        std::vector<std::string>& signaled_target_ids,
        InteractionEvent& last_event
    ) -> std::uint32_t
    {
        RELAYCORE_PROFILE_FUNCTION();
        return update_trigger_overlaps_impl(
            player_bounds,
            candidate_indices.size(),
            [&candidate_indices](const std::size_t candidate) {
                return candidate_indices[candidate];
            },
            triggers,
            doors,
            usables,
            activated_target_ids,
            signaled_target_ids,
            last_event
        );
    }

    auto update_trigger_overlaps(
        const physics::Aabb& player_bounds,
        std::vector<RuntimeTrigger>& triggers,
        std::vector<RuntimeDoor>& doors,
        std::vector<RuntimeUsable>& usables,
        std::vector<std::string>& activated_target_ids,
        std::vector<std::string>& signaled_target_ids,
        InteractionEvent& last_event
    ) -> std::uint32_t
    {
        RELAYCORE_PROFILE_FUNCTION();
        return update_trigger_overlaps_impl(
            player_bounds,
            triggers.size(),
            [](const std::size_t candidate) {
                return candidate;
            },
            triggers,
            doors,
            usables,
            activated_target_ids,
            signaled_target_ids,
            last_event
        );
    }
} // namespace RelayCore::gameplay

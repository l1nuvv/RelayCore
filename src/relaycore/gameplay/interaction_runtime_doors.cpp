#include "relaycore/core/profiling.h"
#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/gameplay/interaction_runtime_detail.h"

#include <glm/common.hpp>
#include <ranges>

namespace RelayCore::gameplay
{
    namespace
    {
        class DoorActivationStrategy {
        public:
            virtual ~DoorActivationStrategy() = default;

            [[nodiscard]] virtual auto activate(const RuntimeDoor& door) const -> DoorState = 0;
        };

        class ToggleDoorActivationStrategy final : public DoorActivationStrategy {
        public:
            [[nodiscard]] auto activate(const RuntimeDoor& door) const -> DoorState override
            {
                if (door.state == DoorState::Opening)
                {
                    return DoorState::Closing;
                }
                if (door.state == DoorState::Open || door.open_fraction > 0.0F)
                {
                    return DoorState::Closing;
                }
                return DoorState::Opening;
            }
        };

        class OpenOnlyDoorActivationStrategy final : public DoorActivationStrategy {
        public:
            [[nodiscard]] auto activate(const RuntimeDoor& door) const -> DoorState override
            {
                if (door.state == DoorState::Open || door.state == DoorState::Opening)
                {
                    return door.state;
                }
                return DoorState::Opening;
            }
        };

        [[nodiscard]] auto resolve_activation_strategy(const DoorActivationPolicy policy)
            -> const DoorActivationStrategy&
        {
            RELAYCORE_PROFILE_FUNCTION();
            static const ToggleDoorActivationStrategy toggle_strategy{};
            static const OpenOnlyDoorActivationStrategy open_only_strategy{};

            switch (policy)
            {
                case DoorActivationPolicy::OpenOnly:
                    return open_only_strategy;
                case DoorActivationPolicy::Toggle:
                default:
                    return toggle_strategy;
            }
        }

        void apply_usable_signal(RuntimeUsable& usable)
        {
            RELAYCORE_PROFILE_FUNCTION();
            switch (usable.signal_action)
            {
                case UsableSignalAction::Unlock:
                    usable.locked = false;
                    break;
                case UsableSignalAction::Lock:
                    usable.locked = true;
                    break;
                case UsableSignalAction::ToggleLock:
                    usable.locked = !usable.locked;
                    break;
                case UsableSignalAction::None:
                default:
                    break;
            }
        }

        void record_signaled_target(
            const std::string_view target,
            const bool signaled,
            std::vector<std::string>& signaled_target_ids
        )
        {
            RELAYCORE_PROFILE_FUNCTION();
            if (!signaled)
            {
                return;
            }

            detail::append_unique_string(signaled_target_ids, target);
        }

        [[nodiscard]] auto make_usable_event(const RuntimeUsable& usable, const UseInteractionOutcome kind)
            -> InteractionEvent
        {
            RELAYCORE_PROFILE_FUNCTION();
            InteractionEvent event{};
            event.kind = kind;
            event.source_kind = InteractionEventSourceKind::Usable;
            event.source_name = usable.name;
            event.source_label = usable.label;
            event.target = usable.target;
            event.chain_target = usable.chain_target;
            event.item_id = usable.item_id;
            return event;
        }
    } // namespace

    auto current_door_bounds(const RuntimeDoor& door) -> physics::Aabb
    {
        RELAYCORE_PROFILE_FUNCTION();
        const glm::vec3 translation = door.open_offset * glm::clamp(door.open_fraction, 0.0F, 1.0F);
        return {door.closed_bounds.mins + translation, door.closed_bounds.maxs + translation};
    }

    auto is_door_blocking(const RuntimeDoor& door) -> bool
    {
        RELAYCORE_PROFILE_FUNCTION();
        return door.blocks_when_closed && door.open_fraction < 0.95F;
    }

    auto is_door_active(const RuntimeDoor& door) -> bool
    {
        RELAYCORE_PROFILE_FUNCTION();
        return door.state != DoorState::Closed;
    }

    auto door_overlaps_actor(const RuntimeDoor& door, const physics::Aabb& actor_bounds) -> bool
    {
        RELAYCORE_PROFILE_FUNCTION();
        return detail::aabb_overlaps(current_door_bounds(door), actor_bounds);
    }

    void collect_blocking_doors(const std::vector<RuntimeDoor>& doors, std::vector<physics::Aabb>& out_blockers)
    {
        RELAYCORE_PROFILE_FUNCTION();
        const physics::Aabb empty_actor_bounds{
            .mins = {0.0F, 0.0F, 0.0F},
            .maxs = {0.0F, 0.0F, 0.0F},
        };
        collect_blocking_doors(doors, empty_actor_bounds, out_blockers);
    }

    void collect_blocking_doors(
        const std::vector<RuntimeDoor>& doors,
        const physics::Aabb& actor_bounds,
        std::vector<physics::Aabb>& out_blockers
    )
    {
        RELAYCORE_PROFILE_FUNCTION();
        out_blockers.clear();
        out_blockers.reserve(doors.size());

        for (const RuntimeDoor& door : doors)
        {
            if (!is_door_blocking(door))
            {
                continue;
            }
            if (door.state == DoorState::Closing && door_overlaps_actor(door, actor_bounds))
            {
                continue;
            }
            out_blockers.push_back(current_door_bounds(door));
        }
    }

    auto activate_target(std::string_view target, std::vector<RuntimeDoor>& doors) -> bool
    {
        RELAYCORE_PROFILE_FUNCTION();
        bool activated = false;
        for (RuntimeDoor& door : doors)
        {
            if (door.name != target)
            {
                continue;
            }
            door.state = resolve_activation_strategy(door.activation_policy).activate(door);
            activated = true;
        }
        return activated;
    }

    auto signal_target(std::string_view target, std::vector<RuntimeDoor>& doors, std::vector<RuntimeUsable>& usables)
        -> bool
    {
        RELAYCORE_PROFILE_FUNCTION();
        bool signaled = false;
        if (target.empty())
        {
            return false;
        }

        if (activate_target(target, doors))
        {
            signaled = true;
        }

        for (RuntimeUsable& usable : usables)
        {
            if (usable.name != target || usable.signal_action == UsableSignalAction::None || !usable.enabled
                || usable.consumed)
            {
                continue;
            }

            apply_usable_signal(usable);
            signaled = true;
        }

        return signaled;
    }

    auto perform_usable_action(
        const RuntimeUsable& usable,
        std::vector<RuntimeDoor>& doors,
        std::vector<RuntimeUsable>& usables
    ) -> bool
    {
        RELAYCORE_PROFILE_FUNCTION();
        switch (usable.action_kind)
        {
            case UsableActionKind::SignalTarget:
                return signal_target(usable.target, doors, usables);

            case UsableActionKind::ActivateTarget:
                if (usable.target.empty())
                {
                    return false;
                }
                (void) activate_target(usable.target, doors);
                return true;

            case UsableActionKind::PickupItem:
            default:
                return false;
        }
    }

    auto apply_usable_interaction(
        RuntimeUsable& usable,
        std::vector<RuntimeDoor>& doors,
        std::vector<RuntimeUsable>& usables,
        std::vector<std::string>& inventory_item_ids,
        std::vector<std::string>& activated_target_ids,
        std::vector<std::string>& signaled_target_ids
    ) -> InteractionEvent
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (!usable.enabled || usable.consumed)
        {
            return {};
        }

        if (usable.locked)
        {
            return make_usable_event(usable, UseInteractionOutcome::Locked);
        }

        const bool has_required_item =
            usable.required_item.empty() || detail::contains_string(inventory_item_ids, usable.required_item);
        usable.requirement_met = has_required_item;
        if (!has_required_item)
        {
            return make_usable_event(usable, UseInteractionOutcome::MissingRequiredItem);
        }

        if (usable.action_kind == UsableActionKind::PickupItem)
        {
            InteractionEvent event = make_usable_event(usable, UseInteractionOutcome::PickedUpItem);
            detail::append_unique_string(inventory_item_ids, usable.item_id);
            usable.consumed = true;
            usable.enabled = false;
            event.primary_applied = true;
            event.source_consumed = true;
            if (!usable.chain_target.empty())
            {
                event.chain_applied = signal_target(usable.chain_target, doors, usables);
                record_signaled_target(usable.chain_target, event.chain_applied, signaled_target_ids);
            }
            return event;
        }

        if (!perform_usable_action(usable, doors, usables))
        {
            return {};
        }

        InteractionEvent event = make_usable_event(
            usable,
            usable.action_kind == UsableActionKind::SignalTarget ? UseInteractionOutcome::SignaledTarget
                                                                 : UseInteractionOutcome::ActivatedTarget
        );
        event.primary_applied = true;
        if (usable.action_kind == UsableActionKind::SignalTarget)
        {
            record_signaled_target(usable.target, true, signaled_target_ids);
        } else
        {
            detail::append_unique_string(activated_target_ids, usable.target);
        }
        if (!usable.chain_target.empty())
        {
            event.chain_applied = signal_target(usable.chain_target, doors, usables);
            record_signaled_target(usable.chain_target, event.chain_applied, signaled_target_ids);
        }
        if (usable.single_use)
        {
            usable.consumed = true;
            usable.enabled = false;
            event.source_consumed = true;
        }
        return event;
    }

    auto update_doors(const float dt, std::vector<RuntimeDoor>& doors) -> std::uint32_t
    {
        RELAYCORE_PROFILE_FUNCTION();
        const physics::Aabb empty_actor_bounds{
            .mins = {0.0F, 0.0F, 0.0F},
            .maxs = {0.0F, 0.0F, 0.0F},
        };
        return update_doors(dt, empty_actor_bounds, doors);
    }

    auto update_doors(const float dt, const physics::Aabb& actor_bounds, std::vector<RuntimeDoor>& doors)
        -> std::uint32_t
    {
        RELAYCORE_PROFILE_ZONE_N("gameplay::update_doors");
        std::uint32_t active_doors = 0;

        for (RuntimeDoor& door : doors)
        {
            switch (door.state)
            {
                case DoorState::Closed:
                    break;

                case DoorState::Opening:
                    door.open_fraction = glm::clamp(door.open_fraction + (door.move_speed * dt), 0.0F, 1.0F);
                    ++active_doors;
                    if (door.open_fraction >= 0.999F)
                    {
                        door.open_fraction = 1.0F;
                        door.state = DoorState::Open;
                    }
                    break;

                case DoorState::Open:
                    ++active_doors;
                    break;

                case DoorState::Closing:
                    door.open_fraction = glm::clamp(door.open_fraction - (door.move_speed * dt), 0.0F, 1.0F);
                    ++active_doors;
                    if (door_overlaps_actor(door, actor_bounds))
                    {
                        door.state = DoorState::Opening;
                        break;
                    }
                    if (door.open_fraction <= 0.001F)
                    {
                        door.open_fraction = 0.0F;
                        door.state = DoorState::Closed;
                    }
                    break;
            }
        }

        return active_doors;
    }
} // namespace RelayCore::gameplay

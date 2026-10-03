#include "relaycore/core/profiling.h"
#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/gameplay/interaction_runtime_detail.h"
#include "relaycore/world/blockout_level.h"

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

    namespace
    {
        void collect_blocking_doors_impl(
            const std::span<const RuntimeDoor> doors,
            const physics::Aabb* actor_bounds,
            std::vector<physics::WorldCollider>& out_blockers
        )
        {
            out_blockers.clear();
            out_blockers.reserve(doors.size());

            for (std::size_t index = 0; index < doors.size(); ++index)
            {
                const auto& door = doors[index];
                if (!is_door_blocking(door))
                {
                    continue;
                }
                if (actor_bounds && door.state == DoorState::Closing && door_overlaps_actor(door, *actor_bounds))
                {
                    continue;
                }
                out_blockers.push_back({
                    .bounds = current_door_bounds(door),
                    .surface = SurfaceMaterial::Metal,
                    .object = {physics::WorldObjectKind::Door, index, door.authored_entity_index},
                });
            }
        }
    } // namespace

    void
    collect_blocking_doors(const std::span<const RuntimeDoor> doors, std::vector<physics::WorldCollider>& out_blockers)
    {
        RELAYCORE_PROFILE_FUNCTION();
        collect_blocking_doors_impl(doors, nullptr, out_blockers);
    }

    void collect_blocking_doors(
        const std::span<const RuntimeDoor> doors,
        const physics::Aabb& actor_bounds,
        std::vector<physics::WorldCollider>& out_blockers
    )
    {
        RELAYCORE_PROFILE_FUNCTION();
        collect_blocking_doors_impl(doors, &actor_bounds, out_blockers);
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

#include "relaycore/core/profiling.h"
#include "relaycore/gameplay/interaction_runtime.h"

#include <glm/common.hpp>

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
    } // namespace

    auto current_door_bounds(const RuntimeDoor& door) -> physics::Aabb
    {
        const glm::vec3 translation = door.open_offset * glm::clamp(door.open_fraction, 0.0F, 1.0F);
        return {door.closed_bounds.mins + translation, door.closed_bounds.maxs + translation};
    }

    auto is_door_blocking(const RuntimeDoor& door) -> bool
    {
        return door.blocks_when_closed && door.open_fraction < 0.95F;
    }

    auto is_door_active(const RuntimeDoor& door) -> bool
    {
        return door.state != DoorState::Closed;
    }

    void collect_blocking_doors(const std::vector<RuntimeDoor>& doors, std::vector<physics::Aabb>& out_blockers)
    {
        out_blockers.clear();
        out_blockers.reserve(doors.size());

        for (const RuntimeDoor& door : doors)
        {
            if (!is_door_blocking(door))
            {
                continue;
            }
            out_blockers.push_back(current_door_bounds(door));
        }
    }

    auto activate_target(std::string_view target, std::vector<RuntimeDoor>& doors) -> bool
    {
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

    auto update_doors(const float dt, std::vector<RuntimeDoor>& doors) -> std::uint32_t
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

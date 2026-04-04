#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/gameplay/interaction_runtime_detail.h"

namespace RelayCore::gameplay
{
    void InteractionRuntimeFactory::build(
        const InteractionDescriptorSet& descriptors,
        std::vector<RuntimeDoor>& doors,
        std::vector<RuntimeTrigger>& triggers,
        std::vector<RuntimeUsable>& usables
    )
    {
        doors.clear();
        triggers.clear();
        usables.clear();

        doors.reserve(descriptors.doors.size());
        for (const DoorDesc& descriptor : descriptors.doors)
        {
            doors.push_back(
                RuntimeDoor{
                    .name = descriptor.name,
                    .closed_bounds = descriptor.closed_bounds,
                    .open_offset = descriptor.open_offset,
                    .open_fraction = 0.0F,
                    .move_speed = descriptor.move_speed,
                    .blocks_when_closed = descriptor.blocks_when_closed,
                    .activation_policy = descriptor.activation_policy,
                    .state = DoorState::Closed,
                }
            );
        }

        triggers.reserve(descriptors.triggers.size());
        for (const TriggerDesc& descriptor : descriptors.triggers)
        {
            triggers.push_back(
                RuntimeTrigger{
                    .name = descriptor.name,
                    .target = descriptor.target,
                    .bounds = descriptor.bounds,
                    .once = descriptor.once,
                    .fired = false,
                }
            );
        }

        usables.reserve(descriptors.usables.size());
        for (const UsableDesc& descriptor : descriptors.usables)
        {
            RuntimeUsable usable{};
            usable.label = descriptor.label;
            usable.target = descriptor.target;
            usable.origin = descriptor.origin;
            usable.use_radius = descriptor.use_radius;
            usable.tracks_target_door = descriptor.track_target_door;
            usable.visual_kind = descriptor.visual_kind;
            usable.visual_bounds = descriptor.visual_bounds;
            usable.has_visual_bounds = descriptor.has_visual_bounds;

            if (usable.has_visual_bounds)
            {
                usable.focus_bounds = descriptor.visual_bounds;
                usable.has_focus_bounds = true;
            }

            if (usable.tracks_target_door)
            {
                if (const RuntimeDoor* door = detail::find_target_door(doors, usable.target); door != nullptr)
                {
                    usable.tracked_offset = descriptor.origin - detail::aabb_center(door->closed_bounds);
                    usable.focus_bounds = door->closed_bounds;
                    usable.has_focus_bounds = true;
                } else
                {
                    usable.tracks_target_door = false;
                }
            }

            usables.push_back(std::move(usable));
        }
    }

    void build_runtime_interactions(
        const BlockoutLevel& level,
        std::vector<RuntimeDoor>& doors,
        std::vector<RuntimeTrigger>& triggers,
        std::vector<RuntimeUsable>& usables
    )
    {
        const InteractionDescriptorSet descriptors = InteractionDescriptorBuilder{level}.build();
        InteractionRuntimeFactory::build(descriptors, doors, triggers, usables);
    }
} // namespace RelayCore::gameplay

#pragma once

#include "relaycore/physics/collision.h"

#include <cstdint>
#include <glm/vec3.hpp>
#include <string>
#include <vector>

namespace RelayCore
{
    struct BlockoutLevel;
}

namespace RelayCore::gameplay
{
    enum class UsableVisualKind : std::uint8_t {
        None = 0,
        Panel,
        Pickup,
    };

    enum class PanelMountAxis : std::uint8_t {
        Centered = 0,
        PositiveX,
        NegativeX,
        PositiveZ,
        NegativeZ,
    };

    enum class DoorActivationPolicy : std::uint8_t {
        Toggle = 0,
        OpenOnly,
    };

    enum class UsableActionKind : std::uint8_t {
        ActivateTarget = 0,
        SignalTarget,
        PickupItem,
    };

    enum class TriggerActionKind : std::uint8_t {
        ActivateTarget = 0,
        SignalTarget,
    };

    enum class UsableSignalAction : std::uint8_t {
        None = 0,
        Unlock,
        Lock,
        ToggleLock,
    };

    struct DoorDesc {
        int authored_entity_index = -1;
        std::string name{};
        physics::Aabb closed_bounds{};
        glm::vec3 open_offset{0.0F, 2.10F, 0.0F};
        float move_speed = 1.5F;
        bool blocks_when_closed = true;
        DoorActivationPolicy activation_policy = DoorActivationPolicy::Toggle;
    };

    struct TriggerDesc {
        int authored_entity_index = -1;
        std::string name{};
        std::string target{};
        physics::Aabb bounds{};
        bool once = true;
        TriggerActionKind action_kind = TriggerActionKind::ActivateTarget;
    };

    struct UsableDesc {
        int authored_entity_index = -1;
        int generated_from_entity_index = -1;
        std::string name{};
        std::string label{};
        std::string target{};
        std::string chain_target{};
        std::string item_id{};
        std::string item_name{};
        std::string required_item{};
        std::string required_item_name{};
        std::string locked_message{};
        std::string success_message{};
        std::string denied_message{};
        glm::vec3 origin{};
        float use_radius = 2.25F;
        bool locked = false;
        bool single_use = false;
        bool track_target_door = false;
        UsableVisualKind visual_kind = UsableVisualKind::None;
        PanelMountAxis panel_mount_axis = PanelMountAxis::Centered;
        UsableActionKind action_kind = UsableActionKind::ActivateTarget;
        UsableSignalAction signal_action = UsableSignalAction::None;
        physics::Aabb visual_bounds{};
        bool has_visual_bounds = false;
    };

    struct InteractionDescriptorSet {
        std::vector<DoorDesc> doors{};
        std::vector<TriggerDesc> triggers{};
        std::vector<UsableDesc> usables{};
    };

    class InteractionDescriptorBuilder {
    public:
        explicit InteractionDescriptorBuilder(const BlockoutLevel& level) noexcept;

        [[nodiscard]] auto build() const -> InteractionDescriptorSet;

    private:
        const BlockoutLevel& level_;
    };
} // namespace RelayCore::gameplay

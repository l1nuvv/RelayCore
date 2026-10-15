#pragma once

#include "relaycore/core/spatial_index_2d.h"
#include "relaycore/gameplay/interaction_descriptors.h"
#include "relaycore/physics/world_query.h"

#include <cstdint>
#include <glm/vec3.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace RelayCore
{
    struct BlockoutLevel;
}

namespace RelayCore::gameplay
{
    enum class DoorState : std::uint8_t {
        Closed = 0,
        Opening,
        Open,
        Closing,
    };

    struct RuntimeDoor {
        int authored_entity_index = -1;
        std::string name{};
        physics::Aabb closed_bounds{};
        glm::vec3 open_offset{0.0F, 2.0F, 0.0F};
        float open_fraction = 0.0F;
        float move_speed = 1.5F;
        bool blocks_when_closed = true;
        DoorActivationPolicy activation_policy = DoorActivationPolicy::Toggle;
        DoorState state = DoorState::Closed;
    };

    struct RuntimeTrigger {
        int authored_entity_index = -1;
        std::string name{};
        std::string target{};
        physics::Aabb bounds{};
        bool once = true;
        bool fired = false;
        bool was_overlapping = false;
        bool overlapping = false;
        TriggerActionKind action_kind = TriggerActionKind::ActivateTarget;
    };

    enum class UsableFeedbackStyle : std::uint8_t {
        None = 0,
        Success,
        Warning,
    };

    enum class UseInteractionOutcome : std::uint8_t {
        None = 0,
        Locked,
        MissingRequiredItem,
        PickedUpItem,
        ActivatedTarget,
        SignaledTarget,
    };

    enum class InteractionEventSourceKind : std::uint8_t {
        None = 0,
        Usable,
        Trigger,
    };

    struct InteractionEvent {
        UseInteractionOutcome kind = UseInteractionOutcome::None;
        InteractionEventSourceKind source_kind = InteractionEventSourceKind::None;
        std::string source_name{};
        std::string source_label{};
        std::string target{};
        std::string chain_target{};
        std::string item_id{};
        bool primary_applied = false;
        bool chain_applied = false;
        bool source_consumed = false;
    };

    struct RuntimeUsable {
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
        glm::vec3 tracked_offset{};
        physics::Aabb focus_bounds{};
        physics::Aabb visual_bounds{};
        float use_radius = 2.25F;
        bool locked = false;
        bool requirement_met = true;
        bool single_use = false;
        bool has_focus_bounds = false;
        bool has_visual_bounds = false;
        bool enabled = true;
        bool consumed = false;
        bool tracks_target_door = false;
        UsableVisualKind visual_kind = UsableVisualKind::None;
        PanelMountAxis panel_mount_axis = PanelMountAxis::Centered;
        UsableActionKind action_kind = UsableActionKind::ActivateTarget;
        UsableSignalAction signal_action = UsableSignalAction::None;
        UsableFeedbackStyle feedback_style = UsableFeedbackStyle::None;
        float feedback_remaining_seconds = 0.0F;
        float feedback_total_seconds = 0.0F;
    };

    struct InteractionQueryResult {
        int usable_index = -1;
        float distance = 0.0F;
    };

    using InteractionSpatialIndex = RelayCore::core::SpatialIndex2D<std::uint32_t>;

    class UsableSpatialIndex {
    public:
        void rebuild(const std::vector<RuntimeUsable>& usables);
        void query(const physics::Aabb& bounds, std::vector<std::uint32_t>& out_indices);

    private:
        struct Entry {
            physics::Aabb bounds{};
            bool enabled = false;
        };
        InteractionSpatialIndex cells_{};
        std::vector<Entry> entries_{};
    };

    class InteractionRuntimeFactory {
    public:
        static void build(
            const InteractionDescriptorSet& descriptors,
            std::vector<RuntimeDoor>& doors,
            std::vector<RuntimeTrigger>& triggers,
            std::vector<RuntimeUsable>& usables
        );
    };

    void build_runtime_interactions(
        const BlockoutLevel& level,
        std::vector<RuntimeDoor>& doors,
        std::vector<RuntimeTrigger>& triggers,
        std::vector<RuntimeUsable>& usables
    );

    [[nodiscard]] auto current_door_bounds(const RuntimeDoor& door) -> physics::Aabb;

    [[nodiscard]] auto is_door_blocking(const RuntimeDoor& door) -> bool;

    [[nodiscard]] auto is_door_active(const RuntimeDoor& door) -> bool;

    [[nodiscard]] auto door_overlaps_actor(const RuntimeDoor& door, const physics::Aabb& actor_bounds) -> bool;

    void
    collect_blocking_doors(const std::span<const RuntimeDoor> doors, std::vector<physics::WorldCollider>& out_blockers);

    void collect_blocking_doors(
        const std::span<const RuntimeDoor> doors,
        const physics::Aabb& actor_bounds,
        std::vector<physics::WorldCollider>& out_blockers
    );

    void update_runtime_usables(const std::vector<RuntimeDoor>& doors, std::vector<RuntimeUsable>& usables);

    void rebuild_trigger_spatial_index(const std::vector<RuntimeTrigger>& triggers, InteractionSpatialIndex& index);

    void rebuild_usable_spatial_index(const std::vector<RuntimeUsable>& usables, UsableSpatialIndex& index);

    void query_trigger_candidates(
        InteractionSpatialIndex& index,
        const physics::Aabb& bounds,
        std::vector<std::uint32_t>& out_indices
    );

    void query_usable_candidates(
        UsableSpatialIndex& index,
        const glm::vec3& eye_position,
        float max_distance,
        std::vector<std::uint32_t>& out_indices
    );

    [[nodiscard]] auto find_focused_usable(
        const glm::vec3& eye_position,
        const glm::vec3& view_forward,
        const std::vector<RuntimeUsable>& usables,
        std::span<const std::uint32_t> candidate_indices,
        float max_distance = 2.25F,
        float min_facing_dot = 0.75F,
        physics::WorldQuery blockers = {}
    ) -> InteractionQueryResult;

    [[nodiscard]] auto find_focused_usable(
        const glm::vec3& eye_position,
        const glm::vec3& view_forward,
        const std::vector<RuntimeUsable>& usables,
        float max_distance = 2.25F,
        float min_facing_dot = 0.75F,
        physics::WorldQuery blockers = {}
    ) -> InteractionQueryResult;

    [[nodiscard]] auto activate_target(std::string_view target, std::vector<RuntimeDoor>& doors) -> bool;

    [[nodiscard]] auto
    signal_target(std::string_view target, std::vector<RuntimeDoor>& doors, std::vector<RuntimeUsable>& usables)
        -> bool;

    [[nodiscard]] auto perform_usable_action(
        const RuntimeUsable& usable,
        std::vector<RuntimeDoor>& doors,
        std::vector<RuntimeUsable>& usables
    ) -> bool;

    [[nodiscard]] auto update_trigger_overlaps(
        const physics::Aabb& player_bounds,
        std::span<const std::uint32_t> candidate_indices,
        std::vector<RuntimeTrigger>& triggers,
        std::vector<RuntimeDoor>& doors,
        std::vector<RuntimeUsable>& usables,
        std::vector<std::string>& activated_target_ids,
        std::vector<std::string>& signaled_target_ids,
        InteractionEvent& last_event
    ) -> std::uint32_t;

    [[nodiscard]] auto update_trigger_overlaps(
        const physics::Aabb& player_bounds,
        std::vector<RuntimeTrigger>& triggers,
        std::vector<RuntimeDoor>& doors,
        std::vector<RuntimeUsable>& usables,
        std::vector<std::string>& activated_target_ids,
        std::vector<std::string>& signaled_target_ids,
        InteractionEvent& last_event
    ) -> std::uint32_t;

    [[nodiscard]] auto update_doors(float dt, std::vector<RuntimeDoor>& doors) -> std::uint32_t;

    [[nodiscard]] auto update_doors(float dt, const physics::Aabb& actor_bounds, std::vector<RuntimeDoor>& doors)
        -> std::uint32_t;
} // namespace RelayCore::gameplay

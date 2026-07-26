#pragma once

#include "relaycore/gameplay/combat_runtime.h"
#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/gameplay/mission_runtime.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace RelayCore
{
    struct BlockoutLevel;
}

namespace RelayCore::gameplay
{
    using RuntimeObjectId = std::uint64_t;

    enum class RuntimeObjectKind : std::uint8_t {
        None = 0,
        Door,
        Trigger,
        Usable,
        Pickup,
    };

    enum class RuntimeBindingKind : std::uint8_t {
        ActivateTarget = 0,
        SignalTarget,
        ChainTarget,
        RequireItem,
        PickupItem,
    };

    struct RuntimeObjectHandle {
        RuntimeObjectId id = 0;
        RuntimeObjectKind kind = RuntimeObjectKind::None;
        std::size_t storage_index = 0;
        int authored_entity_index = -1;
        int generated_from_entity_index = -1;
        std::string debug_handle{};
        std::string name{};
        std::string label{};
        std::string target{};
        std::string source_bucket{};
        std::string source_reference{};
        std::string source_file_path{};
        int source_line = 0;
        int source_column = 0;
    };

    struct RuntimeBindingInfo {
        RuntimeBindingKind kind = RuntimeBindingKind::ActivateTarget;
        RuntimeObjectId source_id = 0;
        RuntimeObjectKind source_kind = RuntimeObjectKind::None;
        std::string source_handle{};
        std::string source_name{};
        std::string source_bucket{};
        std::string source_reference{};
        std::string source_file_path{};
        int source_line = 0;
        int source_column = 0;
        std::string target{};
        std::uint32_t resolved_target_count = 0;
        std::string resolved_targets_summary{};
    };

    class RuntimeObjectRegistry {
    public:
        void clear() noexcept;

        void build(
            const BlockoutLevel& level,
            std::span<const RuntimeDoor> doors,
            std::span<const RuntimeTrigger> triggers,
            std::span<const RuntimeUsable> usables
        );

        [[nodiscard]] auto empty() const noexcept -> bool;

        [[nodiscard]] auto signature() const noexcept -> std::uint64_t;

        [[nodiscard]] auto objects() const noexcept -> std::span<const RuntimeObjectHandle>;

        [[nodiscard]] auto bindings() const noexcept -> std::span<const RuntimeBindingInfo>;

        [[nodiscard]] auto find(RuntimeObjectId id) const -> const RuntimeObjectHandle*;

        [[nodiscard]] auto find_door(std::size_t storage_index) const -> const RuntimeObjectHandle*;

        [[nodiscard]] auto find_trigger(std::size_t storage_index) const -> const RuntimeObjectHandle*;

        [[nodiscard]] auto find_usable(std::size_t storage_index) const -> const RuntimeObjectHandle*;

    private:
        std::vector<RuntimeObjectHandle> objects_{};
        std::vector<RuntimeBindingInfo> bindings_{};
        std::vector<int> door_positions_{};
        std::vector<int> trigger_positions_{};
        std::vector<int> usable_positions_{};
        std::uint64_t signature_ = 0;
    };

    struct RuntimeDoorSnapshot {
        RuntimeObjectId runtime_id = 0;
        float open_fraction = 0.0F;
        DoorState state = DoorState::Closed;
    };

    struct RuntimeTriggerSnapshot {
        RuntimeObjectId runtime_id = 0;
        bool fired = false;
    };

    struct RuntimeUsableSnapshot {
        RuntimeObjectId runtime_id = 0;
        bool locked = false;
        bool consumed = false;
    };

    struct RuntimeTurretSnapshot {
        int authored_entity_index = -1;
        float health = 0.0F;
        float cooldown_remaining_seconds = 0.0F;
        float warmup_remaining_seconds = 0.0F;
        bool enabled = true;
        bool destroyed = false;
        bool warming_up = false;
    };

    struct PlayerCombatSnapshot {
        float health = 100.0F;
        float weapon_cooldown_remaining_seconds = 0.0F;
        float hurt_flash_remaining_seconds = 0.0F;
        float respawn_remaining_seconds = 0.0F;
        bool alive = true;
        std::uint32_t shots_fired = 0;
        std::uint32_t shots_hit = 0;
        std::uint32_t kills = 0;
        std::uint32_t deaths = 0;
        float damage_dealt = 0.0F;
        float damage_taken = 0.0F;
        std::string last_damage_source{};
    };

    struct RuntimeLevelSnapshot {
        std::string level_path{};
        std::uint64_t registry_signature = 0;
        std::uint64_t simulation_tick = 0;
        std::vector<RuntimeDoorSnapshot> doors{};
        std::vector<RuntimeTriggerSnapshot> triggers{};
        std::vector<RuntimeUsableSnapshot> usables{};
        std::vector<RuntimeTurretSnapshot> turrets{};
        std::vector<std::string> inventory_item_ids{};
        std::vector<std::string> activated_target_ids{};
        std::vector<std::string> signaled_target_ids{};
        PlayerCombatSnapshot player_combat{};
        MissionRuntimeSnapshot mission{};
    };

    struct SnapshotRestoreResult {
        bool restored = false;
        std::string error{};
        std::uint32_t restored_doors = 0;
        std::uint32_t restored_triggers = 0;
        std::uint32_t restored_usables = 0;
        std::uint32_t restored_turrets = 0;
    };

    [[nodiscard]] auto runtime_object_kind_label(RuntimeObjectKind kind) -> const char*;

    [[nodiscard]] auto runtime_binding_kind_label(RuntimeBindingKind kind) -> const char*;

    [[nodiscard]] auto runtime_binding_requires_resolved_target(RuntimeBindingKind kind) -> bool;

    [[nodiscard]] auto capture_runtime_level_snapshot(
        std::string level_path,
        const RuntimeObjectRegistry& registry,
        std::span<const RuntimeDoor> doors,
        std::span<const RuntimeTrigger> triggers,
        std::span<const RuntimeUsable> usables,
        std::span<const RuntimeTurret> turrets,
        std::span<const std::string> inventory_item_ids,
        std::span<const std::string> activated_target_ids,
        std::span<const std::string> signaled_target_ids,
        const PlayerCombatState& player_combat,
        const MissionRuntime& mission_runtime,
        std::uint64_t simulation_tick
    ) -> RuntimeLevelSnapshot;

    [[nodiscard]] auto restore_runtime_level_snapshot(
        const RuntimeLevelSnapshot& snapshot,
        std::string_view level_path,
        const RuntimeObjectRegistry& registry,
        std::vector<RuntimeDoor>& doors,
        std::vector<RuntimeTrigger>& triggers,
        std::vector<RuntimeUsable>& usables,
        std::vector<RuntimeTurret>& turrets,
        std::vector<std::string>& inventory_item_ids,
        std::vector<std::string>& activated_target_ids,
        std::vector<std::string>& signaled_target_ids,
        PlayerCombatState& player_combat,
        MissionRuntime& mission_runtime
    ) -> SnapshotRestoreResult;

    [[nodiscard]] auto build_runtime_snapshot_summary(const RuntimeLevelSnapshot& snapshot) -> std::string;

    [[nodiscard]] auto build_runtime_restore_summary(const SnapshotRestoreResult& result) -> std::string;
} // namespace RelayCore::gameplay

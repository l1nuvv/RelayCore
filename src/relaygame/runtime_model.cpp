#include "relaygame/runtime_model.h"

#include "relaycore/debug/inspector_source_reference.h"
#include "relaycore/world/blockout_level.h"

#include <algorithm>
#include <format>
#include <optional>
#include <ranges>
#include <string_view>
#include <unordered_set>

namespace RelayGame
{
    using namespace RelayCore::gameplay;
    using RelayCore::BlockoutLevel;
    using RelayCore::EntitySpawnData;
    namespace physics = RelayCore::physics;
    using RelayCore::AuthoredSourceLocation;
    namespace debug = RelayCore::debug;

    namespace
    {
        struct SourceInfo {
            std::string bucket{};
            std::string reference{};
            std::string file_path{};
            int line = 0;
            int column = 0;
        };

        [[nodiscard]] auto fnv1a_append(std::uint64_t hash, std::string_view value) -> std::uint64_t
        {
            constexpr std::uint64_t kPrime = 1099511628211ULL;
            for (const unsigned char c : value)
            {
                hash ^= c;
                hash *= kPrime;
            }
            return hash;
        }

        [[nodiscard]] auto fnv1a(std::string_view value) -> std::uint64_t
        {
            constexpr std::uint64_t kOffset = 14695981039346656037ULL;
            return fnv1a_append(kOffset, value);
        }

        [[nodiscard]] auto make_source_info_from_authored(const AuthoredSourceLocation& source, std::string bucket)
            -> SourceInfo
        {
            const debug::SourceLocation primary{
                .file_path = source.has_placement() ? source.placement_file_path : source.file_path,
                .line = static_cast<int>(source.has_placement() ? source.placement_line : source.line),
                .column = static_cast<int>(source.has_placement() ? source.placement_column : source.column),
            };

            return SourceInfo{
                .bucket = std::move(bucket),
                .reference = debug::build_source_reference(primary),
                .file_path = primary.file_path,
                .line = primary.line,
                .column = primary.column,
            };
        }

        [[nodiscard]] auto
        try_make_entity_source_info(const BlockoutLevel& level, const int entity_index, const bool generated_from)
            -> std::optional<SourceInfo>
        {
            if (entity_index < 0)
            {
                return std::nullopt;
            }

            const std::size_t index = static_cast<std::size_t>(entity_index);
            if (index >= level.entity_sources.size())
            {
                return std::nullopt;
            }

            const AuthoredSourceLocation& source = level.entity_sources[index];
            if (!source.valid())
            {
                return std::nullopt;
            }

            if (generated_from)
            {
                return make_source_info_from_authored(source, "generated");
            }
            if (source.has_placement())
            {
                return make_source_info_from_authored(source, "instanced");
            }
            return make_source_info_from_authored(source, "direct");
        }

        [[nodiscard]] auto short_hex(const std::uint64_t value) -> std::string
        {
            return std::format("{:016x}", value);
        }

        [[nodiscard]] auto build_debug_handle(
            const RuntimeObjectKind kind,
            const std::string_view name,
            const std::size_t storage_index,
            const RuntimeObjectId id
        ) -> std::string
        {
            if (!name.empty())
            {
                return std::string{runtime_object_kind_label(kind)} + ":" + std::string{name} + "@" + short_hex(id);
            }

            return std::string{runtime_object_kind_label(kind)} + "[" + std::to_string(storage_index) + "]@"
                   + short_hex(id);
        }

        [[nodiscard]] auto join_handle_summary(const std::vector<std::string>& handles) -> std::string
        {
            if (handles.empty())
            {
                return {};
            }

            constexpr std::size_t kPreviewCount = 3;
            std::string result{};
            const std::size_t preview_count = std::min(handles.size(), kPreviewCount);
            for (std::size_t index = 0; index < preview_count; ++index)
            {
                if (index > 0)
                {
                    result += ", ";
                }
                result += handles[index];
            }

            if (handles.size() > preview_count)
            {
                result += " +" + std::to_string(handles.size() - preview_count);
            }
            return result;
        }

        [[nodiscard]] auto build_object_id_key(
            const RuntimeObjectKind kind,
            const std::string_view identity,
            const std::string_view source_reference
        ) -> std::string
        {
            return std::string{runtime_object_kind_label(kind)} + "|" + std::string{source_reference} + "|"
                   + std::string{identity};
        }

        [[nodiscard]] auto resolve_unique_id(
            const RuntimeObjectKind kind,
            const std::string_view identity,
            const std::string_view source_reference,
            const std::size_t storage_index,
            std::unordered_set<RuntimeObjectId>& used_ids
        ) -> RuntimeObjectId
        {
            std::string key = build_object_id_key(kind, identity, source_reference);
            if (source_reference.empty() && identity == runtime_object_kind_label(kind))
            {
                key += "|index=" + std::to_string(storage_index);
            }
            RuntimeObjectId id = fnv1a(key);
            if (id == 0)
            {
                id = 1;
            }

            std::size_t collision = 0;
            while (used_ids.contains(id))
            {
                ++collision;
                key = build_object_id_key(kind, identity, source_reference);
                key += "|index=" + std::to_string(storage_index);
                key += "|collision=" + std::to_string(collision);
                id = fnv1a(key);
                if (id == 0)
                {
                    id = collision + 1;
                }
            }

            used_ids.insert(id);
            return id;
        }

        [[nodiscard]] auto resolve_activate_target_handles(
            std::string_view target,
            std::span<const RuntimeDoor> doors,
            const RuntimeObjectRegistry& registry
        ) -> std::vector<std::string>
        {
            std::vector<std::string> handles{};
            for (std::size_t index = 0; index < doors.size(); ++index)
            {
                if (doors[index].name != target)
                {
                    continue;
                }

                if (const RuntimeObjectHandle* handle = registry.find_door(index); handle != nullptr)
                {
                    handles.push_back(handle->debug_handle);
                }
            }
            return handles;
        }

        [[nodiscard]] auto resolve_signal_target_handles(
            std::string_view target,
            std::span<const RuntimeDoor> doors,
            std::span<const RuntimeUsable> usables,
            const RuntimeObjectRegistry& registry
        ) -> std::vector<std::string>
        {
            std::vector<std::string> handles = resolve_activate_target_handles(target, doors, registry);
            for (std::size_t index = 0; index < usables.size(); ++index)
            {
                if (usables[index].name != target || usables[index].signal_action == UsableSignalAction::None)
                {
                    continue;
                }

                if (const RuntimeObjectHandle* handle = registry.find_usable(index); handle != nullptr)
                {
                    handles.push_back(handle->debug_handle);
                }
            }
            return handles;
        }

        template<typename T>
        auto apply_required_snapshot_entries(
            const std::vector<T>& snapshot_entries,
            const std::size_t expected_count,
            const char* label,
            std::string& out_error
        ) -> bool
        {
            if (snapshot_entries.size() != expected_count)
            {
                out_error = std::string{"Snapshot "} + label + " count mismatch";
                return false;
            }
            return true;
        }
    } // namespace

    auto runtime_object_kind_label(const RuntimeObjectKind kind) -> const char*
    {
        switch (kind)
        {
            case RuntimeObjectKind::Door:
                return "door";
            case RuntimeObjectKind::Trigger:
                return "trigger";
            case RuntimeObjectKind::Usable:
                return "usable";
            case RuntimeObjectKind::Pickup:
                return "pickup";
            case RuntimeObjectKind::None:
            default:
                return "none";
        }
    }

    auto runtime_binding_kind_label(const RuntimeBindingKind kind) -> const char*
    {
        switch (kind)
        {
            case RuntimeBindingKind::SignalTarget:
                return "signal_target";
            case RuntimeBindingKind::ChainTarget:
                return "chain_target";
            case RuntimeBindingKind::RequireItem:
                return "require_item";
            case RuntimeBindingKind::PickupItem:
                return "pickup_item";
            case RuntimeBindingKind::ActivateTarget:
            default:
                return "activate_target";
        }
    }

    auto runtime_binding_requires_resolved_target(const RuntimeBindingKind kind) -> bool
    {
        switch (kind)
        {
            case RuntimeBindingKind::ActivateTarget:
            case RuntimeBindingKind::SignalTarget:
            case RuntimeBindingKind::ChainTarget:
                return true;
            case RuntimeBindingKind::RequireItem:
            case RuntimeBindingKind::PickupItem:
            default:
                return false;
        }
    }

    void RuntimeObjectRegistry::clear() noexcept
    {
        objects_.clear();
        bindings_.clear();
        door_positions_.clear();
        trigger_positions_.clear();
        usable_positions_.clear();
        signature_ = 0;
    }

    void RuntimeObjectRegistry::build(
        const BlockoutLevel& level,
        const std::span<const RuntimeDoor> doors,
        const std::span<const RuntimeTrigger> triggers,
        const std::span<const RuntimeUsable> usables
    )
    {
        clear();

        objects_.reserve(doors.size() + triggers.size() + usables.size());
        door_positions_.assign(doors.size(), -1);
        trigger_positions_.assign(triggers.size(), -1);
        usable_positions_.assign(usables.size(), -1);

        std::unordered_set<RuntimeObjectId> used_ids{};
        used_ids.reserve(doors.size() + triggers.size() + usables.size());

        auto append_handle = [this, &used_ids, &level](
                                 const RuntimeObjectKind kind,
                                 const std::size_t storage_index,
                                 const int authored_entity_index,
                                 const int generated_from_entity_index,
                                 const std::string_view name,
                                 const std::string_view label,
                                 const std::string_view target
                             ) {
            std::optional<SourceInfo> source = try_make_entity_source_info(level, authored_entity_index, false);
            if (!source.has_value())
            {
                source = try_make_entity_source_info(level, generated_from_entity_index, true);
            }

            SourceInfo source_info{};
            if (source.has_value())
            {
                source_info = std::move(*source);
            } else
            {
                source_info.bucket = "untracked";
            }

            std::string identity{};
            if (!name.empty())
            {
                identity = std::string{name};
            } else if (!label.empty())
            {
                identity = std::string{label};
            } else if (!target.empty())
            {
                identity = std::string{target};
            } else
            {
                identity = runtime_object_kind_label(kind);
            }

            const RuntimeObjectId id =
                resolve_unique_id(kind, identity, source_info.reference, storage_index, used_ids);

            RuntimeObjectHandle handle{};
            handle.id = id;
            handle.kind = kind;
            handle.storage_index = storage_index;
            handle.authored_entity_index = authored_entity_index;
            handle.generated_from_entity_index = generated_from_entity_index;
            handle.debug_handle = build_debug_handle(kind, name.empty() ? label : name, storage_index, id);
            handle.name = std::string{name};
            handle.label = std::string{label};
            handle.target = std::string{target};
            handle.source_bucket = std::move(source_info.bucket);
            handle.source_reference = std::move(source_info.reference);
            handle.source_file_path = std::move(source_info.file_path);
            handle.source_line = source_info.line;
            handle.source_column = source_info.column;
            objects_.push_back(std::move(handle));
            return static_cast<int>(objects_.size() - 1);
        };

        for (std::size_t index = 0; index < doors.size(); ++index)
        {
            door_positions_[index] = append_handle(
                RuntimeObjectKind::Door,
                index,
                doors[index].authored_entity_index,
                -1,
                doors[index].name,
                {},
                {}
            );
        }

        for (std::size_t index = 0; index < triggers.size(); ++index)
        {
            trigger_positions_[index] = append_handle(
                RuntimeObjectKind::Trigger,
                index,
                triggers[index].authored_entity_index,
                -1,
                triggers[index].name,
                {},
                triggers[index].target
            );
        }

        for (std::size_t index = 0; index < usables.size(); ++index)
        {
            const RuntimeObjectKind kind = usables[index].action_kind == UsableActionKind::PickupItem
                                               ? RuntimeObjectKind::Pickup
                                               : RuntimeObjectKind::Usable;
            usable_positions_[index] = append_handle(
                kind,
                index,
                usables[index].authored_entity_index,
                usables[index].generated_from_entity_index,
                usables[index].name,
                usables[index].label,
                usables[index].target
            );
        }

        bindings_.reserve(triggers.size() + (usables.size() * 4ULL));

        auto append_binding = [this](
                                  const RuntimeObjectHandle& handle,
                                  const RuntimeBindingKind kind,
                                  const std::string_view target,
                                  const std::vector<std::string>& resolved_handles
                              ) {
            bindings_.push_back(RuntimeBindingInfo{
                .kind = kind,
                .source_id = handle.id,
                .source_kind = handle.kind,
                .source_handle = handle.debug_handle,
                .source_name = handle.name.empty() ? handle.label : handle.name,
                .source_bucket = handle.source_bucket,
                .source_reference = handle.source_reference,
                .source_file_path = handle.source_file_path,
                .source_line = handle.source_line,
                .source_column = handle.source_column,
                .target = std::string{target},
                .resolved_target_count = static_cast<std::uint32_t>(resolved_handles.size()),
                .resolved_targets_summary = join_handle_summary(resolved_handles),
            });
        };

        for (std::size_t index = 0; index < triggers.size(); ++index)
        {
            const RuntimeObjectHandle* handle = find_trigger(index);
            if (handle == nullptr || triggers[index].target.empty())
            {
                continue;
            }

            const RuntimeBindingKind kind = triggers[index].action_kind == TriggerActionKind::SignalTarget
                                                ? RuntimeBindingKind::SignalTarget
                                                : RuntimeBindingKind::ActivateTarget;
            const std::vector<std::string> resolved_handles =
                kind == RuntimeBindingKind::ActivateTarget
                    ? resolve_activate_target_handles(triggers[index].target, doors, *this)
                    : resolve_signal_target_handles(triggers[index].target, doors, usables, *this);
            append_binding(*handle, kind, triggers[index].target, resolved_handles);
        }

        for (std::size_t index = 0; index < usables.size(); ++index)
        {
            const RuntimeObjectHandle* handle = find_usable(index);
            if (handle == nullptr)
            {
                continue;
            }

            if (!usables[index].required_item.empty())
            {
                append_binding(*handle, RuntimeBindingKind::RequireItem, usables[index].required_item, {});
            }
            if (usables[index].action_kind == UsableActionKind::PickupItem && !usables[index].item_id.empty())
            {
                append_binding(*handle, RuntimeBindingKind::PickupItem, usables[index].item_id, {});
            }
            if (!usables[index].target.empty() && usables[index].action_kind != UsableActionKind::PickupItem)
            {
                const RuntimeBindingKind kind = usables[index].action_kind == UsableActionKind::SignalTarget
                                                    ? RuntimeBindingKind::SignalTarget
                                                    : RuntimeBindingKind::ActivateTarget;
                const std::vector<std::string> resolved_handles =
                    kind == RuntimeBindingKind::ActivateTarget
                        ? resolve_activate_target_handles(usables[index].target, doors, *this)
                        : resolve_signal_target_handles(usables[index].target, doors, usables, *this);
                append_binding(*handle, kind, usables[index].target, resolved_handles);
            }
            if (!usables[index].chain_target.empty())
            {
                append_binding(
                    *handle,
                    RuntimeBindingKind::ChainTarget,
                    usables[index].chain_target,
                    resolve_signal_target_handles(usables[index].chain_target, doors, usables, *this)
                );
            }
        }

        signature_ = 14695981039346656037ULL;
        for (const RuntimeObjectHandle& handle : objects_)
        {
            signature_ = fnv1a_append(signature_, handle.debug_handle);
        }
        if (signature_ == 0)
        {
            signature_ = 1;
        }
    }

    auto RuntimeObjectRegistry::empty() const noexcept -> bool
    {
        return objects_.empty();
    }

    auto RuntimeObjectRegistry::signature() const noexcept -> std::uint64_t
    {
        return signature_;
    }

    auto RuntimeObjectRegistry::objects() const noexcept -> std::span<const RuntimeObjectHandle>
    {
        return objects_;
    }

    auto RuntimeObjectRegistry::bindings() const noexcept -> std::span<const RuntimeBindingInfo>
    {
        return bindings_;
    }

    auto RuntimeObjectRegistry::find(const RuntimeObjectId id) const -> const RuntimeObjectHandle*
    {
        const auto it = std::ranges::find_if(objects_, [id](const RuntimeObjectHandle& handle) {
            return handle.id == id;
        });
        return it == objects_.end() ? nullptr : &(*it);
    }

    auto RuntimeObjectRegistry::find_door(const std::size_t storage_index) const -> const RuntimeObjectHandle*
    {
        if (storage_index >= door_positions_.size() || door_positions_[storage_index] < 0)
        {
            return nullptr;
        }
        return &objects_[static_cast<std::size_t>(door_positions_[storage_index])];
    }

    auto RuntimeObjectRegistry::find_trigger(const std::size_t storage_index) const -> const RuntimeObjectHandle*
    {
        if (storage_index >= trigger_positions_.size() || trigger_positions_[storage_index] < 0)
        {
            return nullptr;
        }
        return &objects_[static_cast<std::size_t>(trigger_positions_[storage_index])];
    }

    auto RuntimeObjectRegistry::find_usable(const std::size_t storage_index) const -> const RuntimeObjectHandle*
    {
        if (storage_index >= usable_positions_.size() || usable_positions_[storage_index] < 0)
        {
            return nullptr;
        }
        return &objects_[static_cast<std::size_t>(usable_positions_[storage_index])];
    }

    auto capture_runtime_level_snapshot(
        std::string level_path,
        const RuntimeObjectRegistry& registry,
        const std::span<const RuntimeDoor> doors,
        const std::span<const RuntimeTrigger> triggers,
        const std::span<const RuntimeUsable> usables,
        const std::span<const RuntimeTurret> turrets,
        const std::span<const std::string> inventory_item_ids,
        const std::span<const std::string> activated_target_ids,
        const std::span<const std::string> signaled_target_ids,
        const PlayerCombatState& player_combat,
        const MissionRuntime& mission_runtime,
        const std::uint64_t simulation_tick
    ) -> RuntimeLevelSnapshot
    {
        RuntimeLevelSnapshot snapshot{};
        snapshot.level_path = std::move(level_path);
        snapshot.registry_signature = registry.signature();
        snapshot.simulation_tick = simulation_tick;
        snapshot.inventory_item_ids.assign(inventory_item_ids.begin(), inventory_item_ids.end());
        snapshot.activated_target_ids.assign(activated_target_ids.begin(), activated_target_ids.end());
        snapshot.signaled_target_ids.assign(signaled_target_ids.begin(), signaled_target_ids.end());
        snapshot.mission = mission_runtime.snapshot();
        snapshot.doors.reserve(doors.size());
        snapshot.triggers.reserve(triggers.size());
        snapshot.usables.reserve(usables.size());
        snapshot.turrets.reserve(turrets.size());

        for (std::size_t index = 0; index < doors.size(); ++index)
        {
            if (const RuntimeObjectHandle* handle = registry.find_door(index); handle != nullptr)
            {
                snapshot.doors.push_back(RuntimeDoorSnapshot{
                    .runtime_id = handle->id,
                    .open_fraction = doors[index].open_fraction,
                    .state = doors[index].state,
                });
            }
        }

        for (std::size_t index = 0; index < triggers.size(); ++index)
        {
            if (const RuntimeObjectHandle* handle = registry.find_trigger(index); handle != nullptr)
            {
                snapshot.triggers.push_back(RuntimeTriggerSnapshot{
                    .runtime_id = handle->id,
                    .fired = triggers[index].fired,
                });
            }
        }

        for (std::size_t index = 0; index < usables.size(); ++index)
        {
            if (const RuntimeObjectHandle* handle = registry.find_usable(index); handle != nullptr)
            {
                snapshot.usables.push_back(RuntimeUsableSnapshot{
                    .runtime_id = handle->id,
                    .locked = usables[index].locked,
                    .consumed = usables[index].consumed,
                });
            }
        }

        for (const RuntimeTurret& turret : turrets)
        {
            snapshot.turrets.push_back(RuntimeTurretSnapshot{
                .authored_entity_index = turret.authored_entity_index,
                .health = turret.health,
                .cooldown_remaining_seconds = turret.cooldown_remaining_seconds,
                .warmup_remaining_seconds = turret.warmup_remaining_seconds,
                .enabled = turret.enabled,
                .destroyed = turret.destroyed,
                .warming_up = turret.warming_up,
            });
        }

        snapshot.player_combat = PlayerCombatSnapshot{
            .health = player_combat.health,
            .weapon_cooldown_remaining_seconds = player_combat.weapon_cooldown_remaining_seconds,
            .hurt_flash_remaining_seconds = player_combat.hurt_flash_remaining_seconds,
            .respawn_remaining_seconds = player_combat.respawn_remaining_seconds,
            .alive = player_combat.alive,
            .shots_fired = player_combat.shots_fired,
            .shots_hit = player_combat.shots_hit,
            .kills = player_combat.kills,
            .deaths = player_combat.deaths,
            .damage_dealt = player_combat.damage_dealt,
            .damage_taken = player_combat.damage_taken,
            .last_damage_source = player_combat.last_damage_source,
        };

        return snapshot;
    }

    auto restore_runtime_level_snapshot(
        const RuntimeLevelSnapshot& snapshot,
        const std::string_view level_path,
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
    ) -> SnapshotRestoreResult
    {
        SnapshotRestoreResult result{};

        if (snapshot.level_path.empty() || snapshot.level_path != level_path)
        {
            result.error = "Snapshot level does not match the current session";
            return result;
        }
        if (snapshot.registry_signature == 0 || snapshot.registry_signature != registry.signature())
        {
            result.error = "Snapshot registry signature does not match the current runtime model";
            return result;
        }

        std::vector<RuntimeDoor> staged_doors = doors;
        std::vector<RuntimeTrigger> staged_triggers = triggers;
        std::vector<RuntimeUsable> staged_usables = usables;
        std::vector<RuntimeTurret> staged_turrets = turrets;
        for (RuntimeTrigger& trigger : staged_triggers)
        {
            trigger.was_overlapping = false;
            trigger.overlapping = false;
        }
        std::vector<std::string> staged_inventory(
            snapshot.inventory_item_ids.begin(),
            snapshot.inventory_item_ids.end()
        );
        std::vector<std::string> staged_activated(
            snapshot.activated_target_ids.begin(),
            snapshot.activated_target_ids.end()
        );
        std::vector<std::string> staged_signaled(
            snapshot.signaled_target_ids.begin(),
            snapshot.signaled_target_ids.end()
        );
        PlayerCombatState staged_player_combat = player_combat;
        MissionRuntime staged_mission = mission_runtime;

        if (!apply_required_snapshot_entries(snapshot.doors, doors.size(), "door", result.error)
            || !apply_required_snapshot_entries(snapshot.triggers, triggers.size(), "trigger", result.error)
            || !apply_required_snapshot_entries(snapshot.usables, usables.size(), "usable", result.error)
            || !apply_required_snapshot_entries(snapshot.turrets, turrets.size(), "turret", result.error))
        {
            return result;
        }

        for (const RuntimeDoorSnapshot& door_snapshot : snapshot.doors)
        {
            const RuntimeObjectHandle* handle = registry.find(door_snapshot.runtime_id);
            if (handle == nullptr || handle->kind != RuntimeObjectKind::Door
                || handle->storage_index >= staged_doors.size())
            {
                result.error = "Snapshot door id does not exist in the current runtime registry";
                return result;
            }
            staged_doors[handle->storage_index].open_fraction = door_snapshot.open_fraction;
            staged_doors[handle->storage_index].state = door_snapshot.state;
        }

        for (const RuntimeTriggerSnapshot& trigger_snapshot : snapshot.triggers)
        {
            const RuntimeObjectHandle* handle = registry.find(trigger_snapshot.runtime_id);
            if (handle == nullptr || handle->kind != RuntimeObjectKind::Trigger
                || handle->storage_index >= staged_triggers.size())
            {
                result.error = "Snapshot trigger id does not exist in the current runtime registry";
                return result;
            }
            staged_triggers[handle->storage_index].fired = trigger_snapshot.fired;
        }

        for (const RuntimeUsableSnapshot& usable_snapshot : snapshot.usables)
        {
            const RuntimeObjectHandle* handle = registry.find(usable_snapshot.runtime_id);
            if (handle == nullptr
                || (handle->kind != RuntimeObjectKind::Usable && handle->kind != RuntimeObjectKind::Pickup)
                || handle->storage_index >= staged_usables.size())
            {
                result.error = "Snapshot usable id does not exist in the current runtime registry";
                return result;
            }
            staged_usables[handle->storage_index].locked = usable_snapshot.locked;
            staged_usables[handle->storage_index].consumed = usable_snapshot.consumed;
        }

        for (std::size_t index = 0; index < snapshot.turrets.size(); ++index)
        {
            const RuntimeTurretSnapshot& turret_snapshot = snapshot.turrets[index];
            if (index >= staged_turrets.size())
            {
                result.error = "Snapshot turret index does not exist in the current runtime state";
                return result;
            }
            if (turret_snapshot.authored_entity_index != staged_turrets[index].authored_entity_index)
            {
                result.error = "Snapshot turret entity index does not match the current runtime state";
                return result;
            }
            staged_turrets[index].health = turret_snapshot.health;
            staged_turrets[index].cooldown_remaining_seconds = turret_snapshot.cooldown_remaining_seconds;
            staged_turrets[index].warmup_remaining_seconds = turret_snapshot.warmup_remaining_seconds;
            staged_turrets[index].enabled = turret_snapshot.enabled;
            staged_turrets[index].destroyed = turret_snapshot.destroyed;
            staged_turrets[index].warming_up = turret_snapshot.warming_up;
        }

        staged_player_combat.health = snapshot.player_combat.health;
        staged_player_combat.weapon_cooldown_remaining_seconds =
            snapshot.player_combat.weapon_cooldown_remaining_seconds;
        staged_player_combat.hurt_flash_remaining_seconds = snapshot.player_combat.hurt_flash_remaining_seconds;
        staged_player_combat.respawn_remaining_seconds = snapshot.player_combat.respawn_remaining_seconds;
        staged_player_combat.alive = snapshot.player_combat.alive;
        staged_player_combat.shots_fired = snapshot.player_combat.shots_fired;
        staged_player_combat.shots_hit = snapshot.player_combat.shots_hit;
        staged_player_combat.kills = snapshot.player_combat.kills;
        staged_player_combat.deaths = snapshot.player_combat.deaths;
        staged_player_combat.damage_dealt = snapshot.player_combat.damage_dealt;
        staged_player_combat.damage_taken = snapshot.player_combat.damage_taken;
        staged_player_combat.last_damage_source = snapshot.player_combat.last_damage_source;

        if (!staged_mission.restore(snapshot.mission, result.error))
        {
            return result;
        }

        doors = std::move(staged_doors);
        triggers = std::move(staged_triggers);
        usables = std::move(staged_usables);
        turrets = std::move(staged_turrets);
        inventory_item_ids = std::move(staged_inventory);
        activated_target_ids = std::move(staged_activated);
        signaled_target_ids = std::move(staged_signaled);
        player_combat = std::move(staged_player_combat);
        mission_runtime = std::move(staged_mission);

        result.restored = true;
        result.restored_doors = static_cast<std::uint32_t>(snapshot.doors.size());
        result.restored_triggers = static_cast<std::uint32_t>(snapshot.triggers.size());
        result.restored_usables = static_cast<std::uint32_t>(snapshot.usables.size());
        result.restored_turrets = static_cast<std::uint32_t>(snapshot.turrets.size());
        return result;
    }

    auto build_runtime_snapshot_summary(const RuntimeLevelSnapshot& snapshot) -> std::string
    {
        return std::format(
            "Snapshot saved: {} | {} doors | {} triggers | {} usables | {} turrets | {} items | player {} | tick {}",
            snapshot.level_path.empty() ? "<unknown>" : snapshot.level_path,
            snapshot.doors.size(),
            snapshot.triggers.size(),
            snapshot.usables.size(),
            snapshot.turrets.size(),
            snapshot.inventory_item_ids.size(),
            snapshot.player_combat.alive ? "alive" : "down",
            snapshot.simulation_tick
        );
    }

    auto build_runtime_restore_summary(const SnapshotRestoreResult& result) -> std::string
    {
        if (!result.restored)
        {
            return result.error.empty() ? std::string{"Snapshot restore failed"}
                                        : "Snapshot restore failed: " + result.error;
        }

        return std::format(
            "Snapshot restored: {} doors | {} triggers | {} usables | {} turrets",
            result.restored_doors,
            result.restored_triggers,
            result.restored_usables,
            result.restored_turrets
        );
    }
} // namespace RelayGame

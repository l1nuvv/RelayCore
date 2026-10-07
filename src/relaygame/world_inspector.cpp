#include "relaygame/world_inspector.h"

#include "relaycore/core/profiling.h"
#include "relaycore/debug/inspector_source_reference.h"
#include "relaycore/physics/collision.h"
#include "relaycore/physics/physics_world.h"
#include "relaycore/world/surface_material_utils.h"
#include "relaygame/static_props.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <glm/geometric.hpp>
#include <optional>
#include <string_view>

namespace RelayGame
{
    namespace
    {
        enum class PickKind : std::uint8_t {
            None = 0,
            Usable,
            Door,
            Trigger,
            Solid,
        };

        struct PickHit {
            PickKind kind = PickKind::None;
            std::size_t index = 0;
            float distance = 0.0F;
            RelayCore::physics::Aabb bounds{};
            std::uint32_t priority = 0;
        };

        [[nodiscard]] auto find_entity_property(const RelayCore::EntitySpawnData& entity, const std::string_view key)
            -> const std::string*
        {
            const auto found = entity.properties.find(std::string{key});
            return found == entity.properties.end() ? nullptr : &found->second;
        }

        [[nodiscard]] auto lowercase_copy(const std::string_view value) -> std::string
        {
            std::string normalized(value);
            std::ranges::transform(normalized, normalized.begin(), [](const unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return normalized;
        }

        [[nodiscard]] auto normalized_entity_kind(const RelayCore::EntitySpawnData& entity) -> std::string
        {
            if (const std::string* kind = find_entity_property(entity, "kind"); kind != nullptr && !kind->empty())
            {
                return lowercase_copy(*kind);
            }

            return lowercase_copy(entity.classname);
        }

        [[nodiscard]] auto try_get_entity_by_index(const RelayCore::BlockoutLevel& level, const int entity_index)
            -> const RelayCore::EntitySpawnData*
        {
            if (entity_index < 0)
            {
                return nullptr;
            }

            const std::size_t index = static_cast<std::size_t>(entity_index);
            if (index >= level.entities.size())
            {
                return nullptr;
            }

            return &level.entities[index];
        }

        [[nodiscard]] auto entity_summary(const RelayCore::EntitySpawnData& entity, const std::size_t entity_index)
            -> std::string
        {
            const std::string kind = normalized_entity_kind(entity);
            const std::string* targetname = find_entity_property(entity, "targetname");
            if (targetname != nullptr && !targetname->empty())
            {
                return "entity[" + std::to_string(entity_index) + "] " + kind + " '" + *targetname + "'";
            }

            return "entity[" + std::to_string(entity_index) + "] " + kind;
        }

        void assign_entity_source(
            RelayCore::render::InspectorDiagnostics& diagnostics,
            const RelayCore::BlockoutLevel& level,
            const RelayCore::EntitySpawnData& entity,
            const std::size_t entity_index
        )
        {
            diagnostics.has_authored_source = true;
            diagnostics.authored_source_kind = "entity";
            diagnostics.authored_index = static_cast<int>(entity_index);
            diagnostics.authored_classname = entity.classname;
            diagnostics.authored_entity_kind = normalized_entity_kind(entity);
            if (const std::string* targetname = find_entity_property(entity, "targetname");
                targetname != nullptr && !targetname->empty())
            {
                diagnostics.authored_targetname = *targetname;
            }
            diagnostics.authored_summary = entity_summary(entity, entity_index);
            if (entity_index < level.entity_sources.size())
            {
                const RelayCore::AuthoredSourceLocation& source = level.entity_sources[entity_index];
                if (source.valid())
                {
                    diagnostics.has_authored_location = true;
                    diagnostics.authored_file_path = source.file_path;
                    diagnostics.authored_line = static_cast<int>(source.line);
                    diagnostics.authored_column = static_cast<int>(source.column);
                }
                if (source.has_placement())
                {
                    diagnostics.has_authored_placement_location = true;
                    diagnostics.authored_placement_kind =
                        source.source_kind == "direct" ? std::string{} : source.source_kind;
                    diagnostics.authored_template_name = source.template_name;
                    diagnostics.authored_instance_name = source.instance_name;
                    diagnostics.authored_placement_file_path = source.placement_file_path;
                    diagnostics.authored_placement_line = static_cast<int>(source.placement_line);
                    diagnostics.authored_placement_column = static_cast<int>(source.placement_column);
                }
            }
        }

        void assign_generated_source(
            RelayCore::render::InspectorDiagnostics& diagnostics,
            const std::string& summary,
            const std::string& targetname = {}
        )
        {
            diagnostics.has_authored_source = true;
            diagnostics.authored_source_kind = "generated";
            diagnostics.authored_index = -1;
            diagnostics.authored_targetname = targetname;
            diagnostics.authored_summary = summary;
        }

        void assign_generated_source_from_entity(
            RelayCore::render::InspectorDiagnostics& diagnostics,
            const RelayCore::BlockoutLevel& level,
            const RelayCore::EntitySpawnData& entity,
            const std::size_t entity_index,
            const std::string& summary
        )
        {
            diagnostics.has_authored_source = true;
            diagnostics.authored_source_kind = "generated_from_entity";
            diagnostics.authored_index = static_cast<int>(entity_index);
            diagnostics.authored_classname = entity.classname;
            diagnostics.authored_entity_kind = normalized_entity_kind(entity);
            if (const std::string* targetname = find_entity_property(entity, "targetname");
                targetname != nullptr && !targetname->empty())
            {
                diagnostics.authored_targetname = *targetname;
            }
            diagnostics.authored_summary = summary;
            if (entity_index < level.entity_sources.size())
            {
                const RelayCore::AuthoredSourceLocation& source = level.entity_sources[entity_index];
                if (source.valid())
                {
                    diagnostics.has_authored_location = true;
                    diagnostics.authored_file_path = source.file_path;
                    diagnostics.authored_line = static_cast<int>(source.line);
                    diagnostics.authored_column = static_cast<int>(source.column);
                }
                if (source.has_placement())
                {
                    diagnostics.has_authored_placement_location = true;
                    diagnostics.authored_placement_kind =
                        source.source_kind == "direct" ? std::string{} : source.source_kind;
                    diagnostics.authored_template_name = source.template_name;
                    diagnostics.authored_instance_name = source.instance_name;
                    diagnostics.authored_placement_file_path = source.placement_file_path;
                    diagnostics.authored_placement_line = static_cast<int>(source.placement_line);
                    diagnostics.authored_placement_column = static_cast<int>(source.placement_column);
                }
            }
        }

        void assign_solid_source(
            RelayCore::render::InspectorDiagnostics& diagnostics,
            const RelayCore::BlockoutLevel& level,
            const std::size_t solid_index
        )
        {
            diagnostics.has_authored_source = true;
            diagnostics.authored_source_kind = "solid";
            diagnostics.authored_index = static_cast<int>(solid_index);
            diagnostics.authored_summary = "level.solid[" + std::to_string(solid_index) + "]";
            if (solid_index < level.solid_sources.size())
            {
                const RelayCore::AuthoredSourceLocation& source = level.solid_sources[solid_index];
                if (source.valid())
                {
                    diagnostics.has_authored_location = true;
                    diagnostics.authored_file_path = source.file_path;
                    diagnostics.authored_line = static_cast<int>(source.line);
                    diagnostics.authored_column = static_cast<int>(source.column);
                }
                if (source.has_placement())
                {
                    diagnostics.has_authored_placement_location = true;
                    diagnostics.authored_placement_kind =
                        source.source_kind == "direct" ? std::string{} : source.source_kind;
                    diagnostics.authored_template_name = source.template_name;
                    diagnostics.authored_instance_name = source.instance_name;
                    diagnostics.authored_placement_file_path = source.placement_file_path;
                    diagnostics.authored_placement_line = static_cast<int>(source.placement_line);
                    diagnostics.authored_placement_column = static_cast<int>(source.placement_column);
                }
            }
        }

        [[nodiscard]] auto bounds_center(const RelayCore::physics::Aabb& bounds) -> glm::vec3
        {
            return (bounds.mins + bounds.maxs) * 0.5F;
        }

        [[nodiscard]] auto material_label(const RelayCore::SurfaceMaterial material) -> const char*
        {
            return RelayCore::canonical_surface_material_name(material).data();
        }

        [[nodiscard]] auto usable_action_kind_label(const RelayCore::gameplay::UsableActionKind action_kind) -> const
            char*
        {
            switch (action_kind)
            {
                case RelayCore::gameplay::UsableActionKind::PickupItem:
                    return "pickup_item";
                case RelayCore::gameplay::UsableActionKind::SignalTarget:
                    return "signal_target";
                case RelayCore::gameplay::UsableActionKind::ActivateTarget:
                default:
                    return "activate_target";
            }
        }

        [[nodiscard]] auto trigger_action_kind_label(const RelayCore::gameplay::TriggerActionKind action_kind) -> const
            char*
        {
            switch (action_kind)
            {
                case RelayCore::gameplay::TriggerActionKind::SignalTarget:
                    return "signal_target";
                case RelayCore::gameplay::TriggerActionKind::ActivateTarget:
                default:
                    return "activate_target";
            }
        }

        [[nodiscard]] auto signal_action_label(const RelayCore::gameplay::UsableSignalAction signal_action) -> const
            char*
        {
            switch (signal_action)
            {
                case RelayCore::gameplay::UsableSignalAction::Unlock:
                    return "unlock";
                case RelayCore::gameplay::UsableSignalAction::Lock:
                    return "lock";
                case RelayCore::gameplay::UsableSignalAction::ToggleLock:
                    return "toggle_lock";
                case RelayCore::gameplay::UsableSignalAction::None:
                default:
                    return "none";
            }
        }

        [[nodiscard]] auto visual_kind_label(const RelayCore::gameplay::UsableVisualKind visual_kind) -> const char*
        {
            switch (visual_kind)
            {
                case RelayCore::gameplay::UsableVisualKind::Panel:
                    return "panel";
                case RelayCore::gameplay::UsableVisualKind::Pickup:
                    return "pickup";
                case RelayCore::gameplay::UsableVisualKind::None:
                default:
                    return "none";
            }
        }

        [[nodiscard]] auto panel_mount_axis_label(const RelayCore::gameplay::PanelMountAxis axis) -> const char*
        {
            switch (axis)
            {
                case RelayCore::gameplay::PanelMountAxis::PositiveX:
                    return "+x";
                case RelayCore::gameplay::PanelMountAxis::NegativeX:
                    return "-x";
                case RelayCore::gameplay::PanelMountAxis::PositiveZ:
                    return "+z";
                case RelayCore::gameplay::PanelMountAxis::NegativeZ:
                    return "-z";
                case RelayCore::gameplay::PanelMountAxis::Centered:
                default:
                    return "centered";
            }
        }

        [[nodiscard]] auto door_state_label(const RelayCore::gameplay::DoorState state) -> const char*
        {
            switch (state)
            {
                case RelayCore::gameplay::DoorState::Opening:
                    return "opening";
                case RelayCore::gameplay::DoorState::Open:
                    return "open";
                case RelayCore::gameplay::DoorState::Closing:
                    return "closing";
                case RelayCore::gameplay::DoorState::Closed:
                default:
                    return "closed";
            }
        }

        [[nodiscard]] auto usable_pick_bounds(const RelayCore::gameplay::RuntimeUsable& usable)
            -> RelayCore::physics::Aabb
        {
            if (usable.has_visual_bounds)
            {
                return usable.visual_bounds;
            }
            if (usable.has_focus_bounds)
            {
                return usable.focus_bounds;
            }

            constexpr glm::vec3 kFallbackHalfExtents{0.18F, 0.18F, 0.18F};
            return {
                .mins = usable.origin - kFallbackHalfExtents,
                .maxs = usable.origin + kFallbackHalfExtents,
            };
        }

        [[nodiscard]] auto pick_is_better(const PickHit& candidate, const std::optional<PickHit>& best) -> bool
        {
            if (!best.has_value())
            {
                return true;
            }

            constexpr float kDistanceEpsilon = 0.001F;
            if (candidate.distance < (best->distance - kDistanceEpsilon))
            {
                return true;
            }

            return std::abs(candidate.distance - best->distance) <= kDistanceEpsilon
                   && candidate.priority > best->priority;
        }

        void update_best_pick(
            const glm::vec3& origin,
            const glm::vec3& direction,
            const RelayCore::physics::Aabb& bounds,
            const float max_distance,
            const PickKind kind,
            const std::size_t index,
            const std::uint32_t priority,
            std::optional<PickHit>& best
        )
        {
            if (const std::optional<float> distance =
                    RelayCore::physics::ray_intersection_distance(origin, direction, bounds, max_distance))
            {
                const PickHit candidate{
                    .kind = kind,
                    .index = index,
                    .distance = *distance,
                    .bounds = bounds,
                    .priority = priority,
                };
                if (pick_is_better(candidate, best))
                {
                    best = candidate;
                }
            }
        }

        void assign_runtime_handle(
            RelayCore::render::InspectorDiagnostics& diagnostics,
            const RelayGame::RuntimeObjectRegistry* registry,
            const PickKind kind,
            const std::size_t storage_index
        )
        {
            if (registry == nullptr)
            {
                return;
            }

            const RelayGame::RuntimeObjectHandle* handle = nullptr;
            switch (kind)
            {
                case PickKind::Door:
                    handle = registry->find_door(storage_index);
                    break;
                case PickKind::Trigger:
                    handle = registry->find_trigger(storage_index);
                    break;
                case PickKind::Usable:
                    handle = registry->find_usable(storage_index);
                    break;
                case PickKind::Solid:
                case PickKind::None:
                default:
                    break;
            }

            if (handle == nullptr)
            {
                return;
            }

            diagnostics.has_runtime_handle = true;
            diagnostics.runtime_id = handle->id;
            diagnostics.runtime_handle = handle->debug_handle;
        }

        [[nodiscard]] auto build_diagnostics_for_pick(
            const RelayCore::BlockoutLevel& level,
            std::span<const RelayCore::gameplay::RuntimeDoor> doors,
            std::span<const RelayCore::gameplay::RuntimeTrigger> triggers,
            std::span<const RelayCore::gameplay::RuntimeUsable> usables,
            const RelayGame::RuntimeObjectRegistry* registry,
            const PickHit& pick,
            const glm::vec3& eye_position,
            const glm::vec3& direction
        ) -> RelayCore::render::InspectorDiagnostics
        {
            RelayCore::render::InspectorDiagnostics diagnostics{};
            diagnostics.has_pick = true;
            diagnostics.distance = pick.distance;
            diagnostics.hit_point = eye_position + (direction * pick.distance);
            diagnostics.bounds_mins = pick.bounds.mins;
            diagnostics.bounds_maxs = pick.bounds.maxs;
            diagnostics.origin = bounds_center(pick.bounds);
            diagnostics.has_origin = true;
            assign_runtime_handle(diagnostics, registry, pick.kind, pick.index);

            switch (pick.kind)
            {
                case PickKind::Usable:
                {
                    const RelayCore::gameplay::RuntimeUsable& usable = usables[pick.index];
                    diagnostics.object_kind = "usable";
                    diagnostics.name = usable.name;
                    diagnostics.label = usable.label;
                    diagnostics.target = usable.target;
                    diagnostics.chain_target = usable.chain_target;
                    diagnostics.item_id = usable.item_id;
                    diagnostics.required_item = usable.required_item;
                    diagnostics.action_kind = usable_action_kind_label(usable.action_kind);
                    diagnostics.signal_action = signal_action_label(usable.signal_action);
                    diagnostics.visual_kind = visual_kind_label(usable.visual_kind);
                    diagnostics.panel_mount_axis = panel_mount_axis_label(usable.panel_mount_axis);
                    diagnostics.origin = usable.origin;
                    diagnostics.use_radius = usable.use_radius;
                    diagnostics.enabled = usable.enabled;
                    diagnostics.locked = usable.locked;
                    diagnostics.requirement_met = usable.requirement_met;
                    diagnostics.single_use = usable.single_use;
                    diagnostics.consumed = usable.consumed;
                    diagnostics.tracks_target_door = usable.tracks_target_door;
                    if (const RelayCore::EntitySpawnData* entity =
                            try_get_entity_by_index(level, usable.authored_entity_index);
                        entity != nullptr)
                    {
                        assign_entity_source(
                            diagnostics,
                            level,
                            *entity,
                            static_cast<std::size_t>(usable.authored_entity_index)
                        );
                    } else if (const RelayCore::EntitySpawnData* source_entity =
                                   try_get_entity_by_index(level, usable.generated_from_entity_index);
                               source_entity != nullptr)
                    {
                        assign_generated_source_from_entity(
                            diagnostics,
                            level,
                            *source_entity,
                            static_cast<std::size_t>(usable.generated_from_entity_index),
                            "generated auto-usable for door target '" + usable.target + "'"
                        );
                    } else if (usable.tracks_target_door && !usable.target.empty())
                    {
                        assign_generated_source(
                            diagnostics,
                            "generated auto-usable for door target '" + usable.target + "'",
                            usable.target
                        );
                    }
                    break;
                }
                case PickKind::Door:
                {
                    const RelayCore::gameplay::RuntimeDoor& door = doors[pick.index];
                    diagnostics.object_kind = "door";
                    diagnostics.name = door.name;
                    diagnostics.state = door_state_label(door.state);
                    diagnostics.door_open_fraction = door.open_fraction;
                    diagnostics.blocks_when_closed = door.blocks_when_closed;
                    if (const RelayCore::EntitySpawnData* entity =
                            try_get_entity_by_index(level, door.authored_entity_index);
                        entity != nullptr)
                    {
                        assign_entity_source(
                            diagnostics,
                            level,
                            *entity,
                            static_cast<std::size_t>(door.authored_entity_index)
                        );
                    }
                    break;
                }
                case PickKind::Trigger:
                {
                    const RelayCore::gameplay::RuntimeTrigger& trigger = triggers[pick.index];
                    diagnostics.object_kind = "trigger";
                    diagnostics.name = trigger.name;
                    diagnostics.target = trigger.target;
                    diagnostics.action_kind = trigger_action_kind_label(trigger.action_kind);
                    diagnostics.once = trigger.once;
                    diagnostics.fired = trigger.fired;
                    if (const RelayCore::EntitySpawnData* entity =
                            try_get_entity_by_index(level, trigger.authored_entity_index);
                        entity != nullptr)
                    {
                        assign_entity_source(
                            diagnostics,
                            level,
                            *entity,
                            static_cast<std::size_t>(trigger.authored_entity_index)
                        );
                    }
                    break;
                }
                case PickKind::Solid:
                {
                    const RelayCore::BlockoutSolid& solid = level.solids[pick.index];
                    diagnostics.object_kind = "solid";
                    diagnostics.material = material_label(solid.material);
                    diagnostics.brightness = solid.brightness;
                    assign_solid_source(diagnostics, level, pick.index);
                    break;
                }
                case PickKind::None:
                default:
                    break;
            }

            return diagnostics;
        }
    } // namespace

    auto build_collision_inspector_diagnostics(
        const RelayCore::BlockoutLevel& level,
        const RelayCore::physics::WorldQueryHit& hit,
        const RelayCore::physics::Aabb& bounds
    ) -> RelayCore::render::InspectorDiagnostics
    {
        using namespace RelayCore::physics;
        RelayCore::render::InspectorDiagnostics result;
        result.has_pick = true;
        result.has_collision_hit = true;
        result.distance = hit.distance;
        result.hit_point = hit.position;
        result.hit_normal = hit.normal;
        result.bounds_mins = bounds.mins;
        result.bounds_maxs = bounds.maxs;
        result.material = material_label(hit.surface);
        result.collision_layer = query_layer(hit.object.kind) == WorldQueryMask::Doors ? "Doors" : "Static";
        result.object_kind = hit.object.kind == WorldObjectKind::CollisionMesh  ? "collision_mesh"
                             : hit.object.kind == WorldObjectKind::PropCollider ? "prop_collider"
                             : hit.object.kind == WorldObjectKind::Door         ? "door"
                                                                                : "solid";
        if (hit.object.kind == WorldObjectKind::Solid)
            assign_solid_source(result, level, hit.object.index);
        else if (const auto* entity = try_get_entity_by_index(level, hit.object.authored_entity_index))
        {
            assign_entity_source(result, level, *entity, static_cast<std::size_t>(hit.object.authored_entity_index));
            result.has_origin = true;
            result.origin = entity->origin;
            if (const auto* product = find_entity_property(*entity, "collision_asset"))
                result.collision_product = *product;
            if (const auto* model = find_entity_property(*entity, "model"))
                result.collision_model = *model;
        }
        return result;
    }

    auto build_world_inspector_diagnostics(
        const RelayCore::BlockoutLevel& level,
        std::span<const RelayCore::gameplay::RuntimeDoor> doors,
        std::span<const RelayCore::gameplay::RuntimeTrigger> triggers,
        std::span<const RelayCore::gameplay::RuntimeUsable> usables,
        const glm::vec3& eye_position,
        const glm::vec3& view_forward,
        const float max_distance
    ) -> RelayCore::render::InspectorDiagnostics
    {
        RELAYCORE_PROFILE_ZONE_N("WorldInspector::build");

        RelayCore::render::InspectorDiagnostics diagnostics{};
        const float view_length = glm::length(view_forward);
        if (view_length <= 0.0001F)
        {
            return diagnostics;
        }

        const glm::vec3 direction = view_forward / view_length;
        std::optional<PickHit> best_pick{};

        for (std::size_t usable_index = 0; usable_index < usables.size(); ++usable_index)
        {
            update_best_pick(
                eye_position,
                direction,
                usable_pick_bounds(usables[usable_index]),
                max_distance,
                PickKind::Usable,
                usable_index,
                4,
                best_pick
            );
        }

        for (std::size_t door_index = 0; door_index < doors.size(); ++door_index)
        {
            update_best_pick(
                eye_position,
                direction,
                RelayCore::gameplay::current_door_bounds(doors[door_index]),
                max_distance,
                PickKind::Door,
                door_index,
                3,
                best_pick
            );
        }

        for (std::size_t trigger_index = 0; trigger_index < triggers.size(); ++trigger_index)
        {
            update_best_pick(
                eye_position,
                direction,
                triggers[trigger_index].bounds,
                max_distance,
                PickKind::Trigger,
                trigger_index,
                2,
                best_pick
            );
        }

        for (std::size_t solid_index = 0; solid_index < level.solids.size(); ++solid_index)
        {
            const RelayCore::physics::Aabb bounds{
                .mins = level.solids[solid_index].mins,
                .maxs = level.solids[solid_index].maxs,
            };
            update_best_pick(eye_position, direction, bounds, max_distance, PickKind::Solid, solid_index, 1, best_pick);
        }

        if (!best_pick.has_value())
        {
            return diagnostics;
        }

        return build_diagnostics_for_pick(
            level,
            doors,
            triggers,
            usables,
            nullptr,
            *best_pick,
            eye_position,
            direction
        );
    }

    auto build_world_inspector_diagnostics(
        const RelayCore::BlockoutLevel& level,
        std::span<const RelayCore::gameplay::RuntimeDoor> doors,
        std::span<const RelayCore::gameplay::RuntimeTrigger> triggers,
        std::span<const RelayCore::gameplay::RuntimeUsable> usables,
        const RelayGame::RuntimeObjectRegistry& registry,
        const glm::vec3& eye_position,
        const glm::vec3& view_forward,
        const float max_distance
    ) -> RelayCore::render::InspectorDiagnostics
    {
        RELAYCORE_PROFILE_ZONE_N("WorldInspector::build_with_registry");

        RelayCore::render::InspectorDiagnostics diagnostics{};
        const float view_length = glm::length(view_forward);
        if (view_length <= 0.0001F)
        {
            return diagnostics;
        }

        const glm::vec3 direction = view_forward / view_length;
        std::optional<PickHit> best_pick{};

        for (std::size_t usable_index = 0; usable_index < usables.size(); ++usable_index)
        {
            update_best_pick(
                eye_position,
                direction,
                usable_pick_bounds(usables[usable_index]),
                max_distance,
                PickKind::Usable,
                usable_index,
                4,
                best_pick
            );
        }

        for (std::size_t door_index = 0; door_index < doors.size(); ++door_index)
        {
            update_best_pick(
                eye_position,
                direction,
                RelayCore::gameplay::current_door_bounds(doors[door_index]),
                max_distance,
                PickKind::Door,
                door_index,
                3,
                best_pick
            );
        }

        for (std::size_t trigger_index = 0; trigger_index < triggers.size(); ++trigger_index)
        {
            update_best_pick(
                eye_position,
                direction,
                triggers[trigger_index].bounds,
                max_distance,
                PickKind::Trigger,
                trigger_index,
                2,
                best_pick
            );
        }

        for (std::size_t solid_index = 0; solid_index < level.solids.size(); ++solid_index)
        {
            const RelayCore::physics::Aabb bounds{
                .mins = level.solids[solid_index].mins,
                .maxs = level.solids[solid_index].maxs,
            };
            update_best_pick(eye_position, direction, bounds, max_distance, PickKind::Solid, solid_index, 1, best_pick);
        }

        if (!best_pick.has_value())
        {
            return diagnostics;
        }

        return build_diagnostics_for_pick(
            level,
            doors,
            triggers,
            usables,
            &registry,
            *best_pick,
            eye_position,
            direction
        );
    }

    auto find_world_inspector_diagnostics_by_authored_source(
        const RelayCore::BlockoutLevel& level,
        std::span<const RelayCore::gameplay::RuntimeDoor> doors,
        std::span<const RelayCore::gameplay::RuntimeTrigger> triggers,
        std::span<const RelayCore::gameplay::RuntimeUsable> usables,
        const std::string_view object_kind,
        const std::string_view primary_source_reference,
        const glm::vec3& eye_position,
        const RelayCore::physics::PhysicsWorld* physics
    ) -> RelayCore::render::InspectorDiagnostics
    {
        RELAYCORE_PROFILE_ZONE_N("WorldInspector::find_by_authored_source");

        if (primary_source_reference.empty())
        {
            return {};
        }

        if (physics && (object_kind.empty() || object_kind == "collision_mesh"))
        {
            const auto props = build_static_props(level);
            for (std::size_t i = 0; i < props.size(); ++i)
            {
                const RelayCore::physics::WorldObjectId object{
                    RelayCore::physics::WorldObjectKind::CollisionMesh,
                    i,
                    props[i].authored_entity_index
                };
                const auto bounds = physics->object_bounds(object);
                if (!bounds)
                    continue;
                const auto center = bounds_center(*bounds);
                RelayCore::physics::WorldQueryHit hit{
                    .distance = glm::length(center - eye_position),
                    .position = center,
                    .object = object
                };
                auto diagnostics = build_collision_inspector_diagnostics(level, hit, *bounds);
                diagnostics.has_collision_hit = false;
                diagnostics.material.clear();
                if (RelayCore::debug::build_primary_inspector_source_reference(diagnostics) == primary_source_reference)
                {
                    diagnostics.pinned = true;
                    return diagnostics;
                }
            }
        }

        for (std::size_t usable_index = 0; usable_index < usables.size(); ++usable_index)
        {
            const RelayCore::physics::Aabb bounds = usable_pick_bounds(usables[usable_index]);
            const glm::vec3 center = bounds_center(bounds);
            const glm::vec3 delta = center - eye_position;
            const float distance = glm::length(delta);
            const glm::vec3 direction = distance > 0.0001F ? (delta / distance) : glm::vec3{0.0F, 0.0F, 1.0F};
            PickHit pick{
                .kind = PickKind::Usable,
                .index = usable_index,
                .distance = distance,
                .bounds = bounds,
                .priority = 4,
            };
            RelayCore::render::InspectorDiagnostics diagnostics =
                build_diagnostics_for_pick(level, doors, triggers, usables, nullptr, pick, eye_position, direction);
            if (!object_kind.empty() && diagnostics.object_kind != object_kind)
            {
                continue;
            }
            if (RelayCore::debug::build_primary_inspector_source_reference(diagnostics) == primary_source_reference)
            {
                diagnostics.pinned = true;
                return diagnostics;
            }
        }

        for (std::size_t door_index = 0; door_index < doors.size(); ++door_index)
        {
            const RelayCore::physics::Aabb bounds = RelayCore::gameplay::current_door_bounds(doors[door_index]);
            const glm::vec3 center = bounds_center(bounds);
            const glm::vec3 delta = center - eye_position;
            const float distance = glm::length(delta);
            const glm::vec3 direction = distance > 0.0001F ? (delta / distance) : glm::vec3{0.0F, 0.0F, 1.0F};
            PickHit pick{
                .kind = PickKind::Door,
                .index = door_index,
                .distance = distance,
                .bounds = bounds,
                .priority = 3,
            };
            RelayCore::render::InspectorDiagnostics diagnostics =
                build_diagnostics_for_pick(level, doors, triggers, usables, nullptr, pick, eye_position, direction);
            if (!object_kind.empty() && diagnostics.object_kind != object_kind)
            {
                continue;
            }
            if (RelayCore::debug::build_primary_inspector_source_reference(diagnostics) == primary_source_reference)
            {
                diagnostics.pinned = true;
                return diagnostics;
            }
        }

        for (std::size_t trigger_index = 0; trigger_index < triggers.size(); ++trigger_index)
        {
            const glm::vec3 center = bounds_center(triggers[trigger_index].bounds);
            const glm::vec3 delta = center - eye_position;
            const float distance = glm::length(delta);
            const glm::vec3 direction = distance > 0.0001F ? (delta / distance) : glm::vec3{0.0F, 0.0F, 1.0F};
            PickHit pick{
                .kind = PickKind::Trigger,
                .index = trigger_index,
                .distance = distance,
                .bounds = triggers[trigger_index].bounds,
                .priority = 2,
            };
            RelayCore::render::InspectorDiagnostics diagnostics =
                build_diagnostics_for_pick(level, doors, triggers, usables, nullptr, pick, eye_position, direction);
            if (!object_kind.empty() && diagnostics.object_kind != object_kind)
            {
                continue;
            }
            if (RelayCore::debug::build_primary_inspector_source_reference(diagnostics) == primary_source_reference)
            {
                diagnostics.pinned = true;
                return diagnostics;
            }
        }

        for (std::size_t solid_index = 0; solid_index < level.solids.size(); ++solid_index)
        {
            const RelayCore::physics::Aabb bounds{
                .mins = level.solids[solid_index].mins,
                .maxs = level.solids[solid_index].maxs,
            };
            const glm::vec3 center = bounds_center(bounds);
            const glm::vec3 delta = center - eye_position;
            const float distance = glm::length(delta);
            const glm::vec3 direction = distance > 0.0001F ? (delta / distance) : glm::vec3{0.0F, 0.0F, 1.0F};
            PickHit pick{
                .kind = PickKind::Solid,
                .index = solid_index,
                .distance = distance,
                .bounds = bounds,
                .priority = 1,
            };
            RelayCore::render::InspectorDiagnostics diagnostics =
                build_diagnostics_for_pick(level, doors, triggers, usables, nullptr, pick, eye_position, direction);
            if (!object_kind.empty() && diagnostics.object_kind != object_kind)
            {
                continue;
            }
            if (RelayCore::debug::build_primary_inspector_source_reference(diagnostics) == primary_source_reference)
            {
                diagnostics.pinned = true;
                return diagnostics;
            }
        }

        return {};
    }

    auto find_world_inspector_diagnostics_by_runtime_id(
        const RelayCore::BlockoutLevel& level,
        std::span<const RelayCore::gameplay::RuntimeDoor> doors,
        std::span<const RelayCore::gameplay::RuntimeTrigger> triggers,
        std::span<const RelayCore::gameplay::RuntimeUsable> usables,
        const RelayGame::RuntimeObjectRegistry& registry,
        const RelayGame::RuntimeObjectId runtime_id,
        const glm::vec3& eye_position
    ) -> RelayCore::render::InspectorDiagnostics
    {
        RELAYCORE_PROFILE_ZONE_N("WorldInspector::find_by_runtime_id");

        if (runtime_id == 0)
        {
            return {};
        }

        const RelayGame::RuntimeObjectHandle* handle = registry.find(runtime_id);
        if (handle == nullptr)
        {
            return {};
        }

        const glm::vec3 fallback_direction{0.0F, 0.0F, 1.0F};
        if (handle->kind == RelayGame::RuntimeObjectKind::Door && handle->storage_index < doors.size())
        {
            const RelayCore::physics::Aabb bounds =
                RelayCore::gameplay::current_door_bounds(doors[handle->storage_index]);
            const glm::vec3 center = bounds_center(bounds);
            const glm::vec3 delta = center - eye_position;
            const float distance = glm::length(delta);
            const glm::vec3 direction = distance > 0.0001F ? (delta / distance) : fallback_direction;
            RelayCore::render::InspectorDiagnostics diagnostics = build_diagnostics_for_pick(
                level,
                doors,
                triggers,
                usables,
                &registry,
                PickHit{
                    .kind = PickKind::Door,
                    .index = handle->storage_index,
                    .distance = distance,
                    .bounds = bounds,
                    .priority = 3
                },
                eye_position,
                direction
            );
            diagnostics.pinned = true;
            return diagnostics;
        }

        if (handle->kind == RelayGame::RuntimeObjectKind::Trigger && handle->storage_index < triggers.size())
        {
            const RelayCore::physics::Aabb bounds = triggers[handle->storage_index].bounds;
            const glm::vec3 center = bounds_center(bounds);
            const glm::vec3 delta = center - eye_position;
            const float distance = glm::length(delta);
            const glm::vec3 direction = distance > 0.0001F ? (delta / distance) : fallback_direction;
            RelayCore::render::InspectorDiagnostics diagnostics = build_diagnostics_for_pick(
                level,
                doors,
                triggers,
                usables,
                &registry,
                PickHit{
                    .kind = PickKind::Trigger,
                    .index = handle->storage_index,
                    .distance = distance,
                    .bounds = bounds,
                    .priority = 2,
                },
                eye_position,
                direction
            );
            diagnostics.pinned = true;
            return diagnostics;
        }

        if ((handle->kind == RelayGame::RuntimeObjectKind::Usable
             || handle->kind == RelayGame::RuntimeObjectKind::Pickup)
            && handle->storage_index < usables.size())
        {
            const RelayCore::physics::Aabb bounds = usable_pick_bounds(usables[handle->storage_index]);
            const glm::vec3 center = bounds_center(bounds);
            const glm::vec3 delta = center - eye_position;
            const float distance = glm::length(delta);
            const glm::vec3 direction = distance > 0.0001F ? (delta / distance) : fallback_direction;
            RelayCore::render::InspectorDiagnostics diagnostics = build_diagnostics_for_pick(
                level,
                doors,
                triggers,
                usables,
                &registry,
                PickHit{
                    .kind = PickKind::Usable,
                    .index = handle->storage_index,
                    .distance = distance,
                    .bounds = bounds,
                    .priority = 4,
                },
                eye_position,
                direction
            );
            diagnostics.pinned = true;
            return diagnostics;
        }

        return {};
    }
} // namespace RelayGame

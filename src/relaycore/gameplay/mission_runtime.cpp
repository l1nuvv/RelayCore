#include "relaycore/gameplay/mission_runtime.h"

#include "relaycore/world/blockout_level.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace RelayCore::gameplay
{
    namespace
    {
        [[nodiscard]] auto find_mission_property(const EntitySpawnData& entity, std::string_view key)
            -> const std::string*
        {
            const auto it = entity.properties.find(std::string{key});
            if (it == entity.properties.end())
            {
                return nullptr;
            }
            return &it->second;
        }

        [[nodiscard]] auto
        mission_property_or(const EntitySpawnData& entity, std::string_view key, std::string fallback) -> std::string
        {
            if (const std::string* value = find_mission_property(entity, key); value != nullptr && !value->empty())
            {
                return *value;
            }

            return fallback;
        }

        [[nodiscard]] auto parse_mission_float_property(const EntitySpawnData& entity, std::string_view key)
            -> std::optional<float>
        {
            const std::string* value = find_mission_property(entity, key);
            if (value == nullptr || value->empty())
            {
                return std::nullopt;
            }

            float result = 0.0F;
            const auto [ptr, ec] = std::from_chars(value->data(), value->data() + value->size(), result);
            if (ec != std::errc{} || ptr != value->data() + value->size())
            {
                return std::nullopt;
            }
            return result;
        }

        [[nodiscard]] auto parse_mission_vec3_property(const EntitySpawnData& entity, std::string_view key)
            -> std::optional<glm::vec3>
        {
            const std::string* value = find_mission_property(entity, key);
            if (value == nullptr || value->empty())
            {
                return std::nullopt;
            }

            std::string normalized = *value;
            std::replace(normalized.begin(), normalized.end(), ',', ' ');

            std::istringstream stream(normalized);
            glm::vec3 result{};
            if (!(stream >> result.x >> result.y >> result.z))
            {
                return std::nullopt;
            }

            stream >> std::ws;
            if (!stream.eof())
            {
                return std::nullopt;
            }

            return result;
        }

        [[nodiscard]] auto parse_step_kind(const EntitySpawnData& entity) -> MissionStepKind
        {
            const std::string* kind = find_mission_property(entity, "kind");
            if (kind == nullptr || kind->empty())
            {
                throw std::runtime_error("mission_step.kind is required");
            }

            std::string normalized = *kind;
            std::ranges::transform(normalized, normalized.begin(), [](const unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });

            if (normalized == "door_open" || normalized == "door-open" || normalized == "door")
            {
                return MissionStepKind::DoorOpened;
            }
            if (normalized == "zone" || normalized == "enter_zone" || normalized == "enter-zone")
            {
                return MissionStepKind::EnterZone;
            }
            if (normalized == "pickup" || normalized == "pickup_item" || normalized == "pickup-item"
                || normalized == "collect_item" || normalized == "collect-item" || normalized == "item")
            {
                return MissionStepKind::AcquireItem;
            }
            if (normalized == "activate_target" || normalized == "activate-target" || normalized == "activate"
                || normalized == "signal")
            {
                return MissionStepKind::ActivateTarget;
            }

            throw std::runtime_error("Unsupported mission_step.kind value: " + *kind);
        }

        [[nodiscard]] auto parse_zone_bounds(const EntitySpawnData& entity) -> physics::Aabb
        {
            glm::vec3 size{
                parse_mission_float_property(entity, "width").value_or(2.4F),
                parse_mission_float_property(entity, "height").value_or(2.2F),
                parse_mission_float_property(entity, "depth").value_or(2.4F),
            };
            if (const auto compact_size = parse_mission_vec3_property(entity, "size"); compact_size.has_value())
            {
                size = *compact_size;
            }

            if (size.x <= 0.0F || size.y <= 0.0F || size.z <= 0.0F)
            {
                throw std::runtime_error("mission_step.size values must be positive");
            }

            const glm::vec3 half_extents = size * 0.5F;
            const glm::vec3 center = entity.origin + glm::vec3{0.0F, half_extents.y, 0.0F};
            return {center - half_extents, center + half_extents};
        }

        [[nodiscard]] auto aabb_overlaps(const physics::Aabb& a, const physics::Aabb& b) -> bool
        {
            return a.mins.x < b.maxs.x && a.maxs.x > b.mins.x && a.mins.y < b.maxs.y && a.maxs.y > b.mins.y
                   && a.mins.z < b.maxs.z && a.maxs.z > b.mins.z;
        }
    } // namespace

    MissionBuilder::MissionBuilder(const BlockoutLevel& level) noexcept : level_(level) {}

    auto MissionBuilder::build() const -> MissionDefinition
    {
        MissionDefinition definition{};

        for (const EntitySpawnData& entity : level_.entities)
        {
            if (entity.classname == "mission_settings")
            {
                definition.completion_title =
                    mission_property_or(entity, "completion_title", definition.completion_title);
                definition.completion_detail =
                    mission_property_or(entity, "completion_detail", definition.completion_detail);
                continue;
            }

            if (entity.classname != "mission_step")
            {
                continue;
            }

            MissionStep step{};
            step.kind = parse_step_kind(entity);
            step.title = mission_property_or(entity, "title", "Задача");
            step.detail = mission_property_or(entity, "detail", std::string{});

            if (step.kind == MissionStepKind::DoorOpened || step.kind == MissionStepKind::AcquireItem
                || step.kind == MissionStepKind::ActivateTarget)
            {
                step.target = mission_property_or(entity, "target", mission_property_or(entity, "item", std::string{}));
                if (step.target.empty())
                {
                    throw std::runtime_error(
                        "mission_step target is required for door_open, pickup and activate_target steps"
                    );
                }
            } else
            {
                step.bounds = parse_zone_bounds(entity);
            }

            definition.steps.push_back(std::move(step));
        }

        return definition;
    }

    void MissionRuntime::reset(MissionDefinition definition)
    {
        definition_ = std::move(definition);
        current_step_index_ = 0;
        completed_ = false;
    }

    void MissionRuntime::update(
        const physics::Aabb& player_bounds,
        const std::span<const RuntimeDoor> doors,
        const std::span<const std::string> inventory_items,
        const std::span<const std::string> activated_targets
    )
    {
        if (definition_.steps.empty() || completed_)
        {
            return;
        }

        while (current_step_index_ < definition_.steps.size()
               && current_step_complete(player_bounds, doors, inventory_items, activated_targets))
        {
            ++current_step_index_;
        }

        completed_ = current_step_index_ >= definition_.steps.size();
    }

    auto MissionRuntime::status() const -> MissionStatus
    {
        MissionStatus result{};
        result.active = !definition_.steps.empty();
        result.completed = completed_;
        result.total_steps = static_cast<std::uint32_t>(definition_.steps.size());

        if (!result.active)
        {
            return result;
        }

        if (completed_)
        {
            result.current_step = result.total_steps;
            result.title = definition_.completion_title;
            result.detail = definition_.completion_detail;
            return result;
        }

        result.current_step = static_cast<std::uint32_t>(current_step_index_ + 1);
        const MissionStep& step = definition_.steps[current_step_index_];
        result.title = step.title;
        result.detail = step.detail;
        return result;
    }

    auto MissionRuntime::completed() const noexcept -> bool
    {
        return completed_;
    }

    auto MissionRuntime::current_step_complete(
        const physics::Aabb& player_bounds,
        const std::span<const RuntimeDoor> doors,
        const std::span<const std::string> inventory_items,
        const std::span<const std::string> activated_targets
    ) const -> bool
    {
        if (current_step_index_ >= definition_.steps.size())
        {
            return true;
        }

        const MissionStep& step = definition_.steps[current_step_index_];
        if (step.kind == MissionStepKind::DoorOpened)
        {
            const auto it = std::ranges::find_if(doors, [&step](const RuntimeDoor& door) {
                return door.name == step.target;
            });
            return it != doors.end()
                   && (it->state == DoorState::Opening || it->state == DoorState::Open || it->open_fraction > 0.001F);
        }
        if (step.kind == MissionStepKind::AcquireItem)
        {
            return std::ranges::find(inventory_items, step.target) != inventory_items.end();
        }
        if (step.kind == MissionStepKind::ActivateTarget)
        {
            return std::ranges::find(activated_targets, step.target) != activated_targets.end();
        }

        return aabb_overlaps(player_bounds, step.bounds);
    }
} // namespace RelayCore::gameplay

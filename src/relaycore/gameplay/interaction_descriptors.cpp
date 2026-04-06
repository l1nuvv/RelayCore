#include "relaycore/gameplay/interaction_descriptors.h"

#include "relaycore/world/blockout_level.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <glm/common.hpp>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace RelayCore::gameplay
{
    namespace
    {
        enum class DescriptorEntityKind : std::uint8_t {
            Unknown,
            Door,
            Trigger,
            Usable,
            Panel,
        };

        enum class PanelMountAxis : std::uint8_t {
            Centered,
            PositiveX,
            NegativeX,
            PositiveZ,
            NegativeZ,
        };

        [[nodiscard]] auto make_centered_aabb(const glm::vec3& center, const glm::vec3& half_extents) -> physics::Aabb
        {
            return {center - half_extents, center + half_extents};
        }

        [[nodiscard]] auto find_property(const EntitySpawnData& entity, std::string_view key) -> const std::string*
        {
            const auto it = entity.properties.find(std::string{key});
            if (it == entity.properties.end())
            {
                return nullptr;
            }
            return &it->second;
        }

        [[nodiscard]] auto property_or(const EntitySpawnData& entity, std::string_view key, std::string fallback)
            -> std::string
        {
            if (const std::string* value = find_property(entity, key); value != nullptr && !value->empty())
            {
                return *value;
            }

            return fallback;
        }

        [[nodiscard]] auto parse_entity_kind(const EntitySpawnData& entity) -> DescriptorEntityKind
        {
            const std::string* kind = find_property(entity, "kind");
            if (kind == nullptr || kind->empty())
            {
                return DescriptorEntityKind::Unknown;
            }

            std::string normalized = *kind;
            std::ranges::transform(normalized, normalized.begin(), [](const unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });

            if (normalized == "door")
            {
                return DescriptorEntityKind::Door;
            }
            if (normalized == "trigger")
            {
                return DescriptorEntityKind::Trigger;
            }
            if (normalized == "usable")
            {
                return DescriptorEntityKind::Usable;
            }
            if (normalized == "panel" || normalized == "button")
            {
                return DescriptorEntityKind::Panel;
            }
            return DescriptorEntityKind::Unknown;
        }

        [[nodiscard]] auto parse_usable_visual_kind(const EntitySpawnData& entity, const DescriptorEntityKind kind)
            -> UsableVisualKind
        {
            if (kind == DescriptorEntityKind::Panel)
            {
                return UsableVisualKind::Panel;
            }

            const std::string* value = find_property(entity, "visual");
            if (value == nullptr || value->empty())
            {
                value = find_property(entity, "style");
            }
            if (value == nullptr || value->empty())
            {
                return UsableVisualKind::None;
            }

            std::string normalized = *value;
            std::ranges::transform(normalized, normalized.begin(), [](const unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });

            if (normalized == "panel" || normalized == "button")
            {
                return UsableVisualKind::Panel;
            }
            return UsableVisualKind::None;
        }

        [[nodiscard]] auto parse_float_property(const EntitySpawnData& entity, std::string_view key)
            -> std::optional<float>
        {
            const std::string* value = find_property(entity, key);
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

        [[nodiscard]] auto parse_vec3_property(const EntitySpawnData& entity, std::string_view key)
            -> std::optional<glm::vec3>
        {
            const std::string* value = find_property(entity, key);
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

        [[nodiscard]] auto parse_bool_property(const EntitySpawnData& entity, std::string_view key, const bool fallback)
            -> bool
        {
            const std::string* value = find_property(entity, key);
            if (value == nullptr)
            {
                return fallback;
            }

            std::string normalized = *value;
            std::ranges::transform(normalized, normalized.begin(), [](const unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });

            if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on")
            {
                return true;
            }
            if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off")
            {
                return false;
            }
            return fallback;
        }

        [[nodiscard]] auto parse_move_offset(const EntitySpawnData& entity, const glm::vec3& fallback) -> glm::vec3
        {
            if (const auto offset = parse_vec3_property(entity, "move_offset"); offset.has_value())
            {
                return *offset;
            }

            const auto x = parse_float_property(entity, "move_offset_x");
            const auto y = parse_float_property(entity, "move_offset_y");
            const auto z = parse_float_property(entity, "move_offset_z");
            if (!x.has_value() && !y.has_value() && !z.has_value())
            {
                return fallback;
            }

            return {
                x.value_or(0.0F),
                y.value_or(0.0F),
                z.value_or(0.0F),
            };
        }

        [[nodiscard]] auto parse_activation_policy(const EntitySpawnData& entity) -> DoorActivationPolicy
        {
            const std::string* value = find_property(entity, "activation_mode");
            if (value == nullptr || value->empty())
            {
                value = find_property(entity, "activation_policy");
            }
            if (value == nullptr || value->empty())
            {
                return DoorActivationPolicy::Toggle;
            }

            std::string normalized = *value;
            std::ranges::transform(normalized, normalized.begin(), [](const unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });

            if (normalized == "open_only" || normalized == "open-only" || normalized == "open")
            {
                return DoorActivationPolicy::OpenOnly;
            }
            return DoorActivationPolicy::Toggle;
        }

        [[nodiscard]] auto descriptor_bounds_width(const DoorDesc& door) -> float
        {
            return door.closed_bounds.maxs.x - door.closed_bounds.mins.x;
        }

        [[nodiscard]] auto descriptor_bounds_center(const physics::Aabb& bounds) -> glm::vec3
        {
            return (bounds.mins + bounds.maxs) * 0.5F;
        }

        [[nodiscard]] auto parse_panel_bounds(const EntitySpawnData& entity) -> physics::Aabb
        {
            constexpr float kWallMountClearance = 0.006F;

            glm::vec3 size{
                parse_float_property(entity, "width").value_or(0.18F),
                parse_float_property(entity, "height").value_or(0.42F),
                parse_float_property(entity, "depth")
                    .value_or(parse_float_property(entity, "thickness").value_or(0.12F)),
            };
            if (const auto compact_size = parse_vec3_property(entity, "size"); compact_size.has_value())
            {
                size = *compact_size;
            }

            const float width = std::max(size.x, 0.05F);
            const float height = std::max(size.y, 0.05F);
            const float thickness = std::max(size.z, 0.03F);
            const float half_width = width * 0.5F;
            const float half_height = height * 0.5F;
            const float half_thickness = thickness * 0.5F;

            const std::string* mount_axis_value = find_property(entity, "mount_axis");
            if (mount_axis_value == nullptr || mount_axis_value->empty())
            {
                mount_axis_value = find_property(entity, "panel_axis");
            }

            PanelMountAxis mount_axis = PanelMountAxis::Centered;
            if (mount_axis_value != nullptr && !mount_axis_value->empty())
            {
                std::string normalized = *mount_axis_value;
                std::ranges::transform(normalized, normalized.begin(), [](const unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });

                if (normalized == "x" || normalized == "+x" || normalized == "positive_x" || normalized == "pos_x")
                {
                    mount_axis = PanelMountAxis::PositiveX;
                } else if (normalized == "-x" || normalized == "negative_x" || normalized == "neg_x"
                           || normalized == "minus_x")
                {
                    mount_axis = PanelMountAxis::NegativeX;
                } else if (normalized == "z" || normalized == "+z" || normalized == "positive_z"
                           || normalized == "pos_z")
                {
                    mount_axis = PanelMountAxis::PositiveZ;
                } else if (normalized == "-z" || normalized == "negative_z" || normalized == "neg_z"
                           || normalized == "minus_z")
                {
                    mount_axis = PanelMountAxis::NegativeZ;
                } else
                {
                    throw std::runtime_error("Unsupported panel mount_axis value: " + *mount_axis_value);
                }
            }

            switch (mount_axis)
            {
                case PanelMountAxis::PositiveX:
                    return make_centered_aabb(
                        entity.origin + glm::vec3{half_thickness + kWallMountClearance, 0.0F, 0.0F},
                        glm::vec3{half_thickness, half_height, half_width}
                    );
                case PanelMountAxis::NegativeX:
                    return make_centered_aabb(
                        entity.origin - glm::vec3{half_thickness + kWallMountClearance, 0.0F, 0.0F},
                        glm::vec3{half_thickness, half_height, half_width}
                    );
                case PanelMountAxis::PositiveZ:
                    return make_centered_aabb(
                        entity.origin + glm::vec3{0.0F, 0.0F, half_thickness + kWallMountClearance},
                        glm::vec3{half_width, half_height, half_thickness}
                    );
                case PanelMountAxis::NegativeZ:
                    return make_centered_aabb(
                        entity.origin - glm::vec3{0.0F, 0.0F, half_thickness + kWallMountClearance},
                        glm::vec3{half_width, half_height, half_thickness}
                    );
                case PanelMountAxis::Centered:
                default:
                    return make_centered_aabb(entity.origin, glm::vec3{half_width, half_height, half_thickness});
            }
        }
    } // namespace

    InteractionDescriptorBuilder::InteractionDescriptorBuilder(const BlockoutLevel& level) noexcept : level_(level) {}

    auto InteractionDescriptorBuilder::build() const -> InteractionDescriptorSet
    {
        InteractionDescriptorSet descriptors{};
        descriptors.doors.reserve(level_.entities.size());
        descriptors.triggers.reserve(level_.entities.size());
        descriptors.usables.reserve(level_.entities.size());

        std::size_t door_index = 0;
        for (const EntitySpawnData& entity : level_.entities)
        {
            if (parse_entity_kind(entity) != DescriptorEntityKind::Door)
            {
                continue;
            }

            DoorDesc door{};
            door.name = property_or(entity, "targetname", "door_" + std::to_string(door_index++));

            glm::vec3 size{
                parse_float_property(entity, "width").value_or(1.0F),
                parse_float_property(entity, "height").value_or(2.0F),
                parse_float_property(entity, "thickness").value_or(0.20F),
            };
            if (const auto compact_size = parse_vec3_property(entity, "size"); compact_size.has_value())
            {
                size = *compact_size;
            }

            const float width = std::max(size.x, 0.1F);
            const float height = std::max(size.y, 0.1F);
            const float thickness = std::max(size.z, 0.05F);
            door.closed_bounds = make_centered_aabb(
                entity.origin + glm::vec3{0.0F, height * 0.5F, 0.0F},
                glm::vec3{width * 0.5F, height * 0.5F, thickness * 0.5F}
            );

            glm::vec3 open_offset{0.0F, 2.10F, 0.0F};
            if (const auto travel = parse_float_property(entity, "travel_distance"); travel.has_value())
            {
                open_offset.y = *travel;
            }
            door.open_offset = parse_move_offset(entity, open_offset);
            door.move_speed = std::max(parse_float_property(entity, "move_speed").value_or(1.5F), 0.01F);
            door.blocks_when_closed = parse_bool_property(entity, "blocks_when_closed", true);
            door.activation_policy = parse_activation_policy(entity);
            descriptors.doors.push_back(std::move(door));
        }

        std::size_t use_index = 0;
        std::size_t trigger_index = 0;
        for (const EntitySpawnData& entity : level_.entities)
        {
            const DescriptorEntityKind kind = parse_entity_kind(entity);
            if (kind == DescriptorEntityKind::Unknown || kind == DescriptorEntityKind::Door)
            {
                continue;
            }

            if (kind == DescriptorEntityKind::Usable || kind == DescriptorEntityKind::Panel)
            {
                UsableDesc usable{};
                const std::size_t target_index =
                    descriptors.doors.empty() ? 0 : std::min(use_index, descriptors.doors.size() - 1);
                usable.label = property_or(
                    entity,
                    "label",
                    property_or(entity, "message", kind == DescriptorEntityKind::Panel ? "Use panel" : "Use")
                );
                usable.origin = entity.origin;
                usable.use_radius = std::max(parse_float_property(entity, "use_radius").value_or(1.75F), 0.25F);
                usable.target = property_or(
                    entity,
                    "target",
                    descriptors.doors.empty() ? std::string{} : descriptors.doors[target_index].name
                );
                usable.visual_kind = parse_usable_visual_kind(entity, kind);
                usable.track_target_door = parse_bool_property(
                    entity,
                    "track_target_door",
                    usable.visual_kind == UsableVisualKind::Panel ? false : true
                );

                if (usable.visual_kind == UsableVisualKind::Panel)
                {
                    usable.visual_bounds = parse_panel_bounds(entity);
                    usable.has_visual_bounds = true;
                }

                descriptors.usables.push_back(std::move(usable));
                ++use_index;
                continue;
            }

            TriggerDesc trigger{};
            const std::size_t target_index =
                descriptors.doors.empty() ? 0 : std::min(trigger_index, descriptors.doors.size() - 1);
            trigger.name = property_or(entity, "targetname", "trigger_" + std::to_string(trigger_index));
            trigger.target = property_or(
                entity,
                "target",
                descriptors.doors.empty() ? std::string{} : descriptors.doors[target_index].name
            );

            glm::vec3 size{
                parse_float_property(entity, "width").value_or(1.5F),
                parse_float_property(entity, "height").value_or(2.0F),
                parse_float_property(entity, "depth").value_or(1.5F),
            };
            if (const auto compact_size = parse_vec3_property(entity, "size"); compact_size.has_value())
            {
                size = *compact_size;
            }

            const float width = std::max(size.x, 0.1F);
            const float height = std::max(size.y, 0.1F);
            const float depth = std::max(size.z, 0.1F);
            trigger.bounds = make_centered_aabb(
                entity.origin + glm::vec3{0.0F, height * 0.5F, 0.0F},
                glm::vec3{width * 0.5F, height * 0.5F, depth * 0.5F}
            );
            trigger.once = parse_bool_property(entity, "once", true);
            descriptors.triggers.push_back(std::move(trigger));
            ++trigger_index;
        }

        for (const DoorDesc& door : descriptors.doors)
        {
            const bool has_explicit_usable =
                std::ranges::any_of(descriptors.usables, [&door](const UsableDesc& usable) {
                    return usable.target == door.name;
                });
            if (has_explicit_usable)
            {
                continue;
            }

            UsableDesc usable{};
            usable.label = "Use";
            usable.target = door.name;
            usable.origin = descriptor_bounds_center(door.closed_bounds);
            usable.origin.y = std::clamp(usable.origin.y, door.closed_bounds.mins.y + 1.0F, door.closed_bounds.maxs.y);
            usable.use_radius = std::max(descriptor_bounds_width(door), 2.0F);
            usable.track_target_door = true;
            descriptors.usables.push_back(std::move(usable));
        }

        return descriptors;
    }
} // namespace RelayCore::gameplay

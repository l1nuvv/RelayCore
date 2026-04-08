#include "relaycore/world/blockout_loader_transform.h"

#include "relaycore/world/blockout_loader_parse.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <glm/common.hpp>
#include <sstream>
#include <stdexcept>
#include <string>

namespace RelayCore::detail
{
    auto parse_optional_anchor_name(const toml::table& table, std::string_view table_name) -> std::optional<std::string>
    {
        const toml::node* node = table.get("anchor");
        if (node == nullptr)
        {
            return std::nullopt;
        }

        const auto value = node->value<std::string>();
        if (!value.has_value() || value->empty())
        {
            throw std::runtime_error("Field '" + std::string(table_name) + ".anchor' must be a non-empty string");
        }

        return *value;
    }

    auto resolve_anchor_placement(
        const toml::table& table,
        std::string_view table_name,
        const std::unordered_map<std::string, AnchorPlacement>& anchors
    ) -> AnchorPlacement
    {
        const std::optional<std::string> anchor_name = parse_optional_anchor_name(table, table_name);
        if (!anchor_name.has_value())
        {
            return {};
        }

        const auto found = anchors.find(*anchor_name);
        if (found == anchors.end())
        {
            throw std::runtime_error("Unknown anchor reference: " + *anchor_name);
        }

        return found->second;
    }

    auto resolve_base_offset(
        const toml::table& table,
        std::string_view table_name,
        const std::unordered_map<std::string, AnchorPlacement>& anchors
    ) -> glm::vec3
    {
        return resolve_anchor_placement(table, table_name, anchors).origin + parse_optional_offset(table, table_name);
    }

    void translate_solid(BlockoutSolid& solid, const glm::vec3& offset)
    {
        solid.mins += offset;
        solid.maxs += offset;
    }

    auto parse_optional_rotation_y(const toml::table& table, std::string_view table_name) -> QuarterTurn
    {
        const auto rotation = parse_optional_float_field(table, "rotation_y", std::string(table_name) + ".rotation_y");
        if (!rotation.has_value())
        {
            return QuarterTurn::Deg0;
        }

        const long rounded = std::lround(*rotation);
        if (std::abs(*rotation - static_cast<float>(rounded)) > 0.001F)
        {
            throw std::runtime_error(
                "Field '" + std::string(table_name) + ".rotation_y' must be an integer multiple of 90 degrees"
            );
        }

        const int normalized = static_cast<int>((rounded % 360 + 360) % 360);
        switch (normalized)
        {
            case 0:
                return QuarterTurn::Deg0;
            case 90:
                return QuarterTurn::Deg90;
            case 180:
                return QuarterTurn::Deg180;
            case 270:
                return QuarterTurn::Deg270;
            default:
                throw std::runtime_error(
                    "Field '" + std::string(table_name) + ".rotation_y' must be one of: 0, 90, 180, 270"
                );
        }
    }

    auto quarter_turn_degrees(const QuarterTurn rotation) -> float
    {
        switch (rotation)
        {
            case QuarterTurn::Deg90:
                return 90.0F;
            case QuarterTurn::Deg180:
                return 180.0F;
            case QuarterTurn::Deg270:
                return 270.0F;
            case QuarterTurn::Deg0:
            default:
                return 0.0F;
        }
    }

    auto rotate_vec3_y(const glm::vec3& value, const QuarterTurn rotation) -> glm::vec3
    {
        switch (rotation)
        {
            case QuarterTurn::Deg90:
                return {value.z, value.y, -value.x};
            case QuarterTurn::Deg180:
                return {-value.x, value.y, -value.z};
            case QuarterTurn::Deg270:
                return {-value.z, value.y, value.x};
            case QuarterTurn::Deg0:
            default:
                return value;
        }
    }

    auto rotate_size_xz(const glm::vec3& value, const QuarterTurn rotation) -> glm::vec3
    {
        switch (rotation)
        {
            case QuarterTurn::Deg90:
            case QuarterTurn::Deg270:
                return {value.z, value.y, value.x};
            case QuarterTurn::Deg180:
            case QuarterTurn::Deg0:
            default:
                return value;
        }
    }

    auto rotate_bounds_y(const BlockoutSolid& solid, const QuarterTurn rotation, const glm::vec3& offset)
        -> BlockoutSolid
    {
        std::array<glm::vec3, 8> corners{
            glm::vec3{solid.mins.x, solid.mins.y, solid.mins.z},
            glm::vec3{solid.mins.x, solid.mins.y, solid.maxs.z},
            glm::vec3{solid.mins.x, solid.maxs.y, solid.mins.z},
            glm::vec3{solid.mins.x, solid.maxs.y, solid.maxs.z},
            glm::vec3{solid.maxs.x, solid.mins.y, solid.mins.z},
            glm::vec3{solid.maxs.x, solid.mins.y, solid.maxs.z},
            glm::vec3{solid.maxs.x, solid.maxs.y, solid.mins.z},
            glm::vec3{solid.maxs.x, solid.maxs.y, solid.maxs.z},
        };

        BlockoutSolid rotated = solid;
        glm::vec3 mins = rotate_vec3_y(corners.front(), rotation) + offset;
        glm::vec3 maxs = mins;

        for (const glm::vec3& corner : corners)
        {
            const glm::vec3 transformed = rotate_vec3_y(corner, rotation) + offset;
            mins = glm::min(mins, transformed);
            maxs = glm::max(maxs, transformed);
        }

        rotated.mins = mins;
        rotated.maxs = maxs;
        return rotated;
    }

    auto parse_property_vec3_value(std::string_view value, const std::string& field_name) -> glm::vec3
    {
        std::string normalized{value};
        std::replace(normalized.begin(), normalized.end(), ',', ' ');

        std::istringstream stream(normalized);
        glm::vec3 result{};
        if (!(stream >> result.x >> result.y >> result.z))
        {
            throw std::runtime_error("Field '" + field_name + "' must be a 3-component vector string");
        }

        stream >> std::ws;
        if (!stream.eof())
        {
            throw std::runtime_error("Field '" + field_name + "' must be a 3-component vector string");
        }

        return result;
    }

    auto serialize_vec3_property(const glm::vec3& value) -> std::string
    {
        return format_property_number(value.x) + " " + format_property_number(value.y) + " "
               + format_property_number(value.z);
    }

    auto rotate_panel_axis(std::string_view axis, const QuarterTurn rotation) -> std::string
    {
        std::string normalized{axis};
        std::ranges::transform(normalized, normalized.begin(), [](const unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        std::string canonical{};
        if (normalized == "x" || normalized == "+x" || normalized == "positive_x" || normalized == "pos_x")
        {
            canonical = "+x";
        } else if (normalized == "-x" || normalized == "negative_x" || normalized == "neg_x" || normalized == "minus_x")
        {
            canonical = "-x";
        } else if (normalized == "z" || normalized == "+z" || normalized == "positive_z" || normalized == "pos_z")
        {
            canonical = "+z";
        } else if (normalized == "-z" || normalized == "negative_z" || normalized == "neg_z" || normalized == "minus_z")
        {
            canonical = "-z";
        } else
        {
            throw std::runtime_error("Unsupported panel axis value in module template: " + std::string(axis));
        }

        switch (rotation)
        {
            case QuarterTurn::Deg90:
                if (canonical == "+x")
                    return "-z";
                if (canonical == "-x")
                    return "+z";
                if (canonical == "+z")
                    return "+x";
                return "-x";
            case QuarterTurn::Deg180:
                if (canonical == "+x")
                    return "-x";
                if (canonical == "-x")
                    return "+x";
                if (canonical == "+z")
                    return "-z";
                return "+z";
            case QuarterTurn::Deg270:
                if (canonical == "+x")
                    return "+z";
                if (canonical == "-x")
                    return "-z";
                if (canonical == "+z")
                    return "-x";
                return "+x";
            case QuarterTurn::Deg0:
            default:
                return canonical;
        }
    }

    auto property_kind_or_empty(const EntitySpawnData& entity) -> std::string_view
    {
        const auto found = entity.properties.find("kind");
        return found == entity.properties.end() ? std::string_view{} : std::string_view{found->second};
    }

    auto should_rotate_entity_size(const EntitySpawnData& entity) -> bool
    {
        const std::string_view kind = property_kind_or_empty(entity);
        return kind == "door" || kind == "trigger" || kind == "pickup" || kind == "zone";
    }

    auto property_or_empty(const EntitySpawnData& entity, std::string_view key) -> std::string_view
    {
        const auto found = entity.properties.find(std::string{key});
        return found == entity.properties.end() ? std::string_view{} : std::string_view{found->second};
    }

    auto rotate_module_entity(const EntitySpawnData& entity, const QuarterTurn rotation, const glm::vec3& offset)
        -> EntitySpawnData
    {
        EntitySpawnData transformed = entity;
        transformed.origin = rotate_vec3_y(entity.origin, rotation) + offset;
        transformed.angles = entity.angles;
        transformed.angles.y += quarter_turn_degrees(rotation);

        if (rotation == QuarterTurn::Deg0)
        {
            return transformed;
        }

        if (const auto found = transformed.properties.find("move_offset"); found != transformed.properties.end())
        {
            found->second = serialize_vec3_property(
                rotate_vec3_y(parse_property_vec3_value(found->second, "module_template.entity.move_offset"), rotation)
            );
        }

        if (const auto found = transformed.properties.find("offset"); found != transformed.properties.end())
        {
            found->second = serialize_vec3_property(
                rotate_vec3_y(parse_property_vec3_value(found->second, "module_template.entity.offset"), rotation)
            );
        }

        if (should_rotate_entity_size(transformed))
        {
            if (const auto found = transformed.properties.find("size"); found != transformed.properties.end())
            {
                found->second = serialize_vec3_property(
                    rotate_size_xz(parse_property_vec3_value(found->second, "module_template.entity.size"), rotation)
                );
            }
        }

        if (const auto found = transformed.properties.find("mount_axis"); found != transformed.properties.end())
        {
            found->second = rotate_panel_axis(found->second, rotation);
        }

        if (const auto found = transformed.properties.find("panel_axis"); found != transformed.properties.end())
        {
            found->second = rotate_panel_axis(found->second, rotation);
        }

        return transformed;
    }

    auto rotate_anchor_placement(
        const ModuleTemplateData::Anchor& anchor,
        const QuarterTurn rotation,
        const glm::vec3& offset
    ) -> AnchorPlacement
    {
        AnchorPlacement transformed{};
        transformed.origin = rotate_vec3_y(anchor.origin, rotation) + offset;
        transformed.angles = anchor.angles;
        transformed.angles.y += quarter_turn_degrees(rotation);
        return transformed;
    }

    auto build_module_name_remap(const ModuleTemplateData& module_template, const std::string& prefix)
        -> std::unordered_map<std::string, std::string>
    {
        std::unordered_map<std::string, std::string> remap{};
        if (prefix.empty())
        {
            return remap;
        }

        remap.reserve(module_template.entities.size() * 2);
        for (const EntitySpawnData& entity : module_template.entities)
        {
            if (const std::string_view targetname = property_or_empty(entity, "targetname"); !targetname.empty())
            {
                remap.emplace(std::string{targetname}, prefix + std::string{targetname});
            }

            if (property_kind_or_empty(entity) == "pickup")
            {
                if (const std::string_view item_id = property_or_empty(entity, "item_id"); !item_id.empty())
                {
                    remap.emplace(std::string{item_id}, prefix + std::string{item_id});
                }
            }
        }

        return remap;
    }

    void remap_property_value(
        EntitySpawnData& entity,
        std::string_view key,
        const std::unordered_map<std::string, std::string>& remap,
        const bool create_mapping_if_missing,
        const std::string& prefix
    )
    {
        const auto found = entity.properties.find(std::string{key});
        if (found == entity.properties.end() || found->second.empty())
        {
            return;
        }

        const auto mapped = remap.find(found->second);
        if (mapped != remap.end())
        {
            found->second = mapped->second;
            return;
        }

        if (create_mapping_if_missing && !prefix.empty())
        {
            found->second = prefix + found->second;
        }
    }

    auto apply_module_name_prefix(
        const EntitySpawnData& entity,
        const std::string& prefix,
        const std::unordered_map<std::string, std::string>& remap
    ) -> EntitySpawnData
    {
        if (prefix.empty())
        {
            return entity;
        }

        EntitySpawnData transformed = entity;
        remap_property_value(transformed, "targetname", remap, true, prefix);
        remap_property_value(transformed, "item_id", remap, true, prefix);
        remap_property_value(transformed, "target", remap);
        remap_property_value(transformed, "required_item", remap);
        return transformed;
    }
} // namespace RelayCore::detail

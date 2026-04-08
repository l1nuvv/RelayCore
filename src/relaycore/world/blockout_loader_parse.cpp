#include "relaycore/world/blockout_loader_parse.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace RelayCore::detail
{
    const toml::node& get_required_node(const toml::table& table, std::string_view key, std::string_view table_name)
    {
        const toml::node* node = table.get(key);
        if (node == nullptr)
        {
            throw std::runtime_error(
                "Missing required field '" + std::string(table_name) + "." + std::string(key) + "'"
            );
        }

        return *node;
    }

    glm::vec3 parse_vec3(const toml::node& node, std::string_view field_name)
    {
        const auto* array = node.as_array();
        if (array == nullptr || array->size() != 3)
        {
            throw std::runtime_error("Field '" + std::string(field_name) + "' must be an array of 3 numbers");
        }

        glm::vec3 result{};
        for (glm::length_t i = 0; i < 3; ++i)
        {
            const auto value = (*array)[i].value<double>();
            if (!value.has_value())
            {
                throw std::runtime_error("Field '" + std::string(field_name) + "' must contain only numbers");
            }

            result[i] = static_cast<float>(*value);
        }

        return result;
    }

    auto parse_optional_float_field(const toml::table& table, std::string_view key, std::string_view field_name)
        -> std::optional<float>
    {
        const toml::node* node = table.get(key);
        if (node == nullptr)
        {
            return std::nullopt;
        }

        const auto value = node->value<double>();
        if (!value.has_value())
        {
            throw std::runtime_error("Field '" + std::string(field_name) + "' must be a number");
        }

        return static_cast<float>(*value);
    }

    auto parse_optional_float(
        const toml::table& table,
        std::string_view key,
        std::string_view field_name,
        const float default_value
    ) -> float
    {
        return parse_optional_float_field(table, key, field_name).value_or(default_value);
    }

    SurfaceMaterial parse_surface_material(std::string_view value)
    {
        std::string normalized{value};
        std::ranges::transform(normalized, normalized.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        if (normalized == "concrete")
            return SurfaceMaterial::Concrete;
        if (normalized == "metal")
            return SurfaceMaterial::Metal;
        if (normalized == "tile")
            return SurfaceMaterial::Tile;
        if (normalized == "glass")
            return SurfaceMaterial::Glass;
        if (normalized == "hazard")
            return SurfaceMaterial::Hazard;
        if (normalized == "floor")
            return SurfaceMaterial::Floor;
        if (normalized == "ceiling")
            return SurfaceMaterial::Ceiling;
        if (normalized == "wall")
            return SurfaceMaterial::Wall;
        if (normalized == "wallpanel" || normalized == "wall_panel")
            return SurfaceMaterial::WallPanel;
        if (normalized == "corridor")
            return SurfaceMaterial::Corridor;
        if (normalized == "platform")
            return SurfaceMaterial::Platform;
        if (normalized == "platformtop" || normalized == "platform_top")
            return SurfaceMaterial::PlatformTop;
        if (normalized == "platformtop2" || normalized == "platform_top2")
            return SurfaceMaterial::PlatformTop2;
        if (normalized == "pillar")
            return SurfaceMaterial::Pillar;
        if (normalized == "cover")
            return SurfaceMaterial::Cover;
        if (normalized == "block")
            return SurfaceMaterial::Block;
        if (normalized == "beam")
            return SurfaceMaterial::Beam;
        if (normalized == "trim")
            return SurfaceMaterial::Trim;
        if (normalized == "divider")
            return SurfaceMaterial::Divider;
        if (normalized == "lightstrip" || normalized == "light_strip")
            return SurfaceMaterial::LightStrip;
        if (normalized == "crate")
            return SurfaceMaterial::Crate;
        if (normalized == "post")
            return SurfaceMaterial::Post;
        if (normalized == "dev/floor")
            return SurfaceMaterial::Floor;
        if (normalized == "dev/ceiling")
            return SurfaceMaterial::Ceiling;
        if (normalized == "dev/wall")
            return SurfaceMaterial::Wall;
        if (normalized == "dev/wall_panel")
            return SurfaceMaterial::WallPanel;
        if (normalized == "dev/corridor")
            return SurfaceMaterial::Corridor;
        if (normalized == "dev/platform")
            return SurfaceMaterial::Platform;
        if (normalized == "dev/platform_top")
            return SurfaceMaterial::PlatformTop;
        if (normalized == "dev/platform_top2")
            return SurfaceMaterial::PlatformTop2;
        if (normalized == "dev/pillar")
            return SurfaceMaterial::Pillar;
        if (normalized == "dev/cover")
            return SurfaceMaterial::Cover;
        if (normalized == "dev/block")
            return SurfaceMaterial::Block;
        if (normalized == "dev/beam")
            return SurfaceMaterial::Beam;
        if (normalized == "dev/trim")
            return SurfaceMaterial::Trim;
        if (normalized == "dev/divider")
            return SurfaceMaterial::Divider;
        if (normalized == "dev/light_strip")
            return SurfaceMaterial::LightStrip;
        if (normalized == "dev/crate")
            return SurfaceMaterial::Crate;
        if (normalized == "dev/post")
            return SurfaceMaterial::Post;

        throw std::runtime_error("Unknown surface material: " + std::string(value));
    }

    auto parse_surface_material_field(const toml::node& node, std::string_view field_name) -> SurfaceMaterial
    {
        const auto material = node.value<std::string>();
        if (!material.has_value())
        {
            throw std::runtime_error("Field '" + std::string(field_name) + "' must be a string");
        }

        return parse_surface_material(*material);
    }

    auto parse_optional_surface_material(const toml::table& table, std::string_view key, std::string_view field_name)
        -> std::optional<SurfaceMaterial>
    {
        const toml::node* node = table.get(key);
        if (node == nullptr)
        {
            return std::nullopt;
        }

        return parse_surface_material_field(*node, field_name);
    }

    auto parse_optional_vec3(const toml::table& table, std::string_view key, std::string_view field_name)
        -> std::optional<glm::vec3>
    {
        const toml::node* node = table.get(key);
        if (node == nullptr)
        {
            return std::nullopt;
        }

        return parse_vec3(*node, field_name);
    }

    auto parse_required_vec3(const toml::table& table, std::string_view key, std::string_view table_name) -> glm::vec3
    {
        return parse_vec3(get_required_node(table, key, table_name), std::string(table_name) + "." + std::string(key));
    }

    auto format_property_number(const double value) -> std::string
    {
        std::ostringstream stream;
        stream << std::setprecision(6) << std::defaultfloat << value;
        return stream.str();
    }

    auto serialize_property_scalar(const toml::node& node, std::string_view field_name) -> std::string
    {
        if (const auto value = node.value<std::string>(); value.has_value())
            return *value;
        if (const auto value = node.value<bool>(); value.has_value())
            return *value ? "true" : "false";
        if (const auto value = node.value<std::int64_t>(); value.has_value())
            return std::to_string(*value);
        if (const auto value = node.value<double>(); value.has_value())
            return format_property_number(*value);

        throw std::runtime_error(
            "Field '" + std::string(field_name) + "' must be a string, number, boolean, or array of scalar values"
        );
    }

    auto serialize_property_value(const toml::node& node, std::string_view field_name) -> std::string
    {
        if (const toml::array* array = node.as_array(); array != nullptr)
        {
            std::string result{};
            bool first = true;
            for (const toml::node& item : *array)
            {
                if (!first)
                    result += ' ';
                result += serialize_property_scalar(item, field_name);
                first = false;
            }
            return result;
        }

        return serialize_property_scalar(node, field_name);
    }

    void append_solid_from_table(
        const toml::table& table,
        std::string_view table_name,
        const SolidDefaults* defaults,
        std::vector<BlockoutSolid>& solids
    )
    {
        BlockoutSolid out{};
        const bool has_mins = table.contains("mins");
        const bool has_maxs = table.contains("maxs");
        const bool has_origin = table.contains("origin");

        if (has_mins || has_maxs)
        {
            out.mins = parse_required_vec3(table, "mins", table_name);
            out.maxs = parse_required_vec3(table, "maxs", table_name);
        } else
        {
            if (!has_origin)
            {
                throw std::runtime_error(
                    "Solid entry '" + std::string(table_name) + "' requires either mins/maxs or origin/size"
                );
            }

            const glm::vec3 origin = parse_required_vec3(table, "origin", table_name);
            const std::optional<glm::vec3> explicit_size =
                parse_optional_vec3(table, "size", std::string(table_name) + ".size");
            const std::optional<glm::vec3> size =
                explicit_size.has_value() ? explicit_size : (defaults != nullptr ? defaults->size : std::nullopt);

            if (!size.has_value())
            {
                throw std::runtime_error(
                    "Solid entry '" + std::string(table_name) + "' requires size when using origin"
                );
            }

            if (size->x <= 0.0F || size->y <= 0.0F || size->z <= 0.0F)
            {
                throw std::runtime_error(
                    "Field '" + std::string(table_name) + ".size' must contain only positive numbers"
                );
            }

            const glm::vec3 half_extents = (*size) * 0.5F;
            out.mins = origin - half_extents;
            out.maxs = origin + half_extents;
        }

        if (const auto material =
                parse_optional_surface_material(table, "material", std::string(table_name) + ".material");
            material.has_value())
        {
            out.material = *material;
        } else if (defaults != nullptr && defaults->material.has_value())
        {
            out.material = *defaults->material;
        } else
        {
            throw std::runtime_error("Missing required field '" + std::string(table_name) + ".material'");
        }

        if (const auto tint = parse_optional_vec3(table, "tint", std::string(table_name) + ".tint"); tint.has_value())
        {
            out.tint = *tint;
        } else if (defaults != nullptr && defaults->tint.has_value())
        {
            out.tint = *defaults->tint;
        }

        out.brightness = defaults != nullptr && defaults->brightness.has_value() ? *defaults->brightness : 1.0F;
        out.brightness =
            parse_optional_float(table, "brightness", std::string(table_name) + ".brightness", out.brightness);
        solids.push_back(out);
    }

    void apply_entity_fields(const toml::table& table, std::string_view table_name, EntitySpawnData& out)
    {
        for (const auto& [key, value] : table)
        {
            if (key == "entries")
            {
                continue;
            }
            if ((key == "offset" || key == "anchor") && table_name == "entity_group")
            {
                continue;
            }

            const std::string key_name{key.str()};
            const std::string field_name = std::string(table_name) + "." + key_name;

            if (key == "classname")
            {
                const auto classname = value.value<std::string>();
                if (!classname.has_value())
                {
                    throw std::runtime_error("Field '" + field_name + "' must be a string");
                }

                out.classname = *classname;
                continue;
            }

            if (key == "origin")
            {
                out.origin = parse_vec3(value, field_name);
                continue;
            }

            if (key == "angles")
            {
                out.angles = parse_vec3(value, field_name);
                out.has_angles = true;
                continue;
            }

            out.properties[key_name] = serialize_property_value(value, field_name);
        }
    }

    auto parse_entity_table(const toml::table& table, std::string_view table_name) -> EntitySpawnData
    {
        EntitySpawnData out{};
        apply_entity_fields(table, table_name, out);
        if (out.classname.empty())
        {
            throw std::runtime_error("Missing required field '" + std::string(table_name) + ".classname'");
        }

        return out;
    }

    auto parse_required_string(const toml::table& table, std::string_view key, std::string_view table_name)
        -> std::string
    {
        const auto value = get_required_node(table, key, table_name).value<std::string>();
        if (!value.has_value())
        {
            throw std::runtime_error(
                "Field '" + std::string(table_name) + "." + std::string(key) + "' must be a string"
            );
        }

        return *value;
    }

    auto parse_optional_offset(const toml::table& table, std::string_view table_name) -> glm::vec3
    {
        if (const auto offset = parse_optional_vec3(table, "offset", std::string(table_name) + ".offset");
            offset.has_value())
        {
            return *offset;
        }

        return glm::vec3{0.0F};
    }
} // namespace RelayCore::detail

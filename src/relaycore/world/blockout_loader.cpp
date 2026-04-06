/**
 * @file src/relaycore/world/blockout_loader.cpp
 * @brief Blockout-level TOML loader implementation.
 */

#include "relaycore/world/blockout_loader.h"

#include "relaycore/core/log.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <toml++/toml.hpp>
#include <unordered_map>
#include <utility>

namespace RelayCore
{
    namespace
    {
        struct SolidDefaults {
            std::optional<SurfaceMaterial> material{};
            std::optional<glm::vec3> tint{};
            std::optional<glm::vec3> size{};
            std::optional<float> brightness{};
        };

        /**
         * @brief Returns a required TOML field or throws if it is missing.
         * @param table TOML table that should contain the field.
         * @param key Field name to look up.
         * @param table_name Human-readable table name used for diagnostics.
         * @return Referenced TOML node.
         */
        [[nodiscard]] const toml::node&
        get_required_node(const toml::table& table, std::string_view key, std::string_view table_name)
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

        /**
         * @brief Parses a three-component vector from a TOML array.
         * @param node TOML node to parse.
         * @param field_name Human-readable field name used for diagnostics.
         * @return Parsed vector.
         */
        [[nodiscard]] glm::vec3 parse_vec3(const toml::node& node, std::string_view field_name)
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

        [[nodiscard]] auto
        parse_optional_float_field(const toml::table& table, std::string_view key, std::string_view field_name)
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

        [[nodiscard]] auto parse_optional_float(
            const toml::table& table,
            std::string_view key,
            std::string_view field_name,
            const float default_value
        ) -> float
        {
            return parse_optional_float_field(table, key, field_name).value_or(default_value);
        }

        /**
         * @brief Converts a material name string into a SurfaceMaterial value.
         * @param value Material name read from TOML.
         * @return Parsed surface-material enum value.
         */
        [[nodiscard]] SurfaceMaterial parse_surface_material(std::string_view value)
        {
            std::string normalized{value};
            std::ranges::transform(normalized, normalized.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });

            // Generic names
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

            // New direct names
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

            // Dev-material aliases
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

        [[nodiscard]] auto parse_surface_material_field(const toml::node& node, std::string_view field_name)
            -> SurfaceMaterial
        {
            const auto material = node.value<std::string>();
            if (!material.has_value())
            {
                throw std::runtime_error("Field '" + std::string(field_name) + "' must be a string");
            }

            return parse_surface_material(*material);
        }

        [[nodiscard]] auto
        parse_optional_surface_material(const toml::table& table, std::string_view key, std::string_view field_name)
            -> std::optional<SurfaceMaterial>
        {
            const toml::node* node = table.get(key);
            if (node == nullptr)
            {
                return std::nullopt;
            }

            return parse_surface_material_field(*node, field_name);
        }

        [[nodiscard]] auto
        parse_optional_vec3(const toml::table& table, std::string_view key, std::string_view field_name)
            -> std::optional<glm::vec3>
        {
            const toml::node* node = table.get(key);
            if (node == nullptr)
            {
                return std::nullopt;
            }

            return parse_vec3(*node, field_name);
        }

        [[nodiscard]] auto
        parse_required_vec3(const toml::table& table, std::string_view key, std::string_view table_name) -> glm::vec3
        {
            return parse_vec3(
                get_required_node(table, key, table_name),
                std::string(table_name) + "." + std::string(key)
            );
        }

        [[nodiscard]] auto format_property_number(const double value) -> std::string
        {
            std::ostringstream stream;
            stream << std::setprecision(6) << std::defaultfloat << value;
            return stream.str();
        }

        [[nodiscard]] auto serialize_property_scalar(const toml::node& node, std::string_view field_name) -> std::string
        {
            if (const auto value = node.value<std::string>(); value.has_value())
            {
                return *value;
            }

            if (const auto value = node.value<bool>(); value.has_value())
            {
                return *value ? "true" : "false";
            }

            if (const auto value = node.value<std::int64_t>(); value.has_value())
            {
                return std::to_string(*value);
            }

            if (const auto value = node.value<double>(); value.has_value())
            {
                return format_property_number(*value);
            }

            throw std::runtime_error(
                "Field '" + std::string(field_name) + "' must be a string, number, boolean, or array of scalar values"
            );
        }

        [[nodiscard]] auto serialize_property_value(const toml::node& node, std::string_view field_name) -> std::string
        {
            if (const toml::array* array = node.as_array(); array != nullptr)
            {
                std::string result{};
                bool first = true;
                for (const toml::node& item : *array)
                {
                    if (!first)
                    {
                        result += ' ';
                    }
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

            if (const auto tint = parse_optional_vec3(table, "tint", std::string(table_name) + ".tint");
                tint.has_value())
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
                    continue;
                }

                out.properties[key_name] = serialize_property_value(value, field_name);
            }
        }

        [[nodiscard]] auto parse_entity_table(const toml::table& table, std::string_view table_name) -> EntitySpawnData
        {
            EntitySpawnData out{};
            apply_entity_fields(table, table_name, out);
            if (out.classname.empty())
            {
                throw std::runtime_error("Missing required field '" + std::string(table_name) + ".classname'");
            }

            return out;
        }

        [[nodiscard]] auto
        parse_required_string(const toml::table& table, std::string_view key, std::string_view table_name)
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

        [[nodiscard]] auto parse_optional_offset(const toml::table& table, std::string_view table_name) -> glm::vec3
        {
            if (const auto offset = parse_optional_vec3(table, "offset", std::string(table_name) + ".offset");
                offset.has_value())
            {
                return *offset;
            }

            return glm::vec3{0.0F};
        }

        void append_solid_entries(const toml::array& solids, std::vector<BlockoutSolid>& out_solids)
        {
            for (const toml::node& node : solids)
            {
                const toml::table* solid = node.as_table();
                if (solid == nullptr)
                {
                    throw std::runtime_error("Each [[solid]] entry must be a TOML table");
                }

                append_solid_from_table(*solid, "solid", nullptr, out_solids);
            }
        }

        void append_solid_group_entries(const toml::array& groups, std::vector<BlockoutSolid>& out_solids)
        {
            for (const toml::node& node : groups)
            {
                const toml::table* group = node.as_table();
                if (group == nullptr)
                {
                    throw std::runtime_error("Each [[solid_group]] entry must be a TOML table");
                }

                SolidDefaults defaults{};
                defaults.material = parse_optional_surface_material(*group, "material", "solid_group.material");
                defaults.tint = parse_optional_vec3(*group, "tint", "solid_group.tint");
                defaults.size = parse_optional_vec3(*group, "size", "solid_group.size");
                defaults.brightness = parse_optional_float_field(*group, "brightness", "solid_group.brightness");

                const toml::node* entries_node = group->get("entries");
                if (entries_node == nullptr)
                {
                    entries_node = group->get("boxes");
                }
                if (entries_node == nullptr)
                {
                    throw std::runtime_error("Field 'solid_group.entries' or 'solid_group.boxes' is required");
                }

                const toml::array* entries = entries_node->as_array();
                if (entries == nullptr)
                {
                    throw std::runtime_error("Field 'solid_group.entries' must be an array of tables");
                }

                for (const toml::node& entry_node : *entries)
                {
                    const toml::table* entry = entry_node.as_table();
                    if (entry == nullptr)
                    {
                        throw std::runtime_error("Each solid_group.entries entry must be a TOML table");
                    }

                    append_solid_from_table(*entry, "solid_group.entry", &defaults, out_solids);
                }
            }
        }

        void append_entity_entries(const toml::array& entities, std::vector<EntitySpawnData>& out_entities)
        {
            for (const toml::node& node : entities)
            {
                const toml::table* entity = node.as_table();
                if (entity == nullptr)
                {
                    throw std::runtime_error("Each [[entity]] entry must be a TOML table");
                }

                out_entities.push_back(parse_entity_table(*entity, "entity"));
            }
        }

        void append_entity_group_entries(const toml::array& groups, std::vector<EntitySpawnData>& out_entities)
        {
            for (const toml::node& node : groups)
            {
                const toml::table* group = node.as_table();
                if (group == nullptr)
                {
                    throw std::runtime_error("Each [[entity_group]] entry must be a TOML table");
                }

                const toml::array* entries = get_required_node(*group, "entries", "entity_group").as_array();
                if (entries == nullptr)
                {
                    throw std::runtime_error("Field 'entity_group.entries' must be an array of tables");
                }

                for (const toml::node& entry_node : *entries)
                {
                    const toml::table* entry = entry_node.as_table();
                    if (entry == nullptr)
                    {
                        throw std::runtime_error("Each entity_group.entries entry must be a TOML table");
                    }

                    EntitySpawnData out{};
                    apply_entity_fields(*group, "entity_group", out);
                    apply_entity_fields(*entry, "entity_group.entry", out);
                    if (out.classname.empty())
                    {
                        throw std::runtime_error("Missing required field 'entity_group.classname'");
                    }

                    out_entities.push_back(std::move(out));
                }
            }
        }

        void build_solid_templates(
            const toml::array& groups,
            std::unordered_map<std::string, std::vector<BlockoutSolid>>& templates
        )
        {
            for (const toml::node& node : groups)
            {
                const toml::table* solid_template = node.as_table();
                if (solid_template == nullptr)
                {
                    throw std::runtime_error("Each [[solid_template]] entry must be a TOML table");
                }

                const std::string name = parse_required_string(*solid_template, "name", "solid_template");
                if (name.empty())
                {
                    throw std::runtime_error("Field 'solid_template.name' must not be empty");
                }
                if (templates.contains(name))
                {
                    throw std::runtime_error("Duplicate solid template name: " + name);
                }

                SolidDefaults defaults{};
                defaults.material =
                    parse_optional_surface_material(*solid_template, "material", "solid_template.material");
                defaults.tint = parse_optional_vec3(*solid_template, "tint", "solid_template.tint");
                defaults.size = parse_optional_vec3(*solid_template, "size", "solid_template.size");
                defaults.brightness =
                    parse_optional_float_field(*solid_template, "brightness", "solid_template.brightness");

                const toml::node* entries_node = solid_template->get("entries");
                if (entries_node == nullptr)
                {
                    entries_node = solid_template->get("boxes");
                }
                if (entries_node == nullptr)
                {
                    throw std::runtime_error("Field 'solid_template.entries' or 'solid_template.boxes' is required");
                }

                const toml::array* entries = entries_node->as_array();
                if (entries == nullptr)
                {
                    throw std::runtime_error("Field 'solid_template.entries' must be an array of tables");
                }

                std::vector<BlockoutSolid> built_template{};
                built_template.reserve(entries->size());
                for (const toml::node& entry_node : *entries)
                {
                    const toml::table* entry = entry_node.as_table();
                    if (entry == nullptr)
                    {
                        throw std::runtime_error("Each solid_template.entries entry must be a TOML table");
                    }

                    append_solid_from_table(*entry, "solid_template.entry", &defaults, built_template);
                }

                templates.emplace(name, std::move(built_template));
            }
        }

        void append_solid_instances(
            const toml::array& instances,
            const std::unordered_map<std::string, std::vector<BlockoutSolid>>& templates,
            std::vector<BlockoutSolid>& out_solids
        )
        {
            for (const toml::node& node : instances)
            {
                const toml::table* instance = node.as_table();
                if (instance == nullptr)
                {
                    throw std::runtime_error("Each [[solid_instance]] entry must be a TOML table");
                }

                const std::string template_name = parse_required_string(*instance, "template", "solid_instance");
                const auto found = templates.find(template_name);
                if (found == templates.end())
                {
                    throw std::runtime_error("Unknown solid template: " + template_name);
                }

                const glm::vec3 offset = parse_optional_offset(*instance, "solid_instance");
                for (const BlockoutSolid& source : found->second)
                {
                    BlockoutSolid placed = source;
                    placed.mins += offset;
                    placed.maxs += offset;
                    out_solids.push_back(std::move(placed));
                }
            }
        }
    } // namespace

    BlockoutLevel load_blockout_level(const std::filesystem::path& path)
    {
        if (!std::filesystem::exists(path))
        {
            throw std::runtime_error("Blockout level file does not exist: " + path.string());
        }

        const toml::table data = toml::parse_file(path.string());

        BlockoutLevel level{};
        std::unordered_map<std::string, std::vector<BlockoutSolid>> solid_templates{};

        if (const toml::array* templates = data["solid_template"].as_array(); templates != nullptr)
        {
            build_solid_templates(*templates, solid_templates);
        }

        for (const auto& [key, value] : data)
        {
            const toml::array* entries = value.as_array();
            if (entries == nullptr)
            {
                continue;
            }

            if (key == "solid")
            {
                append_solid_entries(*entries, level.solids);
                continue;
            }

            if (key == "solid_group")
            {
                append_solid_group_entries(*entries, level.solids);
                continue;
            }

            if (key == "solid_instance")
            {
                append_solid_instances(*entries, solid_templates, level.solids);
                continue;
            }

            if (key == "entity")
            {
                append_entity_entries(*entries, level.entities);
                continue;
            }

            if (key == "entity_group")
            {
                append_entity_group_entries(*entries, level.entities);
            }
        }

        log::info(
            "Loaded blockout level '{}' ({} solids, {} entities)",
            path.string(),
            level.solids.size(),
            level.entities.size()
        );

        return level;
    }
} // namespace RelayCore

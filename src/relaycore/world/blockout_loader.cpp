/**
 * @file src/relaycore/world/blockout_loader.cpp
 * @brief Blockout-level TOML loader implementation.
 */

#include "relaycore/world/blockout_loader.h"

#include "relaycore/core/log.h"

#include <toml++/toml.hpp>

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace RelayCore
{
    namespace
    {
        /**
         * @brief Returns a required TOML field or throws if it is missing.
         * @param table TOML table that should contain the field.
         * @param key Field name to look up.
         * @param table_name Human-readable table name used for diagnostics.
         * @return Referenced TOML node.
         */
        [[nodiscard]] const toml::node& get_required_node(
            const toml::table& table,
            std::string_view   key,
            std::string_view   table_name)
        {
            const toml::node* node = table.get(key);
            if (node == nullptr)
            {
                throw std::runtime_error(
                    "Missing required field '" + std::string(table_name) + "." + std::string(key) + "'");
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

            for (std::size_t i = 0; i < 3; ++i)
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

        /**
         * @brief Converts a material name string into a SurfaceMaterial value.
         * @param value Material name read from TOML.
         * @return Parsed surface-material enum value.
         */
        [[nodiscard]] SurfaceMaterial parse_surface_material(std::string_view value)
        {
            if (value == "Concrete") return SurfaceMaterial::Concrete;
            if (value == "Metal") return SurfaceMaterial::Metal;
            if (value == "Tile") return SurfaceMaterial::Tile;
            if (value == "Glass") return SurfaceMaterial::Glass;
            if (value == "Hazard") return SurfaceMaterial::Hazard;

            throw std::runtime_error("Unknown surface material: " + std::string(value));
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

        if (const toml::array* solids = data["solid"].as_array(); solids != nullptr)
        {
            level.solids.reserve(solids->size());

            for (const toml::node& node: *solids)
            {
                const toml::table* solid = node.as_table();
                if (solid == nullptr) { throw std::runtime_error("Each [[solid]] entry must be a TOML table"); }

                BlockoutSolid out{};
                out.mins = parse_vec3(get_required_node(*solid, "mins", "solid"), "solid.mins");
                out.maxs = parse_vec3(get_required_node(*solid, "maxs", "solid"), "solid.maxs");

                const auto material = get_required_node(*solid, "material", "solid").value<std::string>();
                if (!material.has_value()) { throw std::runtime_error("Field 'solid.material' must be a string"); }

                out.material = parse_surface_material(*material);
                level.solids.push_back(out);
            }
        }

        if (const toml::array* entities = data["entity"].as_array(); entities != nullptr)
        {
            level.entities.reserve(entities->size());

            for (const toml::node& node: *entities)
            {
                const toml::table* entity = node.as_table();
                if (entity == nullptr) { throw std::runtime_error("Each [[entity]] entry must be a TOML table"); }

                EntitySpawnData out{};

                const auto classname = get_required_node(*entity, "classname", "entity").value<std::string>();
                if (!classname.has_value()) { throw std::runtime_error("Field 'entity.classname' must be a string"); }

                out.classname = *classname;

                if (const toml::node* origin = entity->get("origin"); origin != nullptr)
                {
                    out.origin = parse_vec3(*origin, "entity.origin");
                }

                if (const toml::node* angles = entity->get("angles"); angles != nullptr)
                {
                    out.angles = parse_vec3(*angles, "entity.angles");
                }

                for (const auto& [key, value]: *entity)
                {
                    if (key == "classname" || key == "origin" || key == "angles") { continue; }

                    if (const auto str = value.value<std::string>(); str.has_value())
                    {
                        out.properties.emplace(std::string{key.str()}, *str);
                    }
                }

                level.entities.push_back(std::move(out));
            }
        }

        log::info(
            "Loaded blockout level '{}' ({} solids, {} entities)",
            path.string(),
            level.solids.size(),
            level.entities.size());

        return level;
    }
} // namespace RelayCore

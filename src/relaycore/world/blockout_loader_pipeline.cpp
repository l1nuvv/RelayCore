#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/world/blockout_loader_detail.h"
#include "relaycore/world/blockout_loader_parse.h"
#include "relaycore/world/blockout_loader_templates.h"
#include "relaycore/world/blockout_loader_transform.h"
#include "relaycore/world/blockout_loader_validation.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <toml++/toml.hpp>
#include <unordered_map>
#include <utility>

namespace RelayCore::detail
{
    namespace
    {
        void append_solid_entries(
            const toml::array& solids,
            const std::unordered_map<std::string, AnchorPlacement>& anchors,
            std::vector<BlockoutSolid>& out_solids,
            std::vector<AuthoredSourceLocation>& out_solid_sources
        )
        {
            for (const toml::node& node : solids)
            {
                const toml::table* solid = node.as_table();
                if (solid == nullptr)
                {
                    throw std::runtime_error("Each [[solid]] entry must be a TOML table");
                }

                append_solid_from_table(*solid, "solid", nullptr, out_solids, &out_solid_sources);
                translate_solid(out_solids.back(), resolve_base_offset(*solid, "solid", anchors));
            }
        }

        void append_solid_group_entries(
            const toml::array& groups,
            const std::unordered_map<std::string, AnchorPlacement>& anchors,
            std::vector<BlockoutSolid>& out_solids,
            std::vector<AuthoredSourceLocation>& out_solid_sources
        )
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
                const glm::vec3 group_offset = resolve_base_offset(*group, "solid_group", anchors);

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

                    append_solid_from_table(*entry, "solid_group.entry", &defaults, out_solids, &out_solid_sources);
                    translate_solid(out_solids.back(), group_offset);
                }
            }
        }

        void append_entity_entries(
            const toml::array& entities,
            const std::unordered_map<std::string, AnchorPlacement>& anchors,
            std::vector<EntitySpawnData>& out_entities,
            std::vector<AuthoredSourceLocation>& out_entity_sources
        )
        {
            for (const toml::node& node : entities)
            {
                const toml::table* entity = node.as_table();
                if (entity == nullptr)
                {
                    throw std::runtime_error("Each [[entity]] entry must be a TOML table");
                }

                AuthoredSourceLocation source_location{};
                EntitySpawnData out = parse_entity_table(*entity, "entity", &source_location);
                out.origin += resolve_base_offset(*entity, "entity", anchors);
                out.properties.erase("anchor");
                out_entities.push_back(std::move(out));
                out_entity_sources.push_back(std::move(source_location));
            }
        }

        void append_entity_group_entries(
            const toml::array& groups,
            const std::unordered_map<std::string, AnchorPlacement>& anchors,
            std::vector<EntitySpawnData>& out_entities,
            std::vector<AuthoredSourceLocation>& out_entity_sources
        )
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

                const glm::vec3 group_offset = resolve_base_offset(*group, "entity_group", anchors);
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
                    out.origin += group_offset + resolve_base_offset(*entry, "entity_group.entry", anchors);
                    out.properties.erase("anchor");
                    if (out.classname.empty())
                    {
                        throw std::runtime_error("Missing required field 'entity_group.classname'");
                    }

                    out_entities.push_back(std::move(out));
                    out_entity_sources.push_back(make_authored_source_location(*entry));
                }
            }
        }

        void build_level_from_document(
            const toml::table& data,
            BlockoutLevel& level,
            std::unordered_map<std::string, AnchorPlacement>& anchors,
            std::unordered_map<std::string, SolidTemplateData>& solid_templates,
            std::unordered_map<std::string, ModuleTemplateData>& module_templates
        )
        {
            if (const toml::array* templates = data["solid_template"].as_array(); templates != nullptr)
            {
                build_solid_templates(*templates, solid_templates);
            }
            if (const toml::array* templates = data["module_template"].as_array(); templates != nullptr)
            {
                build_module_templates(*templates, module_templates);
            }

            if (const toml::array* entries = data["module_instance"].as_array(); entries != nullptr)
            {
                append_module_instances(
                    *entries,
                    module_templates,
                    anchors,
                    level.solids,
                    level.solid_sources,
                    level.entities,
                    level.entity_sources
                );
            }

            if (const toml::array* entries = data["solid_instance"].as_array(); entries != nullptr)
            {
                append_solid_instances(*entries, solid_templates, anchors, level.solids, level.solid_sources);
            }

            if (const toml::array* entries = data["solid"].as_array(); entries != nullptr)
            {
                append_solid_entries(*entries, anchors, level.solids, level.solid_sources);
            }

            if (const toml::array* entries = data["solid_group"].as_array(); entries != nullptr)
            {
                append_solid_group_entries(*entries, anchors, level.solids, level.solid_sources);
            }

            if (const toml::array* entries = data["entity"].as_array(); entries != nullptr)
            {
                append_entity_entries(*entries, anchors, level.entities, level.entity_sources);
            }

            if (const toml::array* entries = data["entity_group"].as_array(); entries != nullptr)
            {
                append_entity_group_entries(*entries, anchors, level.entities, level.entity_sources);
            }
        }
    } // namespace

    BlockoutLevel load_blockout_level_data(const std::filesystem::path& path)
    {
        RELAYCORE_PROFILE_ZONE_N("BlockoutLoader::load");
        if (!std::filesystem::exists(path))
        {
            throw std::runtime_error("Blockout level file does not exist: " + path.string());
        }

        try
        {
            toml::table data{};
            {
                RELAYCORE_PROFILE_ZONE_N("BlockoutLoader::parse");
                data = toml::parse_file(path.string());
            }

            BlockoutLevel level{};
            std::unordered_map<std::string, AnchorPlacement> anchors{};
            std::unordered_map<std::string, SolidTemplateData> solid_templates{};
            std::unordered_map<std::string, ModuleTemplateData> module_templates{};

            {
                RELAYCORE_PROFILE_ZONE_N("BlockoutLoader::build");
                build_level_from_document(data, level, anchors, solid_templates, module_templates);
            }
            if (level.solids.size() != level.solid_sources.size())
            {
                throw std::runtime_error("Internal loader error: solid source metadata is out of sync with solids");
            }
            if (level.entities.size() != level.entity_sources.size())
            {
                throw std::runtime_error("Internal loader error: entity source metadata is out of sync with entities");
            }
            {
                RELAYCORE_PROFILE_ZONE_N("BlockoutLoader::validate");
                validate_loaded_blockout_level(level);
            }

            log::info(
                "Loaded blockout level '{}' ({} solids, {} entities)",
                path.string(),
                level.solids.size(),
                level.entities.size()
            );

            return level;
        } catch (const toml::parse_error& error)
        {
            throw std::runtime_error(
                "Failed to parse blockout level '" + path.string() + "' at line "
                + std::to_string(error.source().begin.line) + ", column " + std::to_string(error.source().begin.column)
                + ": " + std::string{error.description()}
            );
        } catch (const std::exception& error)
        {
            throw std::runtime_error("Failed to load blockout level '" + path.string() + "': " + error.what());
        }
    }
} // namespace RelayCore::detail

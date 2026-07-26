#include "relaycore/world/blockout_loader_templates.h"

#include "relaycore/world/blockout_loader_parse.h"
#include "relaycore/world/blockout_loader_transform.h"

#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

namespace RelayCore::detail
{
    namespace
    {
        void
        validate_module_template_anchors(const ModuleTemplateData& module_template, const std::string& template_name)
        {
            std::unordered_set<std::string> anchor_names{};
            for (const ModuleTemplateData::Anchor& anchor : module_template.anchors)
            {
                if (!anchor_names.insert(anchor.name).second)
                {
                    throw std::runtime_error(
                        "Duplicate module template anchor name '" + anchor.name + "' in module template '"
                        + template_name + "'"
                    );
                }
            }
        }
    } // namespace

    void build_solid_templates(const toml::array& groups, std::unordered_map<std::string, SolidTemplateData>& templates)
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
            defaults.material = parse_optional_surface_material(*solid_template, "material", "solid_template.material");
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

            SolidTemplateData built_template{};
            built_template.solids.reserve(entries->size());
            built_template.solid_sources.reserve(entries->size());
            for (const toml::node& entry_node : *entries)
            {
                const toml::table* entry = entry_node.as_table();
                if (entry == nullptr)
                {
                    throw std::runtime_error("Each solid_template.entries entry must be a TOML table");
                }

                append_solid_from_table(
                    *entry,
                    "solid_template.entry",
                    &defaults,
                    built_template.solids,
                    &built_template.solid_sources
                );
            }

            templates.emplace(name, std::move(built_template));
        }
    }

    void
    build_module_templates(const toml::array& groups, std::unordered_map<std::string, ModuleTemplateData>& templates)
    {
        for (const toml::node& node : groups)
        {
            const toml::table* module_template = node.as_table();
            if (module_template == nullptr)
            {
                throw std::runtime_error("Each [[module_template]] entry must be a TOML table");
            }

            const std::string name = parse_required_string(*module_template, "name", "module_template");
            if (name.empty())
            {
                throw std::runtime_error("Field 'module_template.name' must not be empty");
            }
            if (templates.contains(name))
            {
                throw std::runtime_error("Duplicate module template name: " + name);
            }

            ModuleTemplateData built_template{};

            if (const toml::node* solids_node = module_template->get("solids"); solids_node != nullptr)
            {
                const toml::array* solids = solids_node->as_array();
                if (solids == nullptr)
                {
                    throw std::runtime_error("Field 'module_template.solids' must be an array of tables");
                }

                built_template.solids.reserve(solids->size());
                built_template.solid_sources.reserve(solids->size());
                for (const toml::node& entry_node : *solids)
                {
                    const toml::table* entry = entry_node.as_table();
                    if (entry == nullptr)
                    {
                        throw std::runtime_error("Each module_template.solids entry must be a TOML table");
                    }

                    append_solid_from_table(
                        *entry,
                        "module_template.solid",
                        nullptr,
                        built_template.solids,
                        &built_template.solid_sources
                    );
                }
            }

            if (const toml::node* entities_node = module_template->get("entities"); entities_node != nullptr)
            {
                const toml::array* entities = entities_node->as_array();
                if (entities == nullptr)
                {
                    throw std::runtime_error("Field 'module_template.entities' must be an array of tables");
                }

                built_template.entities.reserve(entities->size());
                built_template.entity_sources.reserve(entities->size());
                for (const toml::node& entry_node : *entities)
                {
                    const toml::table* entry = entry_node.as_table();
                    if (entry == nullptr)
                    {
                        throw std::runtime_error("Each module_template.entities entry must be a TOML table");
                    }

                    AuthoredSourceLocation source_location{};
                    built_template.entities.push_back(
                        parse_entity_table(*entry, "module_template.entity", &source_location)
                    );
                    built_template.entity_sources.push_back(std::move(source_location));
                }
            }

            if (const toml::node* anchors_node = module_template->get("anchors"); anchors_node != nullptr)
            {
                const toml::array* anchors = anchors_node->as_array();
                if (anchors == nullptr)
                {
                    throw std::runtime_error("Field 'module_template.anchors' must be an array of tables");
                }

                built_template.anchors.reserve(anchors->size());
                for (const toml::node& entry_node : *anchors)
                {
                    const toml::table* entry = entry_node.as_table();
                    if (entry == nullptr)
                    {
                        throw std::runtime_error("Each module_template.anchors entry must be a TOML table");
                    }

                    ModuleTemplateData::Anchor anchor{};
                    anchor.name = parse_required_string(*entry, "name", "module_template.anchor");
                    if (anchor.name.empty())
                    {
                        throw std::runtime_error("Field 'module_template.anchor.name' must not be empty");
                    }

                    anchor.origin = parse_required_vec3(*entry, "origin", "module_template.anchor");
                    anchor.angles = parse_optional_vec3(*entry, "angles", "module_template.anchor.angles")
                                        .value_or(glm::vec3{0.0F});
                    built_template.anchors.push_back(std::move(anchor));
                }
            }

            if (built_template.solids.empty() && built_template.entities.empty())
            {
                throw std::runtime_error("Module template '" + name + "' requires at least one solid or entity entry");
            }

            validate_module_template_anchors(built_template, name);
            templates.emplace(name, std::move(built_template));
        }
    }

    void append_solid_instances(
        const toml::array& instances,
        const std::unordered_map<std::string, SolidTemplateData>& templates,
        const std::unordered_map<std::string, AnchorPlacement>& anchors,
        std::vector<BlockoutSolid>& out_solids,
        std::vector<AuthoredSourceLocation>& out_solid_sources
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

            const glm::vec3 offset = resolve_base_offset(*instance, "solid_instance", anchors);
            out_solids.reserve(out_solids.size() + found->second.solids.size());
            out_solid_sources.reserve(out_solid_sources.size() + found->second.solid_sources.size());
            for (std::size_t source_index = 0; source_index < found->second.solids.size(); ++source_index)
            {
                BlockoutSolid placed = found->second.solids[source_index];
                placed.mins += offset;
                placed.maxs += offset;
                out_solids.push_back(placed);
                out_solid_sources.push_back(make_instanced_authored_source_location(
                    found->second.solid_sources[source_index],
                    *instance,
                    "solid_instance",
                    template_name
                ));
            }
        }
    }

    void append_module_instances(
        const toml::array& instances,
        const std::unordered_map<std::string, ModuleTemplateData>& templates,
        std::unordered_map<std::string, AnchorPlacement>& anchors,
        std::vector<BlockoutSolid>& out_solids,
        std::vector<AuthoredSourceLocation>& out_solid_sources,
        std::vector<EntitySpawnData>& out_entities,
        std::vector<AuthoredSourceLocation>& out_entity_sources
    )
    {
        for (const toml::node& node : instances)
        {
            const toml::table* instance = node.as_table();
            if (instance == nullptr)
            {
                throw std::runtime_error("Each [[module_instance]] entry must be a TOML table");
            }

            const std::string template_name = parse_required_string(*instance, "template", "module_instance");
            const auto found = templates.find(template_name);
            if (found == templates.end())
            {
                throw std::runtime_error("Unknown module template: " + template_name);
            }

            const glm::vec3 offset = resolve_base_offset(*instance, "module_instance", anchors);
            const QuarterTurn rotation = parse_optional_rotation_y(*instance, "module_instance");
            const auto instance_name_node = instance->get("name");
            const std::string instance_name = instance_name_node != nullptr
                                                  ? parse_required_string(*instance, "name", "module_instance")
                                                  : std::string{};
            const auto name_prefix_node = instance->get("name_prefix");
            const std::string name_prefix = name_prefix_node != nullptr
                                                ? parse_required_string(*instance, "name_prefix", "module_instance")
                                                : std::string{};
            const auto remap = build_module_name_remap(found->second, name_prefix);

            out_solids.reserve(out_solids.size() + found->second.solids.size());
            out_solid_sources.reserve(out_solid_sources.size() + found->second.solid_sources.size());
            for (std::size_t source_index = 0; source_index < found->second.solids.size(); ++source_index)
            {
                out_solids.push_back(rotate_bounds_y(found->second.solids[source_index], rotation, offset));
                out_solid_sources.push_back(make_instanced_authored_source_location(
                    found->second.solid_sources[source_index],
                    *instance,
                    "module_instance",
                    template_name,
                    instance_name
                ));
            }

            out_entities.reserve(out_entities.size() + found->second.entities.size());
            out_entity_sources.reserve(out_entity_sources.size() + found->second.entity_sources.size());
            for (std::size_t source_index = 0; source_index < found->second.entities.size(); ++source_index)
            {
                out_entities.push_back(apply_module_name_prefix(
                    rotate_module_entity(found->second.entities[source_index], rotation, offset),
                    name_prefix,
                    remap
                ));
                out_entity_sources.push_back(make_instanced_authored_source_location(
                    found->second.entity_sources[source_index],
                    *instance,
                    "module_instance",
                    template_name,
                    instance_name
                ));
            }

            if (!instance_name.empty())
            {
                for (const ModuleTemplateData::Anchor& source : found->second.anchors)
                {
                    const std::string anchor_key = instance_name + ":" + source.name;
                    if (anchors.contains(anchor_key))
                    {
                        throw std::runtime_error("Duplicate anchor placement: " + anchor_key);
                    }
                    anchors[anchor_key] = rotate_anchor_placement(source, rotation, offset);
                }
            }
        }
    }
} // namespace RelayCore::detail

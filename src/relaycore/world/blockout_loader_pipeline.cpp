#include "relaycore/core/log.h"
#include "relaycore/world/blockout_loader_detail.h"
#include "relaycore/world/blockout_loader_parse.h"
#include "relaycore/world/blockout_loader_transform.h"

#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>
#include <toml++/toml.hpp>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace RelayCore::detail
{
    namespace
    {
        [[nodiscard]] auto find_entity_property(const EntitySpawnData& entity, const std::string_view key)
            -> const std::string*
        {
            const auto found = entity.properties.find(std::string{key});
            return found == entity.properties.end() ? nullptr : &found->second;
        }

        [[nodiscard]] auto normalize_lower(std::string value) -> std::string
        {
            std::ranges::transform(value, value.begin(), [](const unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }

        [[nodiscard]] auto normalized_entity_kind(const EntitySpawnData& entity) -> std::string
        {
            if (const std::string* kind = find_entity_property(entity, "kind"); kind != nullptr)
            {
                return normalize_lower(*kind);
            }
            return {};
        }

        [[nodiscard]] auto normalized_entity_action(const EntitySpawnData& entity) -> std::string
        {
            if (const std::string* action = find_entity_property(entity, "action");
                action != nullptr && !action->empty())
            {
                return normalize_lower(*action);
            }
            if (const std::string* action = find_entity_property(entity, "action_kind");
                action != nullptr && !action->empty())
            {
                return normalize_lower(*action);
            }
            return {};
        }

        [[nodiscard]] auto is_signal_action(const EntitySpawnData& entity) -> bool
        {
            const std::string action = normalized_entity_action(entity);
            return action == "signal" || action == "signal_target" || action == "signal-target";
        }

        [[nodiscard]] auto entity_requires_resolved_target(const EntitySpawnData& entity) -> bool
        {
            const std::string kind = normalized_entity_kind(entity);
            if (kind == "trigger")
            {
                return true;
            }

            if (kind == "panel" || kind == "button" || kind == "terminal" || kind == "console" || kind == "usable")
            {
                return !is_signal_action(entity);
            }

            return false;
        }

        [[nodiscard]] auto entity_requires_non_empty_target(const EntitySpawnData& entity) -> bool
        {
            const std::string kind = normalized_entity_kind(entity);
            if (kind == "trigger")
            {
                return true;
            }

            if (kind == "panel" || kind == "button" || kind == "terminal" || kind == "console" || kind == "usable")
            {
                return true;
            }

            return false;
        }

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

        void validate_loaded_level(const BlockoutLevel& level)
        {
            constexpr float kMinExtent = 0.0001F;

            for (std::size_t solid_index = 0; solid_index < level.solids.size(); ++solid_index)
            {
                const BlockoutSolid& solid = level.solids[solid_index];
                const glm::vec3 extents = solid.maxs - solid.mins;
                if (extents.x <= kMinExtent || extents.y <= kMinExtent || extents.z <= kMinExtent)
                {
                    throw std::runtime_error(
                        "Degenerate solid at index " + std::to_string(solid_index) + " has non-positive extents"
                    );
                }
            }

            std::unordered_set<std::string> target_names{};
            for (const EntitySpawnData& entity : level.entities)
            {
                const std::string* targetname = find_entity_property(entity, "targetname");
                if (targetname == nullptr)
                {
                    continue;
                }
                if (targetname->empty())
                {
                    throw std::runtime_error("Entity property 'targetname' must not be empty");
                }
                if (!target_names.insert(*targetname).second)
                {
                    throw std::runtime_error("Duplicate entity targetname: " + *targetname);
                }
            }

            for (const EntitySpawnData& entity : level.entities)
            {
                const std::string kind = normalized_entity_kind(entity);
                const std::string* target = find_entity_property(entity, "target");

                if (entity_requires_non_empty_target(entity) && (target == nullptr || target->empty()))
                {
                    throw std::runtime_error(
                        "Entity of kind '" + (kind.empty() ? std::string{"unknown"} : kind)
                        + "' requires a non-empty target property"
                    );
                }

                if (entity_requires_resolved_target(entity) && target != nullptr && !target->empty()
                    && !target_names.contains(*target))
                {
                    throw std::runtime_error(
                        "Entity of kind '" + kind + "' references unknown target '" + *target + "'"
                    );
                }
            }
        }

        void append_solid_entries(
            const toml::array& solids,
            const std::unordered_map<std::string, AnchorPlacement>& anchors,
            std::vector<BlockoutSolid>& out_solids
        )
        {
            for (const toml::node& node : solids)
            {
                const toml::table* solid = node.as_table();
                if (solid == nullptr)
                {
                    throw std::runtime_error("Each [[solid]] entry must be a TOML table");
                }

                append_solid_from_table(*solid, "solid", nullptr, out_solids);
                translate_solid(out_solids.back(), resolve_base_offset(*solid, "solid", anchors));
            }
        }

        void append_solid_group_entries(
            const toml::array& groups,
            const std::unordered_map<std::string, AnchorPlacement>& anchors,
            std::vector<BlockoutSolid>& out_solids
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

                    append_solid_from_table(*entry, "solid_group.entry", &defaults, out_solids);
                    translate_solid(out_solids.back(), group_offset);
                }
            }
        }

        void append_entity_entries(
            const toml::array& entities,
            const std::unordered_map<std::string, AnchorPlacement>& anchors,
            std::vector<EntitySpawnData>& out_entities
        )
        {
            for (const toml::node& node : entities)
            {
                const toml::table* entity = node.as_table();
                if (entity == nullptr)
                {
                    throw std::runtime_error("Each [[entity]] entry must be a TOML table");
                }

                EntitySpawnData out = parse_entity_table(*entity, "entity");
                out.origin += resolve_base_offset(*entity, "entity", anchors);
                out.properties.erase("anchor");
                out_entities.push_back(std::move(out));
            }
        }

        void append_entity_group_entries(
            const toml::array& groups,
            const std::unordered_map<std::string, AnchorPlacement>& anchors,
            std::vector<EntitySpawnData>& out_entities
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

        void build_module_templates(
            const toml::array& groups,
            std::unordered_map<std::string, ModuleTemplateData>& templates
        )
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
                    for (const toml::node& entry_node : *solids)
                    {
                        const toml::table* entry = entry_node.as_table();
                        if (entry == nullptr)
                        {
                            throw std::runtime_error("Each module_template.solids entry must be a TOML table");
                        }

                        append_solid_from_table(*entry, "module_template.solid", nullptr, built_template.solids);
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
                    for (const toml::node& entry_node : *entities)
                    {
                        const toml::table* entry = entry_node.as_table();
                        if (entry == nullptr)
                        {
                            throw std::runtime_error("Each module_template.entities entry must be a TOML table");
                        }

                        built_template.entities.push_back(parse_entity_table(*entry, "module_template.entity"));
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
                    throw std::runtime_error(
                        "Module template '" + name + "' requires at least one solid or entity entry"
                    );
                }

                validate_module_template_anchors(built_template, name);
                templates.emplace(name, std::move(built_template));
            }
        }

        void append_solid_instances(
            const toml::array& instances,
            const std::unordered_map<std::string, std::vector<BlockoutSolid>>& templates,
            const std::unordered_map<std::string, AnchorPlacement>& anchors,
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

                const glm::vec3 offset = resolve_base_offset(*instance, "solid_instance", anchors);
                for (const BlockoutSolid& source : found->second)
                {
                    BlockoutSolid placed = source;
                    placed.mins += offset;
                    placed.maxs += offset;
                    out_solids.push_back(std::move(placed));
                }
            }
        }

        void append_module_instances(
            const toml::array& instances,
            const std::unordered_map<std::string, ModuleTemplateData>& templates,
            std::unordered_map<std::string, AnchorPlacement>& anchors,
            std::vector<BlockoutSolid>& out_solids,
            std::vector<EntitySpawnData>& out_entities
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
                for (const BlockoutSolid& source : found->second.solids)
                {
                    out_solids.push_back(rotate_bounds_y(source, rotation, offset));
                }

                out_entities.reserve(out_entities.size() + found->second.entities.size());
                for (const EntitySpawnData& source : found->second.entities)
                {
                    out_entities.push_back(
                        apply_module_name_prefix(rotate_module_entity(source, rotation, offset), name_prefix, remap)
                    );
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
    } // namespace

    BlockoutLevel load_blockout_level_data(const std::filesystem::path& path)
    {
        if (!std::filesystem::exists(path))
        {
            throw std::runtime_error("Blockout level file does not exist: " + path.string());
        }

        const toml::table data = toml::parse_file(path.string());

        BlockoutLevel level{};
        std::unordered_map<std::string, AnchorPlacement> anchors{};
        std::unordered_map<std::string, std::vector<BlockoutSolid>> solid_templates{};
        std::unordered_map<std::string, ModuleTemplateData> module_templates{};

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
            append_module_instances(*entries, module_templates, anchors, level.solids, level.entities);
        }

        if (const toml::array* entries = data["solid_instance"].as_array(); entries != nullptr)
        {
            append_solid_instances(*entries, solid_templates, anchors, level.solids);
        }

        if (const toml::array* entries = data["solid"].as_array(); entries != nullptr)
        {
            append_solid_entries(*entries, anchors, level.solids);
        }

        if (const toml::array* entries = data["solid_group"].as_array(); entries != nullptr)
        {
            append_solid_group_entries(*entries, anchors, level.solids);
        }

        if (const toml::array* entries = data["entity"].as_array(); entries != nullptr)
        {
            append_entity_entries(*entries, anchors, level.entities);
        }

        if (const toml::array* entries = data["entity_group"].as_array(); entries != nullptr)
        {
            append_entity_group_entries(*entries, anchors, level.entities);
        }

        validate_loaded_level(level);

        log::info(
            "Loaded blockout level '{}' ({} solids, {} entities)",
            path.string(),
            level.solids.size(),
            level.entities.size()
        );

        return level;
    }
} // namespace RelayCore::detail

#pragma once

#include "relaycore/world/blockout_loader_detail_types.h"

#include <string>
#include <toml++/toml.hpp>
#include <unordered_map>

namespace RelayCore::detail
{
    void
    build_solid_templates(const toml::array& groups, std::unordered_map<std::string, SolidTemplateData>& templates);

    void
    build_module_templates(const toml::array& groups, std::unordered_map<std::string, ModuleTemplateData>& templates);

    void append_solid_instances(
        const toml::array& instances,
        const std::unordered_map<std::string, SolidTemplateData>& templates,
        const std::unordered_map<std::string, AnchorPlacement>& anchors,
        std::vector<BlockoutSolid>& out_solids,
        std::vector<AuthoredSourceLocation>& out_solid_sources
    );

    void append_module_instances(
        const toml::array& instances,
        const std::unordered_map<std::string, ModuleTemplateData>& templates,
        std::unordered_map<std::string, AnchorPlacement>& anchors,
        std::vector<BlockoutSolid>& out_solids,
        std::vector<AuthoredSourceLocation>& out_solid_sources,
        std::vector<EntitySpawnData>& out_entities,
        std::vector<AuthoredSourceLocation>& out_entity_sources
    );
} // namespace RelayCore::detail

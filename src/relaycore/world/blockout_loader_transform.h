#pragma once

#include "relaycore/world/blockout_loader_detail_types.h"

#include <optional>
#include <string>
#include <string_view>
#include <toml++/toml.hpp>
#include <unordered_map>

namespace RelayCore::detail
{
    [[nodiscard]] auto parse_optional_anchor_name(const toml::table& table, std::string_view table_name)
        -> std::optional<std::string>;

    [[nodiscard]] auto resolve_anchor_placement(
        const toml::table& table,
        std::string_view table_name,
        const std::unordered_map<std::string, AnchorPlacement>& anchors
    ) -> AnchorPlacement;

    [[nodiscard]] auto resolve_base_offset(
        const toml::table& table,
        std::string_view table_name,
        const std::unordered_map<std::string, AnchorPlacement>& anchors
    ) -> glm::vec3;

    void translate_solid(BlockoutSolid& solid, const glm::vec3& offset);

    [[nodiscard]] auto parse_optional_rotation_y(const toml::table& table, std::string_view table_name) -> QuarterTurn;

    [[nodiscard]] auto quarter_turn_degrees(QuarterTurn rotation) -> float;

    [[nodiscard]] auto rotate_vec3_y(const glm::vec3& value, QuarterTurn rotation) -> glm::vec3;

    [[nodiscard]] auto rotate_size_xz(const glm::vec3& value, QuarterTurn rotation) -> glm::vec3;

    [[nodiscard]] auto rotate_bounds_y(const BlockoutSolid& solid, QuarterTurn rotation, const glm::vec3& offset)
        -> BlockoutSolid;

    [[nodiscard]] auto parse_property_vec3_value(std::string_view value, const std::string& field_name) -> glm::vec3;

    [[nodiscard]] auto serialize_vec3_property(const glm::vec3& value) -> std::string;

    [[nodiscard]] auto rotate_panel_axis(std::string_view axis, QuarterTurn rotation) -> std::string;

    [[nodiscard]] auto property_kind_or_empty(const EntitySpawnData& entity) -> std::string_view;

    [[nodiscard]] auto should_rotate_entity_size(const EntitySpawnData& entity) -> bool;

    [[nodiscard]] auto property_or_empty(const EntitySpawnData& entity, std::string_view key) -> std::string_view;

    [[nodiscard]] auto
    rotate_module_entity(const EntitySpawnData& entity, QuarterTurn rotation, const glm::vec3& offset)
        -> EntitySpawnData;

    [[nodiscard]] auto
    rotate_anchor_placement(const ModuleTemplateData::Anchor& anchor, QuarterTurn rotation, const glm::vec3& offset)
        -> AnchorPlacement;

    [[nodiscard]] auto build_module_name_remap(const ModuleTemplateData& module_template, const std::string& prefix)
        -> std::unordered_map<std::string, std::string>;

    void remap_property_value(
        EntitySpawnData& entity,
        std::string_view key,
        const std::unordered_map<std::string, std::string>& remap,
        bool create_mapping_if_missing = false,
        const std::string& prefix = {}
    );

    [[nodiscard]] auto apply_module_name_prefix(
        const EntitySpawnData& entity,
        const std::string& prefix,
        const std::unordered_map<std::string, std::string>& remap
    ) -> EntitySpawnData;
} // namespace RelayCore::detail

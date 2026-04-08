#pragma once

#include "relaycore/world/blockout_loader_detail_types.h"

#include <optional>
#include <string>
#include <string_view>
#include <toml++/toml.hpp>

namespace RelayCore::detail
{
    [[nodiscard]] const toml::node&
    get_required_node(const toml::table& table, std::string_view key, std::string_view table_name);

    [[nodiscard]] glm::vec3 parse_vec3(const toml::node& node, std::string_view field_name);

    [[nodiscard]] auto
    parse_optional_float_field(const toml::table& table, std::string_view key, std::string_view field_name)
        -> std::optional<float>;

    [[nodiscard]] auto parse_optional_float(
        const toml::table& table,
        std::string_view key,
        std::string_view field_name,
        float default_value
    ) -> float;

    [[nodiscard]] SurfaceMaterial parse_surface_material(std::string_view value);

    [[nodiscard]] auto parse_surface_material_field(const toml::node& node, std::string_view field_name)
        -> SurfaceMaterial;

    [[nodiscard]] auto
    parse_optional_surface_material(const toml::table& table, std::string_view key, std::string_view field_name)
        -> std::optional<SurfaceMaterial>;

    [[nodiscard]] auto parse_optional_vec3(const toml::table& table, std::string_view key, std::string_view field_name)
        -> std::optional<glm::vec3>;

    [[nodiscard]] auto parse_required_vec3(const toml::table& table, std::string_view key, std::string_view table_name)
        -> glm::vec3;

    [[nodiscard]] auto format_property_number(double value) -> std::string;

    [[nodiscard]] auto serialize_property_scalar(const toml::node& node, std::string_view field_name) -> std::string;

    [[nodiscard]] auto serialize_property_value(const toml::node& node, std::string_view field_name) -> std::string;

    void append_solid_from_table(
        const toml::table& table,
        std::string_view table_name,
        const SolidDefaults* defaults,
        std::vector<BlockoutSolid>& solids
    );

    void apply_entity_fields(const toml::table& table, std::string_view table_name, EntitySpawnData& out);

    [[nodiscard]] auto parse_entity_table(const toml::table& table, std::string_view table_name) -> EntitySpawnData;

    [[nodiscard]] auto
    parse_required_string(const toml::table& table, std::string_view key, std::string_view table_name) -> std::string;

    [[nodiscard]] auto parse_optional_offset(const toml::table& table, std::string_view table_name) -> glm::vec3;
} // namespace RelayCore::detail

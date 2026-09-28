/**
 * @file src/relaycore/world/blockout_level.h
 * @brief Lightweight blockout-level data structures loaded from TOML.
 */

#pragma once

#include <cstdint>
#include <glm/vec3.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace RelayCore
{
    /**
     * @brief Source location for an authored level entry.
     */
    struct AuthoredSourceLocation {
        std::string file_path{};            ///< TOML file path that produced the entry.
        std::uint32_t line = 0;             ///< 1-based source line.
        std::uint32_t column = 0;           ///< 1-based source column.
        std::string source_kind{"direct"};  ///< Direct authoring or instancing path that produced the entry.
        std::string template_name{};        ///< Template name when the entry came from an instance.
        std::string instance_name{};        ///< Optional named placement when the entry came from an instance.
        std::string placement_file_path{};  ///< TOML file path for the placement site when instanced.
        std::uint32_t placement_line = 0;   ///< 1-based placement source line.
        std::uint32_t placement_column = 0; ///< 1-based placement source column.

        [[nodiscard]] auto valid() const noexcept -> bool
        {
            return !file_path.empty() && line > 0 && column > 0;
        }

        [[nodiscard]] auto has_placement() const noexcept -> bool
        {
            return !placement_file_path.empty() && placement_line > 0 && placement_column > 0;
        }
    };

    /**
     * @brief Material tag assigned to a blockout solid.
     */
    enum class SurfaceMaterial : std::uint8_t {
        Concrete, ///< Default concrete-like surface.
        Metal,    ///< Metal surface.
        Tile,     ///< Tile or ceramic surface.
        Glass,    ///< Glass surface.
        Hazard,   ///< Hazard-marked surface.

        Floor,
        Ceiling,
        Wall,
        WallPanel,
        Corridor,
        Platform,
        PlatformTop,
        PlatformTop2,
        Pillar,
        Cover,
        Block,
        Beam,
        Trim,
        Divider,
        LightStrip,
        Crate,
        Post,
        Matte,
        Cotton,
        Twill,
        Leather,
        Rubber,
        ServiceFloor,
        MaintenanceFloor,
        FiltrationFloor,
        FiltrationWall,
    };

    /**
     * @brief Axis-aligned blockout solid loaded from a level file.
     */
    struct BlockoutSolid {
        glm::vec3 mins{};                                    ///< Minimum corner in world space.
        glm::vec3 maxs{};                                    ///< Maximum corner in world space.
        SurfaceMaterial material{SurfaceMaterial::Concrete}; ///< Surface tag for the solid.
        glm::vec3 tint{1.0F, 1.0F, 1.0F};                    ///< Optional per-solid tint multiplier.
        float brightness{1.0F};                              ///< Optional brightness multiplier for accent surfaces.
    };

    /**
     * @brief Spawn information for a blockout entity.
     */
    struct EntitySpawnData {
        std::string classname;                                   ///< Entity class name.
        glm::vec3 origin{};                                      ///< Spawn origin in world space.
        glm::vec3 angles{};                                      ///< Euler angles in degrees.
        bool has_angles = false;                                 ///< True when the level explicitly authored angles.
        std::unordered_map<std::string, std::string> properties; ///< Additional arbitrary string properties.
    };

    /**
     * @brief In-memory representation of a blockout level.
     */
    struct BlockoutLevel {
        // Derived physics-only geometry. It is never rendered or serialized as authored blockout.
        std::vector<BlockoutSolid> prop_colliders;
        std::vector<BlockoutSolid> solids;                  ///< Loaded blockout solids.
        std::vector<AuthoredSourceLocation> solid_sources;  ///< Source location per solid when available.
        std::vector<EntitySpawnData> entities;              ///< Loaded entity definitions.
        std::vector<AuthoredSourceLocation> entity_sources; ///< Source location per entity when available.
    };
} // namespace RelayCore

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
        std::vector<BlockoutSolid> solids;     ///< Loaded blockout solids.
        std::vector<EntitySpawnData> entities; ///< Loaded entity definitions.
    };
} // namespace RelayCore

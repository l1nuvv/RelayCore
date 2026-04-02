//
// Created by l1nuvv on 02.04.2026.
//

#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace RelayCore
{
    enum class SurfaceMaterial : std::uint8_t {
        Concrete,
        Metal,
        Tile,
        Glass,
        Hazard
    };

    struct BlockoutSolid {
        glm::vec3       mins{};
        glm::vec3       maxs{};
        SurfaceMaterial material{SurfaceMaterial::Concrete};
    };

    struct EntitySpawnData {
        std::string                                  classname;
        glm::vec3                                    origin{};
        glm::vec3                                    angles{};
        std::unordered_map<std::string, std::string> properties;
    };

    struct BlockoutLevel {
        std::vector<BlockoutSolid>   solids;
        std::vector<EntitySpawnData> entities;
    };
} // namespace RelayCore

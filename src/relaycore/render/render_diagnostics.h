#pragma once

#include <cstdint>
#include <glm/vec3.hpp>

namespace RelayCore::render
{
    struct WorldDiagnostics {
        std::uint32_t total_solids   = 0;
        std::uint32_t visible_solids = 0;
        std::uint32_t draw_calls     = 0;
        std::uint32_t triangles      = 0;
    };

    struct PlayerDiagnostics {
        glm::vec3     position{};
        glm::vec3     velocity{};
        bool          grounded          = false;
        bool          collision_enabled = true;
        std::uint32_t movement_mode     = 0;
        float         speed             = 0.0F;
    };
} // namespace RelayCore::render

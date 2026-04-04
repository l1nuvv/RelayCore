//
// Created by l1nuvv on 04.04.2026.
//

#pragma once

#include <cstdint>

namespace RelayCore::render
{
    struct WorldDiagnostics {
        std::uint32_t total_solids   = 0;
        std::uint32_t visible_solids = 0;
        std::uint32_t draw_calls     = 0;
        std::uint32_t triangles      = 0;
    };
} // namespace RelayCore::render

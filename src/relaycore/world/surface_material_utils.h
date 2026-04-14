#pragma once

#include "relaycore/world/blockout_level.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace RelayCore
{
    [[nodiscard]] constexpr auto surface_material_count() noexcept -> std::size_t
    {
        return static_cast<std::size_t>(SurfaceMaterial::Post) + 1;
    }

    [[nodiscard]] auto all_surface_materials() noexcept -> const std::array<SurfaceMaterial, surface_material_count()>&;

    [[nodiscard]] auto canonical_surface_material_name(SurfaceMaterial material) noexcept -> std::string_view;

    [[nodiscard]] auto try_parse_surface_material_name(std::string_view value) noexcept
        -> std::optional<SurfaceMaterial>;
} // namespace RelayCore

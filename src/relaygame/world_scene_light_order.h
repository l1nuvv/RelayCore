#pragma once

#include <cmath>

namespace RelayGame::detail
{
    [[nodiscard]] inline auto local_light_precedes(
        const float lhs_score,
        const float lhs_radius,
        const float rhs_score,
        const float rhs_radius
    ) noexcept -> bool
    {
        if (std::isnan(lhs_score))
            return false;
        if (std::isnan(rhs_score))
            return true;
        if (lhs_score != rhs_score)
            return lhs_score < rhs_score;
        if (std::isnan(lhs_radius))
            return false;
        if (std::isnan(rhs_radius))
            return true;
        return lhs_radius > rhs_radius;
    }
} // namespace RelayGame::detail

#include "relaygame/world_scene_light_order.h"

#include <doctest/doctest.h>
#include <limits>

TEST_CASE("local light ordering remains transitive for nearby scores")
{
    constexpr float epsilon = std::numeric_limits<float>::epsilon();
    constexpr float scores[]{0.0F, 0.75F * epsilon, 1.5F * epsilon};
    constexpr float radii[]{1.0F, 2.0F, 3.0F};

    for (int lhs = 0; lhs < 3; ++lhs)
    {
        for (int rhs = 0; rhs < 3; ++rhs)
        {
            CHECK(
                RelayGame::detail::local_light_precedes(scores[lhs], radii[lhs], scores[rhs], radii[rhs]) == (lhs < rhs)
            );
        }
    }
}

TEST_CASE("local light ordering uses radius only for equal scores")
{
    CHECK(RelayGame::detail::local_light_precedes(1.0F, 3.0F, 1.0F, 2.0F));
    CHECK_FALSE(RelayGame::detail::local_light_precedes(1.0F, 2.0F, 1.0F, 3.0F));
    CHECK_FALSE(RelayGame::detail::local_light_precedes(1.0F, 3.0F, 1.0F, 3.0F));
}

TEST_CASE("local light ordering keeps NaN scores after comparable scores")
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    CHECK(RelayGame::detail::local_light_precedes(1.0F, 2.0F, nan, 3.0F));
    CHECK_FALSE(RelayGame::detail::local_light_precedes(nan, 3.0F, 1.0F, 2.0F));
    CHECK_FALSE(RelayGame::detail::local_light_precedes(nan, 3.0F, nan, 2.0F));
    CHECK(RelayGame::detail::local_light_precedes(1.0F, 2.0F, 1.0F, nan));
    CHECK_FALSE(RelayGame::detail::local_light_precedes(1.0F, nan, 1.0F, 2.0F));
}

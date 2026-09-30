#include "relaycore/core/spatial_index_2d.h"

#include <cstdint>
#include <doctest/doctest.h>
#include <limits>
#include <vector>

TEST_CASE("spatial index rejects unrepresentable and unbounded cell ranges")
{
    RelayCore::core::SpatialIndex2D<std::uint32_t> index;
    index.reset(1);
    std::vector<std::uint32_t> results;

    const RelayCore::physics::Aabb unrepresentable{{1.0e20F, 0.0F, 0.0F}, {1.0e20F, 0.0F, 0.0F}};
    CHECK_THROWS_WITH(index.insert(unrepresentable, 0), doctest::Contains("signed cell range"));
    CHECK_THROWS_WITH(index.query(unrepresentable, results), doctest::Contains("signed cell range"));

    const RelayCore::physics::Aabb oversized{{-1000.0F, 0.0F, -1000.0F}, {1000.0F, 0.0F, 1000.0F}};
    CHECK_THROWS_WITH(index.insert(oversized, 0), doctest::Contains("too many cells"));
    CHECK_THROWS_WITH(index.query(oversized, results), doctest::Contains("too many cells"));

    const float nan = std::numeric_limits<float>::quiet_NaN();
    CHECK_THROWS_AS(index.insert({{nan, 0.0F, 0.0F}, {1.0F, 0.0F, 1.0F}}, 0), std::out_of_range);
    CHECK_THROWS_AS(index.set_cell_size(nan), std::invalid_argument);
}

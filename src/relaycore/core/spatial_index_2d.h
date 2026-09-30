#pragma once

#include "relaycore/core/profiling.h"
#include "relaycore/physics/collision.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace RelayCore::core
{
    template<typename Id>
    class SpatialIndex2D {
        static_assert(std::is_integral_v<Id>, "SpatialIndex2D item ids must be integral");

    public:
        explicit SpatialIndex2D(const float cell_size = 4.0F) : cell_size_(checked_cell_size(cell_size)) {}

        void set_cell_size(const float cell_size)
        {
            RELAYCORE_PROFILE_FUNCTION();
            cell_size_ = checked_cell_size(cell_size);
        }

        [[nodiscard]] auto empty() const noexcept -> bool
        {
            RELAYCORE_PROFILE_FUNCTION();
            return cells_.empty();
        }

        void reset(const std::size_t item_count)
        {
            RELAYCORE_PROFILE_FUNCTION();
            cells_.clear();
            marks_.assign(item_count, 0);
            query_id_ = 0;
        }

        void insert(const physics::Aabb& bounds, const Id item_id)
        {
            RELAYCORE_PROFILE_FUNCTION();
            const CellRange range = checked_range(bounds);

            for (std::int64_t cell_x = range.min_x; cell_x <= range.max_x; ++cell_x)
            {
                for (std::int64_t cell_z = range.min_z; cell_z <= range.max_z; ++cell_z)
                {
                    cells_[cell_key(static_cast<int>(cell_x), static_cast<int>(cell_z))].push_back(item_id);
                }
            }
        }

        void validate_bounds(const physics::Aabb& bounds) const
        {
            static_cast<void>(checked_range(bounds));
        }

        void query(const physics::Aabb& bounds, std::vector<Id>& out_indices)
        {
            RELAYCORE_PROFILE_FUNCTION();
            begin_query();
            out_indices.clear();

            const CellRange range = checked_range(bounds);

            for (std::int64_t cell_x = range.min_x; cell_x <= range.max_x; ++cell_x)
            {
                for (std::int64_t cell_z = range.min_z; cell_z <= range.max_z; ++cell_z)
                {
                    const auto it = cells_.find(cell_key(static_cast<int>(cell_x), static_cast<int>(cell_z)));
                    if (it == cells_.end())
                    {
                        continue;
                    }

                    for (const Id item_id : it->second)
                    {
                        const std::size_t mark_index = static_cast<std::size_t>(item_id);
                        if (mark_index >= marks_.size())
                        {
                            continue;
                        }
                        if (marks_[mark_index] == query_id_)
                        {
                            continue;
                        }

                        marks_[mark_index] = query_id_;
                        out_indices.push_back(item_id);
                    }
                }
            }
        }

    private:
        struct CellRange {
            int min_x;
            int max_x;
            int min_z;
            int max_z;
        };

        [[nodiscard]] static auto checked_cell_size(const float cell_size) -> float
        {
            if (!std::isfinite(cell_size) || cell_size <= 0.0F)
                throw std::invalid_argument("SpatialIndex2D cell size must be finite and positive");
            return std::max(cell_size, 0.01F);
        }

        [[nodiscard]] auto cell_coord(const float axis) const -> int
        {
            const double cell = std::floor(static_cast<double>(axis) / static_cast<double>(cell_size_));
            if (!std::isfinite(cell) || cell < std::numeric_limits<int>::min()
                || cell > std::numeric_limits<int>::max())
                throw std::out_of_range("SpatialIndex2D coordinate is outside the signed cell range");
            return static_cast<int>(cell);
        }

        [[nodiscard]] auto checked_range(const physics::Aabb& bounds) const -> CellRange
        {
            const CellRange range{
                cell_coord(bounds.mins.x),
                cell_coord(bounds.maxs.x),
                cell_coord(bounds.mins.z),
                cell_coord(bounds.maxs.z),
            };
            if (range.min_x > range.max_x || range.min_z > range.max_z)
                throw std::invalid_argument("SpatialIndex2D bounds are inverted");

            constexpr std::int64_t kMaxCoveredCells = 65536;
            const std::int64_t width = static_cast<std::int64_t>(range.max_x) - range.min_x + 1;
            const std::int64_t depth = static_cast<std::int64_t>(range.max_z) - range.min_z + 1;
            if (width > kMaxCoveredCells / depth)
                throw std::length_error("SpatialIndex2D bounds cover too many cells");
            return range;
        }

        [[nodiscard]] static auto cell_key(const int cell_x, const int cell_z) noexcept -> std::uint64_t
        {
            return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell_x)) << 32)
                   | static_cast<std::uint32_t>(cell_z);
        }

        void begin_query()
        {
            RELAYCORE_PROFILE_FUNCTION();
            ++query_id_;
            if (query_id_ != 0)
            {
                return;
            }

            std::fill(marks_.begin(), marks_.end(), 0);
            query_id_ = 1;
        }

        float cell_size_ = 4.0F;
        std::unordered_map<std::uint64_t, std::vector<Id>> cells_{};
        std::vector<std::uint32_t> marks_{};
        std::uint32_t query_id_ = 0;
    };
} // namespace RelayCore::core

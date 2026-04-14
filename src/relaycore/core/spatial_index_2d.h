#pragma once

#include "relaycore/core/profiling.h"
#include "relaycore/physics/collision.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace RelayCore::core
{
    template<typename Id>
    class SpatialIndex2D {
        static_assert(std::is_integral_v<Id>, "SpatialIndex2D item ids must be integral");

    public:
        explicit SpatialIndex2D(const float cell_size = 4.0F) : cell_size_(std::max(cell_size, 0.01F)) {}

        void set_cell_size(const float cell_size) noexcept
        {
            RELAYCORE_PROFILE_FUNCTION();
            cell_size_ = std::max(cell_size, 0.01F);
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
            const int min_cell_x = cell_coord(bounds.mins.x);
            const int max_cell_x = cell_coord(bounds.maxs.x);
            const int min_cell_z = cell_coord(bounds.mins.z);
            const int max_cell_z = cell_coord(bounds.maxs.z);

            for (int cell_x = min_cell_x; cell_x <= max_cell_x; ++cell_x)
            {
                for (int cell_z = min_cell_z; cell_z <= max_cell_z; ++cell_z)
                {
                    cells_[cell_key(cell_x, cell_z)].push_back(item_id);
                }
            }
        }

        void query(const physics::Aabb& bounds, std::vector<Id>& out_indices)
        {
            RELAYCORE_PROFILE_FUNCTION();
            begin_query();
            out_indices.clear();

            const int min_cell_x = cell_coord(bounds.mins.x);
            const int max_cell_x = cell_coord(bounds.maxs.x);
            const int min_cell_z = cell_coord(bounds.mins.z);
            const int max_cell_z = cell_coord(bounds.maxs.z);

            for (int cell_x = min_cell_x; cell_x <= max_cell_x; ++cell_x)
            {
                for (int cell_z = min_cell_z; cell_z <= max_cell_z; ++cell_z)
                {
                    const auto it = cells_.find(cell_key(cell_x, cell_z));
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
        [[nodiscard]] auto cell_coord(const float axis) const noexcept -> int
        {
            return static_cast<int>(std::floor(axis / cell_size_));
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

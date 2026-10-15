/**
 * @file src/relaycore/app/frame_timer.cpp
 * @brief FrameTimer implementation.
 */

#include "relaycore/app/frame_timer.h"

#include "relaycore/core/profiling.h"

#include <algorithm>

namespace RelayCore
{
    FrameTimer::FrameTimer(Clock::time_point now) : start_time_(now), previous_frame_time_(now) {}

    auto FrameTimer::begin_frame(Clock::time_point now) -> FrameInfo
    {
        RELAYCORE_PROFILE_FUNCTION();
        const auto raw_delta = now - previous_frame_time_;
        const auto delta = std::min(raw_delta, std::chrono::duration_cast<Clock::duration>(kMaxDelta));
        const auto elapsed = std::chrono::duration<double>(now - start_time_).count();

        previous_frame_time_ = now;

        return FrameInfo{
            .delta_seconds = std::chrono::duration<double>(delta).count(),
            .wall_delta_seconds = std::chrono::duration<double>(raw_delta).count(),
            .elapsed_seconds = elapsed,
            .frame_index = frame_index_++,
        };
    }
} // namespace RelayCore

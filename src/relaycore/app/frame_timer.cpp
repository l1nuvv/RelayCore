#include "relaycore/app/frame_timer.hpp"

#include <algorithm>

namespace RelayCore
{
    FrameTimer::FrameTimer() = default;

    auto FrameTimer::begin_frame() -> FrameInfo
    {
        const auto now       = Clock::now();
        const auto raw_delta = now - previous_frame_time_;
        const auto delta     = std::min(raw_delta, std::chrono::duration_cast<Clock::duration>(kMaxDelta));
        const auto elapsed   = std::chrono::duration<double>(now - start_time_).count();

        previous_frame_time_ = now;

        return FrameInfo{
            .delta_seconds   = std::chrono::duration<double>(delta).count(),
            .elapsed_seconds = elapsed,
            .frame_index     = frame_index_++,
        };
    }
} // namespace RelayCore

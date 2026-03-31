#pragma once

#include <chrono>
#include <cstdint>

namespace RelayCore
{
struct FrameInfo
{
    double delta_seconds = 0.0;
    double elapsed_seconds = 0.0;
    std::uint64_t frame_index = 0;
};

class FrameTimer
{
public:
    FrameTimer();

    [[nodiscard]] auto begin_frame() -> FrameInfo;

private:
    using Clock = std::chrono::steady_clock;

    static constexpr auto kMaxDelta = std::chrono::milliseconds(100);

    Clock::time_point start_time_ = Clock::now();
    Clock::time_point previous_frame_time_ = start_time_;
    std::uint64_t frame_index_ = 0;
};
} // namespace RelayCore

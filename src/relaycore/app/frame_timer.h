/**
 * @file src/relaycore/app/frame_timer.h
 * @brief Frame timing helpers used by the application main loop.
 */

#pragma once

#include <chrono>
#include <cstdint>

namespace RelayCore
{
    /**
     * @brief Timing data produced for one rendered frame.
     */
    struct FrameInfo {
        double        delta_seconds   = 0.0; ///< Clamped time since the previous frame.
        double        elapsed_seconds = 0.0; ///< Time since the timer was created.
        std::uint64_t frame_index     = 0;   ///< Zero-based frame counter.
    };

    /**
     * @brief Produces per-frame timing values for the main loop.
     */
    class FrameTimer {
    public:
        /**
         * @brief Creates a frame timer starting at the current time.
         */
        FrameTimer();

        /**
         * @brief Begins a new frame and returns its timing information.
         * @return Timing data for the current frame.
         */
        [[nodiscard]] auto begin_frame() -> FrameInfo;

    private:
        using Clock = std::chrono::steady_clock;                          ///< Timing source used by the frame timer.

        static constexpr auto kMaxDelta = std::chrono::milliseconds(100); ///< Maximum accepted frame delta.

        Clock::time_point start_time_          = Clock::now();            ///< Timer creation time.
        Clock::time_point previous_frame_time_ = start_time_;             ///< Timestamp of the previous frame.
        std::uint64_t     frame_index_         = 0;                       ///< Number of frames produced so far.
    };
} // namespace RelayCore

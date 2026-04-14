#pragma once

#include "relaycore/app/frame_timer.h"
#include "relaycore/core/profiling.h"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace RelayGame
{
    class FixedSimulationLoop {
    public:
        constexpr FixedSimulationLoop(const double step_seconds, const std::uint32_t max_steps_per_frame) noexcept :
            step_seconds_(step_seconds), max_steps_per_frame_(max_steps_per_frame)
        {}

        void reset() noexcept
        {
            RELAYCORE_PROFILE_FUNCTION();
            accumulator_seconds_ = 0.0;
            tick_index_ = 0;
            last_steps_executed_ = 0;
        }

        template<typename Callback>
        auto advance(const RelayCore::FrameInfo& frame, Callback&& callback) -> std::uint32_t
        {
            RELAYCORE_PROFILE_FUNCTION();
            accumulator_seconds_ = std::min(
                accumulator_seconds_ + frame.delta_seconds,
                step_seconds_ * static_cast<double>(max_steps_per_frame_)
            );

            std::uint32_t steps_executed = 0;
            while (accumulator_seconds_ >= step_seconds_ && steps_executed < max_steps_per_frame_)
            {
                const RelayCore::FrameInfo sim_frame{
                    .delta_seconds = step_seconds_,
                    .elapsed_seconds = frame.elapsed_seconds - accumulator_seconds_ + step_seconds_,
                    .frame_index = tick_index_++,
                };
                std::forward<Callback>(callback)(sim_frame);
                accumulator_seconds_ -= step_seconds_;
                ++steps_executed;
            }

            last_steps_executed_ = steps_executed;
            return steps_executed;
        }

        [[nodiscard]] auto step_seconds() const noexcept -> double
        {
            RELAYCORE_PROFILE_FUNCTION();
            return step_seconds_;
        }

        [[nodiscard]] auto max_steps_per_frame() const noexcept -> std::uint32_t
        {
            RELAYCORE_PROFILE_FUNCTION();
            return max_steps_per_frame_;
        }

        [[nodiscard]] auto pending_seconds() const noexcept -> double
        {
            RELAYCORE_PROFILE_FUNCTION();
            return accumulator_seconds_;
        }

        [[nodiscard]] auto tick_index() const noexcept -> std::uint64_t
        {
            RELAYCORE_PROFILE_FUNCTION();
            return tick_index_;
        }

        [[nodiscard]] auto last_steps_executed() const noexcept -> std::uint32_t
        {
            RELAYCORE_PROFILE_FUNCTION();
            return last_steps_executed_;
        }

    private:
        double step_seconds_ = 1.0 / 120.0;
        std::uint32_t max_steps_per_frame_ = 8;
        double accumulator_seconds_ = 0.0;
        std::uint64_t tick_index_ = 0;
        std::uint32_t last_steps_executed_ = 0;
    };
} // namespace RelayGame

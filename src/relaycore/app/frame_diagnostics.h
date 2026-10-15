#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

namespace RelayCore
{
    // Samples complete application loops, from before events through present (or suspended update).
    // A loop crossing a context boundary belongs to boundary/loading, never to a steady gameplay bucket.
    class FrameDiagnostics {
    public:
        using Clock = std::chrono::steady_clock;
        void set_context(std::string_view mode, std::string_view map);
        void mark_loading();
        void mark_suspended();
        void begin_frame(Clock::time_point now = Clock::now());
        void end_frame(bool enabled, double interval_seconds, Clock::time_point now = Clock::now());
        void flush();

    private:
        struct Context {
            std::string mode = "application";
            std::string map;
            bool operator==(const Context&) const = default;
        };
        Context context_, frame_context_, summary_context_;
        Clock::time_point origin_ = Clock::now(), frame_start_{}, summary_start_{}, summary_end_{};
        bool frame_active_ = false;
        std::vector<double> samples_ms_;
    };
} // namespace RelayCore

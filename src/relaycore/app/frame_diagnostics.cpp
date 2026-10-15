#include "relaycore/app/frame_diagnostics.h"

#include "relaycore/core/log.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace RelayCore
{
    void FrameDiagnostics::set_context(std::string_view mode, std::string_view map)
    {
        if (context_.mode == mode && context_.map == map)
            return;
        context_.mode = mode;
        context_.map = map;
        if (frame_active_ && frame_context_.mode != "loading")
            frame_context_.mode = "boundary";
    }

    void FrameDiagnostics::mark_loading()
    {
        if (frame_active_)
            frame_context_.mode = "loading";
    }

    void FrameDiagnostics::mark_suspended()
    {
        if (frame_active_ && frame_context_.mode != "loading" && frame_context_.mode != "boundary")
            frame_context_.mode = "suspended";
    }

    void FrameDiagnostics::begin_frame(Clock::time_point now)
    {
        frame_context_ = context_;
        frame_start_ = now;
        frame_active_ = true;
    }

    void FrameDiagnostics::end_frame(bool enabled, double interval_seconds, Clock::time_point now)
    {
        if (!frame_active_)
            return;
        frame_active_ = false;
        if (!enabled)
        {
            flush();
            return;
        }
        if (!(frame_context_ == summary_context_))
            flush();
        if (samples_ms_.empty())
        {
            summary_context_ = frame_context_;
            summary_start_ = frame_start_;
        }
        summary_end_ = now;
        samples_ms_.push_back(std::chrono::duration<double, std::milli>(now - frame_start_).count());
        if (std::chrono::duration<double>(now - summary_start_).count() >= std::max(0.1, interval_seconds))
            flush();
    }

    void FrameDiagnostics::flush()
    {
        if (samples_ms_.empty())
            return;
        const double average = std::accumulate(samples_ms_.begin(), samples_ms_.end(), 0.0) / samples_ms_.size();
        std::ranges::sort(samples_ms_);
        const auto percentile = [&](double fraction) {
            return samples_ms_[static_cast<std::size_t>(std::ceil(fraction * samples_ms_.size())) - 1];
        };
        log::info(
            "Frame summary: mode={} map='{}' frames={} wall_interval=[{:.6f},{:.6f}]s "
            "mean_ms={:.3f} p95_ms={:.3f} p99_ms={:.3f} max_ms={:.3f} "
            "scope=events+update+render+present_or_suspended_update map_scope=loop_start percentile=nearest_rank",
            summary_context_.mode,
            summary_context_.map,
            samples_ms_.size(),
            std::chrono::duration<double>(summary_start_ - origin_).count(),
            std::chrono::duration<double>(summary_end_ - origin_).count(),
            average,
            percentile(.95),
            percentile(.99),
            samples_ms_.back()
        );
        samples_ms_.clear();
    }
} // namespace RelayCore

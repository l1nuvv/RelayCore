#pragma once

#include <algorithm>

namespace RelayCore::render
{
    struct RenderResolution {
        float scale = 1, filtered_ms = 0;
        unsigned samples = 0;

        void reset(float ceiling) noexcept
        {
            scale = ceiling;
            filtered_ms = 0;
            samples = 0;
        }

        void update(float gpu_ms, float rendered_scale, float ceiling, float target_ms) noexcept
        {
            // A completion from a previous scale must not steer the new extent.
            if (rendered_scale != scale)
                return;
            filtered_ms = filtered_ms == 0 ? gpu_ms : filtered_ms + .1F * (gpu_ms - filtered_ms);
            if (++samples < 32)
                return;
            if (filtered_ms > target_ms * 1.05F && scale > .5F)
                scale = std::max(.5F, scale - .05F);
            else if (filtered_ms < target_ms * .8F && scale < ceiling)
                scale = std::min(ceiling, scale + .05F);
            else
                return;
            samples = 0;
            filtered_ms = 0;
        }
    };
} // namespace RelayCore::render

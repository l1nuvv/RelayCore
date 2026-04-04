/**
 * @file src/relaycore/core/profiling.h
 * @brief Tracy profiling convenience macros used throughout the runtime.
 */

#pragma once

#if __has_include(<tracy/Tracy.hpp>)
#include <tracy/Tracy.hpp>
#endif

#if defined(TRACY_ENABLE) && __has_include(<tracy/Tracy.hpp>)
/** @brief Opens a Tracy profiling zone in the current scope. */
#define RELAYCORE_PROFILE_ZONE() ZoneScoped
/** @brief Opens a named Tracy profiling zone in the current scope. */
#define RELAYCORE_PROFILE_ZONE_N(name_literal) ZoneScopedN(name_literal)
/** @brief Marks the end of the current frame for Tracy. */
#define RELAYCORE_PROFILE_FRAME() FrameMark
/** @brief Sends a numeric plot sample to Tracy. */
#define RELAYCORE_PROFILE_PLOT(name_literal, value) TracyPlot(name_literal, static_cast<double>(value))
#else
/** @brief No-op replacement when Tracy is unavailable. */
#define RELAYCORE_PROFILE_ZONE() ((void) 0)
/** @brief No-op named-zone replacement when Tracy is unavailable. */
#define RELAYCORE_PROFILE_ZONE_N(name_literal) ((void) 0)
/** @brief No-op frame-mark replacement when Tracy is unavailable. */
#define RELAYCORE_PROFILE_FRAME() ((void) 0)
/** @brief No-op plot replacement when Tracy is unavailable. */
#define RELAYCORE_PROFILE_PLOT(name_literal, value) ((void) 0)
#endif

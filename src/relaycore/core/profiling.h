/**
 * @file src/relaycore/core/profiling.h
 * @brief Tracy profiling convenience macros used throughout the runtime.
 */

#pragma once

#if __has_include(<tracy/Tracy.hpp>)
#include <tracy/Tracy.hpp>
#endif

#define RELAYCORE_PROFILE_CONCAT_INNER(a, b) a##b
#define RELAYCORE_PROFILE_CONCAT(a, b) RELAYCORE_PROFILE_CONCAT_INNER(a, b)

#if defined(TRACY_ENABLE) && __has_include(<tracy/Tracy.hpp>)
/** @brief Opens a Tracy profiling zone in the current scope. */
#define RELAYCORE_PROFILE_ZONE() ZoneNamed(RELAYCORE_PROFILE_CONCAT(___relaycore_tracy_zone_, __LINE__), true)
/** @brief Opens a named Tracy profiling zone in the current scope. */
#define RELAYCORE_PROFILE_ZONE_N(name_literal)                                                                         \
    ZoneNamedN(RELAYCORE_PROFILE_CONCAT(___relaycore_tracy_zone_, __LINE__), name_literal, true)
/** @brief Opens a named Tracy profiling zone using the current function signature. */
#if defined(_MSC_VER)
#define RELAYCORE_PROFILE_FUNCTION() RELAYCORE_PROFILE_ZONE_N(__FUNCSIG__)
#elif defined(__clang__) || defined(__GNUC__)
#define RELAYCORE_PROFILE_FUNCTION() RELAYCORE_PROFILE_ZONE_N(__PRETTY_FUNCTION__)
#else
#define RELAYCORE_PROFILE_FUNCTION() RELAYCORE_PROFILE_ZONE_N(__func__)
#endif
/** @brief Marks the end of the current frame for Tracy. */
#define RELAYCORE_PROFILE_FRAME() FrameMark
/** @brief Sends a numeric plot sample to Tracy. */
#define RELAYCORE_PROFILE_PLOT(name_literal, value) TracyPlot(name_literal, static_cast<double>(value))
#else
/** @brief No-op replacement when Tracy is unavailable. */
#define RELAYCORE_PROFILE_ZONE() ((void) 0)
/** @brief No-op named-zone replacement when Tracy is unavailable. */
#define RELAYCORE_PROFILE_ZONE_N(name_literal) ((void) 0)
/** @brief No-op function-zone replacement when Tracy is unavailable. */
#define RELAYCORE_PROFILE_FUNCTION() ((void) 0)
/** @brief No-op frame-mark replacement when Tracy is unavailable. */
#define RELAYCORE_PROFILE_FRAME() ((void) 0)
/** @brief No-op plot replacement when Tracy is unavailable. */
#define RELAYCORE_PROFILE_PLOT(name_literal, value) ((void) 0)
#endif

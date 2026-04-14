/**
 * @file src/relaycore/core/profiling_opengl.h
 * @brief Tracy OpenGL GPU profiling helpers for GL-backed renderer code.
 */

#pragma once

#include "relaycore/core/profiling.h"

#if defined(TRACY_ENABLE) && __has_include(<tracy/TracyOpenGL.hpp>)
#include <tracy/TracyOpenGL.hpp>

/** @brief Initializes the Tracy OpenGL GPU context for the current GL context. */
#define RELAYCORE_PROFILE_GPU_CONTEXT() TracyGpuContext
/** @brief Assigns a readable name to the Tracy OpenGL GPU context. */
#define RELAYCORE_PROFILE_GPU_CONTEXT_NAME(name_literal)                                                               \
    TracyGpuContextName(name_literal, static_cast<uint16_t>(sizeof(name_literal) - 1))
/** @brief Opens a named Tracy GPU zone in the current scope. */
#define RELAYCORE_PROFILE_GPU_ZONE_N(name_literal)                                                                     \
    TracyGpuNamedZone(RELAYCORE_PROFILE_CONCAT(___relaycore_tracy_gpu_zone_, __LINE__), name_literal, true)
/** @brief Collects completed Tracy GPU queries from the current GL context. */
#define RELAYCORE_PROFILE_GPU_COLLECT() TracyGpuCollect
#else
/** @brief No-op GPU context initializer when Tracy OpenGL profiling is unavailable. */
#define RELAYCORE_PROFILE_GPU_CONTEXT() ((void) 0)
/** @brief No-op GPU context naming when Tracy OpenGL profiling is unavailable. */
#define RELAYCORE_PROFILE_GPU_CONTEXT_NAME(name_literal) ((void) 0)
/** @brief No-op GPU zone replacement when Tracy OpenGL profiling is unavailable. */
#define RELAYCORE_PROFILE_GPU_ZONE_N(name_literal) ((void) 0)
/** @brief No-op GPU collection when Tracy OpenGL profiling is unavailable. */
#define RELAYCORE_PROFILE_GPU_COLLECT() ((void) 0)
#endif

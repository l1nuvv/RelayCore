/**
 * @file src/relaycore/core/sdl_error.h
 * @brief Small helpers for turning SDL failures into exceptions.
 */

#pragma once

#include <SDL3/SDL.h>
#include <stdexcept>
#include <string>
#include <string_view>

namespace RelayCore::core
{
    /**
     * @brief Throws a runtime_error that appends the current SDL error string.
     * @param what Human-readable operation description.
     */
    [[noreturn]] inline void throw_sdl_error(const std::string_view what)
    {
        throw std::runtime_error(std::string(what) + ": " + SDL_GetError());
    }

    /**
     * @brief Throws if an SDL boolean-returning operation failed.
     * @param ok Result returned by the SDL API call.
     * @param what Human-readable operation description.
     */
    inline void require_sdl(const bool ok, const std::string_view what) { if (!ok) { throw_sdl_error(what); } }
} // namespace RelayCore::core

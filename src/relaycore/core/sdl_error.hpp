#pragma once

#include <SDL3/SDL.h>
#include <stdexcept>
#include <string>
#include <string_view>

namespace RelayCore::core
{
    [[noreturn]] inline void throw_sdl_error(const std::string_view what)
    {
        throw std::runtime_error(std::string(what) + ": " + SDL_GetError());
    }

    inline void require_sdl(const bool ok, const std::string_view what)
    {
        if (!ok) { throw_sdl_error(what); }
    }
} // namespace RelayCore::core

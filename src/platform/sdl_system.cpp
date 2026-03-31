#include "platform/sdl_system.hpp"

#include <SDL3/SDL.h>

#include "core/log.hpp"
#include "core/sdl_error.hpp"

namespace RelayCore
{
    SdlSystem::SdlSystem()
    {
        core::require_sdl(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS), "Failed to initialize SDL");

        if (!SDL_SetAppMetadata("RelayCore", "0.1.0", "com.example.RelayCore"))
        {
            log::warn("Failed to set SDL app metadata: {}", SDL_GetError());
        }

        log::info("SDL initialized");
    }

    SdlSystem::~SdlSystem() noexcept { SDL_Quit(); }
} // namespace RelayCore

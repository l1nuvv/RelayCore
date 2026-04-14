/**
 * @file src/relaycore/platform/sdl_system.cpp
 * @brief SDL lifetime wrapper implementation.
 */

#include "relaycore/platform/sdl_system.h"

#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/core/sdl_error.h"

#include <SDL3/SDL.h>

namespace RelayCore
{
    SdlSystem::SdlSystem()
    {
        RELAYCORE_PROFILE_FUNCTION();
        core::require_sdl(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS), "Failed to initialize SDL");

        if (!SDL_SetAppMetadata("RelayCore", "0.1.0", "com.example.RelayCore"))
        {
            log::warn("Failed to set SDL app metadata: {}", SDL_GetError());
        }

        log::info("SDL initialized");
    }

    SdlSystem::~SdlSystem() noexcept
    {
        SDL_Quit();
    }
} // namespace RelayCore

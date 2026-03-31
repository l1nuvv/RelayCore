#pragma once

#if __has_include(<tracy/Tracy.hpp>)
#include <tracy/Tracy.hpp>
#endif

#if defined(TRACY_ENABLE) && __has_include(<tracy/Tracy.hpp>)
#define RELAYCORE_PROFILE_ZONE() ZoneScoped
#define RELAYCORE_PROFILE_ZONE_N(name_literal) ZoneScopedN(name_literal)
#define RELAYCORE_PROFILE_FRAME() FrameMark
#else
#define RELAYCORE_PROFILE_ZONE() ((void)0)
#define RELAYCORE_PROFILE_ZONE_N(name_literal) ((void)0)
#define RELAYCORE_PROFILE_FRAME() ((void)0)
#endif

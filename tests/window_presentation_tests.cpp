#include "relaycore/platform/window.h"

#include <doctest/doctest.h>

TEST_CASE("window presentation policy requires a visible positive drawable extent")
{
    using RelayCore::WindowPresentationState;

    CHECK(RelayCore::can_present(WindowPresentationState{
        .pixel_width = 1280,
        .pixel_height = 720,
    }));
    CHECK_FALSE(RelayCore::can_present(WindowPresentationState{
        .pixel_width = 0,
        .pixel_height = 720,
    }));
    CHECK_FALSE(RelayCore::can_present(WindowPresentationState{
        .pixel_width = 1280,
        .pixel_height = 0,
    }));
    CHECK_FALSE(RelayCore::can_present(WindowPresentationState{
        .pixel_width = 1280,
        .pixel_height = 720,
        .minimized = true,
    }));
    CHECK_FALSE(RelayCore::can_present(WindowPresentationState{
        .pixel_width = 1280,
        .pixel_height = 720,
        .hidden = true,
    }));
    CHECK(RelayCore::can_present(WindowPresentationState{
        .pixel_width = 1280,
        .pixel_height = 720,
    }));
}

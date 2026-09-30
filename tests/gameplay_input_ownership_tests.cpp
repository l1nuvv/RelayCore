#include "relaygame/gameplay_input_ownership.h"

#include <doctest/doctest.h>

TEST_CASE("gameplay input ownership preserves relative mouse gameplay through ImGui hover")
{
    const RelayGame::GameplayInputOwnership ownership =
        RelayGame::resolve_gameplay_input_ownership(true, false, true, false, true, false);

    CHECK_FALSE(ownership.keyboard_blocked);
    CHECK_FALSE(ownership.mouse_blocked);
}

TEST_CASE("gameplay input ownership blocks ImGui mouse interaction in cursor mode")
{
    const RelayGame::GameplayInputOwnership ownership =
        RelayGame::resolve_gameplay_input_ownership(true, false, false, false, true, false);

    CHECK_FALSE(ownership.keyboard_blocked);
    CHECK(ownership.mouse_blocked);
}

TEST_CASE("gameplay input ownership enters ImGui mouse ownership after F1 switches to cursor mode")
{
    const RelayGame::GameplayInputOwnership relative_mode =
        RelayGame::resolve_gameplay_input_ownership(true, false, true, false, true, false);
    const RelayGame::GameplayInputOwnership cursor_mode =
        RelayGame::resolve_gameplay_input_ownership(true, false, false, false, true, false);

    CHECK_FALSE(relative_mode.mouse_blocked);
    CHECK(cursor_mode.mouse_blocked);
}

TEST_CASE("gameplay input ownership only blocks keyboard for ImGui text input")
{
    const RelayGame::GameplayInputOwnership navigation_ownership =
        RelayGame::resolve_gameplay_input_ownership(true, false, true, true, false, false);
    const RelayGame::GameplayInputOwnership text_input_ownership =
        RelayGame::resolve_gameplay_input_ownership(true, false, true, true, false, true);

    CHECK_FALSE(navigation_ownership.keyboard_blocked);
    CHECK(text_input_ownership.keyboard_blocked);
}

TEST_CASE("gameplay input ownership blocks both devices when the window is unavailable")
{
    const RelayGame::GameplayInputOwnership focus_lost =
        RelayGame::resolve_gameplay_input_ownership(false, false, true, false, false, false);
    const RelayGame::GameplayInputOwnership minimized =
        RelayGame::resolve_gameplay_input_ownership(true, true, true, false, false, false);

    CHECK(focus_lost.keyboard_blocked);
    CHECK(focus_lost.mouse_blocked);
    CHECK(minimized.keyboard_blocked);
    CHECK(minimized.mouse_blocked);
}

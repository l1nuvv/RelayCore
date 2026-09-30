#pragma once

namespace RelayGame
{
    struct GameplayInputOwnership {
        bool keyboard_blocked = false;
        bool mouse_blocked = false;
    };

    [[nodiscard]] constexpr auto resolve_gameplay_input_ownership(
        const bool window_focused,
        const bool window_minimized,
        const bool relative_mouse_mode,
        const bool imgui_wants_keyboard,
        const bool imgui_wants_mouse,
        const bool imgui_wants_text_input
    ) noexcept -> GameplayInputOwnership
    {
        const bool window_unavailable = !window_focused || window_minimized;
        return {
            .keyboard_blocked = window_unavailable || (imgui_wants_keyboard && imgui_wants_text_input),
            .mouse_blocked = window_unavailable || (!relative_mouse_mode && imgui_wants_mouse),
        };
    }
} // namespace RelayGame

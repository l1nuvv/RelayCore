#pragma once

#include "relaycore/render/render_diagnostics.h"

namespace RelayCore::render
{
    class GameHud {
    public:
        void draw(const PlayerDiagnostics& player, const InteractionDiagnostics& interaction) const;

        [[nodiscard]] auto
        should_render(const PlayerDiagnostics& player, const InteractionDiagnostics& interaction) const noexcept
            -> bool;

    private:
        [[nodiscard]] static auto has_player_status(const PlayerDiagnostics& player) noexcept -> bool;

        [[nodiscard]] static auto has_interaction_prompt(const InteractionDiagnostics& interaction) noexcept -> bool;

        [[nodiscard]] static auto has_objective_prompt(const InteractionDiagnostics& interaction) noexcept -> bool;

        [[nodiscard]] static auto has_inventory_prompt(const InteractionDiagnostics& interaction) noexcept -> bool;

        [[nodiscard]] static auto has_transient_event(const InteractionDiagnostics& interaction) noexcept -> bool;

        static void draw_player_status_card(const PlayerDiagnostics& player);

        static auto draw_inventory_card(const InteractionDiagnostics& interaction) -> float;

        static void draw_objective_card(const InteractionDiagnostics& interaction);

        static void draw_interaction_prompt_card(const InteractionDiagnostics& interaction);

        static void draw_transient_event_card(const InteractionDiagnostics& interaction, float top);
    };
} // namespace RelayCore::render

#pragma once

#include "relaycore/render/render_diagnostics.h"

namespace RelayCore::render
{
    class GameHud {
    public:
        void draw(const InteractionDiagnostics& interaction) const;

        [[nodiscard]] auto should_render(const InteractionDiagnostics& interaction) const noexcept -> bool;

    private:
        [[nodiscard]] static auto has_interaction_prompt(const InteractionDiagnostics& interaction) noexcept -> bool;

        [[nodiscard]] static auto has_objective_prompt(const InteractionDiagnostics& interaction) noexcept -> bool;

        [[nodiscard]] static auto has_inventory_prompt(const InteractionDiagnostics& interaction) noexcept -> bool;

        [[nodiscard]] static auto has_transient_event(const InteractionDiagnostics& interaction) noexcept -> bool;

        static void draw_inventory_card(const InteractionDiagnostics& interaction);

        static void draw_objective_card(const InteractionDiagnostics& interaction);

        static void draw_interaction_prompt_card(const InteractionDiagnostics& interaction);

        static void draw_transient_event_card(const InteractionDiagnostics& interaction);
    };
} // namespace RelayCore::render

#pragma once

#include "relaycore/render/render_diagnostics.h"

namespace RelayCore::render
{
    class PerspectiveCamera;
}

namespace RelayCore::debug
{
    void
    draw_player_diagnostics_section(const render::PlayerDiagnostics& player, const render::PerspectiveCamera& camera);

    void draw_interaction_diagnostics_section(const render::InteractionDiagnostics& interaction);
} // namespace RelayCore::debug

#include "relaycore/render/renderer.h"

#include <utility>

namespace
{
    static_assert(noexcept(std::declval<RelayCore::Renderer&>()
                               .set_world_diagnostics(std::declval<const RelayCore::render::WorldDiagnostics&>())));
    static_assert(!noexcept(std::declval<RelayCore::Renderer&>()
                                .set_runtime_diagnostics(std::declval<RelayCore::render::RuntimeDiagnostics>())));
    static_assert(!noexcept(std::declval<RelayCore::Renderer&>()
                                .set_player_diagnostics(std::declval<RelayCore::render::PlayerDiagnostics>())));
    static_assert(!noexcept(std::declval<RelayCore::Renderer&>().set_interaction_diagnostics(
        std::declval<RelayCore::render::InteractionDiagnostics>()
    )));
    static_assert(!noexcept(std::declval<RelayCore::Renderer&>()
                                .set_inspector_diagnostics(std::declval<RelayCore::render::InspectorDiagnostics>())));
    static_assert(!noexcept(std::declval<RelayCore::Renderer&>()
                                .set_audio_diagnostics(std::declval<RelayCore::render::AudioDiagnostics>())));
    static_assert(!noexcept(std::declval<RelayCore::Renderer&>().set_iteration_diagnostics(
        std::declval<const RelayCore::render::IterationDiagnostics&>()
    )));
} // namespace

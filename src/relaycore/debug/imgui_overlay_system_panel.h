#pragma once

#include "relaycore/render/render_diagnostics.h"

namespace RelayCore::render
{
    struct RendererConfig;
}

namespace RelayCore::debug
{
    void draw_world_diagnostics_section(const render::WorldDiagnostics& world);

    void draw_runtime_diagnostics_section(const render::RuntimeDiagnostics& runtime);

    void draw_audio_diagnostics_section(const render::AudioDiagnostics& audio);

    void draw_render_tuning_section(const render::WorldDiagnostics& world, render::RendererConfig& renderer_config);
} // namespace RelayCore::debug

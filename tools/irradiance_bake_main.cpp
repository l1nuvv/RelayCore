#include "relaycore/core/log.h"
#include "relaycore/render/render_authoring_config.h"
#include "relaycore/world/blockout_loader.h"
#include "relaygame/world_irradiance.h"
#include "relaygame/world_scene_renderer.h"

#include <algorithm>
#include <iostream>

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        std::cerr << "usage: relaycore_irradiance_bake level.toml render_authoring.toml output.rcprobe\n";
        return 1;
    }
    try
    {
        RelayCore::log::init({.enable_timestamped_log = false, .enable_latest_log = false});
        auto level = RelayCore::load_blockout_level(argv[1]);
        const auto source = RelayGame::world_irradiance_source(level);
        if (!source)
            throw std::runtime_error("level requires an irradiance_volume");
        std::erase_if(level.entities, [](const auto& e) {
            return e.classname == "irradiance_volume";
        });
        const auto authoring = RelayCore::render::load_render_authoring_config(argv[2]);
        const auto prepared = RelayGame::build_prepared_world_scene_data(
            level,
            authoring,
            RelayCore::render::ColorPipelineMode::hardware_srgb
        );
        const auto inputs = RelayGame::build_world_irradiance_scene(prepared, authoring);
        if (std::filesystem::exists(argv[3]))
        {
            const auto existing = RelayCore::render::read_irradiance_probes(argv[3]);
            if (RelayCore::render::irradiance_inputs_match(existing, inputs, source->settings))
            {
                std::cout << "reused " << existing.probes.size() << " probes; signature=" << existing.source_signature
                          << '\n';
                RelayCore::log::shutdown();
                return 0;
            }
        }
        const auto volume = RelayCore::render::bake_irradiance_probes(inputs, source->settings);
        std::filesystem::create_directories(std::filesystem::path{argv[3]}.parent_path());
        RelayCore::render::write_irradiance_probes(argv[3], volume);
        std::cout << "baked " << volume.probes.size() << " probes, " << inputs.triangles.size() << " triangles, "
                  << inputs.lights.size() << " static lights; signature=" << volume.source_signature << '\n';
    } catch (const std::exception& error)
    {
        std::cerr << "irradiance bake: " << error.what() << '\n';
        RelayCore::log::shutdown();
        return 1;
    }
    RelayCore::log::shutdown();
}

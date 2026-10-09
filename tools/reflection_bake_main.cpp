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
        std::cerr << "usage: relaycore_reflection_bake level.toml render_authoring.toml output.rcreflection\n";
        return 1;
    }
    try
    {
        RelayCore::log::init({.enable_timestamped_log = false, .enable_latest_log = false});
        auto level = RelayCore::load_blockout_level(argv[1]);
        const auto source = RelayGame::world_reflection_source(level);
        if (!source)
            throw std::runtime_error("level requires a reflection_probe");
        std::erase_if(level.entities, [](const auto& e) {
            return e.classname == "reflection_probe" || e.classname == "irradiance_volume";
        });
        const auto authoring = RelayCore::render::load_render_authoring_config(argv[2]);
        const auto prepared = RelayGame::build_prepared_world_scene_data(
            level,
            authoring,
            RelayCore::render::ColorPipelineMode::hardware_srgb
        );
        const auto inputs = RelayGame::build_world_irradiance_scene(prepared, authoring);
        const auto ambient = glm::vec3{authoring.lighting.ambient_floor};
        const auto signature = RelayCore::render::reflection_source_signature(inputs, source->settings, ambient);
        if (std::filesystem::exists(argv[3]))
        {
            const auto existing = RelayCore::render::read_reflection_probe(argv[3]);
            if (existing.settings == source->settings && existing.source_signature == signature)
            {
                std::cout << "reused reflection cube; signature=" << signature << '\n';
                RelayCore::log::shutdown();
                return 0;
            }
        }
        const auto probe = RelayCore::render::bake_reflection_probe(inputs, source->settings, ambient);
        std::filesystem::create_directories(std::filesystem::path{argv[3]}.parent_path());
        auto temporary = std::filesystem::path{argv[3]};
        temporary += ".tmp";
        RelayCore::render::write_reflection_probe(temporary, probe);
        std::filesystem::rename(temporary, argv[3]);
        std::cout << "baked reflection cube " << probe.settings.resolution << ", " << probe.mips.size() << " GGX mips, "
                  << inputs.triangles.size() << " static triangles; signature=" << signature << '\n';
    } catch (const std::exception& error)
    {
        std::cerr << "reflection bake: " << error.what() << '\n';
        RelayCore::log::shutdown();
        return 1;
    }
    RelayCore::log::shutdown();
}

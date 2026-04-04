#include "relaygame/demo_game.h"

#include <algorithm>
#include <glm/mat4x4.hpp>

#include "relaycore/app/engine_services.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/core/profiling.h"
#include "relaycore/render/blockout_mesh_builder.h"
#include "relaycore/render/renderer.h"
#include "relaycore/world/blockout_loader.h"

namespace RelayGame
{
    void DemoGame::on_startup(RelayCore::EngineServices& services)
    {
        const auto level_path = RelayCore::core::asset_path("levels/test_room.toml");
        level_                = RelayCore::load_blockout_level(level_path);
        blockout_mesh_.initialize(RelayCore::render::build_blockout_mesh(level_));

        glm::vec3  spawn_position{0.0F, 0.3F, 0.0F};
        const auto spawn_it =
            std::find_if(level_.entities.begin(), level_.entities.end(), [](const RelayCore::EntitySpawnData& entity) {
                return entity.classname == "player_spawn";
            });

        if (spawn_it != level_.entities.end()) { spawn_position = spawn_it->origin; }

        player_controller_.initialize(spawn_position);
        player_controller_.enable_capture(services.window());
        player_controller_.apply_camera(services.camera());

        RelayCore::log::info(
            "DemoGame loaded level: {} solids, {} entities", level_.solids.size(), level_.entities.size()
        );
    }

    void DemoGame::on_event(const SDL_Event& event, RelayCore::EngineServices& services)
    {
        player_controller_.on_event(event, services.window(), services);
    }

    void DemoGame::on_update(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::on_update");
        player_controller_.update(frame, level_);
        player_controller_.apply_camera(services.camera());
    }

    void DemoGame::on_render(const RelayCore::FrameInfo& /*frame*/, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::on_render");
        services.renderer().draw_color_mesh(blockout_mesh_, glm::mat4{1.0F});
    }

    void DemoGame::on_shutdown(RelayCore::EngineServices& /*services*/) noexcept
    {
        RelayCore::log::info("DemoGame shutdown");
    }

    auto make_demo_game() -> std::unique_ptr<RelayCore::AppDelegate> { return std::make_unique<DemoGame>(); }
} // namespace RelayGame

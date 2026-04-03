#include "relaygame/demo_game.h"

#include <glm/mat4x4.hpp>

#include "relaycore/app/engine_services.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/render/blockout_mesh_builder.h"
#include "relaycore/render/renderer.h"
#include "relaycore/world/blockout_loader.h"

namespace RelayGame
{
    void DemoGame::on_startup(RelayCore::EngineServices& services)
    {
        fly_camera_.enable_capture(services.window());

        const auto level_path = RelayCore::core::asset_path("levels/test_room.toml");
        level_                = RelayCore::load_blockout_level(level_path);
        blockout_mesh_.initialize(RelayCore::render::build_blockout_mesh(level_));

        RelayCore::log::info(
            "DemoGame loaded level: {} solids, {} entities",
            level_.solids.size(),
            level_.entities.size());
    }

    void DemoGame::on_event(const SDL_Event& event, RelayCore::EngineServices& services)
    {
        fly_camera_.on_event(event, services.window(), services);
    }

    void DemoGame::on_update(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services)
    {
        fly_camera_.update(frame, services.camera());
    }

    void DemoGame::on_render(const RelayCore::FrameInfo& /*frame*/, RelayCore::EngineServices& services)
    {
        services.renderer().draw_color_mesh(blockout_mesh_, glm::mat4{1.0F});
    }

    void DemoGame::on_shutdown(RelayCore::EngineServices& /*services*/) noexcept
    {
        RelayCore::log::info("DemoGame shutdown");
    }

    auto make_demo_game() -> std::unique_ptr<RelayCore::AppDelegate> { return std::make_unique<DemoGame>(); }
} // namespace RelayGame

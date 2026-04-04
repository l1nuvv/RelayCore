#include "relaygame/demo_game.h"

#include "relaycore/app/engine_services.h"
#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/render/renderer.h"

namespace RelayGame
{
    void DemoGame::on_startup(RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::on_startup");

        runtime_.startup(services);
        world_scene_.initialize(runtime_.level());

        services.renderer().set_world_diagnostics(world_scene_.diagnostics());

        runtime_.publish_diagnostics(services.renderer());
        RelayCore::log::info(
            "DemoGame loaded level: {} solids, {} entities, {} render chunks, {} triangles, {} doors, {} triggers, {} "
            "usables",
            runtime_.level().solids.size(),
            runtime_.level().entities.size(),
            world_scene_.diagnostics().visible_solids,
            world_scene_.diagnostics().triangles,
            runtime_.doors().size(),
            runtime_.triggers().size(),
            runtime_.usables().size()
        );
    }

    void DemoGame::on_event(const SDL_Event& event, RelayCore::EngineServices& services)
    {
        runtime_.on_event(event, services);
    }

    void DemoGame::on_update(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::on_update");
        runtime_.update(frame, services);
        runtime_.publish_diagnostics(services.renderer());
    }

    void DemoGame::on_render(const RelayCore::FrameInfo& /*frame*/, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::on_render");
        world_scene_.render(
            services.renderer(),
            services.camera(),
            runtime_.doors(),
            runtime_.usables(),
            runtime_.focused_usable_index()
        );
        services.renderer().set_world_diagnostics(world_scene_.diagnostics());
    }

    void DemoGame::on_shutdown(RelayCore::EngineServices& /*services*/) noexcept
    {
        world_scene_.shutdown();
        RelayCore::log::info("DemoGame shutdown");
    }

    auto make_demo_game() -> std::unique_ptr<RelayCore::AppDelegate>
    {
        return std::make_unique<DemoGame>();
    }
} // namespace RelayGame

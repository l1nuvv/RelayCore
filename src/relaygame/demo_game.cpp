#include "relaygame/demo_game.h"

#include "relaycore/app/engine_services.h"
#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/render/renderer.h"
#include "relaygame/demo_game_runtime_ops.h"

namespace RelayGame
{
    void DemoGame::start_level_session(RelayCore::EngineServices& services)
    {
        detail::start_level_session(
            detail::DemoGameRuntimeState{
                .runtime = runtime_,
                .world_scene = world_scene_,
                .iteration_diagnostics = iteration_diagnostics_,
                .saved_runtime_snapshot = saved_runtime_snapshot_,
            },
            services
        );
    }

    void DemoGame::reload_live_content(RelayCore::EngineServices& services)
    {
        detail::reload_live_content(
            detail::DemoGameRuntimeState{
                .runtime = runtime_,
                .world_scene = world_scene_,
                .iteration_diagnostics = iteration_diagnostics_,
                .saved_runtime_snapshot = saved_runtime_snapshot_,
            },
            services
        );
    }

    void DemoGame::handle_debug_iteration_tools(RelayCore::EngineServices& services)
    {
        detail::handle_debug_iteration_tools(
            detail::DemoGameRuntimeState{
                .runtime = runtime_,
                .world_scene = world_scene_,
                .iteration_diagnostics = iteration_diagnostics_,
                .saved_runtime_snapshot = saved_runtime_snapshot_,
            },
            services
        );
    }

    void DemoGame::publish_iteration_diagnostics(RelayCore::EngineServices& services)
    {
        detail::publish_iteration_diagnostics(
            detail::DemoGameRuntimeState{
                .runtime = runtime_,
                .world_scene = world_scene_,
                .iteration_diagnostics = iteration_diagnostics_,
                .saved_runtime_snapshot = saved_runtime_snapshot_,
            },
            services
        );
    }

    void DemoGame::on_startup(RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::on_startup");
        start_level_session(services);
    }

    void DemoGame::on_event(const SDL_Event& event, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_FUNCTION();
        runtime_.on_event(event, services);
    }

    void DemoGame::on_update(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::on_update");
        runtime_.update(frame, services);
        runtime_.publish_diagnostics(services.renderer());
        handle_debug_iteration_tools(services);
    }

    void DemoGame::on_render(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::on_render");
        services.renderer().set_scene_time(static_cast<float>(frame.elapsed_seconds));
        world_scene_.render(
            services.renderer(),
            services.camera(),
            static_cast<float>(frame.elapsed_seconds),
            runtime_.doors(),
            runtime_.turrets(),
            runtime_.usables(),
            runtime_.focused_usable_index(),
            services.renderer().inspector_diagnostics(),
            services.renderer().inspector_visualization_enabled()
        );
        services.renderer().set_world_diagnostics(world_scene_.diagnostics());
    }

    void DemoGame::on_shutdown(RelayCore::EngineServices& services) noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        runtime_.shutdown(services);
        world_scene_.shutdown();
        saved_runtime_snapshot_.reset();
        RelayCore::log::info("DemoGame shutdown");
    }

    auto make_demo_game() -> std::unique_ptr<RelayCore::AppDelegate>
    {
        RELAYCORE_PROFILE_FUNCTION();
        return std::make_unique<DemoGame>();
    }
} // namespace RelayGame

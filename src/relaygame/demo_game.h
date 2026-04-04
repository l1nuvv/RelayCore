#pragma once

#include "relaycore/app/app_delegate.h"
#include "relaygame/game_world_runtime.h"
#include "relaygame/world_scene_renderer.h"

#include <memory>

namespace RelayGame
{
    class DemoGame final : public RelayCore::AppDelegate {
    public:
        void on_startup(RelayCore::EngineServices& services) override;

        void on_event(const SDL_Event& event, RelayCore::EngineServices& services) override;

        void on_update(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services) override;

        void on_render(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services) override;

        void on_shutdown(RelayCore::EngineServices& services) noexcept override;

    private:
        RelayGame::GameWorldRuntime runtime_{};
        RelayGame::WorldSceneRenderer world_scene_{};
    };

    [[nodiscard]] auto make_demo_game() -> std::unique_ptr<RelayCore::AppDelegate>;
} // namespace RelayGame

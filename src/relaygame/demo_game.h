#pragma once

#include <memory>

#include "relaycore/app/app_delegate.h"
#include "relaygame/player_controller.h"
#include "relaycore/render/mesh.h"
#include "relaycore/world/blockout_level.h"

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
        RelayGame::GroundedPlayerController player_controller_{};
        RelayCore::BlockoutLevel           level_{};
        RelayCore::render::StaticColorMesh blockout_mesh_{};
    };

    [[nodiscard]] auto make_demo_game() -> std::unique_ptr<RelayCore::AppDelegate>;
} // namespace RelayGame

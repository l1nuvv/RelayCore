#pragma once

#include "relaycore/app/app_delegate.h"
#include "relaycore/gameplay/runtime_model.h"
#include "relaycore/render/render_diagnostics.h"
#include "relaygame/game_world_runtime.h"
#include "relaygame/world_scene_renderer.h"

#include <memory>
#include <optional>

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
        void start_level_session(RelayCore::EngineServices& services);

        void reload_live_content(RelayCore::EngineServices& services);

        void handle_debug_iteration_tools(RelayCore::EngineServices& services);

        void publish_iteration_diagnostics(RelayCore::EngineServices& services);

        RelayGame::GameWorldRuntime runtime_{};
        RelayGame::WorldSceneRenderer world_scene_{};
        RelayCore::render::IterationDiagnostics iteration_diagnostics_{};
        std::optional<RelayCore::gameplay::RuntimeLevelSnapshot> saved_runtime_snapshot_{};
    };

    [[nodiscard]] auto make_demo_game() -> std::unique_ptr<RelayCore::AppDelegate>;
} // namespace RelayGame

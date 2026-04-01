#pragma once

#include <memory>

#include "relaycore/app/app_delegate.hpp"
#include "relaycore/app/fly_camera_controller.hpp"

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
        RelayCore::FlyCameraController fly_camera_{};
        float                          cube_angle_a_ = 0.0F;
        float                          cube_angle_b_ = 0.0F;
    };

    [[nodiscard]] auto make_demo_game() -> std::unique_ptr<RelayCore::AppDelegate>;
} // namespace RelayGame

#pragma once

#include <cstdint>
#include <glm/vec3.hpp>
#include <memory>
#include <vector>

#include "relaycore/app/app_delegate.h"
#include "relaycore/render/mesh.h"
#include "relaycore/world/blockout_level.h"
#include "relaygame/player_controller.h"

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
        struct RenderChunk {
            RelayCore::render::StaticColorMesh mesh{};
            glm::vec3                          mins{};
            glm::vec3                          maxs{};
            std::uint32_t                      triangle_count = 0;
        };

        RelayGame::GroundedPlayerController player_controller_{};
        RelayCore::BlockoutLevel            level_{};
        std::vector<RenderChunk>            render_chunks_{};
        std::uint32_t                       total_solids_         = 0;
        std::uint32_t                       visible_solids_       = 0;
        std::uint32_t                       draw_calls_per_frame_ = 0;
        std::uint32_t                       triangles_per_frame_  = 0;
    };

    [[nodiscard]] auto make_demo_game() -> std::unique_ptr<RelayCore::AppDelegate>;
} // namespace RelayGame

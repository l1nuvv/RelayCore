#include "relaygame/demo_game.hpp"

#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "relaycore/app/engine_services.hpp"
#include "relaycore/core/log.hpp"
#include "relaycore/core/paths.hpp"
#include "relaycore/render/renderer.hpp"
#include "relaycore/world/blockout_loader.hpp"

namespace RelayGame
{
    void DemoGame::on_startup(RelayCore::EngineServices& services)
    {
        fly_camera_.enable_capture(services.window());

        const auto level_path = RelayCore::core::asset_path("levels/test_room.toml");
        level_ = RelayCore::load_blockout_level(level_path);

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
        cube_angle_a_ += static_cast<float>(frame.delta_seconds) * 0.85F;
        cube_angle_b_ += static_cast<float>(frame.delta_seconds) * 0.45F;
    }

    void DemoGame::on_render(const RelayCore::FrameInfo& /*frame*/, RelayCore::EngineServices& services)
    {
        glm::mat4 model(1.0F);
        model = glm::rotate(model, cube_angle_a_, glm::vec3(0.25F, 1.0F, 0.10F));
        model = glm::rotate(model, cube_angle_b_, glm::vec3(1.0F, 0.15F, 0.30F));
        services.renderer().draw_color_cube(model);
    }

    void DemoGame::on_shutdown(RelayCore::EngineServices& /*services*/) noexcept
    {
        RelayCore::log::info("DemoGame shutdown");
    }

    auto make_demo_game() -> std::unique_ptr<RelayCore::AppDelegate>
    {
        return std::make_unique<DemoGame>();
    }
} // namespace RelayGame

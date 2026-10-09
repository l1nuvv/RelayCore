#include "relaycore/core/engine_config.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/renderer.h"
#include "relaygame/level_session_builder.h"
#include "relaygame/world_scene_renderer.h"

#include <SDL3/SDL.h>
#include <array>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
        return 77;
    int result = 0;
    const auto directory = std::filesystem::current_path() / "containment-captures";
    std::filesystem::create_directories(directory);
    RelayCore::log::init({.logs_directory = (directory / "logs").string(), .enable_timestamped_log = false});
    try
    {
        auto config = RelayCore::core::load_default_engine_config();
        config.window.width = 1280;
        config.window.height = 720;
        config.window.vsync = false;
        config.renderer.imgui.enabled = false;
        config.renderer.hdr_enabled = argc > 1 && std::string_view{argv[1]} == "--hdr";
        RelayCore::Window window{config.window};
        SDL_HideWindow(window.handle());
        RelayCore::Renderer renderer{config.renderer, RelayCore::render::ColorPipelineMode::hardware_srgb};
        renderer.initialize(window);
        const auto session = RelayGame::LevelSessionBuilder{config.gameplay}.build(
            RelayCore::core::asset_path("levels/containment_loop.toml")
        );
        RelayGame::WorldSceneRenderer scene;
        scene.initialize(
            RelayGame::build_prepared_world_scene_data(
                session.level,
                renderer.authoring_config(),
                renderer.color_pipeline_mode()
            ),
            renderer.static_models()
        );
        renderer.prepare_resources(scene.resource_items());
        struct View {
            const char* name;
            glm::vec3 eye, target;
        };
        const std::array views{
            View{"observation", {0, 1.65F, 9}, {0, 2, 34}},
            View{"service", {-16, 1.65F, 14}, {-20, 1.5F, 24}},
            View{"archive", {-26, 1.65F, 42}, {-16, 1.4F, 49}},
            View{"west-stairs", {-24, 1.65F, 29}, {-24, 4.4F, 40.5F}},
            View{"upper-return", {-10, 5.65F, 40.5F}, {0, 2, 28}},
            View{"mechanical-deck", {21.5F, 5.65F, 37}, {24, 4.8F, 32}},
            View{"east-stairs", {18, 1.65F, 13.5F}, {18, 4.2F, 25}},
            View{"frame", {-7, 1.65F, 24}, {0, 3, 33}},
            View{"exit", {0, 1.65F, 39}, {0, 1.65F, 49}},
        };
        for (const auto& view : views)
        {
            renderer.camera().set_position(view.eye);
            renderer.camera().set_target(view.target);
            renderer.begin_frame();
            scene.render(renderer, renderer.camera(), 0, session.doors, {}, session.usables, -1, {}, false);
            auto pixels = renderer.capture_frame();
            auto* surface = SDL_CreateSurfaceFrom(
                pixels.width,
                pixels.height,
                SDL_PIXELFORMAT_RGBA32,
                pixels.pixels.data(),
                pixels.width * 4
            );
            if (!surface)
                throw std::runtime_error(SDL_GetError());
            const auto path =
                directory / (std::string{view.name} + (config.renderer.hdr_enabled ? "-hdr.bmp" : "-sdr.bmp"));
            const bool written = SDL_SaveBMP(surface, path.string().c_str());
            SDL_DestroySurface(surface);
            if (!written)
                throw std::runtime_error(SDL_GetError());
        }
        scene.shutdown();
        std::cout << "containment graybox: 9 gameplay views rendered\n";
    } catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    RelayCore::log::shutdown();
    SDL_Quit();
    return result;
}

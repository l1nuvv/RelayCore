#include "relaycore/app/engine_services.h"
#include "relaycore/audio/audio_system.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/renderer.h"
#include "relaygame/demo_game.h"
#include "relaygame/usable_interaction.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <glad/gl.h>
#include <iostream>
#include <stdexcept>

#include "scoped_temp_directory.h"

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    void capture_frame(const char* filename)
    {
        std::vector<unsigned char> pixels(640 * 480 * 3);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, 640, 480, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
        require(glGetError() == GL_NO_ERROR, "Frame capture failed");
        for (int y = 0; y < 240; ++y)
            std::swap_ranges(
                pixels.begin() + y * 640 * 3,
                pixels.begin() + (y + 1) * 640 * 3,
                pixels.begin() + (479 - y) * 640 * 3
            );
        SDL_Surface* surface = SDL_CreateSurfaceFrom(640, 480, SDL_PIXELFORMAT_RGB24, pixels.data(), 640 * 3);
        require(surface != nullptr, "Capture surface failed");
        const bool written = SDL_SaveBMP(surface, filename);
        SDL_DestroySurface(surface);
        require(written, "Capture save failed");
    }
} // namespace

int main()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
        return 77;
    int result = 0;
    RelayCore::test::ScopedTempDirectory directory;
    RelayCore::log::init({.logs_directory = (directory.path() / "logs").string(), .enable_timestamped_log = false});
    try
    {
        auto config = RelayCore::core::load_default_engine_config();
        config.window.width = 640;
        config.window.height = 480;
        config.window.vsync = false;
        config.renderer.imgui.enabled = true;
        config.audio.enabled = false;
        config.gameplay.level_path = "levels/test_room.toml";
        RelayCore::Window window{config.window};
        SDL_HideWindow(window.handle());
        RelayCore::Renderer renderer{config.renderer};
        renderer.initialize(window);
        renderer.on_resize(640, 480);
        RelayCore::AudioSystem audio{config.audio};
        bool running = true;
        RelayCore::EngineServices services{config, window, renderer, audio, running};
        RelayGame::DemoGame game{directory.path() / "player"};
        const auto progress_path = directory.path() / "player/progress.toml";
        const RelayCore::FrameInfo frame{.delta_seconds = 1.0 / 60.0};
        {
            const auto mesh = RelayCore::core::asset_path("models/service_cylinder.obj");
            const auto texture = RelayCore::core::asset_path("models/service_cylinder.bmp");
            auto& models = renderer.static_models();
            const auto resource = models.load(mesh, texture, RelayCore::render::TextureEncoding::linear);
            for (const char* map : {"test_room", "power_restoration"})
            {
                const auto level = RelayGame::LevelSessionBuilder{config.gameplay}.build(
                    RelayCore::core::asset_path(std::string("levels/") + map + ".toml")
                );
                const auto data = RelayGame::build_prepared_world_scene_data(
                    level.level,
                    renderer.authoring_config(),
                    renderer.color_pipeline_mode()
                );
                require(data.props.size() == 2, "Two model instances expected in each model acceptance map");
                RelayGame::WorldSceneRenderer scene;
                scene.initialize(data, models);
                scene.initialize(data, models);
                require(
                    models.load(mesh, texture, RelayCore::render::TextureEncoding::linear) == resource,
                    "Scene reload duplicated unchanged model resource"
                );
                renderer.camera().set_position({-2.5F, 1.9F, -4.1F});
                renderer.camera().set_target({-4, .9F, -3.15F});
                renderer.begin_frame();
                scene.render(renderer, renderer.camera(), 0, {}, {}, {}, -1, {}, false);
                std::vector<unsigned char> pixels(640 * 480 * 3);
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glReadBuffer(GL_BACK);
                glReadPixels(0, 0, 640, 480, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
                require(glGetError() == GL_NO_ERROR, "Static-model frame produced GL error");
                for (int y = 0; y < 240; ++y)
                    std::swap_ranges(
                        pixels.begin() + y * 640 * 3,
                        pixels.begin() + (y + 1) * 640 * 3,
                        pixels.begin() + (479 - y) * 640 * 3
                    );
                SDL_Surface* surface = SDL_CreateSurfaceFrom(640, 480, SDL_PIXELFORMAT_RGB24, pixels.data(), 640 * 3);
                require(surface != nullptr, "Cannot create model screenshot");
                const bool saved = SDL_SaveBMP(surface, (std::string("model_") + map + ".bmp").c_str());
                SDL_DestroySurface(surface);
                require(saved, "Cannot save model screenshot");
            }
            for (const char* map : {"foundation_intake", "foundation_pump"})
            {
                const auto level = RelayGame::LevelSessionBuilder{config.gameplay}.build(
                    RelayCore::core::asset_path(std::string("levels/") + map + ".toml")
                );
                require(level.enemies.size() == 1, "Foundation map needs one rover");
                RelayGame::WorldSceneRenderer scene;
                scene.initialize(
                    RelayGame::build_prepared_world_scene_data(
                        level.level,
                        renderer.authoring_config(),
                        renderer.color_pipeline_mode()
                    ),
                    models
                );
                const auto enemy = level.enemies.front().state.position;
                renderer.camera().set_position(enemy + glm::vec3{0, 1.2F, -2.8F});
                renderer.camera().set_target(enemy + glm::vec3{0, .5F, 0});
                renderer.begin_frame();
                scene.render(renderer, renderer.camera(), 0, {}, {}, {}, -1, {}, false, {}, {}, {}, level.enemies);
                capture_frame((std::string("rover_") + map + ".bmp").c_str());
            }
            const auto copied = directory.path() / "reload.obj";
            std::filesystem::copy_file(mesh, copied);
            const auto before = models.load(copied, texture, RelayCore::render::TextureEncoding::linear);
            std::filesystem::last_write_time(
                copied,
                std::filesystem::last_write_time(copied) + std::chrono::seconds(1)
            );
            const auto after = models.load(copied, texture, RelayCore::render::TextureEncoding::linear);
            require(
                before != after && before->mesh.index_count() == after->mesh.index_count(),
                "Changed source must replace cache resource while old scenes retain theirs"
            );
            bool missing_rejected = false;
            try
            {
                static_cast<void>(
                    models.load(directory.path() / "missing.obj", texture, RelayCore::render::TextureEncoding::linear)
                );
            } catch (const std::exception& error)
            {
                missing_rejected = std::string(error.what()).find("missing.obj") != std::string::npos;
            }
            require(missing_rejected, "Missing model must identify its path");
            bool texture_rejected = false;
            try
            {
                static_cast<void>(
                    models.load(mesh, directory.path() / "missing.bmp", RelayCore::render::TextureEncoding::linear)
                );
            } catch (const std::exception& error)
            {
                texture_rejected = std::string(error.what()).find("missing.bmp") != std::string::npos;
            }
            require(texture_rejected, "Missing texture must identify its path");
            std::cout << "Static model: two maps, shared resource, reload and missing files passed\n";
        }
        const auto key = [&](SDL_Scancode code) {
            SDL_Event event{};
            event.type = SDL_EVENT_KEY_DOWN;
            event.key.scancode = code;
            game.on_event(event, services);
        };
        game.on_startup(services);
        renderer.set_diagnostics_visible(true);
        require(game.has_custom_ui() && !window.relative_mouse_mode(), "Startup must show menu and release mouse");

        for (const auto* map : {"levels/test_room.toml", "levels/rc022_reference.toml"})
        {
            config.gameplay.level_path = map;
            game.begin_episode(false, services);
            require(!game.has_custom_ui() && window.relative_mouse_mode(), "New game failed to enter play");
            game.on_update(frame, services);
            key(SDL_SCANCODE_ESCAPE);
            const auto checkpoint = RelayGame::load_saved_game(progress_path);
            require(
                checkpoint.world.level_path == RelayCore::core::asset_path(map).string(),
                "New map retained old identity"
            );
            const auto paused_tick = renderer.runtime_diagnostics().simulation_tick;
            game.on_update(frame, services);
            require(game.has_custom_ui() && !window.relative_mouse_mode(), "Pause retained mouse capture");
            require(renderer.runtime_diagnostics().simulation_tick == paused_tick, "Pause advanced simulation");
            key(SDL_SCANCODE_ESCAPE);
            const auto campaign_camera = services.camera().position();
            game.enter_editor(services);
            require(game.has_custom_ui() && !window.relative_mouse_mode(), "Editor retained gameplay input");
            const auto editor_checkpoint = RelayGame::load_saved_game(progress_path);
            for (int preview = 0; preview < 2; ++preview)
            {
                game.begin_editor_preview(services);
                require(!game.has_custom_ui(), "Editor preview did not start");
                game.on_update(frame, services);
                game.enter_editor(services);
                require(game.has_custom_ui() && !window.relative_mouse_mode(), "Preview failed to return to editor");
            }
            const auto after_preview = RelayGame::load_saved_game(progress_path);
            require(
                after_preview.world.simulation_tick == editor_checkpoint.world.simulation_tick
                    && after_preview.player.position == editor_checkpoint.player.position
                    && after_preview.played_seconds == editor_checkpoint.played_seconds,
                "Editor preview overwrote campaign progress"
            );
            game.return_from_editor(services);
            require(
                !game.has_custom_ui() && services.camera().position() == campaign_camera,
                "Editor lost campaign camera"
            );
            require(
                renderer.runtime_diagnostics().simulation_tick == editor_checkpoint.world.simulation_tick,
                "Editor lost campaign simulation state"
            );
            game.return_to_menu(services);
            require(game.has_custom_ui() && !window.relative_mouse_mode(), "Menu retained gameplay input");
            require(renderer.runtime_diagnostics().level_name.empty(), "Menu retained old level diagnostics");
        }

        const auto saved = RelayGame::load_saved_game(progress_path);
        config.gameplay.level_path = "levels/missing-06-test.toml";
        game.begin_episode(false, services);
        require(game.has_custom_ui() && !window.relative_mouse_mode(), "Failed load left menu or captured mouse");
        require(
            RelayGame::load_saved_game(progress_path).world.registry_signature == saved.world.registry_signature,
            "Failed level load damaged progress"
        );
        game.begin_episode(true, services);
        require(!game.has_custom_ui(), "Continue did not use saved map identity");
        auto incompatible = saved;
        ++incompatible.level_fingerprint;
        bool rejected = false;
        const auto before_failure = services.camera().position();
        try
        {
            static_cast<void>(RelayGame::detail::prepare_level_session(
                config,
                RelayGame::LevelSessionBuilder{config.gameplay}.build(saved.world.level_path),
                renderer.authoring_config(),
                renderer.color_pipeline_mode(),
                renderer.static_models(),
                &incompatible
            ));
        } catch (const std::exception&)
        {
            rejected = true;
        }
        require(
            rejected && !game.has_custom_ui() && services.camera().position() == before_failure,
            "Incompatible save damaged live session"
        );
        game.on_shutdown(services);
        require(!window.relative_mouse_mode(), "Shutdown retained mouse capture");
        game.on_startup(services);
        game.begin_episode(true, services);
        require(!game.has_custom_ui(), "Restart failed to restore saved map");
        game.on_shutdown(services);

        std::ofstream(progress_path) << "format_version = 99\n";
        game.on_startup(services);
        game.begin_episode(true, services);
        require(game.has_custom_ui() && !window.relative_mouse_mode(), "Unsupported save started a session");
        game.on_shutdown(services);
        std::string saved_text;
        std::ifstream saved_file{progress_path};
        std::getline(saved_file, saved_text);
        require(saved_text == "format_version = 99", "Unsupported save was overwritten");
        std::cout << "game_lifecycle=passed (two maps, pause/menu, preview isolation, failed load, restart)\n";
        {
            const auto a = RelayCore::core::asset_path("levels/foundation_intake.toml");
            const auto b = RelayCore::core::asset_path("levels/foundation_pump.toml");
            const auto campaign_dir = directory.path() / "campaign";
            const auto campaign_path = campaign_dir / "progress.toml";
            RelayGame::GameWorldRuntime seed;
            seed.load_level_session(config, RelayGame::LevelSessionBuilder{config.gameplay}.build(a));
            auto start = seed.capture_saved_game(42, true);
            start.player.position = {0, .01F, 4.75F};
            start.world.inventory_item_ids = {"service_key"};
            start.world.player_combat.health = 73;
            start.world.doors[0].open_fraction = 1;
            start.world.doors[0].state = RelayCore::gameplay::DoorState::Open;
            start.world.usables[0].consumed = true;
            start.world.mission = {true, true, 3};
            RelayGame::write_saved_game(campaign_path, start);
            RelayGame::DemoGame campaign{campaign_dir};
            campaign.on_startup(services);
            campaign.begin_episode(true, services);
            campaign.on_update(frame, services);
            auto pump = RelayGame::load_saved_game(campaign_path);
            renderer.begin_frame();
            campaign.on_render(frame, services);
            capture_frame("campaign_pump.bmp");
            auto pump_session = RelayGame::LevelSessionBuilder{config.gameplay}.build(b);
            auto carried_inventory = pump.world.inventory_item_ids;
            std::vector<std::string> activated, signaled;
            require(
                RelayGame::apply_usable_interaction(
                    pump_session.usables[0],
                    pump_session.doors,
                    pump_session.usables,
                    carried_inventory,
                    activated,
                    signaled
                )
                    .primary_applied,
                "Key carried from the first map did not operate the second map panel"
            );
            require(std::filesystem::path(pump.world.level_path) == b, "Exit did not change map");
            require(
                pump.world.player_combat.health == 73
                    && pump.world.inventory_item_ids == start.world.inventory_item_ids,
                "Transition lost health or inventory"
            );
            require(
                pump.flashlight && pump.played_seconds >= 42 && pump.visited_levels.size() == 1,
                "Transition lost campaign state"
            );
            campaign.enter_editor(services);
            campaign.begin_editor_preview(services);
            campaign.enter_editor(services);
            campaign.return_from_editor(services);
            campaign.return_to_menu(services);
            campaign.on_shutdown(services);
            campaign.on_startup(services);
            campaign.begin_episode(true, services);
            require(!campaign.has_custom_ui(), "Campaign continue failed");
            const RelayGame::LevelExit back{{}, a, "return", false};
            campaign.transition_to(back, services);
            auto returned = RelayGame::load_saved_game(campaign_path);
            campaign.on_update(frame, services);
            renderer.begin_frame();
            campaign.on_render(frame, services);
            capture_frame("campaign_intake.bmp");
            require(
                std::filesystem::path(RelayGame::load_saved_game(campaign_path).world.level_path) == a,
                "Return entry immediately bounced into another transition"
            );
            require(
                std::filesystem::path(returned.world.level_path) == a
                    && returned.world.doors[0].state == RelayCore::gameplay::DoorState::Open,
                "Returning to map lost its door state"
            );
            require(
                returned.world.usables[0].consumed && returned.world.mission.completed,
                "Returning restored pickup or forgot mission"
            );
            require(
                returned.player.position.z == 3.5F && returned.world.player_combat.health == 73,
                "Return entry or health is incorrect"
            );
            campaign.transition_to({{}, directory.path() / "absent-map.toml", "arrival", true}, services);
            require(campaign.has_custom_ui(), "Failed transition must pause with its error");
            require(
                RelayGame::load_saved_game(campaign_path).world.level_path == returned.world.level_path,
                "Failed transition changed saved map"
            );
            RelayCore::log::flush();
            std::ifstream transition_log{RelayCore::log::latest_log_path()};
            std::string transition_error;
            for (std::string line; std::getline(transition_log, line);)
                if (line.find("Переход из '") != std::string::npos)
                    transition_error = line;
            require(
                transition_error.find(returned.world.level_path) != std::string::npos
                    && transition_error.find((directory.path() / "absent-map.toml").string()) != std::string::npos
                    && transition_error.find("вход 'arrival'") != std::string::npos,
                "Failed transition lost its source, destination, or entry"
            );
            campaign.on_shutdown(services);
            RelayGame::LevelEditor editor;
            editor.enter(RelayGame::LevelSessionBuilder{config.gameplay}.build(a).level, services);
            editor.new_map(services, true);
            const auto editor_maps = directory.path() / "editor_maps";
            std::filesystem::create_directories(editor_maps);
            editor.save_map(editor_maps / "room_a.toml", services);
            editor.new_map(services);
            const auto dirty_fingerprint = RelayGame::fingerprint_level(editor.document().level());
            bool protected_changes = false;
            try
            {
                editor.open_map(editor_maps / "room_a.toml", services);
            } catch (const std::exception&)
            {
                protected_changes = true;
            }
            require(protected_changes && editor.document().dirty(), "Map switch discarded unsaved document");
            editor.leave(services);
            editor.resume(services);
            require(
                RelayGame::fingerprint_level(editor.document().level()) == dirty_fingerprint
                    && editor.document().dirty(),
                "Preview return lost unsaved document"
            );
            editor.save_map(editor_maps / "room_b.toml", services);
            const auto relocated_editor_maps = directory.path() / "relocated_editor_maps";
            std::filesystem::rename(editor_maps, relocated_editor_maps);
            editor.open_map(relocated_editor_maps / "room_a.toml", services);
            require(
                editor.save_path() == relocated_editor_maps / "room_a.toml",
                "Editor did not adopt the moved map folder"
            );
            renderer.begin_frame();
            editor.render(frame, services);
            if (renderer.begin_ui(frame, audio, true))
            {
                editor.draw_ui(services);
                renderer.end_ui();
            }
            require(glGetError() == GL_NO_ERROR, "Editor UI/render produced GL error");
            capture_frame("editor_workshop.bmp");
            editor.shutdown();
            std::cout << "0.8/0.9: editor map roundtrip and campaign A -> B -> restart -> A passed\n";
        }
    } catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    RelayCore::log::shutdown();
    SDL_Quit();
    return result;
}

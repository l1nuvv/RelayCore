#include "relaycore/app/engine_services.h"
#include "relaycore/audio/audio_system.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/renderer.h"
#include "relaygame/demo_game.h"
#include "relaygame/demo_game_debug_commands_internal.h"
#include "relaygame/usable_interaction.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <imgui_internal.h>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#endif

#include "native_collision_fixture.h"
#include "scoped_temp_directory.h"

namespace
{
    auto read_rgb(RelayCore::Renderer& renderer) -> std::vector<unsigned char>
    {
        const auto image = renderer.capture_frame();
        std::vector<unsigned char> rgb(size_t(image.width) * image.height * 3);
        for (size_t i = 0; i < rgb.size() / 3; ++i)
            std::copy_n(image.pixels.data() + i * 4, 3, rgb.data() + i * 3);
        return rgb;
    }

    void require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    auto read_file(const std::filesystem::path& path) -> std::string
    {
        std::ifstream input{path, std::ios::binary};
        require(input.is_open(), "Cannot read test evidence");
        return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    }

    void capture_frame(RelayCore::Renderer& renderer, const char* filename)
    {
        auto pixels = read_rgb(renderer);
        SDL_Surface* surface = SDL_CreateSurfaceFrom(640, 480, SDL_PIXELFORMAT_RGB24, pixels.data(), 640 * 3);
        require(surface != nullptr, "Capture surface failed");
        const bool written = SDL_SaveBMP(surface, filename);
        SDL_DestroySurface(surface);
        require(written, "Capture save failed");
    }

    auto test_hdr_settings() -> int
    {
        if (!SDL_Init(SDL_INIT_VIDEO))
            return 77;
        int result = 0;
        RelayCore::test::ScopedTempDirectory directory;
        RelayCore::log::init({.logs_directory = (directory.path() / "logs").string(), .enable_timestamped_log = false});
        try
        {
            auto config = RelayCore::core::load_default_engine_config();
            config.window.width = 800;
            config.window.height = 900;
            config.window.vsync = false;
            config.renderer.imgui.enabled = true;
            config.audio.enabled = false;
            config.gameplay.level_path = "levels/power_restoration.toml";
            RelayCore::Window window{config.window};
            SDL_HideWindow(window.handle());
            RelayCore::Renderer renderer{config.renderer};
            renderer.initialize(window);
            renderer.on_resize(800, 900);
            RelayCore::AudioSystem audio{config.audio};
            bool running = true;
            RelayCore::EngineServices services{config, window, renderer, audio, running};
            const auto player_path = directory.path() / "player";
            RelayGame::DemoGame game{player_path};
            const RelayCore::FrameInfo frame{.delta_seconds = 1.0 / 60.0};
            game.on_startup(services);
            const auto draw = [&] {
                renderer.begin_frame();
                game.on_render(frame, services);
                require(renderer.begin_ui(frame, audio, true), "HDR settings UI was unavailable");
                game.on_ui(services);
                renderer.end_ui();
                return renderer.capture_frame();
            };
            const auto activate_menu = [&](const char* label) {
                auto* menu = ImGui::FindWindowByName("Пауза##player_pause");
                require(menu != nullptr, "Settings menu was not drawn");
                ImGui::ActivateItemByID(menu->GetID(label));
                (void) draw();
            };
            const auto choose_range = [&](const char* label) {
                activate_menu("Режим изображения");
                require(!ImGui::GetCurrentContext()->OpenPopupStack.empty(), "HDR/SDR selector did not open");
                auto* popup = ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
                require(popup != nullptr, "HDR/SDR selector popup was unavailable");
                const int index = std::string_view{label} == "HDR" ? 1 : 0;
                ImGui::ActivateItemByID(ImHashStr(label, 0, popup->GetID(index)));
                (void) draw();
            };
            const auto key = [&](SDL_Scancode code) {
                SDL_Event event{};
                event.type = SDL_EVENT_KEY_DOWN;
                event.key.scancode = code;
                game.on_event(event, services);
            };
            const auto scene_image = [&] {
                renderer.begin_frame();
                game.on_render(frame, services);
                return renderer.capture_frame();
            };
            const auto shadow_contrast = [&](const char* name) {
                auto shadowed = scene_image();
                auto unshadowed_config = renderer.config();
                unshadowed_config.shadow_enabled = false;
                renderer.commit_live_content_reload(
                    renderer.prepare_live_content_reload(unshadowed_config, renderer.authoring_config())
                );
                const auto unshadowed = scene_image();
                auto restored = unshadowed_config;
                restored.shadow_enabled = true;
                renderer.commit_live_content_reload(
                    renderer.prepare_live_content_reload(restored, renderer.authoring_config())
                );
                int pixels = 0, maximum = 0;
                long long total = 0;
                for (size_t i = 0; i < shadowed.pixels.size(); i += 4)
                {
                    int difference = 0;
                    for (int c = 0; c < 3; ++c)
                        difference = std::max(difference, int(unshadowed.pixels[i + c]) - int(shadowed.pixels[i + c]));
                    pixels += difference > 2;
                    maximum = std::max(maximum, difference);
                    total += difference;
                }
                auto* surface = SDL_CreateSurfaceFrom(
                    shadowed.width,
                    shadowed.height,
                    SDL_PIXELFORMAT_RGBA32,
                    shadowed.pixels.data(),
                    shadowed.width * 4
                );
                require(surface != nullptr, "Shadow comparison capture surface failed");
                const auto filename = std::string{"hdr-settings-shadow-"} + name + ".bmp";
                const bool captured = SDL_SaveBMP(surface, filename.c_str());
                SDL_DestroySurface(surface);
                require(captured, "Shadow comparison capture failed");
                std::cout << name << " shadows: pixels=" << pixels << "; max=" << maximum << "; total=" << total
                          << '\n';
                return total;
            };

            (void) draw();
            (void) shadow_contrast("SDR");
            const auto original_sdr = scene_image();
            activate_menu("Настройки");
            const auto before = draw();
            const auto camera = renderer.camera().position();
            const auto tone_shader = RelayCore::core::asset_path("shaders/vulkan/tone_mapping.frag.spv");
            auto backup = tone_shader;
            backup += ".settings-test";
            std::filesystem::rename(tone_shader, backup);
            try
            {
                choose_range("HDR");
                require(
                    !renderer.config().hdr_enabled && !config.renderer.hdr_enabled,
                    "Failed HDR switch changed the active configuration"
                );
                require(
                    renderer.color_pipeline_mode() == RelayCore::render::ColorPipelineMode::legacy_gamma,
                    "Failed HDR switch changed the color pipeline"
                );
            } catch (...)
            {
                std::filesystem::rename(backup, tone_shader);
                throw;
            }
            std::filesystem::rename(backup, tone_shader);
            choose_range("HDR");
            require(
                renderer.config().hdr_enabled && config.renderer.hdr_enabled,
                "Settings did not enable HDR without CLI arguments"
            );
            require(
                renderer.color_pipeline_mode() == RelayCore::render::ColorPipelineMode::hardware_srgb,
                "HDR settings did not select the linear color pipeline"
            );
            require(renderer.camera().position() == camera, "HDR switch moved the player camera");
            const auto hdr_shadows = shadow_contrast("HDR");
            const auto hdr_camera = renderer.camera();
            const auto capture_material = [&](const char* path, glm::vec3 eye, glm::vec3 target) {
                renderer.camera().set_position(eye);
                renderer.camera().set_target(target);
                auto image = scene_image();
                auto* surface = SDL_CreateSurfaceFrom(
                    image.width,
                    image.height,
                    SDL_PIXELFORMAT_RGBA32,
                    image.pixels.data(),
                    image.width * 4
                );
                require(surface != nullptr, "HDR material capture failed");
                const bool saved = SDL_SaveBMP(surface, path);
                SDL_DestroySurface(surface);
                require(saved, "HDR material capture save failed");
            };
            capture_material("hdr-settings-door.bmp", {0, 1.65F, .5F}, {0, 1.5F, 5});
            capture_material("hdr-settings-fan.bmp", {-2, 1.8F, 2.8F}, {-4.77F, 2.5F, 2.8F});
            capture_material("hdr-settings-ceiling.bmp", {0, 1.65F, 12}, {0, 3.2F, 16});
            renderer.camera() = hdr_camera;
            auto hdr_menu = draw();
            require(hdr_menu.pixels != before.pixels, "HDR switch did not change the rendered image");
            auto* surface = SDL_CreateSurfaceFrom(
                hdr_menu.width,
                hdr_menu.height,
                SDL_PIXELFORMAT_RGBA32,
                hdr_menu.pixels.data(),
                hdr_menu.width * 4
            );
            require(surface != nullptr, "HDR settings capture surface failed");
            const bool captured = SDL_SaveBMP(surface, "hdr-settings-menu.bmp");
            SDL_DestroySurface(surface);
            require(captured, "HDR settings menu capture failed");
            activate_menu("Сохранить настройки");
            require(
                RelayGame::load_player_settings(player_path / "player_settings.toml", {}).hdr_enabled,
                "HDR choice was not persisted"
            );
            choose_range("SDR");
            require(!renderer.config().hdr_enabled, "SDR settings did not disable tone mapping");
            require(
                renderer.color_pipeline_mode() == RelayCore::render::ColorPipelineMode::legacy_gamma,
                "Returning to SDR did not restore the startup color pipeline"
            );
            require(
                scene_image().pixels == original_sdr.pixels,
                "SDR -> HDR -> SDR changed the original scene colors or shadows"
            );
            game.begin_episode(false, services);
            key(SDL_SCANCODE_ESCAPE);
            (void) draw();
            activate_menu("Настройки");
            choose_range("HDR");
            require(game.has_custom_ui() && !window.relative_mouse_mode(), "HDR switch disturbed pause");
            key(SDL_SCANCODE_ESCAPE);
            key(SDL_SCANCODE_ESCAPE);
            require(!game.has_custom_ui(), "Game could not resume after HDR switch");
            game.return_to_menu(services);
            (void) draw();
            activate_menu("Настройки");
            choose_range("SDR");
            activate_menu("Сохранить настройки");
            game.on_shutdown(services);

            renderer.commit_live_content_reload(renderer.prepare_live_content_reload(
                config.renderer,
                renderer.authoring_config(),
                RelayCore::render::ColorPipelineMode::legacy_gamma
            ));
            game.on_startup(services);
            game.begin_episode(false, services);
            game.enter_editor(services);
            key(SDL_SCANCODE_ESCAPE);
            (void) draw();
            activate_menu("Настройки");
            choose_range("HDR");
            choose_range("SDR");
            require(
                renderer.color_pipeline_mode() == RelayCore::render::ColorPipelineMode::legacy_gamma,
                "Editor did not restore the SDR color pipeline"
            );
            choose_range("HDR");
            key(SDL_SCANCODE_ESCAPE);
            key(SDL_SCANCODE_ESCAPE);
            (void) draw();
            game.begin_editor_preview(services);
            require(!game.has_custom_ui(), "HDR switch prevented editor preview");
            (void) draw();
            game.return_from_editor(services);
            require(
                !game.has_custom_ui() && renderer.config().hdr_enabled,
                "HDR switch prevented restoring the suspended game"
            );
            game.return_to_menu(services);
            game.on_shutdown(services);

            config.renderer.hdr_enabled = false;
            renderer.commit_live_content_reload(renderer.prepare_live_content_reload(
                config.renderer,
                renderer.authoring_config(),
                RelayCore::render::ColorPipelineMode::legacy_gamma
            ));
            game.on_startup(services);
            require(
                renderer.config().hdr_enabled && config.renderer.hdr_enabled,
                "Restart did not restore HDR from player settings"
            );
            game.begin_episode(false, services);
            key(SDL_SCANCODE_F7);
            game.on_update(frame, services);
            require(
                renderer.iteration_diagnostics().last_reload_succeeded && renderer.config().hdr_enabled,
                "F7 discarded the player's HDR choice"
            );
            game.return_to_menu(services);
            (void) draw();
            activate_menu("Настройки");
            choose_range("SDR");
            activate_menu("Сохранить настройки");
            game.on_shutdown(services);
            game.on_startup(services);
            require(!renderer.config().hdr_enabled, "Restart did not restore SDR from player settings");
            game.return_to_menu(services);
            game.on_shutdown(services);

            renderer.commit_live_content_reload(renderer.prepare_live_content_reload(
                config.renderer,
                renderer.authoring_config(),
                RelayCore::render::ColorPipelineMode::hardware_srgb
            ));
            game.on_startup(services);
            (void) draw();
            const auto hardware_sdr = scene_image();
            require(
                hdr_shadows * 2 >= shadow_contrast("hardware-SDR"),
                "HDR washed out shadows relative to the same linear lighting in SDR"
            );
            activate_menu("Настройки");
            choose_range("HDR");
            choose_range("SDR");
            require(
                renderer.color_pipeline_mode() == RelayCore::render::ColorPipelineMode::hardware_srgb,
                "SDR switch discarded the explicitly selected startup color pipeline"
            );
            require(
                scene_image().pixels == hardware_sdr.pixels,
                "Hardware-sRGB SDR -> HDR -> SDR changed the original scene"
            );
            game.on_shutdown(services);
            std::cout << "HDR/SDR settings: UI, rollback, pause, editor, persistence and F7 passed\n";
        } catch (const std::exception& error)
        {
            std::cerr << error.what() << '\n';
            result = 1;
        }
        RelayCore::log::shutdown();
        SDL_Quit();
        return result;
    }
#ifdef _WIN32
    auto test_update_readiness(const std::string_view mode) -> int
    {
        if (!SDL_Init(SDL_INIT_VIDEO))
            return 77;
        int result = 0;
        RelayCore::test::ScopedTempDirectory directory;
        RelayCore::log::init({.logs_directory = (directory.path() / "logs").string(), .enable_timestamped_log = false});
        try
        {
            const bool failed = mode == "failed-ui" || mode == "failed-no-ui";
            const bool ui_enabled = mode == "ui" || mode == "failed-ui";
            require(failed || mode == "ui" || mode == "no-ui", "Unknown readiness test mode");
            const std::string event_name = "Local\\RelayCore-update-test-" + std::to_string(GetCurrentProcessId());
            const std::unique_ptr<void, decltype(&CloseHandle)> ready{
                CreateEventA(nullptr, TRUE, FALSE, event_name.c_str()),
                CloseHandle
            };
            require(ready != nullptr, "Cannot create readiness event");
            require(
                SDL_SetEnvironmentVariable(
                    SDL_GetEnvironment(),
                    "RELAYCORE_UPDATE_READY_EVENT",
                    event_name.c_str(),
                    true
                ),
                "Cannot set readiness event name"
            );
            auto config = RelayCore::core::load_default_engine_config();
            config.window.width = 640;
            config.window.height = 480;
            config.window.vsync = false;
            config.renderer.imgui.enabled = ui_enabled;
            config.audio.enabled = false;
            config.gameplay.level_path = failed ? "levels/missing-update-test.toml" : "levels/test_room.toml";
            RelayCore::Window window{config.window};
            SDL_HideWindow(window.handle());
            RelayCore::Renderer renderer{config.renderer};
            renderer.initialize(window);
            renderer.on_resize(640, 480);
            require(renderer.ui_available() == ui_enabled, "Readiness test UI availability differs from its mode");
            RelayCore::AudioSystem audio{config.audio};
            bool running = true;
            RelayCore::EngineServices services{config, window, renderer, audio, running};
            RelayGame::DemoGame game{directory.path() / "player"};
            bool startup_threw = false;
            try
            {
                game.on_startup(services);
            } catch (const std::exception&)
            {
                startup_threw = true;
            }
            require(startup_threw == (failed && !ui_enabled), "Unexpected startup failure handling");
            require(WaitForSingleObject(ready.get(), 0) == WAIT_TIMEOUT, "Readiness sent before a runtime frame");
            if (!startup_threw)
            {
                renderer.begin_frame();
                game.on_render({.delta_seconds = 1.0 / 60.0}, services);
            }
            const DWORD expected = failed ? WAIT_TIMEOUT : WAIT_OBJECT_0;
            require(
                WaitForSingleObject(ready.get(), 0) == expected,
                "Runtime readiness does not match startup success"
            );
            game.on_shutdown(services);
            std::cout << "Update readiness " << mode << " passed without on_ui\n";
        } catch (const std::exception& error)
        {
            std::cerr << error.what() << '\n';
            result = 1;
        }
        RelayCore::log::shutdown();
        SDL_Quit();
        return result;
    }
#endif
} // namespace

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view{argv[1]} == "--hdr-settings")
        return test_hdr_settings();
#ifdef _WIN32
    if (argc == 3 && std::string_view{argv[1]} == "--update-readiness")
        return test_update_readiness(argv[2]);
#endif
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
            renderer.begin_frame();
            const auto before_reload = renderer.capture_frame();
            require(renderer.begin_ui(frame, audio, true), "Vulkan UI was not initialized");
            auto changed = config.renderer;
            changed.clear_r = 0.8F;
            renderer.commit_live_content_reload(
                renderer.prepare_live_content_reload(changed, renderer.authoring_config())
            );
            renderer.end_ui();
            require(
                renderer.capture_frame().pixels == before_reload.pixels,
                "UI reload replaced the GPU target still owned by the current frame"
            );
            require(
                SDL_ShowWindow(window.handle()) && SDL_SyncWindow(window.handle()),
                "Could not show Vulkan surface"
            );
            bool presented = false;
            for (int retry = 0; retry < 8 && !presented; ++retry)
            {
                SDL_PumpEvents();
                presented = renderer.present();
            }
            require(
                presented && renderer.capture_frame().pixels == before_reload.pixels,
                "Presentation lost the scene target retained across UI reload"
            );
            require(SDL_HideWindow(window.handle()), "Could not hide lifecycle window");
            renderer.begin_frame();
            require(
                renderer.capture_frame().pixels != before_reload.pixels,
                "Committed Vulkan reload did not take effect on the next frame"
            );
            renderer.commit_live_content_reload(
                renderer.prepare_live_content_reload(config.renderer, renderer.authoring_config())
            );
        }
        {
            RelayGame::GameWorldRuntime encounter;
            encounter.load_level_session(
                config,
                RelayGame::LevelSessionBuilder{config.gameplay}.build(
                    RelayCore::core::asset_path("levels/power_restoration.toml")
                )
            );
            require(encounter.enemies().size() == 1, "Power restoration needs its reserve-room rover");
            auto initial = encounter.capture_saved_game(12, true);
            initial.enemies[0].mode = RelayGame::EnemyMode::Windup;
            initial.enemies[0].timer = .3F;
            initial.enemies[0].health = 25;
            initial.enemies[0].last_seen = initial.enemies[0].position + glm::vec3{0, 0, 1};
            initial.player.position = initial.enemies[0].last_seen;
            initial.player.velocity = {.2F, 0, -.1F};
            initial.player.view_angles = {.5F, -.1F};
            initial.player.grounded = true;
            initial.world.simulation_tick = 42;
            encounter.restore_saved_game(initial);
            encounter.prepare_level_session_activation(services);
            encounter.activate_level_session(services);
            RelayGame::WorldSceneRenderer encounter_scene;
            RelayCore::render::IterationDiagnostics diagnostics;
            std::optional<RelayGame::EncounterSnapshot> snapshot;
            float played = 12, scene_time = 11;
            bool flashlight = true;
            RelayGame::detail::DemoGameRuntimeState
                state{encounter, encounter_scene, diagnostics, snapshot, played, scene_time, flashlight};
            RelayGame::detail::IterationCommandRequests save_request, restore_request;
            save_request.runtime.save_runtime_snapshot = true;
            restore_request.runtime.restore_runtime_snapshot = true;
            const auto command = [&](const auto& request) {
                require(
                    RelayGame::detail::handle_runtime_snapshot_commands(state, services, request),
                    "Snapshot command was not handled"
                );
            };
            command(restore_request);
            require(
                !diagnostics.last_snapshot_succeeded && !diagnostics.last_snapshot_error.empty(),
                "Missing encounter snapshot was not reported"
            );
            command(save_request);
            require(snapshot.has_value() && diagnostics.last_snapshot_succeeded, "Encounter capture failed");
            const auto expected_path = directory.path() / "encounter-expected.toml";
            const auto actual_path = directory.path() / "encounter-actual.toml";
            RelayGame::write_saved_game(expected_path, snapshot->game);
            const auto saved_camera = services.camera().position();
            const auto saved_forward = services.camera().forward();

            auto changed = initial;
            changed.player.position.x += 2;
            changed.player.velocity = {};
            changed.player.view_angles = {-1, .2F};
            changed.enemies[0].health = 0;
            changed.enemies[0].mode = RelayGame::EnemyMode::Dead;
            changed.enemies[0].timer = 0;
            changed.enemies[0].position.z += 1;
            changed.world.player_combat.shots_fired = 3;
            changed.world.player_combat.health = 60;
            changed.world.doors.front().open_fraction = .5F;
            changed.world.doors.front().state = RelayCore::gameplay::DoorState::Opening;
            changed.world.inventory_item_ids = {"power_fuse"};
            changed.world.mission.current_step = 1;
            encounter.restore_saved_game(changed);
            require(
                encounter.debug_apply_player_fire(initial.player.position + glm::vec3{0, 1.6F, 0}, {0, 0, 1}).fired,
                "Test shot did not fire"
            );
            require(
                encounter.player_combat_state().shot_trace.remaining_seconds > 0,
                "Test shot needs a visible trace"
            );
            played = scene_time = 25;
            flashlight = false;
            command(restore_request);
            require(diagnostics.last_snapshot_succeeded, "Full encounter restore failed");
            RelayGame::write_saved_game(actual_path, encounter.capture_saved_game(played, flashlight));
            require(
                read_file(actual_path) == read_file(expected_path),
                "Encounter restore lost player motion, combat, rover, doors, inventory or mission state"
            );
            require(
                scene_time == 11 && services.camera().position() == saved_camera
                    && services.camera().forward() == saved_forward,
                "Encounter restore lost presentation time or camera"
            );
            require(
                encounter.player_combat_state().shot_trace.remaining_seconds == 0,
                "Encounter restore retained a trace from the discarded attempt"
            );
            for (std::size_t i = 0; i < snapshot->environment.size(); ++i)
            {
                const auto& actual = encounter.environment()[i];
                const auto& expected = snapshot->environment[i];
                require(
                    actual.age == expected.age && actual.elapsed == expected.elapsed && actual.phase == expected.phase
                        && actual.strength == expected.strength && actual.active == expected.active,
                    "Encounter restore changed equipment presentation state"
                );
            }

            const RelayCore::FrameInfo attack_frame{.delta_seconds = .1};
            encounter.update(attack_frame, services);
            const auto attack_timer = encounter.enemies()[0].state.timer;
            require(attack_timer < initial.enemies[0].timer, "Rover windup did not advance");
            RelayGame::write_saved_game(expected_path, encounter.capture_saved_game(played, flashlight));
            command(restore_request);
            encounter.update(attack_frame, services);
            RelayGame::write_saved_game(actual_path, encounter.capture_saved_game(played, flashlight));
            require(read_file(actual_path) == read_file(expected_path), "Repeated windup diverged after restore");

            const auto camera_before_failure = services.camera().position();
            snapshot->game.level_fingerprint ^= 1;
            command(restore_request);
            require(
                !diagnostics.last_snapshot_succeeded && !diagnostics.last_snapshot_error.empty(),
                "Incompatible encounter snapshot was not reported"
            );
            RelayGame::write_saved_game(actual_path, encounter.capture_saved_game(played, flashlight));
            require(
                read_file(actual_path) == read_file(expected_path)
                    && services.camera().position() == camera_before_failure,
                "Failed encounter restore changed the active state"
            );
            snapshot->game.level_fingerprint ^= 1;
            snapshot->game.player.position = {1.0e8F, 1.0e8F, 1.0e8F};
            command(restore_request);
            require(
                !diagnostics.last_snapshot_succeeded && !diagnostics.last_snapshot_error.empty(),
                "Invalid encounter camera was not reported"
            );
            RelayGame::write_saved_game(actual_path, encounter.capture_saved_game(played, flashlight));
            require(
                read_file(actual_path) == read_file(expected_path)
                    && services.camera().position() == camera_before_failure,
                "Camera preparation failure partially restored the encounter"
            );
            encounter.shutdown(services);
            std::cout << "Local encounter: full state, repeated rover windup and incompatible snapshot passed\n";
        }
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
                auto pixels = read_rgb(renderer);
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
                capture_frame(renderer, (std::string("rover_") + map + ".bmp").c_str());
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

        const auto native_map = directory.path() / "native_collision.toml";
        RelayCore::test::write_native_collision_fixture(native_map);
        const auto native_map_name = native_map.string();
        for (const auto* map :
             {"levels/test_room.toml", "levels/rc022_reference.toml", native_map_name.c_str(), "levels/bench_relay.toml"
             })
        {
            config.gameplay.level_path = map;
            config.input.capture_mouse_on_start = std::string_view{map} == "levels/test_room.toml";
            game.begin_episode(false, services);
            require(!game.has_custom_ui() && window.relative_mouse_mode(), "New game failed to enter play");
            if (std::string_view{map} == "levels/test_room.toml")
            {
                auto distant_level = RelayGame::LevelSessionBuilder{config.gameplay}.build();
                distant_level.spawn_position = {1.0e8F, 1.0e8F, 1.0e8F};
                auto distant_session = RelayGame::detail::prepare_level_session(
                    config,
                    std::move(distant_level),
                    renderer.authoring_config(),
                    renderer.color_pipeline_mode(),
                    renderer.static_models()
                );
                const auto camera_before = services.camera().position();
                bool invalid_camera_rejected = false;
                try
                {
                    distant_session.runtime.prepare_level_session_activation(services);
                } catch (const std::invalid_argument&)
                {
                    invalid_camera_rejected = true;
                }
                require(
                    invalid_camera_rejected && services.camera().position() == camera_before
                        && window.relative_mouse_mode(),
                    "Unrepresentable session camera changed the active game"
                );
            }
            game.on_update(frame, services);
            if (std::string_view{map} == native_map_name)
            {
                auto moving = RelayGame::load_saved_game(progress_path);
                moving.player.position = {0, .001F, 2};
                moving.player.velocity = {0, 0, 8};
                game.return_to_menu(services);
                game.on_shutdown(services);
                RelayGame::write_saved_game(progress_path, moving);
                game.on_startup(services);
                renderer.set_diagnostics_visible(true);
                game.begin_episode(true, services);
                for (int i = 0; i < 60; ++i)
                    game.on_update(frame, services);
                require(
                    renderer.player_diagnostics().position.z > 3.5F,
                    "Native opening blocked standing player traversal"
                );
                renderer.begin_frame();
                game.on_render(frame, services);
                capture_frame(renderer, "native_collision_traversal.bmp");
            }
            if (std::string_view{map} == "levels/test_room.toml" || std::string_view{map} == native_map_name)
            {
                const auto camera_before_reload = services.camera().position();
                const auto config_path = config.source_path;
                config.source_path = directory.path() / "invalid-engine-config.toml";
                {
                    std::ofstream invalid_config{config.source_path};
                    invalid_config << "broken = [\n";
                }
                key(SDL_SCANCODE_F7);
                game.on_update({}, services);
                config.source_path = config_path;
                require(
                    !game.has_custom_ui() && window.relative_mouse_mode()
                        && services.camera().position() == camera_before_reload,
                    "Failed reload disturbed the active session or input capture"
                );
                require(
                    renderer.iteration_diagnostics().has_last_reload_result
                        && !renderer.iteration_diagnostics().last_reload_succeeded,
                    "Failed reload was not reported"
                );
            }
            if (std::string_view{map} == native_map_name)
            {
                const auto authored = read_file(native_map);
                auto broken = authored;
                const std::string reference = "collision/lab_bench.rccollision";
                broken.replace(broken.find(reference), reference.size(), "collision/missing.rccollision");
                std::ofstream{native_map, std::ios::binary} << broken;
                const auto source_path = config.source_path;
                config.source_path = directory.path() / "native-reload.toml";
                std::ofstream{config.source_path} << "[game]\nlevel_path = \"" << native_map.generic_string() << "\"\n";
                const auto before = services.camera().position();
                key(SDL_SCANCODE_F7);
                game.on_update({}, services);
                require(
                    !renderer.iteration_diagnostics().last_reload_succeeded
                        && renderer.iteration_diagnostics().last_reload_error.find("missing.rccollision")
                               != std::string::npos
                        && services.camera().position() == before && !game.has_custom_ui(),
                    "Missing collision product damaged the active native session"
                );
                std::ofstream{native_map, std::ios::binary} << authored;
                key(SDL_SCANCODE_F7);
                game.on_update({}, services);
                require(
                    renderer.iteration_diagnostics().last_reload_succeeded,
                    "Native session did not recover after restoring collision reference"
                );
                config.source_path = source_path;
            }
            const auto disk_before_snapshot = read_file(progress_path);
            key(SDL_SCANCODE_F9);
            game.on_update({}, services);
            require(renderer.iteration_diagnostics().has_saved_runtime_snapshot, "F9 did not capture encounter");
            const auto snapshot_camera = services.camera().position();
            const auto snapshot_forward = services.camera().forward();
            const auto snapshot_tick = renderer.runtime_diagnostics().simulation_tick;
            SDL_Event look{};
            look.type = SDL_EVENT_MOUSE_MOTION;
            look.motion.xrel = 50;
            game.on_event(look, services);
            game.on_update(frame, services);
            key(SDL_SCANCODE_F10);
            game.on_update({}, services);
            require(
                renderer.iteration_diagnostics().last_snapshot_succeeded
                    && services.camera().position() == snapshot_camera
                    && services.camera().forward() == snapshot_forward
                    && renderer.runtime_diagnostics().simulation_tick == snapshot_tick,
                "F10 did not restore the captured encounter"
            );
            game.on_update({}, services);
            require(read_file(progress_path) == disk_before_snapshot, "F9/F10 overwrote disk progress");
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
            const auto campaign_forward = services.camera().forward();
            const auto campaign_yaw = services.camera().yaw_radians();
            game.enter_editor(services);
            require(game.has_custom_ui() && !window.relative_mouse_mode(), "Editor retained gameplay input");
            const auto editor_checkpoint = RelayGame::load_saved_game(progress_path);
            for (int preview = 0; preview < 2; ++preview)
            {
                game.begin_editor_preview(services);
                require(!game.has_custom_ui(), "Editor preview did not start");
                game.on_update(frame, services);
                key(SDL_SCANCODE_F9);
                key(SDL_SCANCODE_F10);
                game.on_update({}, services);
                require(
                    !renderer.iteration_diagnostics().has_saved_runtime_snapshot,
                    "Editor preview leaked a campaign encounter snapshot"
                );
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
            if (std::string_view{map} == "levels/test_room.toml")
            {
                services.camera().set_orientation(campaign_yaw + 1.5707963268F, 0.0F);
                auto incompatible_renderer = config.renderer;
                incompatible_renderer.camera.world_up = campaign_forward;
                renderer.commit_live_content_reload(
                    renderer.prepare_live_content_reload(incompatible_renderer, renderer.authoring_config())
                );
                game.return_from_editor(services);
                require(
                    game.has_custom_ui() && !window.relative_mouse_mode(),
                    "Failed editor return lost the editor or captured input"
                );
                RelayCore::log::flush();
                std::ifstream return_log{RelayCore::log::latest_log_path()};
                bool camera_error_reported = false;
                for (std::string line; std::getline(return_log, line);)
                    if (line.find("Return from editor: PerspectiveCamera forward and world_up") != std::string::npos)
                        camera_error_reported = true;
                require(camera_error_reported, "Failed editor return did not report the camera error");
                renderer.commit_live_content_reload(
                    renderer.prepare_live_content_reload(config.renderer, renderer.authoring_config())
                );
            }
            game.return_from_editor(services);
            require(
                !game.has_custom_ui() && services.camera().position() == campaign_camera,
                "Editor lost campaign camera"
            );
            require(
                renderer.runtime_diagnostics().simulation_tick == editor_checkpoint.world.simulation_tick,
                "Editor lost campaign simulation state"
            );
            if (std::string_view{map} == native_map_name)
            {
                game.transition_to({{}, native_map, "arrival", false}, services);
                require(
                    !game.has_custom_ui() && renderer.player_diagnostics().position.z < .05F,
                    "Native collision session transition did not activate its entry"
                );
            }
            game.return_to_menu(services);
            require(game.has_custom_ui() && !window.relative_mouse_mode(), "Menu retained gameplay input");
            require(renderer.runtime_diagnostics().level_name.empty(), "Menu retained old level diagnostics");
        }
        config.input.capture_mouse_on_start = true;

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
            require(window.relative_mouse_mode(), "Level transition lost mouse capture");
            renderer.begin_frame();
            campaign.on_render(frame, services);
            capture_frame(renderer, "campaign_pump.bmp");
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
            capture_frame(renderer, "campaign_intake.bmp");
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
            const auto camera_before_failed_transition = services.camera().position();
            campaign.transition_to({{}, directory.path() / "absent-map.toml", "arrival", true}, services);
            require(campaign.has_custom_ui(), "Failed transition must pause with its error");
            require(
                services.camera().position() == camera_before_failed_transition,
                "Failed transition changed the active camera"
            );
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

            const auto save_failure_dir = directory.path() / "save_failure";
            const auto save_failure_path = save_failure_dir / "progress.toml";
            RelayGame::write_saved_game(save_failure_path, start);
            RelayGame::DemoGame save_failure{save_failure_dir};
            save_failure.on_startup(services);
            save_failure.begin_episode(true, services);
            require(!save_failure.has_custom_ui(), "Save-failure scenario did not start the campaign");
            const auto blocked_temp_path = save_failure_path.string() + ".tmp";
            std::filesystem::create_directory(blocked_temp_path);
            save_failure.transition_to({{}, b, "arrival", false}, services);
            require(!save_failure.has_custom_ui(), "Failed progress write incorrectly cancelled the level transition");
            require(window.relative_mouse_mode(), "Failed progress write lost input capture on the new level");
            require(
                std::filesystem::path(RelayGame::load_saved_game(save_failure_path).world.level_path) == a,
                "Failed progress write replaced the previous disk save"
            );
            std::filesystem::remove(blocked_temp_path);
            save_failure.return_to_menu(services);
            require(
                std::filesystem::path(RelayGame::load_saved_game(save_failure_path).world.level_path) == b,
                "Failed progress write left the runtime on the previous level"
            );
            save_failure.on_shutdown(services);

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

            capture_frame(renderer, "editor_workshop.bmp");
            editor.open_map(native_map, services, true);
            editor.set_collision_debug_visible(true);
            services.camera().set_position({0, 1.0F, -1});
            services.camera().set_orientation(1.5707963268F, 0);
            for (int i = 0; i < 2; ++i)
            {
                renderer.begin_frame();
                editor.render(frame, services);
                if (renderer.begin_ui(frame, audio, true))
                {
                    editor.draw_ui(services);
                    renderer.end_ui();
                }
            }
            capture_frame(renderer, "editor_collision_opening.bmp");
            services.camera().set_position({1.05F, 1.0F, -1});
            for (int i = 0; i < 2; ++i)
            {
                renderer.begin_frame();
                editor.render(frame, services);
                if (renderer.begin_ui(frame, audio, true))
                {
                    editor.draw_ui(services);
                    renderer.end_ui();
                }
            }
            capture_frame(renderer, "editor_collision_support.bmp");
            ImGui::GetIO().AddMousePosEvent(320, 240);
            for (int i = 0; i < 4; ++i)
            {
                if (i == 1)
                    ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, true);
                if (i == 2)
                    ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, false);
                renderer.begin_frame();
                editor.render(frame, services);
                if (renderer.begin_ui(frame, audio, true))
                {
                    editor.draw_ui(services);
                    renderer.end_ui();
                }
            }
            require(
                editor.document().selection() == RelayGame::EditorSelection{true, 2},
                "Editor collision picking lost mesh authored identity"
            );
            capture_frame(renderer, "editor_collision_selected.bmp");
            editor.save_map(directory.path() / "native_editor_saved.toml", services);
            require(
                RelayGame::LevelSessionBuilder{config.gameplay}.build(editor.save_path()).physics_fixture != nullptr,
                "Editor collision overlay/save lost native collision product"
            );
            editor.shutdown();
            std::cout << "0.8/0.9: editor map roundtrip and campaign A -> B -> restart -> A passed\n";
        }
        {
            const auto intake = RelayCore::core::asset_path("levels/foundation_intake.toml");
            const auto pump = RelayCore::core::asset_path("levels/foundation_pump.toml");
            auto reload_config = config;
            reload_config.source_path = directory.path() / "live-reload.toml";
            reload_config.gameplay.level_path = intake.string();
            RelayCore::EngineServices reload_services{reload_config, window, renderer, audio, running};
            RelayGame::DemoGame reload_game{directory.path() / "live_reload"};
            reload_game.on_startup(reload_services);
            renderer.set_diagnostics_visible(true);
            reload_game.begin_episode(false, reload_services);
            require(
                renderer.runtime_diagnostics().level_path == intake.string(),
                "Reload scenario did not start on intake"
            );

            {
                std::ofstream file{reload_config.source_path};
                file << "[window]\nwidth = 1920\n[audio]\nenabled = true\n"
                        "[input]\nmove_speed = 7.25\n[game]\nlevel_path = \"levels/foundation_pump.toml\"\n"
                        "[game.controls]\nuse = \"Q\"\n";
            }
            SDL_Event reload_key{};
            reload_key.type = SDL_EVENT_KEY_DOWN;
            reload_key.key.scancode = SDL_SCANCODE_F7;
            reload_game.on_event(reload_key, reload_services);
            reload_game.on_update(frame, reload_services);
            require(
                renderer.runtime_diagnostics().level_path == intake.string()
                    && renderer.interaction_diagnostics().use_binding_label == "Q",
                "Reload changed the active map or ignored the new binding"
            );
            require(
                reload_services.config().input.move_speed == 7.25F
                    && reload_services.config().gameplay.level_path == "levels/foundation_pump.toml"
                    && reload_services.config().window.width == 640 && !reload_services.config().audio.enabled,
                "Reload did not retain effective settings or changed startup-only settings"
            );

            std::ofstream(reload_config.source_path) << "broken = [\n";
            reload_game.on_event(reload_key, reload_services);
            reload_game.on_update(frame, reload_services);
            require(
                !renderer.iteration_diagnostics().last_reload_succeeded
                    && reload_services.config().input.move_speed == 7.25F
                    && renderer.runtime_diagnostics().level_path == intake.string(),
                "Failed reload changed effective settings or the active map"
            );

            reload_game.transition_to({{}, pump, "arrival", false}, reload_services);
            require(
                renderer.runtime_diagnostics().level_path == pump.string()
                    && renderer.interaction_diagnostics().use_binding_label == "Q",
                "Transition did not use the reloaded gameplay settings"
            );
            reload_game.enter_editor(reload_services);
            reload_game.begin_editor_preview(reload_services);
            require(
                renderer.interaction_diagnostics().use_binding_label == "Q",
                "Editor preview did not use the reloaded gameplay settings"
            );
            reload_game.return_to_menu(reload_services);
            reload_game.begin_episode(false, reload_services);
            require(
                renderer.runtime_diagnostics().level_path == pump.string()
                    && renderer.interaction_diagnostics().use_binding_label == "Q",
                "New Game did not use the reloaded start map and controls"
            );
            reload_game.on_shutdown(reload_services);
        }
        for (const bool native_collision : {false, true})
        {
            auto death_config = config;
            death_config.gameplay.level_path =
                native_collision ? native_map_name
                                 : RelayCore::core::asset_path("levels/power_restoration.toml").string();
            RelayCore::EngineServices death_services{death_config, window, renderer, audio, running};
            const auto session = RelayGame::LevelSessionBuilder{death_config.gameplay}.build();
            RelayGame::GameWorldRuntime seed;
            seed.load_level_session(death_config, session);
            seed.debug_set_player_position(native_collision ? glm::vec3{2.5F, .03F, 0} : glm::vec3{6.8F, .01F, 18});
            auto save = seed.capture_saved_game(60, true);
            save.world.player_combat.health = 1;
            const auto user_path = directory.path() / (native_collision ? "native_death_continue" : "death_continue");
            std::filesystem::create_directories(user_path);
            RelayGame::write_saved_game(user_path / "progress.toml", save);
            RelayGame::DemoGame dying{user_path};
            dying.on_startup(death_services);
            dying.begin_episode(true, death_services);
            for (int i = 0; i < 20 && renderer.player_diagnostics().alive; ++i)
                dying.on_update(frame, death_services);
            require(!renderer.player_diagnostics().alive, "Hazard did not cause the test death");
            dying.return_to_menu(death_services);
            dying.on_shutdown(death_services);
            const auto dead = RelayGame::load_saved_game(user_path / "progress.toml");
            require(dead.world.player_combat.deaths == 1, "Quitting while dead discarded the latest world state");
            require(!dead.world.player_combat.alive, "Death continuation must retain its pending respawn");
            RelayGame::DemoGame continued{user_path};
            continued.on_startup(death_services);
            continued.begin_episode(true, death_services);
            for (int i = 0; i < 120; ++i)
                continued.on_update(frame, death_services);
            require(renderer.player_diagnostics().alive, "Continue from death did not respawn");
            continued.return_to_menu(death_services);
            const auto alive = RelayGame::load_saved_game(user_path / "progress.toml");
            require(
                alive.world.player_combat.deaths == 1 && alive.world.player_combat.health == 100,
                "Continue lost the death count or restored damaged health"
            );
            require(
                glm::distance(alive.player.position, session.spawn_position) < .05F,
                "Continue from death did not return to the entrance"
            );
            continued.on_shutdown(death_services);
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

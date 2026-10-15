#include "relaycore/app/engine_services.h"
#include "relaycore/audio/audio_system.h"
#include "relaycore/core/engine_config.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/renderer.h"
#include "relaygame/game_world_runtime.h"
#include "relaygame/level_session_builder.h"
#include "relaygame/world_scene_renderer.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>

#include "scoped_temp_directory.h"

int main(int argc, char** argv)
{
    auto phase_started = std::chrono::steady_clock::now();
    const auto phase = [&](const char* name) {
        const auto now = std::chrono::steady_clock::now();
        std::cout << "test_phase=" << name
                  << "; wall_ms=" << std::chrono::duration<double, std::milli>(now - phase_started).count() << '\n';
        phase_started = now;
    };
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
        config.audio.enabled = false;
        config.renderer.hdr_enabled = argc > 1 && std::string_view{argv[1]} == "--hdr";
        const bool color_audit = argc == 2 && std::string_view{argv[1]} == "--color-audit";
        RelayCore::Window window{config.window};
        SDL_HideWindow(window.handle());
        RelayCore::Renderer renderer{config.renderer, RelayCore::render::ColorPipelineMode::hardware_srgb};
        renderer.initialize(window);
        phase("startup_renderer");
        auto session = RelayGame::LevelSessionBuilder{config.gameplay}.build(
            RelayCore::core::asset_path("levels/containment_loop.toml")
        );
        phase("level_session");
        RelayGame::WorldSceneRenderer scene;
        const auto standby_usables = session.usables;
        const auto scene_data = RelayGame::build_prepared_world_scene_data(
            session.level,
            renderer.authoring_config(),
            renderer.color_pipeline_mode()
        );
        phase("world_cpu");
        scene.initialize(scene_data, renderer.static_models());
        phase("scene_assets");
        renderer.prepare_resources(scene.resource_items());
        phase("gpu_resources");
        double capture_ms = 0, bmp_ms = 0;
        size_t capture_count = 0;
        const auto save_capture = [&](const std::string& name) {
            const auto started = std::chrono::steady_clock::now();
            auto pixels = renderer.capture_frame();
            const auto captured = std::chrono::steady_clock::now();
            capture_ms += std::chrono::duration<double, std::milli>(captured - started).count();
            auto* surface = SDL_CreateSurfaceFrom(
                pixels.width,
                pixels.height,
                SDL_PIXELFORMAT_RGBA32,
                pixels.pixels.data(),
                pixels.width * 4
            );
            if (!surface)
                throw std::runtime_error(SDL_GetError());
            const auto path = directory / (name + (config.renderer.hdr_enabled ? "-hdr.bmp" : "-sdr.bmp"));
            const bool written = SDL_SaveBMP(surface, path.string().c_str());
            SDL_DestroySurface(surface);
            if (!written)
                throw std::runtime_error(SDL_GetError());
            bmp_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - captured).count();
            ++capture_count;
            return pixels;
        };
        struct View {
            const char* name;
            glm::vec3 eye, target;
        };
        const std::array views{
            View{"observation", {0, 1.65F, 9}, {0, 2, 34}},
            View{"observation-workplace", {0, 1.65F, 9}, {-4.5F, 1.1F, 8}},
            View{"observation-desk", {-4.3F, 1.65F, 7.4F}, {-4.3F, 1.25F, 10.1F}},
            View{"service", {-16, 1.65F, 14}, {-20, 1.5F, 24}},
            View{"workshop-pickup", {-24, 1.65F, 16.5F}, {-24, 1.3F, 19}},
            View{"archive", {-26, 1.65F, 42}, {-16, 1.4F, 49}},
            View{"archive-north", {-8, 1.65F, 50}, {-23, 1.4F, 50}},
            View{"archive-cassette", {-24, 1.65F, 48.5F}, {-24, 1.1F, 51}},
            View{"archive-workplace", {-24.5F, 1.65F, 49}, {-27.05F, 1.25F, 49}},
            View{"laboratory", {16, 1.65F, 43}, {23, 1.6F, 50}},
            View{"laboratory-bench", {20, 1.65F, 43.8F}, {20, 1.5F, 46}},
            View{"west-stairs", {-24, 1.65F, 29}, {-24, 4.4F, 40.5F}},
            View{"upper-return", {-10, 5.65F, 40.5F}, {0, 2, 28}},
            View{"mechanical-deck", {21.5F, 5.65F, 37}, {24, 4.8F, 32}},
            View{"mechanical-deck-front", {21.5F, 5.65F, 29}, {24, 4.9F, 32}},
            View{"east-stairs", {18, 1.65F, 13.5F}, {18, 4.2F, 25}},
            View{"frame", {-7, 1.65F, 24}, {0, 3, 33}},
            View{"hall-return", {0, 1.65F, 38}, {0, 2, 24}},
            View{"exit", {0, 1.65F, 39}, {0, 1.65F, 49}},
            View{"guardian-front", {0, 1.85F, 30}, {0, 1.85F, 34}},
            View{"guardian-back", {0, 1.85F, 38}, {0, 1.85F, 34}},
            View{"guardian-side", {4, 1.85F, 34}, {0, 1.85F, 34}},
        };
        const auto draw_view = [&](const View& view) {
            renderer.camera().set_position(view.eye);
            renderer.camera().set_target(view.target);
            renderer.begin_frame();
            scene.render(
                renderer,
                renderer.camera(),
                0,
                session.doors,
                {},
                session.usables,
                -1,
                {},
                false,
                {},
                {},
                {},
                session.enemies,
                nullptr,
                {},
                &*session.containment
            );
        };
        const auto set_display = [&](bool hdr, float exposure) {
            config.renderer.hdr_enabled = hdr;
            config.renderer.exposure_ev = exposure;
            auto prepared = renderer.prepare_live_content_reload(config.renderer, renderer.authoring_config());
            prepared.prepare_resources(scene.resource_items());
            renderer.commit_live_content_reload(std::move(prepared));
        };
        for (const auto& view : views)
        {
            draw_view(view);
            const auto sdr = save_capture(view.name);
            if (!color_audit
                || (std::string_view{view.name} != "observation" && std::string_view{view.name} != "laboratory-bench"
                    && std::string_view{view.name} != "frame"))
                continue;
            std::array<RelayCore::render::CpuRgbaTextureData, 3> exposed;
            for (int ev = -1; ev <= 1; ++ev)
            {
                set_display(true, float(ev));
                draw_view(view);
                exposed[ev + 1] = save_capture("color-" + std::string{view.name} + "-ev" + std::to_string(ev));
            }
            std::uint64_t difference = 0, outliers = 0;
            std::array<std::uint64_t, 3> brightness{};
            for (size_t i = 0; i < sdr.pixels.size(); ++i)
            {
                if (i % 4 == 3)
                    continue;
                const int delta = std::abs(int(sdr.pixels[i]) - int(exposed[1].pixels[i]));
                difference += delta;
                outliers += delta > 2;
                for (size_t ev = 0; ev < exposed.size(); ++ev)
                    brightness[ev] += exposed[ev].pixels[i];
                if (exposed[0].pixels[i] > exposed[1].pixels[i] + 2 || exposed[1].pixels[i] > exposed[2].pixels[i] + 2)
                    throw std::runtime_error("Exposure reversed scene brightness in " + std::string{view.name});
            }
            const double channels = double(sdr.width) * sdr.height * 3;
            std::cout << "color_view=" << view.name << " mean_abs_byte=" << difference / channels
                      << " outlier_fraction=" << outliers / channels << " exposure_mean=" << brightness[0] / channels
                      << ',' << brightness[1] / channels << ',' << brightness[2] / channels << '\n';
            // Permit attachment quantization and sparse transparency/MSAA edge differences.
            if (difference / channels > .5 || outliers / channels > .01)
                throw std::runtime_error("SDR/HDR zero-exposure scene mismatch in " + std::string{view.name});
            if (!(brightness[0] < brightness[1] && brightness[1] < brightness[2]))
                throw std::runtime_error("Exposure did not change scene brightness in " + std::string{view.name});
            set_display(false, 0);
        }

        RelayCore::AudioSystem audio{config.audio};
        bool running = true;
        RelayCore::EngineServices services{config, window, renderer, audio, running};
        RelayGame::GameWorldRuntime runtime;
        runtime.load_level_session(config, std::move(session));
        phase("environment_views");
        RelayCore::test::ScopedTempDirectory isolated;
        for (int i = 1; i < argc; ++i)
        {
            constexpr std::string_view prefix = "--previous-level=";
            const std::string_view argument{argv[i]};
            if (!argument.starts_with(prefix))
                continue;
            RelayGame::GameWorldRuntime previous;
            previous.load_level_session(
                config,
                RelayGame::LevelSessionBuilder{config.gameplay}.build(std::filesystem::path{argument.substr(prefix.size(
                ))})
            );
            const auto before = runtime.capture_saved_game(0, false);
            const auto path = isolated.path() / "previous-progress.toml";
            RelayGame::write_saved_game(path, previous.capture_saved_game(0, false));
            bool rejected = false;
            try
            {
                runtime.restore_saved_game(RelayGame::load_saved_game(path));
            } catch (const std::runtime_error& error)
            {
                rejected = std::string_view{error.what()} == "Карта изменилась после сохранения. Начните новый эпизод.";
            }
            const auto after = runtime.capture_saved_game(0, false);
            if (!rejected || after.player.position != before.player.position
                || after.world.simulation_tick != before.world.simulation_tick)
                throw std::runtime_error("Previous-map Continue was not safely rejected");
            std::cout << "Previous-map disk Continue rejected with the authored-map diagnostic; state preserved\n";
        }
        const auto use = [&](std::string_view name) {
            const auto found = std::ranges::find(runtime.usables(), name, &RelayCore::gameplay::RuntimeUsable::name);
            if (found == runtime.usables().end())
                throw std::runtime_error("Missing apparatus control");
            return runtime.debug_apply_usable_interaction(static_cast<std::size_t>(found - runtime.usables().begin()))
                .kind;
        };
        const auto capture =
            [&](const std::string& name, glm::vec3 eye = {-7, 1.65F, 24}, glm::vec3 target = {0, 2.5F, 29}) {
                renderer.camera().set_position(eye);
                renderer.camera().set_target(target);
                renderer.begin_frame();
                scene.render(
                    renderer,
                    renderer.camera(),
                    0,
                    runtime.doors(),
                    {},
                    runtime.usables(),
                    -1,
                    {},
                    false,
                    runtime.activated_target_ids(),
                    {},
                    {},
                    runtime.enemies(),
                    &runtime.player_combat_state(),
                    {},
                    &*runtime.containment()
                );
                renderer.player_ui().set_screen(RelayCore::render::PlayerUiScreen::Hud);
                renderer.player_ui().set_hud(runtime.build_hud_presentation(&renderer.camera()));
                renderer.finish_scene();
                if (!renderer.begin_ui({}, audio))
                    throw std::runtime_error("Containment HUD unavailable");
                renderer.end_ui();
                save_capture("apparatus-" + name);
            };
        const auto before_combat = runtime.capture_saved_game(0, false);
        capture("enemy-health", {-21, 1.65F, 19}, {-21, .6F, 23});
        (void) runtime.debug_apply_player_fire({-21, .6F, 19}, {0, 0, 1});
        capture("enemy-damage", {-21, 1.65F, 19}, {-21, .6F, 23});
        runtime.restore_saved_game(before_combat);
        capture("standby");
        (void) runtime.debug_apply_player_fire({0, 1.65F, 20}, {0, 0, 1});
        capture("armored-hit", {0, 1.65F, 28}, {0, 1.85F, 34});
        (void) use("limiter_cassette");
        (void) use("limiter_socket");
        (void) use("grip_service");
        runtime.debug_update_containment(1.5F);
        capture("drive-moving");
        runtime.set_paused(true, services);
        const auto paused = runtime.capture_runtime_snapshot();
        runtime.update({.delta_seconds = .1}, services);
        if (runtime.simulation_tick() != paused.simulation_tick
            || runtime.containment()->state.grip_fraction != paused.containment->grip_fraction)
            throw std::runtime_error("Paused apparatus advanced");
        runtime.set_paused(false, services);
        runtime.update({.delta_seconds = .1}, services);
        if (runtime.containment()->state.grip_fraction <= paused.containment->grip_fraction)
            throw std::runtime_error("Apparatus failed to resume");
        runtime.debug_update_containment(2);
        capture("ready");
        const auto grip_model = renderer.static_models().load(
            RelayCore::core::asset_path("models/containment_grip.rcmodel"),
            RelayCore::render::TextureEncoding::srgb
        );
        for (const bool powered : {false, true})
            for (const bool flashlight : {false, true})
            {
                const auto targets = powered ? runtime.activated_target_ids() : std::span<const std::string>{};
                renderer.camera().set_position({-7, 1.65F, 24});
                renderer.camera().set_target({0, 3, 33});
                renderer.set_spot_light(
                    {.position = renderer.camera().position(),
                     .direction = glm::normalize(glm::vec3{0, 3, 33} - renderer.camera().position()),
                     .enabled = flashlight}
                );
                renderer.begin_frame();
                scene.render(
                    renderer,
                    renderer.camera(),
                    0,
                    runtime.doors(),
                    {},
                    powered ? runtime.usables() : std::span<const RelayCore::gameplay::RuntimeUsable>{standby_usables},
                    -1,
                    {},
                    false,
                    targets,
                    {},
                    {},
                    runtime.enemies(),
                    nullptr,
                    {},
                    &*runtime.containment()
                );
                const bool emitting = std::ranges::any_of(scene.render_items(), [&](const auto& item) {
                    return item.emissive_texture && item.emissive_factor != glm::vec3{0}
                           && std::ranges::any_of(grip_model->meshes, [&](const auto& mesh) {
                                  return item.mesh == &mesh;
                              });
                });
                if (emitting != powered)
                    throw std::runtime_error("Containment plate emission disagrees with apparatus power");
                save_capture(
                    std::string{"light-"} + (powered ? "powered" : "standby")
                    + (flashlight ? "-flashlight" : "-ambient")
                );
                if (!flashlight)
                {
                    renderer.camera().set_position({-3.8F, 4.8F, 26});
                    renderer.camera().set_target({0, 5.1F, 27.3F});
                    renderer.begin_frame();
                    scene.render(
                        renderer,
                        renderer.camera(),
                        0,
                        runtime.doors(),
                        {},
                        powered ? runtime.usables()
                                : std::span<const RelayCore::gameplay::RuntimeUsable>{standby_usables},
                        -1,
                        {},
                        false,
                        targets,
                        {},
                        {},
                        runtime.enemies(),
                        nullptr,
                        {},
                        &*runtime.containment()
                    );
                    save_capture(powered ? "crosshead-powered" : "crosshead-standby");
                }
            }
        renderer.set_spot_light({});
        if (std::ranges::any_of(std::span{argv + 1, static_cast<std::size_t>(argc - 1)}, [](const char* arg) {
                return std::string_view{arg} == "--motion";
            }))
            for (int frame = 0; frame < 48; ++frame)
            {
                const float angle = -.3F + frame * .6F / 47;
                renderer.camera().set_position({4 * std::sin(angle), 1.85F, 34 - 4 * std::cos(angle)});
                renderer.camera().set_target({0, 2.8F, 34});
                renderer.begin_frame();
                scene.render(
                    renderer,
                    renderer.camera(),
                    0,
                    runtime.doors(),
                    {},
                    runtime.usables(),
                    -1,
                    {},
                    false,
                    runtime.activated_target_ids(),
                    {},
                    {},
                    runtime.enemies(),
                    nullptr,
                    {},
                    &*runtime.containment()
                );
                save_capture("motion-" + std::to_string(frame));
            }
        runtime.debug_set_player_position({0, .241F, 24.5F});
        for (int i = 0; i < 1800 && runtime.containment()->state.mode != RelayGame::GuardianMode::Windup; ++i)
            runtime.debug_update_containment(1.0F / 120);
        if (runtime.containment()->state.mode != RelayGame::GuardianMode::Windup)
            throw std::runtime_error("Guardian failed to pursue");
        capture("windup");
        runtime.debug_set_player_position({-7, .01F, 26});
        runtime.debug_update_containment(.72F);
        capture("windup-raised");
        for (int i = 0; i < 23; ++i)
            runtime.debug_update_containment(1.0F / 120);
        capture("recovery");
        if (use("containment_fire") != RelayCore::gameplay::UseInteractionOutcome::ActivatedTarget)
            throw std::runtime_error("Trap recovery did not enable activation");
        runtime.debug_update_containment(1);
        capture("capturing");
        RelayGame::write_saved_game(isolated.path() / "progress.toml", runtime.capture_saved_game(0, false));
        const auto suspended = RelayGame::load_saved_game(isolated.path() / "progress.toml");
        runtime.debug_update_containment(1);
        runtime.restore_saved_game(suspended);
        if (runtime.containment()->state.capture_fraction != .5F)
            throw std::runtime_error("Disk restore lost the moving grip state");
        runtime.update({.delta_seconds = .1}, services);
        for (int i = 0; i < 121; ++i)
            runtime.update({.delta_seconds = 1.0 / 120}, services);
        if (runtime.containment()->state.mode != RelayGame::GuardianMode::Contained)
            throw std::runtime_error("Runtime failed to finish restored containment");
        capture("contained");
        runtime.debug_update_mission(RelayCore::physics::make_player_aabb({0, .01F, 9}, .3F, 1.8F));
        for (int i = 0; i < 250; ++i)
            runtime.update({.delta_seconds = 1.0 / 120}, services);
        runtime.debug_set_player_position({0, .01F, 49});
        runtime.debug_update_mission(RelayCore::physics::make_player_aabb({0, .01F, 49}, .3F, 1.8F));
        if (runtime.active_level_exit() || runtime.has_completion_exit() || !runtime.mission_status().completed)
            throw std::runtime_error("Contained final chapter did not finish the campaign");

        RelayGame::GameWorldRuntime combat;
        combat.load_level_session(
            config,
            RelayGame::LevelSessionBuilder{config.gameplay}.build(
                RelayCore::core::asset_path("levels/containment_loop.toml")
            )
        );
        combat.debug_set_player_position({-26, .01F, 51});
        auto combat_save = combat.capture_saved_game(0, false);
        for (auto& enemy : combat_save.enemies)
        {
            enemy.mode = RelayGame::EnemyMode::Dead;
            enemy.health = 0;
        }
        auto& shooter = combat_save.enemies[2];
        shooter.position = {-22, .01F, 51.5F};
        shooter.last_seen = {-26, .01F, 51};
        shooter.health = 84;
        shooter.mode = RelayGame::EnemyMode::Windup;
        shooter.timer = .35F;
        shooter.yaw = -97;
        combat.restore_saved_game(combat_save);
        const auto combat_capture = [&](const char* name) {
            renderer.camera().set_position({-26, 1.66F, 53});
            renderer.camera().set_target({-22, .75F, 51.5F});
            renderer.begin_frame();
            scene.render(
                renderer,
                renderer.camera(),
                0,
                combat.doors(),
                {},
                combat.usables(),
                -1,
                {},
                false,
                combat.activated_target_ids(),
                {},
                {},
                combat.enemies(),
                &combat.player_combat_state(),
                {},
                &*combat.containment()
            );
            save_capture(name);
        };
        combat_capture("shooter-warning");
        combat.set_paused(true, services);
        combat.update({.delta_seconds = .1}, services);
        if (combat.enemies()[2].state.timer != .35F)
            throw std::runtime_error("Paused shooter advanced its warning");
        combat.set_paused(false, services);
        shooter.timer = .006F;
        combat.restore_saved_game(combat_save);
        combat.update({.delta_seconds = 1.0 / 120}, services);
        if (combat.enemies()[2].state.mode != RelayGame::EnemyMode::Recovery
            || combat.player_combat_state().health != 88)
            throw std::runtime_error("Runtime shooter failed its single warned shot");
        combat_capture("shooter-shot");
        RelayGame::write_saved_game(isolated.path() / "combat.toml", combat.capture_saved_game(0, false));
        combat.restore_saved_game(RelayGame::load_saved_game(isolated.path() / "combat.toml"));
        combat_capture("shooter-recovery");
        combat_save = combat.capture_saved_game(0, false);
        combat_save.world.player_combat.health = 0;
        combat_save.world.player_combat.alive = false;
        combat_save.world.player_combat.respawn_remaining_seconds = 2;
        combat.restore_saved_game(combat_save);
        const auto dead_timer = combat.enemies()[2].state.timer;
        combat.update({.delta_seconds = .1}, services);
        if (combat.enemies()[2].state.timer != dead_timer)
            throw std::runtime_error("Shooter advanced during player death");
        combat_save.world.player_combat.health = 100;
        combat_save.world.player_combat.alive = true;
        combat_save.world.player_combat.respawn_remaining_seconds = 0;
        combat_save.enemies[2].mode = RelayGame::EnemyMode::Dead;
        combat_save.enemies[2].health = 0;
        combat_save.enemies[2].timer = 0;
        combat.restore_saved_game(combat_save);
        combat_capture("shooter-destroyed");

        phase("gameplay_views_and_checks");
        std::cout << "test_captures=" << capture_count << "; render_or_readback_ms=" << capture_ms
                  << "; bmp_ms=" << bmp_ms << '\n';
        scene.shutdown();
        std::cout << "containment gameplay/environment/state views, shooter warning/shot/recovery/death, "
                     "pause/resume, disk continuation and exit "
                     "lifecycle verified\n";
    } catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    RelayCore::log::shutdown();
    SDL_Quit();
    return result;
}

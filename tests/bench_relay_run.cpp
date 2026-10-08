#include "relaycore/app/engine_services.h"
#include "relaycore/app/frame_timer.h"
#include "relaycore/audio/audio_system.h"
#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/renderer.h"
#include "relaygame/demo_game.h"

#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <span>

#include "bench_relay_route.h"
#include "cooling_loop_route.h"
#include "pressure_transfer_route.h"
#include "scoped_temp_directory.h"

namespace
{
    using Clock = std::chrono::steady_clock;
    auto ms(Clock::time_point start, Clock::time_point end) -> double
    {
        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    void report(std::string_view name, std::vector<double> samples)
    {
        std::ranges::sort(samples);
        std::cout << name << "; samples=" << samples.size() << "; p50=" << samples[samples.size() / 2]
                  << "; p95=" << samples[samples.size() * 95 / 100] << "; p99=" << samples[samples.size() * 99 / 100]
                  << "; max=" << samples.back() << '\n';
    }
    auto cooling_saves(const RelayCore::core::GameplayConfig& config) -> std::array<RelayGame::SavedGame, 4>
    {
        RelayCore::test::CoolingLoopRoute route{config};
        auto entry = route.save();
        entry.player.position = {0, .01F, 5.5F};
        entry.player.view_angles = {1.5707963268F, 0};
        route.close_rover = true;
        route.walk({-4.5F, .01F, 2});
        static_cast<void>(route.use("cooling_shotgun"));
        route.walk({-3, .01F, 2});
        route.walk({-3, .01F, 6});
        route.walk({-4, .01F, 7});
        auto service = route.save();
        route.walk({-4, .01F, 18.8F});
        route.walk({-5.3F, .01F, 18.5F});
        route.isolate();
        route.reach_control();
        auto standby = route.save();
        static_cast<void>(route.use("cooling_control"));
        auto powered = route.save();
        standby.player.position = powered.player.position = {-3, .01F, 21.5F};
        standby.player.view_angles = powered.player.view_angles = {.7F, .08F};
        route.leave();
        if (!route.mission.completed())
            throw std::runtime_error("Cooling loop route did not complete");
        return {entry, service, standby, powered};
    }

    auto bench_saves(const RelayCore::core::GameplayConfig& config) -> std::array<RelayGame::SavedGame, 4>
    {
        RelayCore::test::BenchRelayRoute route{config};
        const auto entry = route.save();
        RelayCore::test::BenchRelayRoute distant_route{config, RelayCore::test::BenchRelayTactic::Distance};
        distant_route.open_bay();
        distant_route.wait(120);
        auto distance = distant_route.save();
        distance.player.view_angles = {1.83F, -.1F};
        distant_route.walk({0, .01F, 2.2F});
        distant_route.finish();
        route.enter_bay();
        route.walk({-2.6F, .01F, 3.1F});
        bool rover_visible = false;
        for (int tick = 0; tick < 600; ++tick)
        {
            const auto eye = route.position + glm::vec3{0, 1.65F, 0};
            const auto target = route.session.enemies.front().state.position + glm::vec3{0, .45F, 0};
            const auto delta = target - eye;
            if (!route.world().raycast(eye, glm::normalize(delta), glm::length(delta)))
            {
                rover_visible = true;
                break;
            }
            route.tick(false);
        }
        if (!rover_visible || route.session.enemies.front().state.mode == RelayGame::EnemyMode::Dead)
            throw std::runtime_error("Bench relay cover capture needs a visible approaching rover");
        auto cover = route.save();
        cover.world.selected_weapon = RelayGame::PlayerWeapon::Shotgun;
        const auto to_rover = glm::normalize(
            route.session.enemies.front().state.position + glm::vec3{0, .45F, 0} - route.position
            - glm::vec3{0, 1.62F, 0}
        );
        cover.player.view_angles = {std::atan2(to_rover.z, to_rover.x), std::asin(to_rover.y)};
        route.power_relay();
        auto powered = route.save();
        powered.player.view_angles = {.21F, -.1F};
        route.leave_bay();
        if (!distant_route.mission.completed() || distant_route.player.kills != 2 || !route.mission.completed()
            || route.player.kills != 2 || route.shotgun_shots == 0 || route.precise_shots == 0)
            throw std::runtime_error("Bench relay scripted route failed");
        std::cout << "distance_rover_hit_metres=" << distant_route.first_rover_hit_range
                  << "; distance_turret_exposed_ticks=" << distant_route.turret_exposed_ticks
                  << "; cover_rover_hit_metres=" << route.first_rover_hit_range
                  << "; cover_turret_exposed_ticks=" << route.turret_exposed_ticks << '\n';
        return {entry, distance, cover, powered};
    }

    auto transfer_saves(const RelayCore::core::GameplayConfig& config, bool atmosphere = false)
        -> std::array<RelayGame::SavedGame, 4>
    {
        RelayCore::test::PressureRoute route{config};
        route.reach_mixed();
        auto entry = route.save();
        entry.player.view_angles = {1.5707963268F, -.1F};
        route.mixed_tactic = RelayCore::test::PressureTactic::Distance;
        auto distance = route.save();
        distance.player.view_angles = {1.5707963268F, -.18F};
        route.fight_mixed(RelayCore::test::PressureTactic::Distance);
        const auto unpowered = route.save();
        route.power_transfer();
        auto powered = route.save();
        powered.player.view_angles = {2.4F, -.1F};
        route.leave();
        if (!route.mission.completed() || route.player.kills != 4)
            throw std::runtime_error("Pressure transfer distant route did not complete");

        RelayCore::test::PressureRoute close{config};
        close.reach_mixed();
        close.mixed_tactic = RelayCore::test::PressureTactic::Cover;
        close.walk_to({28, .01F, 30.3F});
        auto cover = close.save();
        cover.world.selected_weapon = RelayGame::PlayerWeapon::Shotgun;
        const auto delta = glm::normalize(
            close.session.enemies[1].state.position + glm::vec3{0, .45F, 0} - close.position - glm::vec3{0, 1.65F, 0}
        );
        cover.player.view_angles = {std::atan2(delta.z, delta.x), std::asin(delta.y)};
        close.fight_mixed(RelayCore::test::PressureTactic::Cover);
        close.power_transfer();
        close.leave();
        if (!close.mission.completed() || close.player.kills != 4 || close.mixed_shotgun_shots == 0)
            throw std::runtime_error("Pressure transfer close route did not complete");
        std::cout << "distance_rover_hit_metres=" << route.mixed_rover_hit_range
                  << "; distance_turret_exposed_ticks=" << route.mixed_exposed_ticks
                  << "; cover_rover_hit_metres=" << close.mixed_rover_hit_range
                  << "; cover_turret_exposed_ticks=" << close.mixed_exposed_ticks << '\n';
        return atmosphere ? std::array{unpowered, powered, unpowered, powered}
                          : std::array{entry, distance, cover, powered};
    }
} // namespace

int main(int argc, char** argv)
{
    const auto arguments = std::span{argv + 1, static_cast<std::size_t>(argc - 1)};
    const bool profile = std::ranges::any_of(arguments, [](std::string_view arg) {
        return arg == "--profile";
    });
    const bool transfer = std::ranges::any_of(arguments, [](std::string_view arg) {
        return arg == "--pressure-transfer";
    });
    const bool cooling = std::ranges::any_of(arguments, [](std::string_view arg) {
        return arg == "--cooling-loop";
    });
    const bool mounting_views = std::ranges::any_of(arguments, [](std::string_view arg) {
        return arg == "--mounting-views";
    });
    const bool visual_slice = std::ranges::any_of(arguments, [](std::string_view arg) {
        return arg == "--visual-slice";
    });
    const bool atmosphere = std::ranges::any_of(arguments, [](std::string_view arg) {
        return arg == "--atmosphere";
    });
    const bool equipment_bay = std::ranges::any_of(arguments, [](std::string_view arg) {
        return arg == "--equipment-bay";
    });
    const bool gallery = std::ranges::any_of(arguments, [](std::string_view arg) {
        return arg == "--gallery";
    });
    const bool service_hall = std::ranges::any_of(arguments, [](std::string_view arg) {
        return arg == "--service-hall";
    });
    const bool standby = std::ranges::any_of(arguments, [](std::string_view arg) {
        return arg == "--standby";
    });
    const bool exit_corridor = std::ranges::any_of(arguments, [](std::string_view arg) {
        return arg == "--exit-corridor";
    });
    const bool junctions = std::ranges::any_of(arguments, [](std::string_view arg) {
        return arg == "--junctions";
    });
    const bool exit_open = std::ranges::any_of(arguments, [](std::string_view arg) {
        return arg == "--exit-open";
    });
    const std::string_view workload = cooling ? "cooling_loop" : transfer ? "pressure_transfer" : "bench_relay";
    if (!SDL_Init(SDL_INIT_VIDEO))
        return 77;
    int result = 0;
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::log::init({.logs_directory = (temp.path() / "logs").string(), .enable_timestamped_log = false});
    try
    {
        auto config = RelayCore::core::load_default_engine_config();
        config.gameplay.level_path = cooling    ? "levels/cooling_loop.toml"
                                     : transfer ? "levels/pressure_transfer.toml"
                                                : "levels/bench_relay.toml";
        config.window.width = 1280;
        config.window.height = 720;
        config.window.high_dpi = false;
        config.window.vsync = false;
        config.audio.enabled = false;
        config.renderer.imgui.enabled = true;
        config.renderer.hdr_enabled = true;
        RelayCore::Window window{config.window};
        if (!profile)
            SDL_HideWindow(window.handle());
        RelayCore::Renderer renderer{config.renderer, RelayCore::render::ColorPipelineMode::hardware_srgb};
        renderer.initialize(window);
        renderer.on_resize(1280, 720);
        RelayCore::AudioSystem audio{config.audio};
        bool running = true;
        RelayCore::EngineServices services{config, window, renderer, audio, running};
        auto saves = cooling    ? cooling_saves(config.gameplay)
                     : transfer ? transfer_saves(config.gameplay, atmosphere || exit_corridor || junctions)
                                : bench_saves(config.gameplay);
        if (!transfer && (visual_slice || mounting_views || (cooling && atmosphere)))
        {
            auto positions =
                cooling
                    ? std::array<glm::vec3, 4>{{{4.5F, .01F, 5}, {-4, .01F, 12}, {-3.5F, .01F, 19}, {3, .01F, 24.5F}}}
                    : std::array<glm::vec3, 4>{{{2, .01F, -2.5F}, {2, .01F, 2.5F}, {2, .01F, 9.5F}, {0, .01F, 11.8F}}};
            auto angles =
                cooling ? std::array<glm::vec2, 4>{{{2.1F, .03F}, {.2F, .03F}, {-3.05F, 0}, {-.6F, 0}}}
                        : std::array<glm::vec2, 4>{{{2.8F, -.12F}, {2, -.06F}, {-2.2F, -.16F}, {-1.5707963268F, .04F}}};
            if (mounting_views)
            {
                positions =
                    cooling
                        ? std::
                              array<glm::vec3, 4>{{{-2, .01F, 2}, {3, .01F, 19}, {3, .01F, 16.5F}, {-4.5F, .01F, 17.5F}}
                              }
                        : std::array<glm::vec3, 4>{{{0, .01F, -2}, {2, .01F, 10}, {0, .01F, 2.5F}, {0, .01F, 11.8F}}};
                angles = cooling
                             ? std::array<glm::vec2, 4>{{{2.6F, -.18F}, {-2.15F, .02F}, {2.38F, .50F}, {2.7F, .1F}}}
                             : std::array<glm::vec2, 4>{
                                   {{1.5707963268F, .15F}, {1.2F, .02F}, {-1.5707963268F, .02F}, {-1.5707963268F, .04F}}
                               };
            }
            if (cooling && atmosphere)
            {
                positions = {{{-3.5F, .01F, 23}, {4.3F, .01F, 23}, {4.3F, .01F, 13}, {3, .01F, 23.3F}}};
                angles = {{{3.1415926536F, .25F}, {0, .05F}, {-1.5707963268F, .03F}, {1.5707963268F, .15F}}};
            }
            const auto powered = saves[3];
            for (std::size_t i = 0; i < saves.size(); ++i)
            {
                saves[i] = powered;
                saves[i].player.position = positions[i];
                saves[i].player.view_angles = angles[i];
            }
        }
        if (transfer && mounting_views)
        {
            const std::array<glm::vec3, 4> positions{
                {{4, .01F, 4}, {14, .01F, 23}, {29.5F, .01F, 27.5F}, {37.7F, .01F, 39}}
            };
            const std::array<glm::vec2, 4> angles{{{.5F, .3F}, {-.463648F, .55F}, {-.5F, .15F}, {.35F, .3F}}};
            const auto powered = saves[3];
            for (std::size_t i = 0; i < saves.size(); ++i)
            {
                saves[i] = powered;
                saves[i].player.position = positions[i];
                saves[i].player.view_angles = angles[i];
            }
        }
        if (transfer && visual_slice)
        {
            const std::array<glm::vec3, 4> positions{{{29, .01F, 28}, {34, .01F, 31}, {37.3F, .01F, 39}, {38, .01F, 42}}
            };
            const std::array<glm::vec2, 4> angles{{{.94F, 0}, {1.06F, 0}, {.4F, 0}, {-2.25F, 0}}};
            const auto powered = saves[3];
            for (std::size_t i = 0; i < saves.size(); ++i)
            {
                saves[i] = powered;
                saves[i].player.position = positions[i];
                saves[i].player.view_angles = angles[i];
            }
        }
        if (transfer && equipment_bay)
        {
            const std::array<glm::vec3, 4> positions{
                {{5.8F, .01F, 1.8F}, {3.3F, .01F, 8.1F}, {4, .01F, 6}, {4, .01F, 9}}
            };
            const std::array<glm::vec2, 4> angles{{{2.24F, -.08F}, {-2.7F, 0}, {1.5707963268F, 0}, {-1.5707963268F, 0}}
            };
            const auto powered = saves[3];
            for (std::size_t i = 0; i < saves.size(); ++i)
            {
                saves[i] = powered;
                saves[i].player.position = positions[i];
                saves[i].player.view_angles = angles[i];
            }
        }
        if (transfer && (gallery || service_hall))
        {
            auto state = saves[3];
            if (standby)
            {
                RelayCore::test::PressureRoute route{config.gameplay};
                route.walk_to({4, .01F, 5.3F});
                route.walk_to({2, .01F, 5.3F});
                static_cast<void>(route.use("transfer_shotgun"));
                route.walk_to({6.1F, .01F, 8.5F});
                static_cast<void>(route.use("service_reader"));
                state = route.save();
            }
            const auto positions =
                gallery ? std::array<glm::vec3, 4>{{{9, .01F, 22}, {27, .01F, 21}, {27, .01F, 20}, {19, .01F, 21}}}
                        : std::array<glm::vec3, 4>{{{4, .01F, 11}, {4, .01F, 21}, {6, .01F, 17}, {11, .01F, 23}}};
            const auto angles = gallery
                                    ? std::array<glm::vec2, 4>{{{0, -.03F}, {3.03F, -.03F}, {.9F, .08F}, {-.78F, .02F}}}
                                    : std::array<glm::vec2, 4>{
                                          {{1.5707963268F, -.08F}, {-1.5707963268F, .04F}, {1.1F, .02F}, {-2.65F, 0}}
                                      };
            for (std::size_t i = 0; i < saves.size(); ++i)
            {
                saves[i] = state;
                saves[i].player.position = positions[i];
                saves[i].player.view_angles = angles[i];
            }
        }
        if (transfer && atmosphere)
            for (std::size_t i = 0; i < saves.size(); ++i)
            {
                saves[i].player.position = i < 2 ? glm::vec3{34, .01F, 31} : glm::vec3{37.3F, .01F, 39};
                saves[i].player.view_angles = i < 2 ? glm::vec2{1.06F, 0} : glm::vec2{.4F, 0};
            }
        if (transfer && (exit_corridor || junctions))
        {
            auto state = saves[standby ? 0 : 1];
            if (exit_corridor && exit_open)
            {
                RelayCore::test::PressureRoute route{config.gameplay};
                route.reach_mixed();
                route.fight_mixed(RelayCore::test::PressureTactic::Distance);
                route.power_transfer();
                route.walk_to({31, .01F, 42.5F});
                static_cast<void>(route.use("exit_reader"));
                route.wait(270);
                state = route.save();
            }
            const auto positions =
                exit_corridor
                    ? std::array<glm::vec3, 4>{{{29, .01F, 39}, {28, .01F, 45}, {28, .01F, 50}, {32, .01F, 42}}}
                    : std::array<glm::vec3, 4>{{{4, .01F, 8.5F}, {8.7F, .01F, 22}, {30, .01F, 24}, {29, .01F, 28}}};
            const auto angles =
                exit_corridor ? std::array<
                                    glm::vec2,
                                    4>{{{1.77F, .03F}, {1.5707963268F, .02F}, {-1.5707963268F, .02F}, {2.45F, .08F}}}
                              : std::array<glm::vec2, 4>{
                                    {{1.5707963268F, -.15F},
                                     {3.1415926536F, -.15F},
                                     {1.5707963268F, -.12F},
                                     {-1.5707963268F, -.12F}}
                                };
            for (std::size_t i = 0; i < saves.size(); ++i)
            {
                saves[i] = state;
                saves[i].player.position = positions[i];
                saves[i].player.view_angles = angles[i];
            }
        }
        const std::array names = cooling && atmosphere       ? std::array{"header", "start", "auxiliary", "exit"}
                                 : !transfer && visual_slice ? std::array{"overview", "machinery", "control", "reverse"}
                                 : cooling                   ? std::array{"entry", "service", "standby", "powered"}
                                 : exit_corridor             ? std::array{"approach", "forward", "reverse", "reader"}
                                 : junctions    ? std::array{"bay-hall", "hall-gallery", "gallery-room", "room-gallery"}
                                 : gallery      ? std::array{"forward", "reverse", "turn", "reader"}
                                 : service_hall ? std::array{"rover", "reverse", "portal", "corner"}
                                 : equipment_bay ? std::array{"bench", "cabinet", "door", "reverse"}
                                 : atmosphere
                                     ? std::array{"hall-before", "hall-after", "control-before", "control-after"}
                                 : visual_slice   ? std::array{"overview", "machinery", "control", "reverse"}
                                 : mounting_views ? std::array{"service", "gallery", "notice", "fan"}
                                                  : std::array{"entry", "distance", "cover", "powered"};
        const std::string capture_directory =
            cooling && atmosphere         ? "cooling-signage-captures"
            : !transfer && mounting_views ? (cooling ? "cooling-mounting-captures" : "bench-mounting-captures")
            : !transfer && visual_slice   ? (cooling ? "cooling-visual-captures" : "bench-visual-captures")
            : cooling                     ? "cooling-loop-captures"
            : transfer                    ? (exit_corridor    ? "pressure-exit-captures"
                                             : junctions      ? "pressure-junction-captures"
                                             : gallery        ? "pressure-gallery-captures"
                                             : service_hall   ? "pressure-service-captures"
                                             : equipment_bay  ? "pressure-equipment-captures"
                                             : atmosphere     ? "pressure-atmosphere-captures"
                                             : visual_slice   ? "pressure-visual-captures"
                                             : mounting_views ? "pressure-mounting-captures"
                                                              : "pressure-transfer-captures")
                                          : "bench-relay-captures";
        std::filesystem::create_directories(capture_directory);
        std::cout << "workload=" << workload
                  << "; resolution=1280x720; vsync=off; hdr=on; pipeline=hardware_srgb; shadows="
                  << config.renderer.shadow_enabled << '\n';
        for (int pass = 0; pass < 2; ++pass)
            for (std::size_t stage = 0; stage < saves.size(); ++stage)
            {
                RELAYCORE_PROFILE_ZONE_N("BenchRelay::state");
                RELAYCORE_PROFILE_PLOT("bench_relay_pass", pass);
                RELAYCORE_PROFILE_PLOT("bench_relay_state", stage);
                const auto directory = temp.path() / (std::to_string(pass) + names[stage]);
                RelayGame::write_saved_game(directory / "progress.toml", saves[stage]);
                RelayGame::DemoGame game{directory};
                const auto loading = Clock::now();
                game.on_startup(services);
                game.begin_episode(true, services);
                if (game.has_custom_ui())
                    throw std::runtime_error("Bench relay disk continue did not enter gameplay");
                std::cout << "pass=" << pass << "; state=" << names[stage] << "; load_ms=" << ms(loading, Clock::now())
                          << '\n';
                std::array<std::vector<double>, 5> timings;
                for (auto& values : timings)
                    values.reserve(8192);
                const auto beginning = Clock::now();
                auto previous = beginning;
                unsigned frames = 0;
                do
                {
                    RELAYCORE_PROFILE_ZONE_N("BenchRelay::frame");
                    SDL_Event event;
                    while (SDL_PollEvent(&event))
                        if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
                            throw std::runtime_error("Bench relay run cancelled");
                    const auto now = Clock::now();
                    const RelayCore::FrameInfo frame{
                        .delta_seconds = profile && frames > 0 ? std::min(ms(previous, now) / 1000, .1) : 1.0 / 120,
                        .elapsed_seconds = ms(beginning, now) / 1000,
                        .frame_index = frames
                    };
                    previous = now;
                    const auto start = Clock::now();
                    renderer.begin_frame();
                    const auto begun = Clock::now();
                    game.on_update(frame, services);
                    const auto updated = Clock::now();
                    game.on_render(frame, services);
                    renderer.finish_scene();
                    if (renderer.begin_ui(frame, audio, game.has_custom_ui()))
                    {
                        game.on_ui(services);
                        renderer.end_ui();
                    }
                    const auto rendered = Clock::now();
                    if (!renderer.present() && profile)
                        throw std::runtime_error("Bench relay presentation failed");
                    const auto presented = Clock::now();
                    const std::array times{
                        ms(start, presented),
                        ms(start, begun),
                        ms(begun, updated),
                        ms(updated, rendered),
                        ms(rendered, presented)
                    };
                    for (std::size_t i = 0; i < times.size(); ++i)
                        timings[i].push_back(times[i]);
                    RELAYCORE_PROFILE_PLOT("bench_relay_draws", renderer.world_diagnostics().draw_calls);
                    if (!profile && frames == 2 && pass == 0)
                    {
                        const auto image = renderer.capture_frame();
                        auto* surface = SDL_CreateSurfaceFrom(
                            image.width,
                            image.height,
                            SDL_PIXELFORMAT_RGBA32,
                            const_cast<unsigned char*>(image.pixels.data()),
                            image.width * 4
                        );
                        if (!surface
                            || !SDL_SaveBMP(
                                surface,
                                (capture_directory + "/" + std::string{names[stage]} + ".bmp").c_str()
                            ))
                            throw std::runtime_error(SDL_GetError());
                        SDL_DestroySurface(surface);
                    }
                    RELAYCORE_PROFILE_FRAME();
                    ++frames;
                } while (profile ? ms(beginning, Clock::now()) < 2000 : frames < 4);
                const std::array labels{"frame_ms", "begin_wait_ms", "update_ms", "render_ui_ms", "present_ms"};
                for (std::size_t i = 0; i < timings.size(); ++i)
                    report(labels[i], std::move(timings[i]));
                std::cout << "draws=" << renderer.world_diagnostics().draw_calls
                          << "; triangles=" << renderer.world_diagnostics().triangles << '\n';
                game.return_to_menu(services);
                game.on_shutdown(services);
            }
        std::cout << workload << "=passed\n";
    } catch (const std::exception& error)
    {
        std::cerr << workload << "=failed: " << error.what() << '\n';
        result = 1;
    }
    RelayCore::log::shutdown();
    SDL_Quit();
    return result;
}

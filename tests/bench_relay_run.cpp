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

#include "bench_relay_route.h"
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
} // namespace

int main(int argc, char** argv)
{
    const bool profile = argc > 1 && std::string_view{argv[1]} == "--profile";
    if (!SDL_Init(SDL_INIT_VIDEO))
        return 77;
    int result = 0;
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::log::init({.logs_directory = (temp.path() / "logs").string(), .enable_timestamped_log = false});
    try
    {
        auto config = RelayCore::core::load_default_engine_config();
        config.gameplay.level_path = "levels/bench_relay.toml";
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
        RelayCore::test::BenchRelayRoute route{config.gameplay};
        const auto entry = route.save();
        route.enter_bay();
        auto encounter = route.save();
        encounter.world.selected_weapon = RelayGame::PlayerWeapon::Shotgun;
        const auto to_rover = glm::normalize(
            route.session.enemies.front().state.position + glm::vec3{0, .45F, 0} - route.position
            - glm::vec3{0, 1.62F, 0}
        );
        encounter.player.view_angles = {std::atan2(to_rover.z, to_rover.x), std::asin(to_rover.y)};
        route.finish();
        if (!route.mission.completed() || route.player.kills != 2 || route.shotgun_shots == 0
            || route.precise_shots == 0)
            throw std::runtime_error("Bench relay scripted route failed");
        auto powered = route.save();
        powered.world.mission = {.active = true, .completed = false, .current_step = 4};
        powered.player.position = {1.6F, .01F, 9.5F};
        powered.player.view_angles = {-2.14F, -.15F};
        const std::array saves{entry, encounter, powered};
        const std::array names{"entry", "encounter", "powered"};
        std::filesystem::create_directories("bench-relay-captures");
        std::cout << "workload=bench_relay; resolution=1280x720; vsync=off; hdr=on; pipeline=hardware_srgb; shadows="
                  << config.renderer.shadow_enabled << "; route_ticks=" << route.ticks << '\n';
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
                                ("bench-relay-captures/" + std::string{names[stage]} + ".bmp").c_str()
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
        std::cout << "bench_relay=passed\n";
    } catch (const std::exception& error)
    {
        std::cerr << "bench_relay=failed: " << error.what() << '\n';
        result = 1;
    }
    RelayCore::log::shutdown();
    SDL_Quit();
    return result;
}

#include "relaycore/app/application.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/core/profiling.h"
#include "relaygame/demo_game.h"
#include "relaygame/player_settings.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <toml++/toml.hpp>

#include "scoped_temp_directory.h"

namespace
{
    using Clock = std::chrono::steady_clock;
    struct Stage {
        const char* name;
        glm::vec3 position, target;
    };
    constexpr std::array stages{
        Stage{"observation", {0, .01F, 9}, {0, 2, 34}},
        Stage{"workshop", {-21, .01F, 19}, {-21, .6F, 23}},
        Stage{"archive", {-26, .01F, 51}, {-17.5F, 1.2F, 51.5F}},
        Stage{"return", {-7, .01F, 24}, {0, 3, 33}},
        Stage{"retry", {-26, .01F, 51}, {-17.5F, 1.2F, 51.5F}},
        Stage{"pause", {0, .01F, 9}, {0, 2, 34}},
        Stage{"resize", {0, .01F, 9}, {0, 2, 34}},
    };

    class ContainmentProfile final : public RelayCore::AppDelegate {
    public:
        ContainmentProfile(std::filesystem::path output, RelayGame::GraphicsPreset preset, bool motion_captures) :
            output_(std::move(output)), preset_(preset), motion_captures_(motion_captures)
        {}

        void on_startup(RelayCore::EngineServices& services) override
        {
            start_stage(services);
        }
        void on_event(const SDL_Event& event, RelayCore::EngineServices& services) override
        {
            game_->on_event(event, services);
        }
        void on_update(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services) override
        {
            const auto now = Clock::now();
            const double frame_ms = std::chrono::duration<double, std::milli>(now - previous_update_).count();
            previous_update_ = now;
            elapsed_ = std::chrono::duration<double>(now - stage_beginning_).count();
            if (elapsed_ >= 4)
            {
                const auto& player = services.renderer().player_diagnostics();
                if (stage_ == 1 && player.shots_hit == 0)
                    throw std::runtime_error("Workshop firing did not hit an enemy");
                if (stage_ == 2 && player.damage_taken == 0)
                    throw std::runtime_error("Archive shooter did not damage the player");
                if (stage_ == 4 && (player.deaths == 0 || !player.alive))
                    throw std::runtime_error("Chapter retry did not complete death/respawn");
                report_frames();
                game_->return_to_menu(services);
                game_->on_shutdown(services);
                if (++stage_ == stages.size())
                {
                    stage_ = 0;
                    if (++pass_ == 2)
                    {
                        services.request_quit();
                        return;
                    }
                }
                start_stage(services);
                return;
            }
            RELAYCORE_PROFILE_PLOT("containment_pass", pass_);
            RELAYCORE_PROFILE_PLOT("containment_stage", stage_);
            const bool capturing = pass_ == 0 && elapsed_ > 1 && !captured_;
            RELAYCORE_PROFILE_PLOT("containment_settled", elapsed_ > .5 && stage_ != 6 && !capturing);
            if (elapsed_ > .5 && !captured_previous_frame_)
                frames_.push_back(frame_ms);
            captured_previous_frame_ = false;
            if (pass_ == 0 && elapsed_ > 1 && !captured_)
                capture(services, stages[stage_].name);
            if (motion_captures_ && pass_ == 0 && stage_ < 2 && elapsed_ >= next_motion_ && elapsed_ < 3)
            {
                capture(services, std::string{"motion-"} + stages[stage_].name + "-" + std::to_string(motion_frame_++));
                next_motion_ += .125;
            }
            if (stage_ == 1 && elapsed_ >= next_shot_)
            {
                SDL_Event event{};
                event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
                event.button.button = SDL_BUTTON_LEFT;
                game_->on_event(event, services);
                next_shot_ += .6;
            }
            if (stage_ == 0 || stage_ == 3)
            {
                SDL_Event event{};
                event.type = SDL_EVENT_MOUSE_MOTION;
                event.motion.xrel = static_cast<float>(std::cos(elapsed_ * 2) * frame.delta_seconds * 90);
                game_->on_event(event, services);
            }
            if (stage_ == 6)
            {
                const int size = static_cast<int>(elapsed_);
                if (size != resized_)
                {
                    const bool smaller = size % 2 == 0;
                    if (!SDL_SetWindowSize(services.window().handle(), smaller ? 1280 : 1920, smaller ? 720 : 1080))
                        throw std::runtime_error(SDL_GetError());
                    resized_ = size;
                }
            }
            game_->on_update(frame, services);
        }
        void on_render(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services) override
        {
            game_->on_render(frame, services);
            RELAYCORE_PROFILE_PLOT("containment_draws", services.renderer().world_diagnostics().draw_calls);
            RELAYCORE_PROFILE_PLOT("containment_triangles", services.renderer().world_diagnostics().triangles);
        }
        auto has_custom_ui() const noexcept -> bool override
        {
            return game_->has_custom_ui();
        }
        void on_ui(RelayCore::EngineServices& services) override
        {
            game_->on_ui(services);
        }
        void on_shutdown(RelayCore::EngineServices& services) noexcept override
        {
            if (game_)
                game_->on_shutdown(services);
        }

    private:
        void capture(RelayCore::EngineServices& services, const std::string& name)
        {
            RELAYCORE_PROFILE_ZONE_N("ContainmentProfile::capture");
            const auto pixels = services.renderer().capture_frame();
            auto* surface = SDL_CreateSurfaceFrom(
                pixels.width,
                pixels.height,
                SDL_PIXELFORMAT_RGBA32,
                const_cast<unsigned char*>(pixels.pixels.data()),
                pixels.width * 4
            );
            if (!surface)
                throw std::runtime_error(SDL_GetError());
            const bool saved = SDL_SaveBMP(surface, (output_ / (name + ".bmp")).string().c_str());
            SDL_DestroySurface(surface);
            if (!saved)
                throw std::runtime_error(SDL_GetError());
            captured_ = true;
            captured_previous_frame_ = true;
        }
        void start_stage(RelayCore::EngineServices& services)
        {
            RELAYCORE_PROFILE_ZONE_N("ContainmentProfile::prepare_stage");
            const auto beginning = Clock::now();
            RelayGame::GameWorldRuntime seed;
            seed.load_level_session(
                services.config(),
                RelayGame::LevelSessionBuilder{services.config().gameplay}.build(
                    RelayCore::core::asset_path("levels/containment_loop.toml")
                )
            );
            if (stage_ == 3)
            {
                for (const auto name : {"limiter_cassette", "limiter_socket", "grip_service", "shortcut_release"})
                {
                    const auto usable =
                        std::ranges::find(seed.usables(), name, &RelayCore::gameplay::RuntimeUsable::name);
                    if (usable == seed.usables().end())
                        throw std::runtime_error("Chapter apparatus control missing");
                    const auto outcome =
                        seed.debug_apply_usable_interaction(static_cast<std::size_t>(usable - seed.usables().begin()));
                    if (outcome.kind == RelayCore::gameplay::UseInteractionOutcome::None)
                        throw std::runtime_error("Chapter apparatus control was not activated");
                }
                seed.debug_update_containment(4);
            }
            auto save = seed.capture_saved_game(0, false);
            save.player.position = stages[stage_].position;
            const auto direction =
                glm::normalize(stages[stage_].target - save.player.position - glm::vec3{0, 1.62F, 0});
            save.player.view_angles = {std::atan2(direction.z, direction.x), std::asin(direction.y)};
            if (stage_ == 2 || stage_ == 4)
            {
                auto& shooter = save.enemies.at(2);
                shooter.position = {-17.5F, .01F, 51.5F};
                shooter.last_seen = save.player.position;
                shooter.mode = RelayGame::EnemyMode::Windup;
                shooter.timer = .35F;
                if (stage_ == 4)
                    save.world.player_combat.health = 1;
            }
            const auto directory = isolated_.path() / (std::to_string(pass_) + stages[stage_].name);
            RelayGame::write_saved_game(directory / "progress.toml", save);
            RelayGame::PlayerSettings settings{.vsync = false, .hdr_enabled = true};
            RelayGame::apply_graphics_preset(settings, preset_);
            RelayGame::save_player_settings(directory / "player_settings.toml", settings);
            game_ = std::make_unique<RelayGame::DemoGame>(directory);
            game_->on_startup(services);
            game_->begin_episode(true, services);
            if (game_->has_custom_ui())
                throw std::runtime_error("Chapter disk Continue did not enter gameplay");
            if (stage_ == 5)
            {
                SDL_Event event{};
                event.type = SDL_EVENT_KEY_DOWN;
                event.key.scancode = SDL_SCANCODE_ESCAPE;
                game_->on_event(event, services);
                if (!game_->has_custom_ui())
                    throw std::runtime_error("Pause UI did not open");
            }
            elapsed_ = 0;
            stage_beginning_ = Clock::now();
            previous_update_ = stage_beginning_;
            next_shot_ = .8;
            captured_ = false;
            captured_previous_frame_ = false;
            resized_ = -1;
            motion_frame_ = 0;
            next_motion_ = 1;
            frames_.clear();
            frames_.reserve(8192);
            std::cout << "pass=" << pass_ << "; stage=" << stages[stage_].name
                      << "; load_ms=" << std::chrono::duration<double, std::milli>(Clock::now() - beginning).count()
                      << '\n';
        }
        void report_frames()
        {
            if (frames_.empty())
                throw std::runtime_error("Chapter stage produced no settled frames");
            std::ranges::sort(frames_);
            std::cout << "frame; pass=" << pass_ << "; stage=" << stages[stage_].name << "; samples=" << frames_.size()
                      << "; p50_ms=" << frames_[frames_.size() / 2] << "; p95_ms=" << frames_[frames_.size() * 95 / 100]
                      << "; p99_ms=" << frames_[frames_.size() * 99 / 100] << "; max_ms=" << frames_.back() << '\n';
        }
        RelayCore::test::ScopedTempDirectory isolated_;
        std::filesystem::path output_;
        RelayGame::GraphicsPreset preset_;
        bool motion_captures_;
        std::unique_ptr<RelayGame::DemoGame> game_;
        std::vector<double> frames_;
        Clock::time_point stage_beginning_{}, previous_update_{};
        std::size_t stage_ = 0;
        int pass_ = 0, resized_ = -1;
        double elapsed_ = 0, next_shot_ = .8;
        double next_motion_ = 1;
        int motion_frame_ = 0;
        bool captured_ = false, captured_previous_frame_ = false;
    };
} // namespace

int main(int argc, char** argv)
{
    auto preset = RelayGame::GraphicsPreset::Custom;
    std::string preset_name = "baseline";
    const bool motion_captures = argc == 4 && std::string_view{argv[3]} == "--motion-captures";
    if (argc != 1)
    {
        if ((argc != 3 && !motion_captures) || std::string_view{argv[1]} != "--quality-preset")
            return 1;
        preset_name = argv[2];
        if (preset_name == "low")
            preset = RelayGame::GraphicsPreset::Low;
        else if (preset_name == "balanced")
            preset = RelayGame::GraphicsPreset::Balanced;
        else if (preset_name == "high")
            preset = RelayGame::GraphicsPreset::High;
        else
            return 1;
    }
    const auto output = std::filesystem::current_path() / "containment-profile";
    std::filesystem::create_directories(output);
    RelayCore::log::init({.logs_directory = (output / "logs").string(), .enable_timestamped_log = false});
    int result = 0;
    try
    {
        RelayCore::test::ScopedTempDirectory temp;
        auto config = toml::parse_file(RelayCore::core::asset_path("config/engine.toml").string());
        auto& window = *config["window"].as_table();
        window.insert_or_assign("width", 1920);
        window.insert_or_assign("height", 1080);
        window.insert_or_assign("high_dpi", false);
        window.insert_or_assign("vsync", false);
        config["render"].as_table()->insert_or_assign("hdr_enabled", true);
        const auto path = temp.path() / "engine.toml";
        {
            std::ofstream file{path};
            file << config;
        }
        std::cout << "containment_profile; resolution=1920x1080; vsync=off; audio=on; quality=" << preset_name
                  << "; MSAA=4; motion_captures=" << motion_captures << "; camera_sweeps_only\n";
        RelayCore::Application app{
            std::make_unique<ContainmentProfile>(output, preset, motion_captures),
            path,
            RelayCore::render::ColorPipelineMode::hardware_srgb
        };
        result = app.run();
        std::cout << "containment_profile=passed\n";
    } catch (const std::exception& error)
    {
        std::cerr << "containment_profile=failed: " << error.what() << '\n';
        result = 1;
    }
    RelayCore::log::shutdown();
    return result;
}

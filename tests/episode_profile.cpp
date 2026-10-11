#include "relaycore/app/application.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/core/profiling.h"
#include "relaygame/demo_game.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <toml++/toml.hpp>

#include "scoped_temp_directory.h"

namespace
{
    struct Stage {
        const char* name;
        int actions;
        glm::vec3 position;
        glm::vec3 target;
        double seconds = 4;
    };
    const std::array stages{
        Stage{"menu", 0, {-2, 0.01F, -3.5F}, {-2, 1.65F, 5}, 2},
        Stage{"entrance", 0, {-2, 0.01F, -3.5F}, {-2, 1.65F, 5}},
        Stage{"rover", 2, {19.5F, 0.01F, 34}, {21.5F, 0.65F, 34}},
        Stage{"power_on", 3, {14.3F, 0.01F, 20.2F}, {14.3F, 1.5F, 22}},
        Stage{"filter_turret", 4, {7.5F, 0.01F, 26}, {7.5F, 1.55F, 30.7F}},
        Stage{"cooling_on", 5, {7, 0.01F, 6.6F}, {7, 1.4F, 5.44F}},
        Stage{"return_turret", 6, {4.5F, 0.01F, 7}, {2.48F, 1.55F, 8.5F}},
        Stage{"death_respawn", 0, {6.8F, 0.01F, 18}, {7.5F, 1.55F, 30.7F}},
        Stage{"pause", 6, {3.5F, 0.01F, 3.5F}, {0, 1.5F, 5}, 2},
        Stage{"finale_complete", 7, {0, 0.01F, 9}, {0, 1.5F, 12}, 8},
        Stage{"editor", 0, {-2, 0.01F, -3.5F}, {-2, 1.65F, 5}, 2},
    };

    class EpisodeProfile final : public RelayCore::AppDelegate {
    public:
        explicit EpisodeProfile(std::filesystem::path data) : data_(std::move(data)) {}

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
            elapsed_ += frame.delta_seconds;
            if (elapsed_ >= stages[stage_].seconds)
            {
                const auto& player = services.renderer().player_diagnostics();
                if ((stage_ == 2 || stage_ == 4 || stage_ == 6) && player.shots_hit == 0)
                    throw std::runtime_error("Profile encounter did not register a hit");
                if (stage_ == 7 && (player.deaths == 0 || !player.alive))
                    throw std::runtime_error("Profile did not complete death/respawn");
                if ((stage_ == 3 || stage_ == 5)
                    && services.renderer().interaction_diagnostics().objective_current
                           <= static_cast<std::uint32_t>(stages[stage_].actions + 1))
                    throw std::runtime_error("Profile device interaction did not advance the mission");
                report_frames();
                game_->on_shutdown(services);
                if (++stage_ == stages.size())
                {
                    game_.reset();
                    services.request_quit();
                    return;
                }
                start_stage(services);
            }
            RELAYCORE_PROFILE_PLOT("episode_stage", stage_);
            RELAYCORE_PROFILE_PLOT("episode_settled", elapsed_ > 0.5);
            if (elapsed_ > 0.5)
                frames_.push_back(frame.delta_seconds * 1000);
            if ((stage_ == 3 || stage_ == 5) && elapsed_ > 0.7 && !used_)
            {
                key(SDL_SCANCODE_E, services);
                used_ = true;
            }
            if ((stage_ == 2 || stage_ == 4 || stage_ == 6) && elapsed_ >= next_shot_)
            {
                SDL_Event shot{};
                shot.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
                shot.button.button = SDL_BUTTON_LEFT;
                game_->on_event(shot, services);
                next_shot_ += 0.6;
            }
            game_->on_update(frame, services);
        }

        void on_render(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services) override
        {
            game_->on_render(frame, services);
        }

        auto has_custom_ui() const noexcept -> bool override
        {
            return game_ && game_->has_custom_ui();
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
        void key(SDL_Scancode code, RelayCore::EngineServices& services)
        {
            SDL_Event event{};
            event.type = SDL_EVENT_KEY_DOWN;
            event.key.scancode = code;
            game_->on_event(event, services);
        }

        void start_stage(RelayCore::EngineServices& services)
        {
            RELAYCORE_PROFILE_ZONE_N("EpisodeProfile::prepare_stage");
            const auto& stage = stages[stage_];
            RelayGame::GameWorldRuntime seed;
            seed.load_level_session(
                services.config(),
                RelayGame::LevelSessionBuilder{services.config().gameplay}.build()
            );
            constexpr std::array actions{
                "spare_fuse",
                "maintenance_isolator",
                "bus_relay",
                "power_breaker",
                "filter_purge",
                "cooling_control",
                "airlock_reader"
            };
            for (int i = 0; i < stage.actions; ++i)
            {
                const auto found =
                    std::ranges::find(seed.usables(), actions[i], &RelayCore::gameplay::RuntimeUsable::name);
                if (found == seed.usables().end())
                    throw std::runtime_error("Profile fixture interaction missing");
                static_cast<void>(
                    seed.debug_apply_usable_interaction(static_cast<std::size_t>(found - seed.usables().begin()))
                );
                seed.debug_update_mission(RelayCore::physics::make_player_aabb({3.5F, 0.01F, 7}, 0.3F, 1.8F));
            }
            seed.debug_update_mission(RelayCore::physics::make_player_aabb(stage.position, 0.3F, 1.8F));
            auto save = seed.capture_saved_game(0, false);
            save.player.position = stage.position;
            const glm::vec3 direction = glm::normalize(stage.target - stage.position - glm::vec3{0, 1.62F, 0});
            save.player.view_angles = {std::atan2(direction.z, direction.x), std::asin(direction.y)};
            if (stage_ == 7)
                save.world.player_combat.health = 1;
            const auto directory = data_ / stage.name;
            RelayGame::write_saved_game(directory / "progress.toml", save);
            game_ = std::make_unique<RelayGame::DemoGame>(directory);
            game_->on_startup(services);
            if (stage_ != 0)
                key(SDL_SCANCODE_RETURN, services);
            if (stage_ == 8)
                key(SDL_SCANCODE_ESCAPE, services);
            if (stage_ == 10)
                game_->enter_editor(services);
            elapsed_ = 0;
            next_shot_ = 0.8;
            used_ = false;
            frames_.clear();
            frames_.reserve(8192);
            std::cout << "stage_begin=" << stage_ << "; name=" << stage.name << '\n';
        }

        void report_frames()
        {
            if (frames_.empty())
                throw std::runtime_error("Profile stage produced no settled frames");
            std::ranges::sort(frames_);
            std::cout << "frame; stage=" << stages[stage_].name << "; samples=" << frames_.size()
                      << "; p50_ms=" << frames_[frames_.size() / 2] << "; p95_ms=" << frames_[frames_.size() * 95 / 100]
                      << "; max_ms=" << frames_.back() << '\n';
        }

        std::filesystem::path data_;
        std::unique_ptr<RelayGame::DemoGame> game_;
        std::size_t stage_ = 0;
        double elapsed_ = 0, next_shot_ = 0;
        bool used_ = false;
        std::vector<double> frames_;
    };
} // namespace

int main()
{
    RelayCore::test::ScopedTempDirectory directory;
    RelayCore::log::init({.logs_directory = (directory.path() / "logs").string(), .enable_timestamped_log = false});
    int result = 0;
    try
    {
        auto config = toml::parse_file(RelayCore::core::asset_path("config/engine.toml").string());
        config["window"].as_table()->insert_or_assign("width", 1280);
        config["window"].as_table()->insert_or_assign("height", 720);
        config["window"].as_table()->insert_or_assign("vsync", false);
        config["window"].as_table()->insert_or_assign("high_dpi", false);
        config["audio"].as_table()->insert_or_assign("master_volume", 0.0);
        const auto config_path = directory.path() / "engine.toml";
        {
            std::ofstream output{config_path};
            output << config;
        }
        RelayCore::Application app{std::make_unique<EpisodeProfile>(directory.path() / "player"), config_path};
        result = app.run();
        std::cout << "episode_profile=passed\n";
    } catch (const std::exception& error)
    {
        std::cerr << "episode_profile=failed: " << error.what() << '\n';
        result = 1;
    }
    RelayCore::log::shutdown();
    return result;
}

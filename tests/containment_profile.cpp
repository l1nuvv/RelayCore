#include "relaycore/app/application.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/core/profiling.h"
#include "relaygame/demo_game.h"
#include "relaygame/player_settings.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <toml++/toml.hpp>

#include "scoped_temp_directory.h"
#if defined(RELAYCORE_PROFILE_ALLOCATIONS)
#include "profile_allocations.h"
#endif

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
        ContainmentProfile(
            std::filesystem::path output,
            RelayGame::GraphicsPreset preset,
            bool motion_captures,
            std::filesystem::path settings,
            bool captures,
            bool scripted_input,
            int passes,
            double stage_seconds,
            bool diagnostics,
            bool health_feedback
        ) :
            output_(std::move(output)), preset_(preset), motion_captures_(motion_captures),
            settings_path_(std::move(settings)), captures_(captures), scripted_input_(scripted_input), passes_(passes),
            stage_seconds_(stage_seconds), diagnostics_(diagnostics), health_feedback_(health_feedback)
        {}

        void on_startup(RelayCore::EngineServices& services) override
        {
#if defined(RELAYCORE_PROFILE_ALLOCATIONS)
            allocations_.open(output_ / "allocations.csv");
            if (!allocations_)
                throw std::runtime_error("Cannot write allocation samples");
            allocations_ << "frame,pass,stage,settled,count,bytes,update_count,"
                            "update_bytes,render_count,render_bytes,ui_count,ui_bytes\n";
#endif
            start_stage(services);
        }
        void on_event(const SDL_Event& event, RelayCore::EngineServices& services) override
        {
            if (!scripted_input_)
                game_->on_event(event, services);
        }
        void on_update(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services) override
        {
#if defined(RELAYCORE_PROFILE_ALLOCATIONS)
            const auto current = RelayCore::test::allocation_sample();
            if (have_sample_)
                allocations_ << frame_number_++ << ',' << sample_pass_ << ',' << sample_stage_ << ',' << sample_settled_
                             << ',' << current.count - previous_sample_.count << ','
                             << current.bytes - previous_sample_.bytes << ',' << update_allocations_.count << ','
                             << update_allocations_.bytes << ',' << render_allocations_.count << ','
                             << render_allocations_.bytes << ',' << ui_allocations_.count << ','
                             << ui_allocations_.bytes << '\n';
            previous_sample_ = RelayCore::test::allocation_sample();
            have_sample_ = true;
            sample_pass_ = pass_;
            sample_stage_ = stage_;
            sample_settled_ = elapsed_ > .5 && stage_ != 6;
            update_allocations_ = render_allocations_ = ui_allocations_ = {};
#endif
            if (scripted_input_)
            {
                RELAYCORE_PROFILE_ZONE_N("ContainmentProfile::isolate_input");
                SDL_ResetKeyboard();
            }
            const auto now = Clock::now();
            const double frame_ms = std::chrono::duration<double, std::milli>(now - previous_update_).count();
            previous_update_ = now;
            elapsed_ = std::chrono::duration<double>(now - stage_beginning_).count();
            if (diagnostics_ && stage_ == 6 ? resized_ >= 15 && elapsed_ >= transition_at_ : elapsed_ >= stage_seconds_)
            {
                const auto& player = services.renderer().player_diagnostics();
                if (stage_ == 1 && player.shots_hit == 0)
                    throw std::runtime_error("Workshop firing did not hit an enemy");
                if (stage_ == 2 && player.damage_taken == 0)
                    throw std::runtime_error("Archive shooter did not damage the player");
                if (stage_ == 4 && (player.deaths == 0 || !player.alive))
                    throw std::runtime_error("Chapter retry did not complete death/respawn");
#if defined(RELAYCORE_PROFILE_ALLOCATIONS)
                sample_settled_ = false;
#endif
                if (diagnostics_)
                    sample_memory(services, "stage-end");
                report_frames();
                game_->return_to_menu(services);
                game_->on_shutdown(services);
                if (++stage_ == stages.size())
                {
                    stage_ = 0;
                    if (++pass_ == passes_)
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
            const bool capturing = captures_ && pass_ == 0 && elapsed_ > 1 && !captured_;
            RELAYCORE_PROFILE_PLOT("containment_settled", elapsed_ > .5 && stage_ != 6 && !capturing);
            if (elapsed_ > .5 && !captured_previous_frame_)
                frames_.push_back(frame_ms);
            captured_previous_frame_ = false;
            if (capturing)
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
            if (stage_ == 6 && !diagnostics_)
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
            if (stage_ == 6 && diagnostics_)
                transition(services);
#if defined(RELAYCORE_PROFILE_ALLOCATIONS)
            const auto before = RelayCore::test::allocation_sample();
#endif
            game_->on_update(frame, services);
#if defined(RELAYCORE_PROFILE_ALLOCATIONS)
            const auto after = RelayCore::test::allocation_sample();
            update_allocations_ = {.count = after.count - before.count, .bytes = after.bytes - before.bytes};
#endif
        }
        void on_render(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services) override
        {
#if defined(RELAYCORE_PROFILE_ALLOCATIONS)
            const auto before = RelayCore::test::allocation_sample();
#endif
            game_->on_render(frame, services);
#if defined(RELAYCORE_PROFILE_ALLOCATIONS)
            const auto after = RelayCore::test::allocation_sample();
            render_allocations_ = {.count = after.count - before.count, .bytes = after.bytes - before.bytes};
#endif
            {
                RELAYCORE_PROFILE_ZONE_N("ContainmentProfile::sample_extent");
                const auto extent = services.window().pixel_extent();
                RELAYCORE_PROFILE_PLOT("containment_width", extent.width);
                RELAYCORE_PROFILE_PLOT("containment_height", extent.height);
            }
            RELAYCORE_PROFILE_PLOT("containment_draws", services.renderer().world_diagnostics().draw_calls);
            RELAYCORE_PROFILE_PLOT("containment_triangles", services.renderer().world_diagnostics().triangles);
            if (diagnostics_ && memory_pending_ && elapsed_ >= transition_at_ - .5)
            {
                sample_memory(services, "transition-rendered");
                memory_pending_ = false;
            }
        }
        auto has_custom_ui() const noexcept -> bool override
        {
            return game_->has_custom_ui();
        }
        void on_ui(RelayCore::EngineServices& services) override
        {
#if defined(RELAYCORE_PROFILE_ALLOCATIONS)
            const auto before = RelayCore::test::allocation_sample();
#endif
            if (health_feedback_ && stage_ != 5)
            {
                RELAYCORE_PROFILE_ZONE_N("HUD profile snapshot");
                const double phase = std::fmod(elapsed_, 2.0);
                const float health = phase < .6 ? 100.0F : phase < .7 ? 75.0F : phase < .8 ? 50.0F : 20.0F;
                RELAYCORE_PROFILE_PLOT("health_feedback_active", phase >= .6 && phase < 1.4);
                RELAYCORE_PROFILE_PLOT("health_feedback_target", health);
                services.renderer().player_ui().set_hud(
                    {.visible = true,
                     .health = health,
                     .max_health = 100,
                     .weapon = "ПИСТОЛЕТ",
                     .hint = "1 / 2 — сменить оружие",
                     .objective_header = "ПЕРЕПУСК ДАВЛЕНИЯ",
                     .objective_title = "Восстановить контур",
                     .objective_detail = "Проверить оборудование и открыть обратный маршрут."}
                );
            } else
                game_->on_ui(services);
#if defined(RELAYCORE_PROFILE_ALLOCATIONS)
            const auto after = RelayCore::test::allocation_sample();
            ui_allocations_ = {.count = after.count - before.count, .bytes = after.bytes - before.bytes};
#endif
        }
        void on_shutdown(RelayCore::EngineServices& services) noexcept override
        {
            if (game_)
                game_->on_shutdown(services);
        }

    private:
        void sample_memory(RelayCore::EngineServices& services, const char* boundary)
        {
            RELAYCORE_PROFILE_ZONE_N("ContainmentProfile::memory_boundary");
            const auto label =
                std::to_string(pass_) + "/" + stages[stage_].name + "/" + boundary + "/" + std::to_string(resized_);
            services.renderer().log_memory_usage(label);
#if defined(RELAYCORE_PROFILE_ALLOCATIONS)
            const auto sample = RelayCore::test::allocation_sample();
            std::cout << "cpp_memory; reason=" << label << "; count=" << sample.count << "; bytes=" << sample.bytes
                      << '\n';
#endif
        }
        void key(RelayCore::EngineServices& services, SDL_Scancode scancode)
        {
            SDL_Event event{};
            event.type = SDL_EVENT_KEY_DOWN;
            event.key.scancode = scancode;
            game_->on_event(event, services);
        }
        void setting(const char* id, const char* value)
        {
            auto* context = Rml::GetContext("player");
            auto* control =
                context ? dynamic_cast<Rml::ElementFormControl*>(context->GetDocument("menu")->GetElementById(id))
                        : nullptr;
            if (!control)
                throw std::runtime_error("Missing diagnostic settings control");
            control->SetValue(value);
            control->DispatchEvent("change", {});
        }
        void transition(RelayCore::EngineServices& services)
        {
            if (elapsed_ < transition_at_)
                return;
            const auto step = resized_ + 1;
            RELAYCORE_PROFILE_ZONE_N("ContainmentProfile::transition");
            resized_ = step;
            switch (step)
            {
                case 0:
                    if (!SDL_RestoreWindow(services.window().handle())
                        || !SDL_SetWindowSize(services.window().handle(), 1280, 720))
                        throw std::runtime_error(SDL_GetError());
                    break;
                case 1:
                    if (services.window().pixel_extent().width != 1280)
                        throw std::runtime_error("Diagnostic resize did not reach 1280 pixels");
                    if (!SDL_SetWindowSize(services.window().handle(), 1920, 1080))
                        throw std::runtime_error(SDL_GetError());
                    break;
                case 2:
                    setting("render-scale", "0.75");
                    break;
                case 3:
                    setting("render-scale", "1");
                    break;
                case 4:
                    setting("dynamic-resolution", "1");
                    break;
                case 5:
                    setting("dynamic-resolution", "0");
                    break;
                case 6:
                    setting("graphics-preset", "2");
                    break;
                case 7:
                    setting("graphics-preset", "3");
                    break;
                case 8:
                    key(services, SDL_SCANCODE_F9);
                    break;
                case 9:
                    key(services, SDL_SCANCODE_F10);
                    break;
                case 10:
                    key(services, SDL_SCANCODE_F7);
                    break;
                case 11:
                    game_->enter_editor(services);
                    break;
                case 12:
                    game_->return_from_editor(services);
                    break;
                case 13:
                    game_->return_to_menu(services);
                    break;
                case 14:
                    game_->begin_episode(true, services);
                    break;
                default:
                    break;
            }
            transition_at_ = std::chrono::duration<double>(Clock::now() - stage_beginning_).count() + 1;
            memory_pending_ = true;
            const auto extent = services.window().pixel_extent();
            std::cout << "transition; pass=" << pass_ << "; step=" << step << "; pixels=" << extent.width << 'x'
                      << extent.height << '\n';
        }
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
            if (!settings_path_.empty())
                settings = RelayGame::load_player_settings(settings_path_, settings);
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
            transition_at_ = 0;
            motion_frame_ = 0;
            next_motion_ = 1;
            frames_.clear();
            frames_.reserve(8192);
            if (diagnostics_)
                sample_memory(services, "stage-start");
            const auto extent = services.window().pixel_extent();
            std::cout << "pass=" << pass_ << "; stage=" << stages[stage_].name << "; pixel_width=" << extent.width
                      << "; pixel_height=" << extent.height << "; refresh_hz=" << services.window().refresh_rate_hz()
                      << "; vsync=" << services.window().vsync_enabled() << "; fov=" << settings.fov
                      << "; scale=" << settings.render_scale
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
#if defined(RELAYCORE_PROFILE_ALLOCATIONS)
        std::ofstream allocations_;
        RelayCore::test::AllocationSample previous_sample_{}, update_allocations_{}, render_allocations_{},
            ui_allocations_{};
        uint64_t frame_number_ = 0;
        int sample_pass_ = 0;
        size_t sample_stage_ = 0;
        bool have_sample_ = false, sample_settled_ = false;
#endif
        RelayCore::test::ScopedTempDirectory isolated_;
        std::filesystem::path output_;
        RelayGame::GraphicsPreset preset_;
        bool motion_captures_;
        std::filesystem::path settings_path_;
        bool captures_;
        bool scripted_input_;
        int passes_;
        double stage_seconds_;
        bool diagnostics_, memory_pending_ = false;
        bool health_feedback_;
        std::unique_ptr<RelayGame::DemoGame> game_;
        std::vector<double> frames_;
        Clock::time_point stage_beginning_{}, previous_update_{};
        std::size_t stage_ = 0;
        int pass_ = 0, resized_ = -1;
        double elapsed_ = 0, next_shot_ = .8, transition_at_ = 0;
        double next_motion_ = 1;
        int motion_frame_ = 0;
        bool captured_ = false, captured_previous_frame_ = false;
    };
} // namespace

int main(int argc, char** argv)
{
    auto preset = RelayGame::GraphicsPreset::Custom;
    std::string preset_name = "baseline";
    bool motion_captures = false, captures = true, scripted_input = false, diagnostics = false, health_feedback = false;
    int passes = 2;
    double stage_seconds = 4;
    std::filesystem::path settings_path, config_path;
    for (int argument = 1; argument < argc; ++argument)
    {
        const std::string_view option{argv[argument]};
        if ((option == "--passes" || option == "--stage-seconds") && argument + 1 < argc)
        {
            const std::string_view value{argv[++argument]};
            int parsed = 0;
            const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
            if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || parsed < 1 || parsed > 600)
                return 1;
            if (option == "--passes")
                passes = parsed;
            else
                stage_seconds = parsed;
        } else if (option == "--diagnostics")
            diagnostics = true;
        else if (option == "--motion-captures")
            motion_captures = true;
        else if (option == "--no-captures")
            captures = false;
        else if (option == "--scripted-input-only")
            scripted_input = true;
        else if (option == "--health-feedback")
            health_feedback = true;
        else if ((option == "--settings-file" || option == "--config-file") && argument + 1 < argc)
        {
            auto& path = option == "--settings-file" ? settings_path : config_path;
            path = argv[++argument];
            if (!std::filesystem::is_regular_file(path))
            {
                std::cerr << option << ": file does not exist: " << path << '\n';
                return 1;
            }
        } else if (option == "--quality-preset" && argument + 1 < argc)
        {
            preset_name = argv[++argument];
            if (preset_name == "low")
                preset = RelayGame::GraphicsPreset::Low;
            else if (preset_name == "balanced")
                preset = RelayGame::GraphicsPreset::Balanced;
            else if (preset_name == "high")
                preset = RelayGame::GraphicsPreset::High;
            else
                return 1;
        } else
            return 1;
    }
    if ((motion_captures && !captures) || (diagnostics && captures) || stage_seconds < 4)
        return 1;
    const auto output = std::filesystem::current_path() / "containment-profile";
    std::filesystem::create_directories(output);
    RelayCore::log::init({.logs_directory = (output / "logs").string(), .enable_timestamped_log = false});
    int result = 0;
    try
    {
        RelayCore::test::ScopedTempDirectory temp;
        auto config = toml::parse_file(
            (config_path.empty() ? RelayCore::core::asset_path("config/engine.toml") : config_path).string()
        );
        if (!config["window"].is_table() || !config["render"].is_table())
            throw std::runtime_error("Profile config requires [window] and [render] tables");
        auto& window = *config["window"].as_table();
        if (config_path.empty())
        {
            window.insert_or_assign("width", 1920);
            window.insert_or_assign("height", 1080);
            window.insert_or_assign("high_dpi", false);
        }
        window.insert_or_assign("vsync", false);
        config["render"].as_table()->insert_or_assign("hdr_enabled", true);
        const auto path = temp.path() / "engine.toml";
        {
            std::ofstream file{path};
            file << config;
        }
        std::cout << "containment_profile; width=" << window["width"].value_or(0)
                  << "; height=" << window["height"].value_or(0) << "; quality=" << preset_name
                  << "; settings=" << settings_path << "; config=" << config_path << "; captures=" << captures
                  << "; motion_captures=" << motion_captures << "; scripted_input=" << scripted_input
                  << "; health_feedback=" << health_feedback << "; camera_sweeps_only\n";
        RelayCore::Application app{
            std::make_unique<ContainmentProfile>(
                output,
                preset,
                motion_captures,
                settings_path,
                captures,
                scripted_input,
                passes,
                stage_seconds,
                diagnostics,
                health_feedback
            ),
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

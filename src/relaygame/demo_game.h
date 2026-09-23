#pragma once

#include "relaycore/app/app_delegate.h"
#include "relaycore/render/render_diagnostics.h"
#include "relaygame/demo_game_runtime_ops.h"
#include "relaygame/game_world_runtime.h"
#include "relaygame/level_editor.h"
#include "relaygame/player_settings.h"
#include "relaygame/runtime_model.h"
#include "relaygame/world_scene_renderer.h"

#include <memory>
#include <optional>

namespace RelayGame
{
    class DemoGame final : public RelayCore::AppDelegate {
    public:
        explicit DemoGame(std::filesystem::path user_data_directory = {}) :
            user_data_directory_(std::move(user_data_directory))
        {}
        void on_startup(RelayCore::EngineServices& services) override;

        void on_event(const SDL_Event& event, RelayCore::EngineServices& services) override;

        void on_update(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services) override;

        void on_render(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services) override;

        void on_shutdown(RelayCore::EngineServices& services) noexcept override;

        [[nodiscard]] auto has_custom_ui() const noexcept -> bool override
        {
            return editor_.active() || screen_ != Screen::Playing;
        }
        void on_ui(RelayCore::EngineServices& services) override;

        void begin_episode(bool continuing, RelayCore::EngineServices& services);
        void return_to_menu(RelayCore::EngineServices& services);
        void enter_editor(RelayCore::EngineServices& services);
        void begin_editor_preview(RelayCore::EngineServices& services);
        void return_from_editor(RelayCore::EngineServices& services);
        void transition_to(LevelExit exit, RelayCore::EngineServices& services);

    private:
        enum class Screen {
            MainMenu,
            Playing,
            Editor,
            Paused,
            Finale,
            Complete
        };
        void change_screen(Screen screen, RelayCore::EngineServices& services);
        void install_level_session(detail::PreparedLevelSession prepared, RelayCore::EngineServices& services);
        struct EditorReturnState {
            detail::PreparedLevelSession session;
            float played_seconds = 0;
            float scene_time = 0;
            bool flashlight = false;
            bool progress_enabled = false;
            Screen return_screen = Screen::Playing;
            std::vector<SavedGame> visited_levels;
        };
        std::optional<EditorReturnState> editor_return_;
        glm::vec3 editor_camera_position_{};
        glm::vec2 editor_camera_angles_{};
        Screen paused_screen_ = Screen::Playing;
        void initialize_progress();
        auto save_progress() noexcept -> bool;
        void begin_finale(RelayCore::EngineServices& services);
        void update_finale(float dt, RelayCore::EngineServices& services);
        void draw_finale();
        std::filesystem::path user_data_directory_;
        std::filesystem::path progress_path_;
        std::optional<SavedGame> disk_save_;
        std::string progress_message_;
        Screen screen_ = Screen::MainMenu;
        bool progress_enabled_ = false;
        bool show_settings_ = false;
        bool confirm_new_game_ = false;
        bool save_file_exists_ = false;
        bool window_focused_ = true;
        float played_seconds_ = 0;
        float autosave_elapsed_ = 0;
        std::uint32_t saved_step_ = 0;
        float finale_time_ = 0;
        glm::vec3 finale_start_position_{};
        glm::vec2 finale_start_angles_{};
        void initialize_player_settings(RelayCore::EngineServices& services);
        void apply_player_settings(RelayCore::EngineServices& services);
        void set_pause(bool paused, RelayCore::EngineServices& services);
        void draw_pause_menu(RelayCore::EngineServices& services);
        PlayerSettings settings_{};
        std::filesystem::path settings_path_;
        std::string settings_message_;
        bool settings_dirty_ = false;
        bool flashlight_on_ = false;
        float scene_time_ = 0.0F;
        void start_level_session(RelayCore::EngineServices& services);

        void reload_live_content(RelayCore::EngineServices& services);

        void handle_debug_iteration_tools(RelayCore::EngineServices& services);

        void publish_iteration_diagnostics(RelayCore::EngineServices& services);

        RelayGame::GameWorldRuntime runtime_{};
        RelayGame::WorldSceneRenderer world_scene_{};
        LevelEditor editor_{};
        std::vector<SavedGame> visited_levels_;
        std::string new_game_level_;
        RelayCore::render::IterationDiagnostics iteration_diagnostics_{};
        std::optional<RelayGame::RuntimeLevelSnapshot> saved_runtime_snapshot_{};
    };

    [[nodiscard]] auto make_demo_game() -> std::unique_ptr<RelayCore::AppDelegate>;
} // namespace RelayGame

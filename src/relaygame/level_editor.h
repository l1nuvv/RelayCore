#pragma once

#include "relaycore/render/camera.h"
#include "relaygame/level_editor_document.h"
#include "relaygame/world_scene_renderer.h"

#include <SDL3/SDL_events.h>
#include <memory>
#include <mutex>

namespace RelayCore
{
    class EngineServices;
    struct FrameInfo;
} // namespace RelayCore

namespace RelayGame
{
    class LevelEditor {
    public:
        void enter(const RelayCore::BlockoutLevel& level, RelayCore::EngineServices& services);
        void leave(RelayCore::EngineServices& services);
        void resume(RelayCore::EngineServices& services);
        [[nodiscard]] auto exit_requested() const noexcept -> bool
        {
            return exit_requested_;
        }
        void shutdown() noexcept;
        void suspend_camera() noexcept
        {
            flying_ = false;
            drag_axis_ = -1;
        }
        void on_event(const SDL_Event& event, RelayCore::EngineServices& services);
        void update(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services);
        void render(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services);
        void draw_ui(RelayCore::EngineServices& services);
        void open_map(const std::filesystem::path& path, RelayCore::EngineServices& services, bool discard = false);
        void new_map(RelayCore::EngineServices& services, bool discard = false);
        void save_map(const std::filesystem::path& path, RelayCore::EngineServices& services);
        [[nodiscard]] auto active() const -> bool
        {
            return active_;
        }
        [[nodiscard]] auto play_requested() const -> bool
        {
            return play_requested_;
        }
        void request_play()
        {
            play_requested_ = true;
        }
        void report_error(std::string message)
        {
            message_ = std::move(message);
            play_requested_ = false;
        }
        [[nodiscard]] auto document() const -> const LevelEditorDocument&
        {
            return document_;
        }
        [[nodiscard]] auto save_path() const -> const std::filesystem::path&
        {
            return save_path_;
        }

    private:
        struct OpenDialogResult {
            std::mutex mutex;
            std::filesystem::path path;
            std::string error;
            bool finished = false;
        };

        void rebuild(RelayCore::EngineServices& services);
        void rebuild_scene(
            const RelayCore::BlockoutLevel& level,
            const std::filesystem::path& path,
            RelayCore::EngineServices& services
        );
        void draw_map_tools(RelayCore::EngineServices& services);
        void draw_object_properties(RelayCore::EngineServices& services);
        void draw_asset_tools(glm::vec3 position);
        void refresh_catalogs();
        void draw_selection(RelayCore::EngineServices& services);
        LevelEditorDocument document_;
        WorldSceneRenderer scene_;
        std::vector<RelayCore::gameplay::RuntimeDoor> doors_;
        std::vector<RelayCore::gameplay::RuntimeUsable> usables_;
        std::vector<RelayCore::gameplay::RuntimeTurret> turrets_;
        std::vector<RuntimeEnemy> enemies_;
        std::vector<std::string> light_targets_;
        std::vector<EnvironmentEffect> environment_;
        std::filesystem::path save_path_;
        std::filesystem::path user_maps_path_;
        std::filesystem::path pending_map_;
        std::shared_ptr<OpenDialogResult> open_dialog_;
        std::vector<std::filesystem::path> map_files_;
        std::vector<std::string> model_files_, texture_files_, validation_;
        std::string selected_model_ = "models/service_cylinder.obj", selected_texture_ = "models/service_cylinder.bmp";
        std::string next_map_ = "levels/foundation_pump.toml", next_entry_ = "arrival";
        std::vector<std::string> entry_names_{"arrival"};
        char map_name_[128] = "editor_level";
        bool pending_new_ = false;
        bool place_by_click_ = true;
        bool placing_ = false;
        std::size_t validated_revision_ = 0;
        std::string message_;
        std::size_t preview_revision_ = 0;
        float grid_ = 0.25F;
        float panel_width_ = 330.0F;
        int drag_axis_ = -1;
        glm::vec2 drag_mouse_{};
        glm::vec2 drag_screen_axis_{};
        glm::vec3 drag_center_{};
        glm::vec3 drag_size_{};
        bool active_ = false;
        bool has_document_ = false;
        bool flying_ = false;
        bool play_requested_ = false;
        bool exit_requested_ = false;
    };
} // namespace RelayGame

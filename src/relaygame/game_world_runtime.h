#pragma once

#include "relaycore/app/frame_timer.h"
#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/gameplay/mission_runtime.h"
#include "relaycore/physics/collision.h"
#include "relaycore/render/render_diagnostics.h"
#include "relaycore/world/blockout_level.h"
#include "relaygame/fixed_simulation_loop.h"
#include "relaygame/level_session_builder.h"
#include "relaygame/player_controller.h"

#include <SDL3/SDL.h>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace RelayCore
{
    class EngineServices;
    class Renderer;
} // namespace RelayCore

namespace RelayGame
{
    class GameWorldRuntime {
    public:
        void startup(RelayCore::EngineServices& services);

        void shutdown(RelayCore::EngineServices& services) noexcept;

        void on_event(const SDL_Event& event, RelayCore::EngineServices& services);

        void update(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services);

        void publish_diagnostics(RelayCore::Renderer& renderer) const;

        [[nodiscard]] auto level() const noexcept -> const RelayCore::BlockoutLevel&;

        [[nodiscard]] auto doors() const noexcept -> const std::vector<RelayCore::gameplay::RuntimeDoor>&;

        [[nodiscard]] auto triggers() const noexcept -> const std::vector<RelayCore::gameplay::RuntimeTrigger>&;

        [[nodiscard]] auto usables() const noexcept -> const std::vector<RelayCore::gameplay::RuntimeUsable>&;

        [[nodiscard]] auto focused_usable_index() const noexcept -> int;

    private:
        struct AudioCueSet {
            std::filesystem::path ambient_loop{};
            std::filesystem::path panel_press{};
            std::filesystem::path door_open{};
            std::filesystem::path door_close{};
            std::filesystem::path access_denied{};
        };

        struct InteractionState {
            int focused_usable_index = -1;
            float focused_distance = 0.0F;
            std::string last_target{};
            std::string use_binding_label{"E"};
            bool use_pressed = false;
            std::uint32_t fired_triggers = 0;
            std::uint32_t active_doors = 0;
        };

        struct TransientHudEvent {
            std::string text{};
            RelayCore::render::HudEventStyle style = RelayCore::render::HudEventStyle::Neutral;
            float remaining_seconds = 0.0F;
            float total_seconds = 0.0F;
        };

        void simulate_fixed_step(const RelayCore::FrameInfo& sim_frame, RelayCore::EngineServices& services);

        void refresh_runtime_usables_and_focus();

        void refresh_interaction_focus();

        void configure_audio_cues(const RelayCore::EngineServices& services);

        void process_use_input(bool use_pressed_this_step, RelayCore::EngineServices& services);

        void play_gameplay_audio(RelayCore::EngineServices& services, const std::filesystem::path& cue_path) const;

        void play_door_state_change_audio(RelayCore::EngineServices& services);

        void sync_door_audio_state();

        void tick_transient_hud_event(float delta_seconds) noexcept;

        void tick_usable_feedback(float delta_seconds) noexcept;

        void push_usable_feedback(
            RelayCore::gameplay::RuntimeUsable& usable,
            RelayCore::gameplay::UsableFeedbackStyle style,
            float duration_seconds
        ) noexcept;

        void push_transient_hud_event(std::string text, RelayCore::render::HudEventStyle style, float duration_seconds);

        void push_mission_status_event(
            const RelayCore::gameplay::MissionStatus& mission,
            RelayCore::render::HudEventStyle style,
            float duration_seconds
        );

        void record_activated_target(std::string_view target);

        [[nodiscard]] auto has_inventory_item(std::string_view item_id) const -> bool;

        void collect_inventory_item(const RelayCore::gameplay::RuntimeUsable& usable);

        [[nodiscard]] auto build_focused_usable_label(const RelayCore::gameplay::RuntimeUsable& usable) const
            -> std::string;

        [[nodiscard]] auto build_inventory_summary() const -> std::string;

        [[nodiscard]] auto build_player_diagnostics() const -> RelayCore::render::PlayerDiagnostics;

        [[nodiscard]] auto build_interaction_diagnostics() const -> RelayCore::render::InteractionDiagnostics;

        RelayGame::GroundedPlayerController player_controller_{};
        RelayCore::BlockoutLevel level_{};
        std::vector<RelayCore::gameplay::RuntimeDoor> doors_{};
        std::vector<RelayCore::gameplay::RuntimeTrigger> triggers_{};
        std::vector<RelayCore::gameplay::RuntimeUsable> usables_{};
        RelayCore::gameplay::InteractionSpatialIndex trigger_spatial_index_{};
        RelayCore::gameplay::InteractionSpatialIndex usable_spatial_index_{};
        std::vector<std::uint32_t> trigger_candidates_{};
        std::vector<std::uint32_t> usable_candidates_{};
        std::vector<RelayCore::physics::Aabb> dynamic_blockers_{};
        std::vector<std::string> inventory_item_ids_{};
        std::vector<std::string> activated_target_ids_{};
        InteractionState interaction_state_{};
        TransientHudEvent transient_hud_event_{};
        RelayCore::gameplay::MissionRuntime mission_runtime_{};
        AudioCueSet audio_cues_{};
        std::vector<RelayCore::gameplay::DoorState> previous_door_states_{};
        FixedSimulationLoop simulation_loop_{1.0 / 120.0, 8};
    };
} // namespace RelayGame

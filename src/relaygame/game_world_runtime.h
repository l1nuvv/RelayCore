#pragma once

#include "relaycore/app/frame_timer.h"
#include "relaycore/gameplay/interaction_runtime.h"
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

        void simulate_fixed_step(const RelayCore::FrameInfo& sim_frame, RelayCore::EngineServices& services);

        void refresh_runtime_usables_and_focus();

        void refresh_interaction_focus();

        void play_gameplay_audio(RelayCore::EngineServices& services, const std::filesystem::path& cue_path) const;

        void play_door_state_change_audio(RelayCore::EngineServices& services);

        void sync_door_audio_state();

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
        InteractionState interaction_state_{};
        AudioCueSet audio_cues_{};
        std::vector<RelayCore::gameplay::DoorState> previous_door_states_{};
        FixedSimulationLoop simulation_loop_{1.0 / 120.0, 8};
    };
} // namespace RelayGame

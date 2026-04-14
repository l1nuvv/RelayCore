#pragma once

#include "relaycore/app/frame_timer.h"
#include "relaycore/gameplay/combat_runtime.h"
#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/gameplay/mission_runtime.h"
#include "relaycore/gameplay/runtime_model.h"
#include "relaycore/physics/collision.h"
#include "relaycore/render/render_diagnostics.h"
#include "relaycore/world/blockout_level.h"
#include "relaygame/fixed_simulation_loop.h"
#include "relaygame/level_session_builder.h"
#include "relaygame/player_controller.h"
#include "relaygame/world_audio_zones.h"

#include <SDL3/SDL.h>
#include <cstdint>
#include <filesystem>
#include <optional>
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

        void load_level_session(const RelayCore::core::EngineConfig& config, LevelSession session);

        void activate_level_session(RelayCore::EngineServices& services);

        void shutdown(RelayCore::EngineServices& services) noexcept;

        void on_event(const SDL_Event& event, RelayCore::EngineServices& services);

        void update(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services);

        void publish_diagnostics(RelayCore::Renderer& renderer) const;

        [[nodiscard]] auto level() const noexcept -> const RelayCore::BlockoutLevel&;

        [[nodiscard]] auto doors() const noexcept -> const std::vector<RelayCore::gameplay::RuntimeDoor>&;

        [[nodiscard]] auto triggers() const noexcept -> const std::vector<RelayCore::gameplay::RuntimeTrigger>&;

        [[nodiscard]] auto usables() const noexcept -> const std::vector<RelayCore::gameplay::RuntimeUsable>&;

        [[nodiscard]] auto turrets() const noexcept -> const std::vector<RelayCore::gameplay::RuntimeTurret>&;

        [[nodiscard]] auto focused_usable_index() const noexcept -> int;

        [[nodiscard]] auto consume_reload_level_requested() noexcept -> bool;

        [[nodiscard]] auto consume_open_inspector_source_requested() noexcept -> bool;

        struct InspectorRestoreState {
            RelayCore::gameplay::RuntimeObjectId runtime_object_id = 0;
            std::string primary_source_reference{};
        };

        [[nodiscard]] auto capture_pinned_inspector_restore_state() const -> std::optional<InspectorRestoreState>;

        void restore_pinned_inspector(const InspectorRestoreState& state);

        [[nodiscard]] auto capture_runtime_snapshot() const -> RelayCore::gameplay::RuntimeLevelSnapshot;

        [[nodiscard]] auto restore_runtime_snapshot(const RelayCore::gameplay::RuntimeLevelSnapshot& snapshot)
            -> RelayCore::gameplay::SnapshotRestoreResult;

        [[nodiscard]] auto runtime_registry() const noexcept -> const RelayCore::gameplay::RuntimeObjectRegistry&;

        [[nodiscard]] auto runtime_registry_diagnostics() const
            -> std::vector<RelayCore::render::RuntimeRegistryEntryDiagnostics>;

        [[nodiscard]] auto runtime_binding_diagnostics() const
            -> std::vector<RelayCore::render::RuntimeBindingDiagnostics>;

        [[nodiscard]] auto level_path() const noexcept -> const std::string&;

        [[nodiscard]] auto simulation_tick() const noexcept -> std::uint64_t;

        [[nodiscard]] auto consume_save_runtime_snapshot_requested() noexcept -> bool;

        [[nodiscard]] auto consume_restore_runtime_snapshot_requested() noexcept -> bool;

        [[nodiscard]] auto authoring_warnings() const noexcept
            -> const std::vector<RelayCore::render::AuthoringWarningDiagnostics>&;

    private:
        struct AudioCueSet {
            std::filesystem::path ambient_loop{};
            std::filesystem::path panel_press{};
            std::filesystem::path pickup{};
            std::filesystem::path door_open{};
            std::filesystem::path door_close{};
            std::filesystem::path access_denied{};
            std::filesystem::path mission_complete{};
            std::filesystem::path weapon_fire{};
            std::filesystem::path turret_fire{};
            std::filesystem::path turret_destroyed{};
            std::filesystem::path player_hurt{};
            std::filesystem::path player_respawn{};
        };

        struct AudioState {
            bool initialized = false;
            bool has_active_loop = false;
            int active_zone_index = -1;
            float master_volume = 0.0F;
            float active_loop_volume = 0.0F;
            std::uint32_t ambient_switch_count = 0;
            std::uint32_t one_shot_count = 0;
            std::string active_source_kind{};
            std::string active_zone_name{};
            std::filesystem::path active_loop_path{};
            std::filesystem::path last_one_shot_path{};
        };

        struct RuntimeState {
            std::string level_name{};
            std::string level_path{};
            bool stress_level = false;
            std::uint32_t fixed_steps_last_frame = 0;
            std::uint32_t active_trigger_overlaps = 0;
        };

        struct InteractionState {
            int focused_usable_index = -1;
            float focused_distance = 0.0F;
            RelayCore::gameplay::InteractionEvent last_event{};
            std::string use_binding_label{"E"};
            bool use_pressed = false;
            std::uint32_t fired_triggers = 0;
            std::uint32_t active_doors = 0;
        };

        struct CombatState {
            glm::vec3 spawn_position{0.0F};
            std::optional<glm::vec2> spawn_view_angles{};
            RelayCore::gameplay::PlayerCombatState player{};
            std::vector<RelayCore::gameplay::RuntimeTurret> turrets{};
            std::vector<RelayCore::gameplay::RuntimeHazardVolume> hazards{};
            std::string fire_binding_label{"Mouse1"};
            bool fire_pressed = false;
            std::uint32_t active_hazard_contacts = 0;
            std::uint32_t turret_shots_last_frame = 0;
        };

        struct TransientHudEvent {
            std::string text{};
            RelayCore::render::HudEventStyle style = RelayCore::render::HudEventStyle::Neutral;
            float remaining_seconds = 0.0F;
            float total_seconds = 0.0F;
        };

        struct InspectorState {
            bool pinned = false;
            RelayCore::render::InspectorDiagnostics pinned_diagnostics{};
        };

        void simulate_fixed_step(const RelayCore::FrameInfo& sim_frame, RelayCore::EngineServices& services);

        void refresh_runtime_usables_and_focus();

        void refresh_interaction_focus();

        void configure_audio_cues(const RelayCore::core::GameplayAudioConfig& gameplay_audio);

        void process_use_input(bool use_pressed_this_step, RelayCore::EngineServices& services);

        void process_fire_input(bool fire_pressed_this_step, RelayCore::EngineServices& services);

        void apply_combat_target_event(const RelayCore::gameplay::CombatTargetEvent& event);

        void play_gameplay_audio(RelayCore::EngineServices& services, const std::filesystem::path& cue_path);

        void play_door_state_change_audio(RelayCore::EngineServices& services);

        void sync_door_audio_state();

        void update_ambient_audio(RelayCore::EngineServices& services);

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

        void respawn_player_at_spawn() noexcept;

        void remember_interaction_event(const RelayCore::gameplay::InteractionEvent& event) noexcept;

        [[nodiscard]] auto has_inventory_item(std::string_view item_id) const -> bool;

        [[nodiscard]] auto build_focused_usable_label(const RelayCore::gameplay::RuntimeUsable& usable) const
            -> std::string;

        [[nodiscard]] auto build_focused_usable_detail(const RelayCore::gameplay::RuntimeUsable& usable) const
            -> std::string;

        [[nodiscard]] auto interaction_query_distance() const noexcept -> float;

        [[nodiscard]] auto build_inventory_summary() const -> std::string;

        [[nodiscard]] auto build_player_diagnostics() const -> RelayCore::render::PlayerDiagnostics;

        [[nodiscard]] auto build_runtime_diagnostics() const -> RelayCore::render::RuntimeDiagnostics;

        [[nodiscard]] auto build_interaction_diagnostics() const -> RelayCore::render::InteractionDiagnostics;

        [[nodiscard]] auto build_live_inspector_diagnostics() const -> RelayCore::render::InspectorDiagnostics;

        [[nodiscard]] auto build_inspector_diagnostics() const -> RelayCore::render::InspectorDiagnostics;

        [[nodiscard]] auto build_audio_diagnostics() const -> RelayCore::render::AudioDiagnostics;

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
        std::vector<std::string> signaled_target_ids_{};
        RelayCore::gameplay::RuntimeObjectRegistry runtime_registry_{};
        InteractionState interaction_state_{};
        InspectorState inspector_state_{};
        CombatState combat_state_{};
        TransientHudEvent transient_hud_event_{};
        RelayCore::gameplay::MissionRuntime mission_runtime_{};
        AudioCueSet audio_cues_{};
        AudioState audio_state_{};
        RuntimeState runtime_state_{};
        std::vector<RelayCore::render::AuthoringWarningDiagnostics> authoring_warnings_{};
        std::vector<RelayGame::RuntimeAudioZone> audio_zones_{};
        std::vector<RelayCore::gameplay::DoorState> previous_door_states_{};
        FixedSimulationLoop simulation_loop_{1.0 / 120.0, 8};
    };
} // namespace RelayGame

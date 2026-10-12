#pragma once
#include "relaycore/app/frame_timer.h"
#include "relaycore/gameplay/combat_runtime.h"
#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/physics/collision.h"
#include "relaycore/render/player_ui_data.h"
#include "relaycore/render/render_diagnostics.h"
#include "relaycore/world/blockout_level.h"
#include "relaygame/fixed_simulation_loop.h"
#include "relaygame/gameplay_input_ownership.h"
#include "relaygame/level_session_builder.h"
#include "relaygame/mission_runtime.h"
#include "relaygame/player_controller.h"
#include "relaygame/player_feedback.h"
#include "relaygame/runtime_model.h"
#include "relaygame/save_game.h"
#include "relaygame/scientist_pose.h"
#include "relaygame/world_audio_zones.h"

#include <SDL3/SDL.h>
#include <array>
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
    struct EncounterSnapshot {
        SavedGame game;
        float scene_time = 0;
        std::vector<EnvironmentEffect> environment;
    };

    class GameWorldRuntime {
    public:
        [[nodiscard]] auto capture_saved_game(float played_seconds, bool flashlight) const -> SavedGame;
        void restore_saved_game(const SavedGame& save);
        [[nodiscard]] auto capture_encounter_snapshot(float played_seconds, float scene_time, bool flashlight) const
            -> EncounterSnapshot;
        void restore_encounter_snapshot(const EncounterSnapshot& snapshot);
        [[nodiscard]] auto active_level_exit() const -> const LevelExit*;
        [[nodiscard]] auto has_completion_exit() const noexcept -> bool
        {
            for (const auto& exit : exits_)
                if (exit.requires_completion)
                    return true;
            return false;
        }
        [[nodiscard]] auto ending() const noexcept -> const std::optional<EpisodeEnding>&
        {
            return ending_;
        }
        void begin_ending();
        void update_ending(float delta_seconds, RelayCore::EngineServices& services);
        void load_level_session(const RelayCore::core::EngineConfig& config, LevelSession session);
        [[nodiscard]] auto
        query_physics_fixture(const glm::vec3& origin, const glm::vec3& direction, float max_distance) const
            -> std::optional<RelayCore::physics::WorldQueryHit>;

        void prepare_level_session_activation(
            RelayCore::EngineServices& services,
            bool allow_capture = true,
            const RelayCore::render::PerspectiveCameraConfig* projection = nullptr
        );
        void activate_level_session(RelayCore::EngineServices& services);
        void set_paused(bool paused, RelayCore::EngineServices& services);
        [[nodiscard]] auto paused() const noexcept -> bool
        {
            return paused_;
        }
        void apply_player_settings(float sensitivity, float camera_motion) noexcept;

        void shutdown(RelayCore::EngineServices& services) noexcept;
        void retire_level_session(RelayCore::EngineServices& services) noexcept;

        void on_event(const SDL_Event& event, RelayCore::EngineServices& services);

        void update(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services);

        void publish_diagnostics(RelayCore::Renderer& renderer) const;
        [[nodiscard]] auto build_hud_presentation(const RelayCore::render::PerspectiveCamera* camera = nullptr)
            -> RelayCore::render::HudPresentation;

        [[nodiscard]] auto level() const noexcept -> const RelayCore::BlockoutLevel&;

        [[nodiscard]] auto doors() const noexcept -> const std::vector<RelayCore::gameplay::RuntimeDoor>&;

        [[nodiscard]] auto triggers() const noexcept -> const std::vector<RelayCore::gameplay::RuntimeTrigger>&;

        [[nodiscard]] auto usables() const noexcept -> const std::vector<RelayCore::gameplay::RuntimeUsable>&;

        [[nodiscard]] auto enemies() const noexcept -> const std::vector<RuntimeEnemy>&
        {
            return enemies_;
        }
        [[nodiscard]] auto containment() const noexcept -> const std::optional<ContainmentEncounter>&
        {
            return containment_;
        }
        void debug_update_containment(float dt);
        [[nodiscard]] auto turrets() const noexcept -> const std::vector<RelayCore::gameplay::RuntimeTurret>&;

        [[nodiscard]] auto focused_usable_index() const noexcept -> int;

        struct IterationCommandRequests {
            bool reload_level = false;
            bool open_inspector_source = false;
            bool save_runtime_snapshot = false;
            bool restore_runtime_snapshot = false;
        };

        [[nodiscard]] auto consume_iteration_command_requests() noexcept -> IterationCommandRequests;

        struct InspectorRestoreState {
            RelayGame::RuntimeObjectId runtime_object_id = 0;
            std::string primary_source_reference{};
        };

        [[nodiscard]] auto capture_pinned_inspector_restore_state() const -> std::optional<InspectorRestoreState>;

        void restore_pinned_inspector(const InspectorRestoreState& state);

        [[nodiscard]] auto capture_runtime_snapshot() const -> RelayGame::RuntimeLevelSnapshot;

        [[nodiscard]] auto restore_runtime_snapshot(const RelayGame::RuntimeLevelSnapshot& snapshot)
            -> RelayGame::SnapshotRestoreResult;

        [[nodiscard]] auto runtime_registry() const noexcept -> const RelayGame::RuntimeObjectRegistry&;

        [[nodiscard]] auto runtime_registry_diagnostics() const
            -> std::vector<RelayCore::render::RuntimeRegistryEntryDiagnostics>;

        [[nodiscard]] auto runtime_binding_diagnostics() const
            -> std::vector<RelayCore::render::RuntimeBindingDiagnostics>;

        [[nodiscard]] auto level_path() const noexcept -> const std::string&;

        [[nodiscard]] auto simulation_tick() const noexcept -> std::uint64_t;

        [[nodiscard]] auto authoring_warnings() const noexcept
            -> const std::vector<RelayCore::render::AuthoringWarningDiagnostics>&;

        [[nodiscard]] auto mission_status() const -> RelayGame::MissionStatus;

        [[nodiscard]] auto player_combat_state() const noexcept -> const RelayCore::gameplay::PlayerCombatState&;
        [[nodiscard]] auto player_pellet_traces() const noexcept
            -> std::span<const RelayCore::gameplay::CombatShotTrace>
        {
            return combat_state_.pellet_traces;
        }

        [[nodiscard]] auto selected_weapon() const noexcept -> PlayerWeapon
        {
            return selected_weapon_;
        }
        [[nodiscard]] auto bullet_impact_marks() const noexcept -> std::span<const BulletImpactMark>
        {
            return bullet_impact_marks_;
        }
        auto select_weapon(PlayerWeapon weapon) noexcept -> bool;
        [[nodiscard]] auto shotgun_cycle_remaining_seconds() const noexcept -> float
        {
            return combat_state_.shotgun_cycle_remaining_seconds;
        }
        [[nodiscard]] auto pistol_cycle_remaining_seconds() const noexcept -> float
        {
            return combat_state_.pistol_cycle_remaining_seconds;
        }

        [[nodiscard]] auto scientist_pose() const noexcept -> ScientistPose;

        [[nodiscard]] auto inventory_item_ids() const noexcept -> const std::vector<std::string>&;

        [[nodiscard]] auto activated_target_ids() const noexcept -> const std::vector<std::string>&;
        [[nodiscard]] auto environment() const noexcept -> std::span<const EnvironmentEffect>
        {
            return environment_;
        }

        [[nodiscard]] auto signaled_target_ids() const noexcept -> const std::vector<std::string>&;

        [[nodiscard]] auto debug_apply_usable_interaction(std::size_t usable_index)
            -> RelayCore::gameplay::InteractionEvent;

        [[nodiscard]] auto debug_apply_player_fire(const glm::vec3& origin, const glm::vec3& direction)
            -> RelayCore::gameplay::CombatFireResult;

        [[nodiscard]] auto debug_player_position() const noexcept -> glm::vec3;

        [[nodiscard]] auto debug_player_eye_position() const noexcept -> glm::vec3;

        void debug_set_player_position(const glm::vec3& position) noexcept;

        [[nodiscard]] auto debug_apply_combat_threats(
            const RelayCore::physics::Aabb& player_bounds,
            const glm::vec3& player_eye_position,
            float dt
        ) -> RelayCore::gameplay::CombatThreatResult;

        [[nodiscard]] auto debug_advance_player_respawn(float dt) noexcept -> bool;

        void debug_tick_combat(float dt) noexcept;

        void debug_update_mission(const RelayCore::physics::Aabb& actor_bounds);

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
            std::filesystem::path shotgun_fire{};
            std::filesystem::path weapon_hit{};
            std::filesystem::path impact_hard{};
            std::filesystem::path impact_metal{};
            std::filesystem::path threat_windup{};
            std::filesystem::path rover_attack{};
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
            std::array<RelayCore::gameplay::CombatShotTrace, 7> pellet_traces{};
            float shotgun_cycle_remaining_seconds = 0;
            float pistol_cycle_remaining_seconds = 0;
            std::vector<RelayCore::gameplay::RuntimeTurret> turrets{};
            std::vector<RelayCore::gameplay::RuntimeHazardVolume> hazards{};
            std::string fire_binding_label{"Mouse1"};
            bool fire_pressed = false;
            bool hazard_contact_last_step = false;
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

        struct FrameGameplayActions {
            bool use_pressed = false;
            bool fire_pressed = false;
            bool inspector_pin_pressed = false;
            std::optional<PlayerWeapon> weapon_selection;
        };

        void simulate_fixed_step(
            const RelayCore::FrameInfo& sim_frame,
            FrameGameplayActions& actions,
            RelayCore::EngineServices& services
        );

        void refresh_runtime_usables_and_focus();

        void refresh_interaction_focus();

        void configure_audio_cues(const RelayCore::core::GameplayAudioConfig& gameplay_audio);

        void update_containment(float dt);
        auto apply_runtime_usable_interaction(RelayCore::gameplay::RuntimeUsable& usable)
            -> RelayCore::gameplay::InteractionEvent;
        void process_use_input(bool use_pressed_this_step, RelayCore::EngineServices& services);

        void process_fire_input(bool fire_pressed_this_step, RelayCore::EngineServices& services);
        void tick_combat_state(float dt) noexcept;

        void apply_combat_target_event(const RelayCore::gameplay::CombatTargetEvent& event);

        void play_gameplay_audio(
            RelayCore::EngineServices& services,
            const std::filesystem::path& cue_path,
            std::optional<glm::vec3> position = std::nullopt
        );

        void play_door_state_change_audio(RelayCore::EngineServices& services);

        void sync_door_audio_state();

        void update_ambient_audio(RelayCore::EngineServices& services);

        auto apply_player_attack(const glm::vec3& origin, const glm::vec3& direction) -> PlayerAttackResult;

        void tick_transient_hud_event(float delta_seconds) noexcept;

        void tick_usable_feedback(float delta_seconds) noexcept;

        void push_usable_feedback(
            RelayCore::gameplay::RuntimeUsable& usable,
            RelayCore::gameplay::UsableFeedbackStyle style,
            float duration_seconds
        ) noexcept;

        void push_transient_hud_event(std::string text, RelayCore::render::HudEventStyle style, float duration_seconds);

        void push_mission_status_event(
            const RelayGame::MissionStatus& mission,
            RelayCore::render::HudEventStyle style,
            float duration_seconds
        );

        [[nodiscard]] auto advance_dead_player_state(float sim_delta_seconds) noexcept -> bool;

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

        void import_level_session(LevelSession session);

        void reset_runtime_for_loaded_level(
            const RelayCore::core::EngineConfig& config,
            const glm::vec3& spawn_position,
            const std::optional<glm::vec2>& spawn_view_angles
        );

        void refresh_audio_runtime_state(RelayCore::EngineServices& services);

        void clear_loaded_level_state() noexcept;

        void begin_frame_update(float delta_seconds);

        void sync_imgui_capture(RelayCore::EngineServices& services) noexcept;

        void clear_pending_gameplay_actions() noexcept;

        [[nodiscard]] auto capture_frame_gameplay_actions() noexcept -> FrameGameplayActions;

        [[nodiscard]] auto capture_frame_iteration_commands() noexcept -> IterationCommandRequests;

        void handle_inspector_pin_input();

        void update_trigger_overlap_count();

        void refresh_active_door_count() noexcept;

        void update_door_runtime(float sim_delta_seconds) noexcept;

        void finalize_frame_update(RelayCore::EngineServices& services, float delta_seconds);
        void update_presentation(RelayCore::EngineServices& services, float delta_seconds);
        std::optional<EpisodeEnding> ending_;

        RelayGame::GroundedPlayerController player_controller_{};
        PlayerFeedback player_feedback_{};
        PlayerWeapon selected_weapon_ = PlayerWeapon::Precise;
        [[nodiscard]] auto world_query(bool include_guardian = true) -> RelayCore::physics::WorldQuery;
        std::vector<RelayCore::physics::WorldCollider> world_colliders_;
        std::shared_ptr<const RelayCore::physics::PhysicsWorld> physics_fixture_;
        std::vector<RelayCore::physics::WorldCollider> query_doors_;
        float camera_motion_ = 0.6F;
        bool paused_ = false;
        std::array<std::filesystem::path, 16> player_sounds_{};
        std::size_t step_variant_ = 0;
        RelayCore::BlockoutLevel level_{};
        std::vector<LevelExit> exits_;
        std::vector<RelayCore::gameplay::RuntimeDoor> doors_{};
        std::vector<RelayCore::gameplay::RuntimeTrigger> triggers_{};
        std::vector<RelayCore::gameplay::RuntimeUsable> usables_{};
        RelayCore::gameplay::InteractionSpatialIndex trigger_spatial_index_{};
        RelayCore::gameplay::InteractionSpatialIndex usable_spatial_index_{};
        std::vector<std::uint32_t> trigger_candidates_{};
        std::vector<std::uint32_t> usable_candidates_{};
        std::vector<RelayCore::physics::WorldCollider> dynamic_blockers_{};
        std::vector<std::string> inventory_item_ids_{};
        std::vector<std::string> activated_target_ids_{};
        std::vector<std::string> signaled_target_ids_{};
        RelayGame::RuntimeObjectRegistry runtime_registry_{};
        InteractionState interaction_state_{};
        InspectorState inspector_state_{};
        CombatState combat_state_{};
        TransientHudEvent transient_hud_event_{};
        struct DamageNumber {
            glm::vec3 position{};
            float amount = 0, remaining_seconds = 0;
        };
        std::vector<DamageNumber> damage_numbers_;
        std::vector<BulletImpactMark> bullet_impact_marks_;
        RelayGame::MissionRuntime mission_runtime_{};
        AudioCueSet audio_cues_{};
        AudioState audio_state_{};
        bool spatial_audio_ready_ = false;
        RuntimeState runtime_state_{};
        std::vector<RelayCore::render::AuthoringWarningDiagnostics> authoring_warnings_{};
        std::vector<RelayGame::RuntimeAudioZone> audio_zones_{};
        std::vector<EnvironmentEffect> environment_{};
        RelayGame::MissionDefinition loaded_mission_definition_{};
        std::vector<MissionRetrySpawn> retry_spawns_{};
        std::vector<RelayCore::gameplay::RuntimeTurret> loaded_level_turrets_{};
        std::optional<ContainmentEncounter> containment_;
        std::array<RelayCore::physics::WorldCollider, 1> guardian_collider_;
        std::vector<RuntimeEnemy> enemies_;
        std::vector<RelayCore::physics::WorldCollider> enemy_blockers_;
        std::vector<RelayCore::gameplay::RuntimeHazardVolume> loaded_level_hazards_{};
        std::vector<RelayCore::gameplay::DoorState> previous_door_states_{};
        FrameGameplayActions frame_actions_{};
        IterationCommandRequests frame_iteration_commands_{};
        GameplayInputOwnership input_ownership_{};
        bool window_has_focus_ = true;
        bool window_minimized_ = false;
        FixedSimulationLoop simulation_loop_{1.0 / 120.0, 8};
    };
} // namespace RelayGame

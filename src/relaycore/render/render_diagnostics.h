#pragma once

#include <cstdint>
#include <glm/vec3.hpp>
#include <string>
#include <vector>

namespace RelayCore::render
{
    enum class DiagnosticsSourceFilter : std::uint8_t {
        All = 0,
        Direct,
        Instanced,
        Generated,
    };

    enum class HudEventStyle : std::uint8_t {
        Neutral = 0,
        Success,
        Warning,
    };

    struct WorldDiagnostics {
        std::uint32_t total_solids = 0;
        std::uint32_t visible_solids = 0;
        std::uint32_t total_chunks = 0;
        std::uint32_t opaque_total_chunks = 0;
        std::uint32_t transparent_total_chunks = 0;
        std::uint32_t visible_chunks = 0;
        std::uint32_t culled_chunks = 0;
        std::uint32_t opaque_visible_chunks = 0;
        std::uint32_t transparent_visible_chunks = 0;
        std::uint32_t draw_calls = 0;
        std::uint32_t opaque_draw_calls = 0;
        std::uint32_t overlay_draw_calls = 0;
        std::uint32_t transparent_draw_calls = 0;
        std::uint32_t door_draw_calls = 0;
        std::uint32_t usable_draw_calls = 0;
        std::uint32_t highlight_draw_calls = 0;
        std::uint32_t feedback_draw_calls = 0;
        std::uint32_t inspector_draw_calls = 0;
        std::uint32_t inspector_marker_draw_calls = 0;
        std::uint32_t triangles = 0;
        std::uint32_t opaque_triangles = 0;
        std::uint32_t overlay_triangles = 0;
        std::uint32_t transparent_triangles = 0;
        std::uint32_t total_render_lights = 0;
        std::uint32_t local_light_count = 0;
        std::uint32_t active_local_light_budget = 0;
        std::uint32_t shadow_cascade_count = 0;
        bool shadow_enabled = false;
        float indirect_bounce_strength = 0.0F;
        std::uint32_t shadow_map_resolution = 0;
        std::uintptr_t shadow_preview_texture_id = 0;
        std::uintptr_t shadow_far_preview_texture_id = 0;
        float shadow_contact_strength = 0.0F;
        float shadow_bias_near = 0.0F;
        float shadow_bias_far = 0.0F;
        float shadow_normal_offset_near = 0.0F;
        float shadow_normal_offset_far = 0.0F;
        float shadow_pcf_radius_near = 0.0F;
        float shadow_pcf_radius_far = 0.0F;
        float shadow_cascade_split_distance = 0.0F;
        float shadow_cascade_blend_range = 0.0F;
        float shadow_projection_radius = 0.0F;
        float shadow_far_projection_radius = 0.0F;
        float shadow_focus_distance = 0.0F;
        float shadow_far_focus_distance = 0.0F;
        float shadow_texel_world_size = 0.0F;
        float shadow_far_texel_world_size = 0.0F;
    };

    struct RuntimeDiagnostics {
        bool has_level = false;
        bool stress_level = false;
        std::uint32_t level_entity_count = 0;
        std::uint32_t fixed_steps = 0;
        std::uint32_t fixed_step_budget = 0;
        std::uint32_t active_trigger_overlaps = 0;
        std::uint64_t simulation_tick = 0;
        float fixed_step_ms = 0.0F;
        float simulation_backlog_ms = 0.0F;
        std::string level_name{};
        std::string level_path{};
    };

    struct PlayerDiagnostics {
        glm::vec3 position{};
        glm::vec3 velocity{};
        bool grounded = false;
        bool alive = true;
        bool collision_enabled = true;
        bool fire_pressed = false;
        std::uint32_t movement_mode = 0;
        std::uint32_t shots_fired = 0;
        std::uint32_t shots_hit = 0;
        std::uint32_t kills = 0;
        std::uint32_t deaths = 0;
        std::uint32_t total_turrets = 0;
        std::uint32_t destroyed_turrets = 0;
        std::uint32_t active_hazard_contacts = 0;
        std::uint32_t turret_shots_last_frame = 0;
        float speed = 0.0F;
        float health = 0.0F;
        float max_health = 0.0F;
        float weapon_cooldown_remaining_seconds = 0.0F;
        float hurt_flash_strength = 0.0F;
        float hit_confirmation_strength = 0.0F;
        float respawn_remaining_seconds = 0.0F;
        float damage_dealt = 0.0F;
        float damage_taken = 0.0F;
        std::uint32_t static_collision_solids = 0;
        std::uint32_t static_collision_candidates = 0;
        std::uint32_t dynamic_blockers = 0;
        std::uint32_t collision_world_solids = 0;
        std::string primary_attack_label{};
        std::string last_damage_source{};
        bool ground_probe_hit = false;
        bool ground_snap_applied = false;
        bool attempted_step_move = false;
        bool used_step_move = false;
        bool hit_x = false;
        bool hit_y = false;
        bool hit_z = false;
    };

    struct InteractionDiagnostics {
        bool has_focused_usable = false;
        bool has_objective = false;
        bool has_inventory = false;
        bool mission_complete = false;
        float focused_distance = 0.0F;
        std::uint32_t active_doors = 0;
        std::uint32_t fired_triggers = 0;
        std::uint32_t total_doors = 0;
        std::uint32_t total_triggers = 0;
        std::uint32_t total_usables = 0;
        std::uint32_t enabled_usables = 0;
        std::uint32_t blocked_usables = 0;
        std::uint32_t consumed_usables = 0;
        std::uint32_t activated_target_count = 0;
        std::uint32_t signaled_target_count = 0;
        std::uint32_t trigger_candidate_count = 0;
        std::uint32_t usable_candidate_count = 0;
        std::uint32_t objective_current = 0;
        std::uint32_t objective_total = 0;
        std::string focused_label{};
        std::string focused_detail{};
        std::string focused_name{};
        std::string focused_target{};
        std::string focused_chain_target{};
        std::string focused_required_item{};
        std::string focused_item_id{};
        std::string focused_action_kind{};
        std::string focused_signal_action{};
        std::string focused_visual_kind{};
        std::string focused_panel_mount_axis{};
        std::string focused_target_door_state{};
        std::string last_event_kind{};
        std::string last_event_source{};
        std::string last_event_target{};
        std::string last_event_chain_target{};
        std::string last_event_item{};
        std::string use_binding_label{};
        std::string objective_title{};
        std::string objective_detail{};
        std::string inventory_summary{};
        std::string transient_event_text{};
        glm::vec3 focused_origin{};
        float focused_use_radius = 0.0F;
        float focused_target_door_open_fraction = 0.0F;
        bool focused_enabled = false;
        bool focused_locked = false;
        bool focused_requirement_met = false;
        bool focused_single_use = false;
        bool focused_consumed = false;
        bool focused_tracks_target_door = false;
        bool focused_has_target_door = false;
        bool use_pressed = false;
        bool has_transient_event = false;
        float transient_event_strength = 0.0F;
        HudEventStyle transient_event_style = HudEventStyle::Neutral;
    };

    struct InspectorDiagnostics {
        bool has_pick = false;
        bool pinned = false;
        bool has_origin = false;
        bool has_authored_source = false;
        bool has_authored_location = false;
        bool has_authored_placement_location = false;
        bool has_runtime_handle = false;
        bool enabled = false;
        bool locked = false;
        bool requirement_met = false;
        bool single_use = false;
        bool consumed = false;
        bool tracks_target_door = false;
        bool blocks_when_closed = false;
        bool once = false;
        bool fired = false;
        float distance = 0.0F;
        float use_radius = 0.0F;
        float brightness = 0.0F;
        float door_open_fraction = 0.0F;
        std::uint64_t runtime_id = 0;
        int authored_index = -1;
        int authored_line = 0;
        int authored_column = 0;
        glm::vec3 hit_point{};
        glm::vec3 origin{};
        glm::vec3 bounds_mins{};
        glm::vec3 bounds_maxs{};
        std::string object_kind{};
        std::string name{};
        std::string label{};
        std::string material{};
        std::string target{};
        std::string chain_target{};
        std::string item_id{};
        std::string required_item{};
        std::string action_kind{};
        std::string signal_action{};
        std::string state{};
        std::string visual_kind{};
        std::string panel_mount_axis{};
        std::string runtime_handle{};
        std::string authored_source_kind{};
        std::string authored_file_path{};
        std::string authored_placement_kind{};
        std::string authored_placement_file_path{};
        std::string authored_classname{};
        std::string authored_entity_kind{};
        std::string authored_targetname{};
        std::string authored_summary{};
        std::string authored_template_name{};
        std::string authored_instance_name{};
        int authored_placement_line = 0;
        int authored_placement_column = 0;
    };

    struct RuntimeRegistryEntryDiagnostics {
        std::uint64_t runtime_id = 0;
        std::uint32_t storage_index = 0;
        std::string runtime_handle{};
        std::string object_kind{};
        std::string source_bucket{};
        std::string name{};
        std::string label{};
        std::string target{};
        std::string source_reference{};
        std::string source_file_path{};
        int source_line = 0;
        int source_column = 0;
    };

    struct RuntimeBindingDiagnostics {
        std::string source_handle{};
        std::string source_kind{};
        std::string source_name{};
        std::string source_bucket{};
        std::string binding_kind{};
        std::string target{};
        std::string source_reference{};
        std::string source_file_path{};
        int source_line = 0;
        int source_column = 0;
        bool expects_resolved_target = false;
        bool unresolved_target = false;
        bool ambiguous_target = false;
        std::uint32_t resolved_target_count = 0;
        std::string resolved_targets_summary{};
    };

    struct AudioDiagnostics {
        bool initialized = false;
        bool has_active_loop = false;
        bool has_active_zone = false;
        float master_volume = 0.0F;
        float active_loop_volume = 0.0F;
        std::uint32_t ambient_zone_count = 0;
        std::uint32_t ambient_switch_count = 0;
        std::uint32_t one_shot_count = 0;
        int active_zone_index = -1;
        int active_zone_priority = 0;
        float active_zone_gain = 0.0F;
        std::string active_source_kind{};
        std::string active_zone_name{};
        std::string active_loop_path{};
        std::string last_one_shot_path{};
    };

    struct AuthoringWarningDiagnostics {
        std::string message{};
        std::string source_bucket{};
        std::string primary_label{};
        std::string primary_file_path{};
        int primary_line = 0;
        int primary_column = 0;
        std::string secondary_label{};
        std::string secondary_file_path{};
        int secondary_line = 0;
        int secondary_column = 0;
    };

    struct IterationDiagnostics {
        bool has_last_reload_result = false;
        bool last_reload_succeeded = false;
        bool has_last_command_result = false;
        bool last_command_succeeded = false;
        bool has_saved_runtime_snapshot = false;
        bool has_last_snapshot_result = false;
        bool last_snapshot_succeeded = false;
        std::string last_reload_summary{};
        std::string last_reload_error{};
        std::string last_command_summary{};
        std::string last_command_error{};
        std::string saved_snapshot_summary{};
        std::string last_snapshot_summary{};
        std::string last_snapshot_error{};
        std::vector<AuthoringWarningDiagnostics> authoring_warnings{};
        std::vector<RuntimeRegistryEntryDiagnostics> runtime_registry{};
        std::vector<RuntimeBindingDiagnostics> runtime_bindings{};
    };
} // namespace RelayCore::render

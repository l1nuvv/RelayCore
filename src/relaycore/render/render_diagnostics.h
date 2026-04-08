#pragma once

#include <cstdint>
#include <glm/vec3.hpp>
#include <string>

namespace RelayCore::render
{
    enum class HudEventStyle : std::uint8_t {
        Neutral = 0,
        Success,
        Warning,
    };

    struct WorldDiagnostics {
        std::uint32_t total_solids = 0;
        std::uint32_t visible_solids = 0;
        std::uint32_t draw_calls = 0;
        std::uint32_t triangles = 0;
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

    struct PlayerDiagnostics {
        glm::vec3 position{};
        glm::vec3 velocity{};
        bool grounded = false;
        bool collision_enabled = true;
        std::uint32_t movement_mode = 0;
        float speed = 0.0F;
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
        std::uint32_t objective_current = 0;
        std::uint32_t objective_total = 0;
        std::string focused_label{};
        std::string last_target{};
        std::string use_binding_label{};
        std::string objective_title{};
        std::string objective_detail{};
        std::string inventory_summary{};
        std::string transient_event_text{};
        bool use_pressed = false;
        bool has_transient_event = false;
        float transient_event_strength = 0.0F;
        HudEventStyle transient_event_style = HudEventStyle::Neutral;
    };
} // namespace RelayCore::render

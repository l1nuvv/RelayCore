#include "relaycore/render/camera.h"
#include "relaycore/render/player_ui_data.h"
#include "relaygame/game_world_runtime.h"
#include "relaygame/usable_interaction.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
namespace RelayGame
{
    auto GameWorldRuntime::build_hud_presentation(const RelayCore::render::PerspectiveCamera* camera)
        -> RelayCore::render::HudPresentation
    {
        using RelayCore::render::HudPresentation;
        const auto& player = combat_state_.player;
        HudPresentation hud;
        hud.visible = player.max_health > 0;
        hud.alive = player.alive;
        hud.health = player.health;
        hud.max_health = player.max_health;
        hud.hurt = std::clamp(player.hurt_flash_remaining_seconds / 0.30F, 0.0F, 1.0F);
        hud.damage_alert = std::clamp(player.damage_direction_remaining_seconds / 0.30F, 0.0F, 1.0F);
        if (camera && player.damage_source_position_valid)
        {
            const auto direction = player.damage_source_position - camera->position();
            const glm::vec3 flat{direction.x, 0.0F, direction.z};
            const glm::vec3 forward{camera->forward().x, 0.0F, camera->forward().z};
            const glm::vec3 right{camera->right().x, 0.0F, camera->right().z};
            if (glm::length(flat) > 0.01F && glm::length(forward) > 0.01F && glm::length(right) > 0.01F)
                hud.damage_direction_degrees = glm::degrees(std::atan2(
                    glm::dot(glm::normalize(flat), glm::normalize(right)),
                    glm::dot(glm::normalize(flat), glm::normalize(forward))
                ));
        }
        hud.hit = std::clamp(player.hit_confirmation_remaining_seconds / 0.16F, 0.0F, 1.0F);
        hud.reticle_radius = selected_weapon_ == PlayerWeapon::Shotgun ? 12.0F : 0.0F;
        hud.weapon = player.alive
                         ? (selected_weapon_ == PlayerWeapon::Shotgun ? "Дробовик" : "Пистолет")
                         : "Ко входу через " + std::to_string(int(std::ceil(player.respawn_remaining_seconds))) + " с";
        if (player.alive && player.weapon_cooldown_remaining_seconds > 0)
            hud.weapon += " · Пауза";
        if (player.alive)
            hud.weapon = "[" + (combat_state_.fire_binding_label.empty() ? "Mouse1" : combat_state_.fire_binding_label)
                         + "] " + hud.weapon;
        if (!player.alive)
            hud.hint = "Выполненные задачи и изменения мира сохранятся.";
        else if (combat_state_.active_hazard_contacts > 0)
            hud.hint = "Отойдите с опасной поверхности";
        else if (std::ranges::find(inventory_item_ids_, shotgun_item_id) != inventory_item_ids_.end())
            hud.hint = "[1] Пистолет  [2] Дробовик";
        if (interaction_state_.focused_usable_index >= 0)
        {
            const auto& usable = usables_[size_t(interaction_state_.focused_usable_index)];
            hud.prompt = "["
                         + (interaction_state_.use_binding_label.empty() ? "E" : interaction_state_.use_binding_label)
                         + "] " + build_focused_usable_label(usable);
            hud.prompt_detail = build_focused_usable_detail(usable);
            hud.interaction_ready = usable.enabled && !usable.locked && usable.requirement_met && !usable.consumed;
        }
        const auto mission = mission_status();
        if (mission.total_steps > 0)
        {
            hud.objective_header = mission.completed ? "ЗАДАЧА ВЫПОЛНЕНА"
                                                     : "ЗАДАЧА " + std::to_string(mission.current_step) + "/"
                                                           + std::to_string(mission.total_steps);
            hud.objective_title = mission.title;
            hud.objective_detail = mission.detail;
        }
        if (containment_ && !mission.completed && player.alive)
            hud.hint = containment_->status_text(activated_target_ids_);
        hud.inventory = build_inventory_summary();
        if (transient_hud_event_.remaining_seconds > 0)
        {
            hud.event = transient_hud_event_.text;
            hud.event_tone = transient_hud_event_.style == RelayCore::render::HudEventStyle::Success
                                 ? RelayCore::render::HudEventTone::Success
                             : transient_hud_event_.style == RelayCore::render::HudEventStyle::Warning
                                 ? RelayCore::render::HudEventTone::Warning
                                 : RelayCore::render::HudEventTone::Neutral;
            hud.event_strength = transient_hud_event_.total_seconds > 0
                                     ? transient_hud_event_.remaining_seconds / transient_hud_event_.total_seconds
                                     : 0;
        }
        if (camera && hud.visible && player.alive)
        {
            const auto query = world_query(false);
            const auto add_marker = [&](glm::vec3 anchor,
                                        glm::vec3 sight_target,
                                        std::string text,
                                        bool bar,
                                        float fraction,
                                        float opacity,
                                        bool damage = false) {
                const auto clip = camera->view_projection_matrix() * glm::vec4{anchor, 1};
                if (clip.w <= 0 || clip.z < -clip.w || clip.z > clip.w)
                    return;
                const auto ndc = glm::vec3{clip} / clip.w;
                if (std::abs(ndc.x) > 1 || std::abs(ndc.y) > 1)
                    return;
                const auto delta = sight_target - camera->position();
                const float distance = glm::length(delta);
                if (distance > 24)
                    return;
                if (distance > .01F && query.raycast(camera->position(), delta / distance, distance - .01F))
                {
                    const auto head_delta = anchor - glm::vec3{0, .3F, 0} - camera->position();
                    const float head_distance = glm::length(head_delta);
                    if (head_distance > .01F
                        && query.raycast(camera->position(), head_delta / head_distance, head_distance - .01F))
                        return;
                }
                hud.world_markers.push_back(
                    {(ndc.x + 1) * 50, (1 - ndc.y) * 50, fraction, opacity, bar, damage, std::move(text)}
                );
            };
            for (const auto& enemy : enemies_)
            {
                if (enemy.state.mode == EnemyMode::Dead)
                    continue;
                const auto bounds = enemy_bounds(enemy.state);
                add_marker(
                    {enemy.state.position.x, bounds.maxs.y + .25F, enemy.state.position.z},
                    (bounds.mins + bounds.maxs) * .5F,
                    std::to_string(int(std::ceil(enemy.state.health))) + " / "
                        + std::to_string(int(std::ceil(enemy.max_health))),
                    true,
                    enemy.state.health / enemy.max_health,
                    1
                );
            }
            for (const auto& turret : combat_state_.turrets)
                if (turret.enabled && !turret.destroyed)
                {
                    const auto center = (turret.bounds.mins + turret.bounds.maxs) * .5F;
                    add_marker(
                        {center.x, turret.bounds.maxs.y + .25F, center.z},
                        center,
                        std::to_string(int(std::ceil(turret.health))) + " / "
                            + std::to_string(int(std::ceil(turret.max_health))),
                        true,
                        turret.health / turret.max_health,
                        1
                    );
                }
            if (containment_)
            {
                const auto& state = containment_->state;
                const bool capturing = state.mode == GuardianMode::Capturing;
                const auto text = state.mode == GuardianMode::Contained ? "Хранитель · Сдержан"
                                  : capturing ? "Сдерживание " + std::to_string(int(state.capture_fraction * 100)) + "%"
                                  : state.mode == GuardianMode::Recovery   ? "Хранитель · Восстановление"
                                  : state.mode == GuardianMode::Suppressed ? "Хранитель · Подавлен"
                                                                           : "Хранитель · Броня";
                add_marker(
                    state.position + glm::vec3{0, 3.85F, 0},
                    state.position + glm::vec3{0, 2, 0},
                    text,
                    capturing,
                    state.capture_fraction,
                    1
                );
            }
            for (const auto& number : damage_numbers_)
            {
                const auto anchor = number.position + glm::vec3{0, (.9F - number.remaining_seconds) * .7F, 0};
                add_marker(
                    anchor,
                    number.position - glm::vec3{0, .4F, 0},
                    number.amount > 0 ? "−" + std::to_string(int(std::lround(number.amount))) : "Броня",
                    false,
                    0,
                    std::min(1.0F, number.remaining_seconds / .25F),
                    true
                );
            }
        }
        return hud;
    }
} // namespace RelayGame

#pragma once
#include <string>
#include <vector>
namespace RelayCore::render
{
    enum class HudEventTone {
        Neutral,
        Success,
        Warning
    };
    struct HudWorldMarker {
        float x = 0, y = 0, fraction = 0, opacity = 1;
        bool health_bar = false, damage = false;
        std::string text;
    };
    struct HudPresentation {
        bool visible = false, alive = true, interaction_ready = false;
        float health = 0, max_health = 0, hurt = 0, hit = 0, reticle_radius = 0;
        std::string weapon, hint, objective_header, objective_title, objective_detail, inventory, prompt, prompt_detail,
            event;
        float event_strength = 0;
        HudEventTone event_tone = HudEventTone::Neutral;
        std::vector<HudWorldMarker> world_markers;
    };
    struct PlayerUiAction {
        std::string name, value;
    };
    enum class PlayerUiScreen {
        Hidden,
        Hud,
        Menu,
        Finale
    };
} // namespace RelayCore::render

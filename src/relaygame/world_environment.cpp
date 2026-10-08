#include "relaygame/world_environment.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <stdexcept>

namespace RelayGame
{
    auto build_world_environment(const RelayCore::BlockoutLevel& level) -> std::vector<EnvironmentEffect>
    {
        std::vector<EnvironmentEffect> result;
        int light_index = 0;
        for (const auto& entity : level.entities)
        {
            if (entity.classname != "world_effect" && entity.classname != "render_light")
                continue;
            const auto property = [&](const char* key, const char* fallback = "") {
                const auto found = entity.properties.find(key);
                return found == entity.properties.end() ? std::string{fallback} : found->second;
            };
            const auto number = [&](const char* key, float fallback) {
                if (!entity.properties.contains(key))
                    return fallback;
                const auto value = property(key);
                float parsed{};
                const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
                if (error != std::errc{} || end != value.data() + value.size() || !std::isfinite(parsed))
                    throw std::runtime_error(entity.classname + "." + key + " must be finite numeric data");
                return parsed;
            };
            EnvironmentEffect effect;
            effect.origin = entity.origin;
            effect.angles = entity.angles;
            effect.required_target = property("required_target");
            effect.stop_target = property("stop_target");
            effect.delay = number("start_delay", 0);
            effect.scale = number("scale", 1);
            effect.fixture_offset = number("fixture_offset", 0.485F);
            if (effect.delay < 0 || effect.scale <= 0)
                throw std::runtime_error(entity.classname + ": start_delay must be >= 0 and scale > 0");
            if (entity.classname == "render_light")
            {
                effect.kind = EnvironmentKind::Lamp;
                effect.light_index = light_index++;
                const auto state = property("state", "working");
                if (state != "working" && state != "damaged")
                    throw std::runtime_error("render_light.state must be working or damaged");
                effect.damaged = state == "damaged";
                effect.fixture_width =
                    number("fixture_width", !effect.required_target.empty() || effect.damaged ? .56F : 0);
                effect.fixture_depth = number("fixture_depth", 0.18F);
                if (effect.fixture_width < 0 || effect.fixture_depth <= 0)
                    throw std::runtime_error("render_light: fixture_width must be >= 0 and fixture_depth > 0");
            } else
            {
                const auto kind = property("kind");
                if (kind == "fan")
                    effect.kind = EnvironmentKind::Fan;
                else if (kind == "steam")
                    effect.kind = EnvironmentKind::Steam;
                else if (kind == "sparks")
                    effect.kind = EnvironmentKind::Sparks;
                else if (kind == "decal")
                    effect.kind = EnvironmentKind::Decal;
                else
                    throw std::runtime_error("world_effect.kind must be fan, steam, sparks or decal: " + kind);
                effect.mark = property("mark", "arrow");
                if (effect.kind == EnvironmentKind::Decal && effect.mark != "arrow" && effect.mark != "warning"
                    && effect.mark != "01" && effect.mark != "02" && effect.mark != "03")
                    throw std::runtime_error("world_effect.mark must be arrow, warning, 01, 02 or 03");
            }
            result.push_back(std::move(effect));
        }
        update_world_environment(result, {}, 0);
        return result;
    }

    void update_world_environment(std::span<EnvironmentEffect> effects, std::span<const std::string> targets, float dt)
    {
        const auto triggered = [&](const std::string& target) {
            return !target.empty() && std::ranges::find(targets, target) != targets.end();
        };
        for (auto& effect : effects)
        {
            effect.age += dt;
            const bool active =
                (effect.required_target.empty() || triggered(effect.required_target)) && !triggered(effect.stop_target);
            effect.elapsed = active ? (effect.active ? effect.elapsed + dt : 0) : 0;
            effect.active = active;
            const float running = effect.elapsed - effect.delay;
            float target = active && running >= 0 ? 1.0F : 0.0F;
            if (effect.kind == EnvironmentKind::Lamp)
            {
                if (target > 0 && (!effect.required_target.empty() && running < 0.8F))
                    target = std::fmod(running, 0.23F) < 0.08F ? 0.08F : 0.65F;
                if (effect.damaged && std::fmod(effect.age, 2.7F) < 0.20F)
                    target *= 0.08F;
                effect.strength = target;
            } else if (effect.kind == EnvironmentKind::Fan || effect.kind == EnvironmentKind::Steam)
            {
                // Exponential response keeps startup and coast-down independent of frame rate.
                const float rate = effect.kind == EnvironmentKind::Fan ? 0.75F : 2.0F;
                effect.strength += (target - effect.strength) * (1.0F - std::exp(-dt * rate));
                effect.phase = std::fmod(effect.phase + effect.strength * dt * 8.0F, 6.2831853F);
            } else
                effect.strength = target;
        }
    }

    auto environment_sound(EnvironmentKind kind) -> const char*
    {
        switch (kind)
        {
            case EnvironmentKind::Fan:
                return "audio/environment/ventilation.wav";
            case EnvironmentKind::Steam:
                return "audio/environment/steam.wav";
            case EnvironmentKind::Sparks:
                return "audio/environment/electrical.wav";
            default:
                return "";
        }
    }
} // namespace RelayGame

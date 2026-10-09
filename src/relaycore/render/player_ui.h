#pragma once
#include "relaycore/render/player_ui_data.h"

#include <SDL3/SDL_events.h>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>
struct SDL_Window;
namespace RelayCore::render
{
    class VulkanDevice;
    class PlayerUi {
    public:
        PlayerUi(SDL_Window* window, const VulkanDevice& device);
        ~PlayerUi();
        PlayerUi(const PlayerUi&) = delete;
        auto operator=(const PlayerUi&) -> PlayerUi& = delete;
        void set_screen(PlayerUiScreen screen);
        void set_hud(const HudPresentation& hud);
        void set_text(const std::string& id, const std::string& text);
        void set_visible(const std::string& id, bool visible);
        void set_disabled(const std::string& id, bool disabled);
        void set_value(const std::string& id, const std::string& value);
        auto consume_actions() -> std::vector<PlayerUiAction>;
        void process_event(const SDL_Event& event);
        auto captures_input() const noexcept -> bool;
        auto visible() const noexcept -> bool;
        auto has_focused_control() const noexcept -> bool;
        auto wants_text_input() const noexcept -> bool;
        void prepare(int width, int height);
        void record(VkCommandBuffer commands);

    private:
        struct State;
        std::unique_ptr<State> state_;
    };
} // namespace RelayCore::render

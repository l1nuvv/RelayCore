#include "relaycore/render/player_ui.h"

#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/core/profiling.h"
#include "relaycore/render/vulkan_rml_renderer.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/ElementUtilities.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "../../../extern/rmlui/RmlUi_Platform_SDL.h"

namespace RelayCore::render
{
    struct PlayerUi::State : Rml::EventListener {
        SDL_Window* window;
        SystemInterface_SDL system;
        TextInputMethodEditor_SDL ime;
        VulkanRmlRenderer renderer;
        Rml::Context* context = nullptr;
        Rml::ElementDocument* hud = nullptr;
        Rml::ElementDocument* menu = nullptr;
        Rml::ElementDocument* finale = nullptr;
        PlayerUiScreen screen = PlayerUiScreen::Hidden;
        std::vector<PlayerUiAction> actions;
        struct WorldMarkerElements {
            Rml::Element *root, *text, *bar, *fill;
        };
        std::vector<WorldMarkerElements> world_markers;
        std::unordered_map<std::string, std::string> text_cache;
        std::unordered_map<std::string, std::string> property_cache;
        std::unordered_set<Rml::FontFaceHandle> prepared_fonts;
        bool initialized = false;
        struct HealthFeedback {
            float previous = 0, maximum = 0, target = 0, shown = 0, ghost = 0;
            float speed = 0, ghost_speed = 0, hold = 0, flash = 0;
            bool initialized = false, alive = false;
        } health;
        State(SDL_Window* target, const VulkanDevice& device) :
            window(target), system(target), renderer(device, core::asset_path("shaders/vulkan"))
        {}
        ~State()
        {
            if (context)
            {
                if (menu)
                {
                    menu->RemoveEventListener("click", this);
                    menu->RemoveEventListener("change", this);
                    menu->RemoveEventListener("keydown", this);
                }
                Rml::RemoveContext("player");
            }
            if (initialized)
                Rml::Shutdown();
            Rml::SetRenderInterface(nullptr);
            Rml::SetSystemInterface(nullptr);
        }
        void ProcessEvent(Rml::Event& event) override
        {
            auto* element = event.GetTargetElement();
            while (element && !element->HasAttribute("data-action"))
                element = element->GetParentNode();
            if (!element || element->HasAttribute("disabled"))
                return;
            if (event.GetType() == "keydown" && element->GetTagName() == "button")
            {
                const auto key = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
                if (key == Rml::Input::KI_RETURN || key == Rml::Input::KI_SPACE)
                {
                    element->Click();
                    event.StopPropagation();
                }
                return;
            }
            const auto kind = element->GetAttribute<Rml::String>("data-event", "click");
            if (event.GetType() != kind)
                return;
            const auto* control = dynamic_cast<Rml::ElementFormControl*>(element);
            actions.push_back(
                {element->GetAttribute<Rml::String>("data-action", ""), control ? control->GetValue() : ""}
            );
        }
        auto element(const std::string& id) -> Rml::Element*
        {
            for (auto* document : {hud, menu, finale})
                if (auto* result = document->GetElementById(id))
                    return result;
            throw std::runtime_error("Missing player UI element: " + id);
        }
        void property(const std::string& id, const std::string& name, const std::string& value)
        {
            auto [it, inserted] = property_cache.try_emplace(id + "/" + name, value);
            if (!inserted && it->second == value)
                return;
            it->second = value;
            element(id)->SetProperty(name, value);
        }
    };
    PlayerUi::PlayerUi(SDL_Window* window, const VulkanDevice& device) : state_(std::make_unique<State>(window, device))
    {
        Rml::SetSystemInterface(&state_->system);
        Rml::SetRenderInterface(&state_->renderer);
        if (!Rml::Initialise())
            throw std::runtime_error("RmlUi initialization failed");
        state_->initialized = true;
        if (!Rml::LoadFontFace(core::asset_path("fonts/RobotoMono-Regular.ttf").string()))
            throw std::runtime_error("Cannot load player UI font: fonts/RobotoMono-Regular.ttf");
        state_->context = Rml::CreateContext("player", {1280, 720}, nullptr, &state_->ime);
        if (!state_->context)
            throw std::runtime_error("Cannot create RmlUi player context");
        state_->hud = state_->context->LoadDocument(core::asset_path("ui/hud.rml").string());
        state_->menu = state_->context->LoadDocument(core::asset_path("ui/menu.rml").string());
        state_->finale = state_->context->LoadDocument(core::asset_path("ui/finale.rml").string());
        if (!state_->hud || !state_->menu || !state_->finale)
            throw std::runtime_error("Cannot load player RML documents from assets/ui");
        state_->menu->AddEventListener("click", state_.get());
        state_->menu->AddEventListener("change", state_.get());
        state_->menu->AddEventListener("keydown", state_.get());
    }
    PlayerUi::~PlayerUi() = default;
    void PlayerUi::set_screen(PlayerUiScreen screen)
    {
        if (state_->screen == screen)
            return;
        state_->screen = screen;
        reset_hud_feedback();
        for (int button = 0; button < 3; ++button)
            state_->context->ProcessMouseButtonUp(button, 0);
        state_->hud->Hide();
        state_->menu->Hide();
        state_->finale->Hide();
        state_->context->ProcessMouseLeave();
        state_->actions.clear();
        if (screen == PlayerUiScreen::Hud)
            state_->hud->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        if (screen == PlayerUiScreen::Menu)
            state_->menu->Show(Rml::ModalFlag::Modal, Rml::FocusFlag::Document);
        if (screen == PlayerUiScreen::Finale)
            state_->finale->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        if (screen != PlayerUiScreen::Menu)
            SDL_StopTextInput(state_->window);
    }
    void PlayerUi::set_text(const std::string& id, const std::string& text)
    {
        auto [it, inserted] = state_->text_cache.try_emplace(id, text);
        if (!inserted && it->second == text)
            return;
        it->second = text;
        state_->element(id)->SetInnerRML(Rml::StringUtilities::EncodeRml(text));
    }
    void PlayerUi::set_visible(const std::string& id, bool visible)
    {
        auto* element = state_->element(id);
        const auto display = visible ? "block" : "none";
        if (element->GetProperty<Rml::String>("display") != display)
            element->SetProperty("display", display);
    }
    void PlayerUi::set_disabled(const std::string& id, bool disabled)
    {
        auto* element = state_->element(id);
        if (element->HasAttribute("disabled") == disabled)
            return;
        if (disabled)
            element->SetAttribute("disabled", "");
        else
            element->RemoveAttribute("disabled");
        element->SetPseudoClass("disabled", disabled);
        element->SetProperty("tab-index", disabled ? "none" : "auto");
        if (disabled)
        {
            element->SetProperty("focus", "none");
            element->Blur();
        } else
            element->RemoveProperty("focus");
    }
    void PlayerUi::set_value(const std::string& id, const std::string& value)
    {
        auto* control = dynamic_cast<Rml::ElementFormControl*>(state_->element(id));
        if (!control)
            throw std::runtime_error("Player UI value target is not a form control: " + id);
        if (control->GetValue() != value)
            control->SetValue(value);
    }
    auto PlayerUi::consume_actions() -> std::vector<PlayerUiAction>
    {
        return std::exchange(state_->actions, {});
    }
    auto PlayerUi::captures_input() const noexcept -> bool
    {
        return state_->screen == PlayerUiScreen::Menu;
    }
    auto PlayerUi::visible() const noexcept -> bool
    {
        return state_->screen != PlayerUiScreen::Hidden;
    }
    auto PlayerUi::has_focused_control() const noexcept -> bool
    {
        const auto* focus = state_->context->GetFocusElement();
        return captures_input() && focus && focus != state_->menu;
    }
    auto PlayerUi::wants_text_input() const noexcept -> bool
    {
        const auto* focus = state_->context->GetFocusElement();
        return captures_input() && focus
               && (focus->GetTagName() == "textarea"
                   || (focus->GetTagName() == "input" && focus->GetAttribute<Rml::String>("type", "text") == "text"));
    }
    void PlayerUi::process_event(const SDL_Event& event)
    {
        if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_WINDOW_MINIMIZED)
        {
            state_->context->ProcessMouseLeave();
            for (int button = 0; button < 3; ++button)
                state_->context->ProcessMouseButtonUp(button, 0);
            state_->actions.clear();
            if (captures_input())
                state_->menu->Focus();
            SDL_CaptureMouse(false);
            SDL_StopTextInput(state_->window);
        }
        if (captures_input())
        {
            if (event.type == SDL_EVENT_TEXT_EDITING)
                state_->ime.HandleEdit(event.edit);
            else
            {
                auto copy = event;
                RmlSDL::InputEventHandler(state_->context, state_->window, copy);
            }
        }
    }
    void PlayerUi::prepare(int width, int height)
    {
        RELAYCORE_PROFILE_ZONE_N("RmlUi update/render");
        state_->renderer.begin_frame(width, height);
        state_->context->SetDimensions({width, height});
        // Documents use dp; pointer input and viewport use framebuffer pixels, converted by the SDL backend.
        const float fit = std::min(float(width) / 1280.0F, float(height) / 720.0F);
        state_->context->SetDensityIndependentPixelRatio(std::max(0.75F, fit));
        {
            RELAYCORE_PROFILE_ZONE_N("RmlUi context update");
            state_->context->Update();
        }
        {
            RELAYCORE_PROFILE_ZONE_N("RmlUi context render");
            state_->context->Render();
        }
    }
    void PlayerUi::prepare_fonts(int width, int height)
    {
        RELAYCORE_PROFILE_ZONE_N("RmlUi prepare fonts");
        state_->context->SetDimensions({width, height});
        const float fit = std::min(float(width) / 1280.0F, float(height) / 720.0F);
        state_->context->SetDensityIndependentPixelRatio(std::max(0.75F, fit));

        // Dynamic damage markers use a font size absent from the static document.
        auto marker = state_->hud->CreateElement("div");
        marker->SetClass("world-marker", true);
        marker->SetClass("damage", true);
        auto* marker_element = state_->hud->AppendChild(std::move(marker));
        state_->context->Update();
        const Rml::String glyphs =
            " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"
            "АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯабвгдеёжзийклмнопрстуфхцчшщъыьэюя×—–−·…→←↑↓•°№«»";
        auto* engine = Rml::GetFontEngineInterface();
        auto& manager = state_->context->GetRenderManager();
        const Rml::String language = "ru";
        const Rml::TextShapingContext shaping{language};
        const auto prepare_element = [&](const auto& self, Rml::Element* element) -> void {
            const auto face = element->GetFontFaceHandle();
            if (face && state_->prepared_fonts.insert(face).second)
            {
                // Width discovery must precede mesh generation: adding glyphs invalidates the atlas.
                engine->GetStringWidth(face, glyphs, shaping);
                Rml::TexturedMeshList meshes;
                engine
                    ->GenerateString(manager, face, 0, glyphs, {}, Rml::ColourbPremultiplied{255}, 1, shaping, meshes);
                for (const auto& mesh : meshes)
                {
                    const auto dimensions = mesh.texture.GetDimensions();
                    if (dimensions.x <= 0 || dimensions.y <= 0)
                        throw std::runtime_error("Cannot prepare player UI font atlas");
                }
            }
            for (int i = 0; i < element->GetNumChildren(); ++i)
                self(self, element->GetChild(i));
        };
        for (auto* document : {state_->hud, state_->menu, state_->finale})
            prepare_element(prepare_element, document);
        state_->hud->RemoveChild(marker_element);
    }
    auto PlayerUi::retain_render_resources() const -> std::shared_ptr<const void>
    {
        return state_->renderer.retain_render_resources();
    }
    void PlayerUi::record(VkCommandBuffer commands)
    {
        state_->renderer.record(commands);
    }
    void PlayerUi::reset_hud_feedback()
    {
        state_->health.initialized = false;
    }
    void PlayerUi::advance_hud(double seconds)
    {
        auto& h = state_->health;
        if (!h.initialized || state_->screen != PlayerUiScreen::Hud)
            return;
        const float dt = std::isfinite(seconds) && seconds > 0 ? float(std::min(seconds, 10.0)) : 0;
        h.shown = std::max(h.target, h.shown - h.speed * dt);
        const float drain_dt = std::max(0.0F, dt - h.hold);
        h.hold = std::max(0.0F, h.hold - dt);
        h.ghost = std::max(h.shown, h.ghost - h.ghost_speed * drain_dt);
        h.flash = std::max(0.0F, h.flash - dt);
        state_->property("health-main", "width", std::to_string(h.shown * 100) + "%");
        state_->property("health-ghost", "width", std::to_string(h.ghost * 100) + "%");
        state_->property("health-flash", "opacity", std::to_string(h.flash / 0.08F * 0.45F));
    }
    void PlayerUi::set_hud(const HudPresentation& data)
    {
        auto& h = state_->health;
        const bool valid = std::isfinite(data.health) && std::isfinite(data.max_health) && data.max_health > 0;
        const float target = valid && data.alive ? std::clamp(data.health / data.max_health, 0.0F, 1.0F) : 0;
        const bool snap = !h.initialized || !data.visible || state_->screen != PlayerUiScreen::Hud || !valid
                          || data.max_health != h.maximum || data.alive != h.alive || !data.alive;
        if (snap || data.health > h.previous)
        {
            h.shown = h.ghost = target;
            h.hold = h.flash = 0;
        } else if (data.health < h.previous)
        {
            h.ghost = std::max(h.ghost, h.shown);
            h.hold = 0.15F;
            h.flash = 0.08F;
            h.speed = std::abs(h.shown - target) / 0.15F;
            h.ghost_speed = std::abs(h.ghost - target) / 0.32F;
        }
        h.target = target;
        h.previous = data.health;
        h.maximum = data.max_health;
        h.alive = data.alive;
        h.initialized = data.visible && valid && state_->screen == PlayerUiScreen::Hud;
        set_visible("health-track", valid);
        state_->property("health-main", "width", std::to_string(h.shown * 100) + "%");
        state_->property("health-ghost", "width", std::to_string(h.ghost * 100) + "%");
        state_->property("health-flash", "opacity", std::to_string(h.flash / 0.08F * 0.45F));
        while (state_->world_markers.size() < data.world_markers.size())
        {
            auto root = state_->hud->CreateElement("div");
            root->SetClass("world-marker", true);
            auto text = state_->hud->CreateElement("div");
            text->SetClass("marker-text", true);
            auto bar = state_->hud->CreateElement("div");
            bar->SetClass("marker-bar", true);
            auto fill = state_->hud->CreateElement("div");
            fill->SetClass("marker-fill", true);
            const State::WorldMarkerElements elements{root.get(), text.get(), bar.get(), fill.get()};
            bar->AppendChild(std::move(fill));
            root->AppendChild(std::move(text));
            root->AppendChild(std::move(bar));
            state_->element("world-markers")->AppendChild(std::move(root));
            state_->world_markers.push_back(elements);
        }
        for (std::size_t i = 0; i < state_->world_markers.size(); ++i)
        {
            const auto& elements = state_->world_markers[i];
            const bool visible = data.visible && data.alive && i < data.world_markers.size();
            elements.root->SetProperty("display", visible ? "block" : "none");
            if (!visible)
                continue;
            const auto& marker = data.world_markers[i];
            elements.root->SetProperty("left", std::to_string(marker.x) + "%");
            elements.root->SetProperty("top", std::to_string(marker.y) + "%");
            elements.root->SetProperty("opacity", std::to_string(marker.opacity));
            elements.root->SetClass("damage", marker.damage);
            elements.text->SetInnerRML(Rml::StringUtilities::EncodeRml(marker.text));
            elements.bar->SetProperty("display", marker.health_bar ? "block" : "none");
            elements.fill->SetProperty("width", std::to_string(marker.fraction * 100) + "%");
        }
        set_visible("status", data.visible);
        set_text(
            "health",
            std::to_string(int(std::lround(data.health))) + " / " + std::to_string(int(std::lround(data.max_health)))
        );
        set_text("status-label", data.alive ? "ЗДОРОВЬЕ" : "ВОЗВРАЩЕНИЕ");
        set_text("weapon", data.weapon);
        set_text("hint", data.hint);
        set_visible("objective", !data.objective_header.empty());
        set_text("objective-header", data.objective_header);
        set_text("objective-title", data.objective_title);
        set_text("objective-detail", data.objective_detail);
        set_visible("inventory", !data.inventory.empty());
        set_text("inventory", data.inventory);
        set_visible("prompt-card", !data.prompt.empty());
        set_text("prompt", data.prompt);
        set_text("prompt-detail", data.prompt_detail);
        set_visible("event-card", !data.event.empty() && data.event_strength > 0.001F);
        set_text("event-text", data.event);
        set_visible("reticle", data.visible && data.alive);
        auto* reticle = state_->element("reticle");
        reticle->SetClass("ready", data.interaction_ready);
        reticle->SetClass("focused", !data.prompt.empty());
        state_->element("status")->SetClass("danger", !data.alive || data.health < data.max_health * 0.35F);
        state_->property("damage-direction", "opacity", std::to_string(data.damage_alert));
        state_->property(
            "damage-direction",
            "transform",
            "rotate(" + std::to_string(data.damage_direction_degrees) + "deg)"
        );
        state_->property("hit", "opacity", std::to_string(data.hit));
        state_->property("spread", "width", std::to_string(data.reticle_radius * 2) + "px");
        state_->property("spread", "height", std::to_string(data.reticle_radius * 2) + "px");
        state_->property("spread", "border-radius", std::to_string(data.reticle_radius) + "px");
        state_->property("spread", "margin-left", std::to_string(-data.reticle_radius) + "px");
        state_->property("spread", "margin-top", std::to_string(-data.reticle_radius) + "px");
        state_->property("spread", "display", data.reticle_radius > 0 ? "block" : "none");
        state_->element("event-card")->SetClass("success", data.event_tone == HudEventTone::Success);
        state_->element("event-card")->SetClass("warning", data.event_tone == HudEventTone::Warning);
    }
} // namespace RelayCore::render

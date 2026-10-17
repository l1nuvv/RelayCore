#include "relaycore/app/engine_services.h"
#include "relaycore/audio/audio_system.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/renderer.h"
#include "relaygame/demo_game.h"
#include "relaygame/player_settings.h"
#include "relaygame/save_game.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/ElementUtilities.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>

#include "scoped_temp_directory.h"

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
    void save(const RelayCore::render::CpuRgbaTextureData& image, const std::filesystem::path& path)
    {
        auto* surface = SDL_CreateSurfaceFrom(
            image.width,
            image.height,
            SDL_PIXELFORMAT_RGBA32,
            const_cast<uint8_t*>(image.pixels.data()),
            image.width * 4
        );
        require(surface != nullptr, "Player UI capture surface failed");
        const bool written = SDL_SaveBMP(surface, path.string().c_str());
        SDL_DestroySurface(surface);
        require(written, "Player UI capture failed");
    }
} // namespace
int main(int argc, char** argv)
{
    const bool quality_integration = argc == 2 && std::string_view{argv[1]} == "--quality-presets";
    if (argc != 1 && !quality_integration)
        return 1;
    if (!SDL_Init(SDL_INIT_VIDEO))
        return 77;
    int result = 0;
    RelayCore::test::ScopedTempDirectory directory;
    RelayCore::log::init({.logs_directory = (directory.path() / "logs").string(), .enable_timestamped_log = false});
    try
    {
        auto config = RelayCore::core::load_default_engine_config();
        config.renderer.imgui.enabled = false;
        config.renderer.descriptor_indexing = quality_integration;
        config.audio.enabled = false;
        config.window.width = 1280;
        config.window.height = 720;
        config.window.vsync = false;
        RelayCore::Window window{config.window};
        SDL_HideWindow(window.handle());
        RelayCore::Renderer renderer{config.renderer};
        renderer.initialize(window);
        const auto check_prepared_fonts = [&] {
            auto* context = Rml::GetContext("player");
            auto* engine = Rml::GetFontEngineInterface();
            const Rml::String language = "ru";
            const Rml::TextShapingContext shaping{language};
            const auto check = [&](const auto& self, Rml::Element* element) -> void {
                if (const auto face = element->GetFontFaceHandle())
                {
                    const auto version = engine->GetVersion(face);
                    const Rml::String text = "Ёжик: здоровье 97 / 100; ВОЗВРАЩЕНИЕ × — … −20 · Броня";
                    engine->GetStringWidth(face, text, shaping);
                    Rml::TexturedMeshList meshes;
                    engine->GenerateString(
                        context->GetRenderManager(),
                        face,
                        0,
                        text,
                        {},
                        Rml::ColourbPremultiplied{255},
                        1,
                        shaping,
                        meshes
                    );
                    require(engine->GetVersion(face) == version, "First-use Cyrillic/numbers rebuilt a prepared atlas");
                }
                for (int i = 0; i < element->GetNumChildren(); ++i)
                    self(self, element->GetChild(i));
            };
            for (const char* name : {"hud", "menu", "finale"})
                check(check, context->GetDocument(name));
        };
        check_prepared_fonts();
        renderer.player_ui().prepare_fonts(1280, 720);
        check_prepared_fonts();
        RelayCore::AudioSystem audio{config.audio};
        bool running = true;
        RelayCore::EngineServices services{config, window, renderer, audio, running};
        const auto user_path = directory.path() / "player";
        constexpr const char* profile_variable = "RELAYCORE_USER_DATA_ROOT";
        const auto* prior_profile = SDL_getenv_unsafe(profile_variable);
        const std::optional<std::string> previous_profile =
            prior_profile ? std::optional<std::string>{prior_profile} : std::nullopt;
        for (const char* invalid : {"relative/player", ""})
        {
            require(SDL_setenv_unsafe(profile_variable, invalid, 1) == 0, "Cannot select invalid profile fixture");
            bool rejected = false;
            try
            {
                auto invalid_profile = RelayGame::make_demo_game();
                invalid_profile->on_startup(services);
            } catch (const std::runtime_error&)
            {
                rejected = true;
            }
            require(rejected, "Invalid profile override fell back to user progress");
        }
        require(
            SDL_setenv_unsafe(profile_variable, user_path.string().c_str(), 1) == 0,
            "Cannot isolate playtest profile"
        );
        auto delegate = RelayGame::make_demo_game();
        delegate->on_startup(services);
        if (previous_profile)
            require(
                SDL_setenv_unsafe(profile_variable, previous_profile->c_str(), 1) == 0,
                "Cannot restore profile environment"
            );
        else
            require(SDL_unsetenv_unsafe(profile_variable) == 0, "Cannot restore default profile environment");
        auto& game = static_cast<RelayGame::DemoGame&>(*delegate);
        require(renderer.ui_available() && !renderer.developer_ui_available(), "Player UI requires developer ImGui");
        const auto captures = std::filesystem::current_path() / "player-ui-captures";
        std::filesystem::create_directories(captures);
        const RelayCore::FrameInfo frame{.delta_seconds = 1.0 / 60.0};
        const auto draw = [&](const std::string& name) {
            renderer.begin_frame();
            game.on_render(frame, services);
            require(renderer.begin_ui(frame, audio, game.has_custom_ui()), "Player UI unavailable with ImGui disabled");
            game.on_ui(services);
            renderer.end_ui();
            auto image = renderer.capture_frame();
            if (!name.empty())
                save(image, captures / (name + ".bmp"));
            return image;
        };
        auto* document = Rml::GetContext("player")->GetDocument("menu");
        const auto control = [&](const char* id) -> Rml::Element* {
            auto* element = document->GetElementById(id);
            require(element != nullptr, "Missing authored player UI control");
            return element;
        };
        const auto key = [&](SDL_Scancode scan, SDL_Keycode code) {
            SDL_Event event{};
            event.type = SDL_EVENT_KEY_DOWN;
            event.key.scancode = scan;
            event.key.key = code;
            renderer.process_event(event);
            game.on_event(event, services);
            game.on_update({}, services);
        };
        const auto click = [&](const char* id) {
            auto* element = control(id);
            element->ScrollIntoView();
            (void) draw("");
            Rml::Rectanglef bounds;
            require(
                Rml::ElementUtilities::GetBoundingBox(bounds, element, Rml::BoxArea::Border),
                "Menu control has no projected bounds"
            );
            const auto center = bounds.Center();
            auto* hit = Rml::GetContext("player")->GetElementAtPoint(center);
            while (hit && hit != element)
                hit = hit->GetParentNode();
            if (!hit)
                throw std::runtime_error(std::string{"Menu click is outside the visible control: "} + id);
            const float density = SDL_GetWindowPixelDensity(window.handle());
            SDL_Event event{};
            event.type = SDL_EVENT_MOUSE_MOTION;
            event.motion.x = center.x / density;
            event.motion.y = center.y / density;
            renderer.process_event(event);
            game.on_event(event, services);
            event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            event.button.button = SDL_BUTTON_LEFT;
            renderer.process_event(event);
            game.on_event(event, services);
            event.type = SDL_EVENT_MOUSE_BUTTON_UP;
            renderer.process_event(event);
            game.on_event(event, services);
            game.on_update({}, services);
        };
        const auto change = [&](const char* id, const char* value) {
            auto* element = dynamic_cast<Rml::ElementFormControl*>(control(id));
            require(element != nullptr, "Player setting is not a form control");
            element->SetValue(value);
            element->DispatchEvent("change", {});
            game.on_update({}, services);
        };
        const auto branded_menu = draw("branding");
        Rml::Rectanglef seal_bounds;
        require(
            Rml::ElementUtilities::GetBoundingBox(seal_bounds, control("seal"), Rml::BoxArea::Content),
            "Brand image has no projected bounds"
        );
        const auto seal_pixel = [&](float x, float y) {
            const auto point = seal_bounds.TopLeft() + seal_bounds.Size() * Rml::Vector2f{x / 128, y / 128};
            const auto offset = (size_t(point.y) * branded_menu.width + size_t(point.x)) * 4;
            return std::array{
                branded_menu.pixels.at(offset),
                branded_menu.pixels.at(offset + 1),
                branded_menu.pixels.at(offset + 2)
            };
        };
        const auto axis = seal_pixel(64, 16), aperture = seal_pixel(64, 60), gap = seal_pixel(64, 30);
        require(axis[0] > 150 && axis[1] > 130 && axis[2] < 140, "Brand BMP did not render in the menu palette");
        require(
            aperture[0] + 70 < axis[0] && gap[0] + 70 < axis[0],
            "Brand BMP lost its transparent counter or axis gap"
        );
        for (const auto lost : {SDL_EVENT_WINDOW_FOCUS_LOST, SDL_EVENT_WINDOW_MINIMIZED})
        {
            auto* button = control("settings-button");
            button->ScrollIntoView();
            (void) draw("");
            Rml::Rectanglef bounds;
            require(
                Rml::ElementUtilities::GetBoundingBox(bounds, button, Rml::BoxArea::Border),
                "Settings bounds missing"
            );
            const auto center = bounds.Center();
            const float density = SDL_GetWindowPixelDensity(window.handle());
            SDL_Event event{};
            event.type = SDL_EVENT_MOUSE_MOTION;
            event.motion.x = center.x / density;
            event.motion.y = center.y / density;
            renderer.process_event(event);
            event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            event.button.button = SDL_BUTTON_LEFT;
            renderer.process_event(event);
            require(button->IsPseudoClassSet("active"), "Pressed menu control did not become active");
            event.type = lost;
            renderer.process_event(event);
            game.on_event(event, services);
            require(!renderer.player_ui().has_focused_control(), "Inactive window retained menu control focus");
            require(!button->IsPseudoClassSet("active"), "Inactive window retained a pressed menu control");
            require(renderer.player_ui().consume_actions().empty(), "Focus cancellation activated a menu action");
            event.type = lost == SDL_EVENT_WINDOW_MINIMIZED ? SDL_EVENT_WINDOW_RESTORED : SDL_EVENT_WINDOW_FOCUS_GAINED;
            renderer.process_event(event);
            game.on_event(event, services);
            event.type = SDL_EVENT_MOUSE_BUTTON_UP;
            event.button.button = SDL_BUTTON_LEFT;
            renderer.process_event(event);
            game.on_event(event, services);
            game.on_update({}, services);
            (void) draw("");
            require(!control("volume")->IsVisible(true), "Focus return completed an interrupted menu click");
            click("settings-button");
            (void) draw("");
            require(control("volume")->IsVisible(true), "Focus return prevented a fresh menu click");
            click("settings-button");
            (void) draw("");
        }
        for (const auto size : {Rml::Vector2i{1280, 720}, {1920, 1080}, {2560, 1440}, {800, 600}})
        {
            require(SDL_SetWindowSize(window.handle(), size.x, size.y), "Player UI resize failed");
            SDL_PumpEvents();
            renderer.on_resize(size.x, size.y);
            renderer.player_ui().prepare_fonts(size.x, size.y);
            check_prepared_fonts();
            (void) draw("menu-" + std::to_string(size.x));
            require(control("new-game")->IsVisible(true), "Resize hid the main action");
        }
        require(SDL_SetWindowSize(window.handle(), 1280, 720), "Restore size failed");
        renderer.on_resize(1280, 720);
        SDL_PumpEvents();
        (void) draw("");
        require(control("continue")->HasAttribute("disabled"), "Continue without a save must be disabled");
        require(control("continue")->IsPseudoClassSet("disabled"), "Disabled action lost its visual state");
        key(SDL_SCANCODE_TAB, SDLK_TAB);
        require(renderer.player_ui().has_focused_control(), "Tab did not focus a menu control");
        change("scenario", "levels/pressure_transfer.toml");
        click("new-game");
        require(
            RelayGame::load_saved_game(user_path / "progress.toml").world.level_path
                == RelayCore::core::asset_path("levels/pressure_transfer.toml").string(),
            "Factory profile override did not isolate New Game progress"
        );
        require(
            window.relative_mouse_mode() && !renderer.player_ui().captures_input(),
            "New Game leaked menu input ownership"
        );
        game.on_update(frame, services);
        (void) draw("gameplay");
        const auto initial = renderer.player_diagnostics();
        key(SDL_SCANCODE_ESCAPE, SDLK_ESCAPE);
        require(!window.relative_mouse_mode() && renderer.player_ui().captures_input(), "Pause did not block gameplay");
        (void) draw("pause");
        require(control("settings-button")->Focus(), "Menu button cannot receive keyboard focus");
        key(SDL_SCANCODE_RETURN, SDLK_RETURN);
        (void) draw("settings");
        require(control("volume")->IsVisible(true), "Keyboard did not open settings");
        change("volume", "0.35");
        change("fov", "82");
        change("sensitivity", "2.5");
        const auto camera_position = renderer.camera().position();
        for (const char* value : {"3", "1", "2", "3"})
        {
            change("graphics-preset", value);
            control("graphics-preset")->ScrollIntoView();
            (void) draw(std::string{"preset-"} + value);
            const auto& active = renderer.config();
            const bool enhanced = std::string_view{value} != "1";
            const bool high = std::string_view{value} == "3";
            require(
                active.hdr_enabled == enhanced && active.bloom_enabled == enhanced && active.ssao_enabled == enhanced
                    && active.taa_enabled == enhanced && active.ssr_enabled == high
                    && active.volumetrics_enabled == high && active.ssgi_quality == (high ? 2 : 0)
                    && active.reflection_quality == (high ? 2 : 0) && active.dof_quality == 0
                    && active.motion_blur_quality == 0 && active.render_scale == 1
                    && !active.dynamic_resolution_enabled,
                "Graphics preset did not apply the complete effect configuration"
            );
            require(
                active.descriptor_indexing == quality_integration,
                "Graphics preset discarded the requested optional binding path"
            );
            require(
                game.has_custom_ui() && !window.relative_mouse_mode() && renderer.camera().position() == camera_position
                    && renderer.player_diagnostics().position == initial.position,
                "Graphics preset disturbed the paused player"
            );
            require(
                dynamic_cast<Rml::ElementFormControl*>(control("graphics-preset"))->GetValue() == value,
                "Graphics preset selector did not reflect the active configuration"
            );
            click("save-settings");
            const auto saved = RelayGame::load_player_settings(user_path / "player_settings.toml", {});
            require(
                static_cast<int>(RelayGame::graphics_preset(saved)) == std::stoi(value) && saved.fov == 82
                    && std::abs(saved.volume - .35F) < .001F,
                "Graphics preset did not persist or changed player preferences"
            );
        }
        change("bloom", "1");
        change("ssao", "1");
        change("ssr", "1");
        change("volumetrics", "1");
        change("ssgi-quality", "2");
        change("reflection-quality", "1");
        change("dof-quality", "1");
        change("motion-blur-quality", "2");
        change("taa", "1");
        change("render-scale", "0.67");
        change("dynamic-resolution", "1");
        require(
            renderer.config().hdr_enabled && renderer.config().bloom_enabled && renderer.config().ssao_enabled
                && renderer.config().ssr_enabled && renderer.config().volumetrics_enabled
                && renderer.config().taa_enabled && renderer.config().render_scale == .67F
                && renderer.config().dynamic_resolution_enabled && renderer.config().ssgi_quality == 2
                && renderer.config().reflection_quality == 1 && renderer.config().dof_quality == 1
                && renderer.config().motion_blur_quality == 2,
            "RML effect controls did not apply to the renderer"
        );
        (void) draw("settings-adjusted");
        require(
            dynamic_cast<Rml::ElementFormControl*>(control("graphics-preset"))->GetValue() == "0",
            "Manual graphics edits did not select Custom"
        );
        control("menu-panel")->SetScrollTop(control("menu-panel")->GetScrollHeight());
        (void) draw("settings-effects");
        click("save-settings");
        const auto settings = RelayGame::load_player_settings(user_path / "player_settings.toml", {});
        require(
            settings.hdr_enabled && settings.bloom_enabled && settings.ssao_enabled && settings.ssr_enabled
                && settings.volumetrics_enabled && settings.taa_enabled && settings.render_scale == .67F
                && settings.dynamic_resolution_enabled && settings.ssgi_quality == 2 && settings.reflection_quality == 1
                && settings.dof_quality == 1 && settings.motion_blur_quality == 2,
            "RML effect settings were not persisted"
        );
        change("hdr", "0");
        require(
            !renderer.config().hdr_enabled && !renderer.config().bloom_enabled && !renderer.config().ssr_enabled
                && !renderer.config().taa_enabled && renderer.config().render_scale == 1
                && !renderer.config().dynamic_resolution_enabled && renderer.config().ssgi_quality == 0
                && renderer.config().reflection_quality == 0 && renderer.config().dof_quality == 0
                && renderer.config().motion_blur_quality == 0,
            "Selecting SDR retained HDR-only effects"
        );
        std::cout << "Saved settings: volume=" << settings.volume << " fov=" << settings.fov
                  << " sensitivity=" << settings.sensitivity << '\n';
        require(
            std::abs(settings.volume - .35F) < .001F && settings.fov == 82
                && std::abs(settings.sensitivity - .0025F) < .00001F,
            "RML settings were not persisted"
        );
        SDL_Event input{};
        input.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        input.button.button = SDL_BUTTON_LEFT;
        renderer.process_event(input);
        game.on_event(input, services);
        input.type = SDL_EVENT_KEY_DOWN;
        input.key.scancode = SDL_SCANCODE_W;
        input.key.key = SDLK_W;
        renderer.process_event(input);
        game.on_event(input, services);
        game.on_update(frame, services);
        require(
            renderer.player_diagnostics().shots_fired == initial.shots_fired
                && renderer.player_diagnostics().position == initial.position,
            "Modal player menu leaked gameplay actions"
        );
        input.type = SDL_EVENT_MOUSE_BUTTON_UP;
        renderer.process_event(input);
        game.on_event(input, services);
        key(SDL_SCANCODE_ESCAPE, SDLK_ESCAPE);
        (void) draw("");
        click("resume");
        require(
            window.relative_mouse_mode() && !renderer.player_ui().captures_input(),
            "Resume click leaked menu ownership"
        );
        game.on_update({}, services);
        require(renderer.player_diagnostics().shots_fired == initial.shots_fired, "Resume click fired a weapon");
        key(SDL_SCANCODE_ESCAPE, SDLK_ESCAPE);
        (void) draw("");
        click("to-menu");
        require(renderer.player_ui().captures_input(), "Save and menu lost UI focus");
        (void) draw("");
        click("continue");
        require(window.relative_mouse_mode(), "Continue did not start saved gameplay");
        game.transition_to({.destination = RelayCore::core::asset_path("levels/missing-rmlui-test.toml")}, services);
        (void) draw("transition-error");
        require(
            renderer.player_ui().captures_input() && !control("progress-error")->GetInnerRML().empty(),
            "Failed transition lost its player error"
        );
        game.on_shutdown(services);
        renderer.player_ui().set_screen(RelayCore::render::PlayerUiScreen::Finale);
        renderer.player_ui().set_text(
            "finale-text",
            "Питание и охлаждение в норме. Следующий маршрут: исследовательский блок."
        );
        renderer.begin_frame();
        renderer.finish_scene();
        require(renderer.begin_ui(frame, audio), "Finale UI unavailable");
        renderer.end_ui();
        save(renderer.capture_frame(), captures / "finale.bmp");
        renderer.player_ui().set_screen(RelayCore::render::PlayerUiScreen::Hud);
        renderer.player_ui().set_hud({.visible = true, .health = 100, .max_health = 100});
        RelayCore::render::HudPresentation combat_hud{.visible = true, .health = 100, .max_health = 100};
        auto* hud_document = Rml::GetContext("player")->GetDocument("hud");
        const auto hud_property = [&](const char* id, const char* property) {
            return hud_document->GetElementById(id)->GetLocalProperty(property)->Get<float>();
        };
        const auto fraction = [&](const char* id) {
            return hud_property(id, "width") / 100.0F;
        };
        const auto set_health = [&](float hp) {
            combat_hud.health = hp;
            renderer.player_ui().set_hud(combat_hud);
        };
        for (const float hp : {35.0F, 34.0F})
        {
            set_health(hp);
            require(
                hud_document->GetElementById("status")->IsClassSet("danger") == (hp < 35),
                "Health warning threshold changed"
            );
        }
        set_health(100);
        set_health(75);
        require(
            hud_document->GetElementById("health")->GetInnerRML().find("75") != Rml::String::npos,
            "Health text must update before animation"
        );
        require(fraction("health-main") == 1 && fraction("health-ghost") == 1, "Damage did not retain previous fill");
        renderer.player_ui().advance_hud(0.075);
        require(
            fraction("health-main") > 0.75F && fraction("health-main") < 1 && fraction("health-ghost") == 1,
            "Main drain or ghost hold failed"
        );
        set_health(50);
        set_health(20);
        renderer.player_ui().advance_hud(1);
        require(
            std::abs(fraction("health-main") - 0.2F) < 0.001F && std::abs(fraction("health-ghost") - 0.2F) < 0.001F
                && hud_property("health-flash", "opacity") == 0,
            "Rapid hits failed to settle"
        );
        for (const int fps : {30, 60, 144})
        {
            renderer.player_ui().reset_hud_feedback();
            set_health(100);
            set_health(25);
            for (int i = 0; i < fps; ++i)
            {
                renderer.player_ui().advance_hud(1.0 / fps);
                const float elapsed = float(i + 1) / float(fps);
                const float expected = std::max(0.25F, 1.0F - 0.75F * elapsed / 0.15F);
                require(
                    std::abs(fraction("health-main") - expected) < 0.001F
                        && fraction("health-ghost") >= fraction("health-main"),
                    "Health interpolation or layer ordering depends on frame rate"
                );
            }
            require(
                std::abs(fraction("health-main") - 0.25F) < 0.001F
                    && std::abs(fraction("health-ghost") - 0.25F) < 0.001F,
                "Health feedback depends on frame rate"
            );
        }
        set_health(80);
        require(
            fraction("health-main") == 0.8F && fraction("health-ghost") == 0.8F
                && hud_property("health-flash", "opacity") == 0,
            "Healing left stale damage feedback"
        );
        combat_hud.max_health = 200;
        set_health(80);
        require(fraction("health-main") == 0.4F, "Maximum health change did not snap");
        combat_hud.alive = false;
        set_health(0);
        require(fraction("health-main") == 0 && fraction("health-ghost") == 0, "Death did not empty health");
        combat_hud.alive = true;
        combat_hud.max_health = 100;
        set_health(100);
        set_health(50);
        renderer.player_ui().set_screen(RelayCore::render::PlayerUiScreen::Menu);
        renderer.player_ui().set_screen(RelayCore::render::PlayerUiScreen::Hud);
        set_health(50);
        require(
            fraction("health-main") == 0.5F && hud_property("health-flash", "opacity") == 0,
            "Pause/resume replayed damage"
        );
        renderer.player_ui().reset_hud_feedback();
        set_health(25);
        require(
            fraction("health-main") == 0.25F && hud_property("health-flash", "opacity") == 0,
            "Session restore replayed damage"
        );
        set_health(20);
        const float before_invalid_dt = fraction("health-main");
        renderer.player_ui().advance_hud(-1);
        renderer.player_ui().advance_hud(std::numeric_limits<double>::quiet_NaN());
        require(fraction("health-main") == before_invalid_dt, "Invalid dt advanced health feedback");
        renderer.player_ui().advance_hud(10);
        combat_hud.max_health = 0;
        set_health(20);
        require(
            hud_document->GetElementById("health-track")->GetLocalProperty("display")->ToString() == "none",
            "Invalid health scale is visible"
        );
        combat_hud.max_health = 100;
        set_health(100);
        set_health(100);
        set_health(50);
        renderer.begin_frame();
        const RelayCore::FrameInfo feedback_frame{.wall_delta_seconds = 0.075};
        require(renderer.begin_ui(feedback_frame, audio), "Health dt handoff unavailable");
        renderer.end_ui();
        require(
            std::abs(fraction("health-main") - 0.75F) < 0.001F && fraction("health-ghost") == 1,
            "Renderer must advance health exactly once after the snapshot without ImGui"
        );
        for (const auto size : {Rml::Vector2i{800, 600}, {1280, 720}, {2560, 1440}})
        {
            require(SDL_SetWindowSize(window.handle(), size.x, size.y), "Health HUD resize failed");
            SDL_PumpEvents();
            renderer.on_resize(size.x, size.y);
            for (const float hp : {100.0F, 50.0F, 25.0F, 0.0F})
            {
                renderer.player_ui().reset_hud_feedback();
                combat_hud.alive = hp > 0;
                set_health(hp);
                renderer.begin_frame();
                require(renderer.begin_ui(frame, audio), "Health HUD unavailable");
                renderer.end_ui();
                save(
                    renderer.capture_frame(),
                    captures / ("health-" + std::to_string(size.x) + "-" + std::to_string(int(hp)) + ".bmp")
                );
            }
        }
        require(SDL_SetWindowSize(window.handle(), 1280, 720), "Restore health HUD size failed");
        SDL_PumpEvents();
        renderer.on_resize(1280, 720);
        combat_hud.alive = true;
        set_health(100);
        set_health(50);
        renderer.player_ui().advance_hud(0.04);
        renderer.begin_frame();
        require(renderer.begin_ui(frame, audio), "Health feedback capture unavailable");
        renderer.end_ui();
        save(renderer.capture_frame(), captures / "health-hit.bmp");
        renderer.player_ui().advance_hud(0.18);
        renderer.begin_frame();
        require(renderer.begin_ui(frame, audio), "Health ghost capture unavailable");
        renderer.end_ui();
        save(renderer.capture_frame(), captures / "health-ghost.bmp");
        set_health(100);
        combat_hud.world_markers = {{50, 40, .58F, 1, true, false, "35 / 60"}, {50, 34, 0, 1, false, true, "−25"}};
        renderer.player_ui().set_hud(combat_hud);
        renderer.begin_frame();
        require(renderer.begin_ui(frame, audio), "Combat HUD unavailable");
        renderer.end_ui();
        save(renderer.capture_frame(), captures / "enemy-health-damage.bmp");
        combat_hud.world_markers.clear();
        combat_hud.hurt = 1;
        combat_hud.damage_alert = 1;
        for (const float angle : {0.0F, 90.0F, 180.0F, -90.0F})
        {
            combat_hud.damage_direction_degrees = angle;
            renderer.player_ui().set_hud(combat_hud);
            renderer.begin_frame();
            require(renderer.begin_ui(frame, audio), "Damage direction HUD unavailable");
            renderer.end_ui();
            save(renderer.capture_frame(), captures / ("damage-direction-" + std::to_string(int(angle)) + ".bmp"));
        }
        renderer.player_ui().set_hud({.visible = true, .health = 100, .max_health = 100});
        std::array<uint8_t, 3> reference{};
        int mode_index = 0;
        for (const auto mode :
             {RelayCore::render::ColorPipelineMode::legacy_gamma,
              RelayCore::render::ColorPipelineMode::hardware_srgb,
              RelayCore::render::ColorPipelineMode::hardware_srgb})
        {
            auto display = renderer.config();
            display.hdr_enabled = mode_index == 2;
            display.exposure_ev = 3;
            renderer.commit_live_content_reload(
                renderer.prepare_live_content_reload(display, renderer.authoring_config(), mode)
            );
            renderer.begin_frame();
            require(renderer.begin_ui(frame, audio), "UI color composition unavailable");
            renderer.end_ui();
            const auto image = renderer.capture_frame();
            const size_t center = size_t(image.height / 2 * image.width + image.width / 2) * 4;
            const std::array<uint8_t, 3> pixel{
                image.pixels[center],
                image.pixels[center + 1],
                image.pixels[center + 2]
            };
            require(
                pixel == std::array<uint8_t, 3>{210, 213, 196},
                "RmlUi color transfer altered an opaque reticle pixel"
            );
            if (mode_index == 0)
                reference = pixel;
            else
                require(pixel == reference, "SDR/HDR/exposure changed opaque RmlUi color");
            save(image, captures / ("color-" + std::to_string(mode_index++) + ".bmp"));
        }
        std::cout << "Player UI: ImGui disabled, four sizes, SDL mouse/keyboard, "
                     "new/continue/pause/settings/save/error/cleanup passed\n";
    } catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    RelayCore::log::shutdown();
    SDL_Quit();
    return result;
}

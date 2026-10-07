#include "relaycore/app/engine_services.h"
#include "relaycore/app/frame_timer.h"
#include "relaycore/audio/audio_system.h"
#include "relaycore/core/engine_config.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/core/profiling.h"
#include "relaycore/gameplay/combat_runtime.h"
#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/renderer.h"
#include "relaycore/render/shadow_math.h"
#include "relaycore/world/blockout_loader.h"
#include "relaygame/demo_game.h"
#include "relaygame/enemy_runtime.h"
#include "relaygame/game_version.h"
#include "relaygame/level_editor.h"
#include "relaygame/level_session_builder.h"
#include "relaygame/usable_interaction.h"
#include "relaygame/world_irradiance.h"
#include "relaygame/world_scene_renderer.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <glm/ext/matrix_transform.hpp>
#include <imgui.h>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "scoped_temp_directory.h"

namespace
{
    auto read_rgb(RelayCore::Renderer& renderer) -> std::vector<unsigned char>
    {
        const auto image = renderer.capture_frame();
        std::vector<unsigned char> rgb(size_t(image.width) * image.height * 3);
        for (size_t i = 0; i < rgb.size() / 3; ++i)
            std::copy_n(image.pixels.data() + i * 4, 3, rgb.data() + i * 3);
        return rgb;
    }

    constexpr int width = 1280;
    constexpr int height = 720;

    auto capture(
        RelayCore::Renderer& renderer,
        const std::filesystem::path& path,
        const int capture_width = width,
        const int capture_height = height
    ) -> double
    {
        auto pixels = read_rgb(renderer);
        double sum = 0.0;
        for (const auto value : pixels)
        {
            sum += value;
        }
        const std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface{
            SDL_CreateSurfaceFrom(
                capture_width,
                capture_height,
                SDL_PIXELFORMAT_RGB24,
                pixels.data(),
                capture_width * 3
            ),
            SDL_DestroySurface
        };
        if (!surface || !SDL_SaveBMP(surface.get(), path.string().c_str()))
        {
            throw std::runtime_error("Could not save power restoration capture: " + std::string{SDL_GetError()});
        }
        return sum / static_cast<double>(pixels.size());
    }
} // namespace

int main(int argc, char** argv)
{
    const bool profile_views = argc > 1 && std::string_view{argv[1]} == "--profile-views";
    const bool art_review = argc > 1 && std::string_view{argv[1]}.starts_with("--art-review");
    const bool art_legacy = argc > 1 && std::string_view{argv[1]} == "--art-review-legacy";
    const auto arguments = std::span{argv + 1, static_cast<size_t>(argc - 1)};
    const bool no_shadows = std::ranges::any_of(arguments, [](const char* value) {
        return std::string_view{value} == "--no-shadows";
    });
    const bool hdr = (art_review && !art_legacy) || (argc > 1 && std::string_view{argv[1]} == "--hdr");
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr << SDL_GetError() << '\n';
        return 77;
    }
    int result = 0;
    RelayCore::log::init({
        .logs_directory = (std::filesystem::current_path() / "power-captures/logs").string(),
        .enable_timestamped_log = false,
    });
    try
    {
        RelayCore::test::ScopedTempDirectory hdr_data;
        auto config_path = RelayCore::core::asset_path(
            art_review && !art_legacy ? "config/occult_lab_b17.toml" : "config/engine.toml"
        );
        if (hdr && !art_review)
        {
            std::ifstream source(config_path);
            std::string contents{std::istreambuf_iterator<char>{source}, {}};
            contents.insert(contents.find("[render]") + 8, "\nhdr_enabled = true\nexposure_ev = 0\n");
            config_path = hdr_data.path() / "hdr-engine.toml";
            std::ofstream(config_path) << contents;
        }
        auto config = RelayCore::core::load_engine_config(config_path);
        config.window.width = width;
        config.window.height = height;
        config.window.high_dpi = false;
        config.window.vsync = false;
        config.renderer.imgui.enabled = true;
        if (no_shadows || (argc > 1 && std::string_view{argv[1]} == "--no-directional-shadows"))
            config.renderer.shadow_enabled = false;
        RelayCore::Window window{config.window};
        SDL_HideWindow(window.handle());
        RelayCore::Renderer renderer{
            config.renderer,
            hdr ? RelayCore::render::ColorPipelineMode::hardware_srgb
                : RelayCore::render::ColorPipelineMode::legacy_gamma
        };
        renderer.initialize(window);
        renderer.on_resize(width, height);
        if (art_review)
        {
            const auto review_directory = std::filesystem::path{"power-captures"} / (art_legacy ? "legacy" : "hdr")
                                          / (no_shadows ? "no-shadows" : "shadows");
            std::filesystem::create_directories(review_directory);
            struct View {
                const char* map;
                const char* image;
                glm::vec3 eye;
                glm::vec3 target;
                float fov = 60;
            };
            const std::array views{
                View{"occult_lab_b17", "b17-entrance.bmp", {0, 1.65F, -8}, {0, 1.65F, -4}},
                View{"occult_lab_b17", "b17-laboratory.bmp", {-.7F, 1.65F, -3.4F}, {2, 1.4F, 1.2F}},
                View{"occult_lab_b17", "b17-observation.bmp", {-3.1F, 1.65F, -1.8F}, {-4.1F, 1.2F, 3.8F}},
                View{"occult_lab_b17", "b17-instruments.bmp", {2, 1.65F, 2.9F}, {4.25F, 1.3F, 4.3F}},
                View{"occult_lab_b17", "b17-sample-bench.bmp", {4.5F, 1.55F, -2.15F}, {4.5F, 1.2F, -3.55F}},
                View{"power_restoration", "revamp-power.bmp", {0, 1.65F, -3}, {2, 1.3F, 2}},
                View{"power_restoration", "fix-ceiling.bmp", {-2.8F, 1.65F, -2.4F}, {-2.8F, 3.6F, -.7F}},
                View{"power_restoration", "fix-pipe.bmp", {3, 1.65F, 2.8F}, {4.65F, 2.85F, 2.8F}},
                View{"power_restoration", "fix-turret.bmp", {3.8F, 1.65F, 7.3F}, {2.2F, 1.65F, 7.3F}},
                View{"occult_lab_b17", "fix-scientist.bmp", {-1.0F, 1.65F, 2.9F}, {-3, 1.2F, 3.8F}},
                View{"occult_lab_b17", "scientist-front.bmp", {-2.13F, .95F, 3.39F}, {-3, .95F, 3.8F}, 100},
                View{"occult_lab_b17", "scientist-side.bmp", {-3.5F, 1.3F, 2.1F}, {-3, .95F, 3.8F}},
                View{"bench_relay", "revamp-bench.bmp", {0, 1.65F, -3}, {-2.5F, 1.3F, -1}},
                View{"bench_relay", "fix-wreck.bmp", {0, 1.45F, 5.5F}, {0, .35F, 7}},
                View{"bench_relay", "wreck-side.bmp", {1.0F, .6F, 6.15F}, {0, .25F, 7}},
                View{"pressure_transfer", "revamp-transfer.bmp", {4, 1.65F, 1.3F}, {2, 1.2F, 4}},
            };
            for (const auto& view : views)
            {
                const auto start = std::chrono::steady_clock::now();
                const auto session = RelayGame::LevelSessionBuilder{config.gameplay}.build(
                    RelayCore::core::asset_path(std::string{"levels/"} + view.map + ".toml")
                );
                const auto loaded = std::chrono::steady_clock::now();
                const auto prepared = RelayGame::build_prepared_world_scene_data(
                    session.level,
                    renderer.authoring_config(),
                    renderer.color_pipeline_mode()
                );
                const auto prepared_at = std::chrono::steady_clock::now();
                RelayGame::WorldSceneRenderer scene;
                scene.initialize(prepared, renderer.static_models());
                const auto initialized = std::chrono::steady_clock::now();
                const auto milliseconds = [](auto duration) {
                    return std::chrono::duration<double, std::milli>(duration).count();
                };
                std::cout << view.image << " load_ms=" << milliseconds(loaded - start)
                          << " prepare_ms=" << milliseconds(prepared_at - loaded)
                          << " initialize_ms=" << milliseconds(initialized - prepared_at) << '\n';
                renderer.camera().set_position(view.eye);
                renderer.camera().set_target(view.target);
                renderer.camera().apply_projection_config({.vertical_fov_radians = glm::radians(view.fov)});
                renderer.begin_frame();
                auto enemies = RelayGame::build_enemies(session.level);
                if (std::string_view{view.image} == "fix-wreck.bmp" || std::string_view{view.image} == "wreck-side.bmp")
                    for (auto& enemy : enemies)
                    {
                        enemy.state.mode = RelayGame::EnemyMode::Dead;
                        enemy.state.health = 0;
                        enemy.state.position = {0, .01F, 7};
                    }
                scene.render(
                    renderer,
                    renderer.camera(),
                    0,
                    session.doors,
                    session.combat.turrets,
                    session.usables,
                    -1,
                    {},
                    false,
                    {},
                    {},
                    {},
                    enemies
                );
                (void) capture(renderer, review_directory / view.image);
                scene.shutdown();
            }
            return 0;
        }
        if (config.renderer.shadow_enabled)
        {
            RelayCore::render::StaticColorMesh box;
            box.initialize(RelayCore::render::make_color_box_mesh_data());
            const RelayCore::render::RenderItem floor{
                .mesh = &box,
                .model_matrix = glm::translate(glm::mat4{1.0F}, {0.0F, -0.1F, 0.0F})
                                * glm::scale(glm::mat4{1.0F}, {6.0F, 0.2F, 6.0F})
            };
            const RelayCore::render::RenderItem occluder{
                .mesh = &box,
                .model_matrix = glm::translate(glm::mat4{1.0F}, {0.0F, 0.5F, 0.0F})
                                * glm::scale(glm::mat4{1.0F}, {0.5F, 1.0F, 0.5F})
            };
            const std::array floor_only{floor};
            const std::array with_occluder{floor, occluder};
            const std::array lamp{RelayCore::render::LocalLight{
                .position = {-1.0F, 2.0F, 0.0F},
                .color = {2.0F, 2.0F, 2.0F},
                .radius = 5.0F
            }};
            renderer.set_local_lights(lamp);
            renderer.camera().set_position({2.0F, 4.0F, 5.0F});
            renderer.camera().set_target({0.8F, 0.0F, 0.0F});
            const glm::vec4 clip = renderer.camera().view_projection_matrix() * glm::vec4{0.8F, 0.0F, 0.0F, 1.0F};
            const int pixel_x = static_cast<int>((clip.x / clip.w * 0.5F + 0.5F) * width);
            const int pixel_y = static_cast<int>((clip.y / clip.w * 0.5F + 0.5F) * height);
            const auto floor_brightness = [&](const std::span<const RelayCore::render::RenderItem> casters) {
                renderer.begin_frame();
                renderer.draw_render_queue(floor_only, casters);
                const auto image = renderer.capture_frame();
                const auto* pixel = image.pixels.data() + (size_t(height - 1 - pixel_y) * width + pixel_x) * 4;
                return static_cast<int>(pixel[0]) + pixel[1] + pixel[2];
            };
            const int lit = floor_brightness(floor_only);
            const int shadowed = floor_brightness(with_occluder);
            if (shadowed >= lit - 15)
                throw std::runtime_error("Point light occluder did not darken the floor");
            renderer.set_local_lights({});
        }
        const auto level = RelayCore::load_blockout_level(RelayCore::core::asset_path("levels/power_restoration.toml"));
        RelayGame::WorldSceneRenderer scene;
        scene.initialize(
            RelayGame::build_prepared_world_scene_data(
                level,
                renderer.authoring_config(),
                renderer.color_pipeline_mode()
            ),
            renderer.static_models()
        );
        std::vector<RelayCore::gameplay::RuntimeDoor> doors;
        std::vector<RelayCore::gameplay::RuntimeTrigger> triggers;
        std::vector<RelayCore::gameplay::RuntimeUsable> usables;
        RelayCore::gameplay::build_runtime_interactions(level, doors, triggers, usables);
        const auto combat = RelayCore::gameplay::build_runtime_combat(level);
        const auto reserve_enemies = RelayGame::build_enemies(level);
        std::vector<std::string> inventory, activated, signaled;
        const auto use = [&](const std::string_view name) {
            const auto found = std::ranges::find(usables, name, &RelayCore::gameplay::RuntimeUsable::name);
            if (found == usables.end())
            {
                throw std::runtime_error("Missing episode interaction");
            }
            return RelayGame::apply_usable_interaction(*found, doors, usables, inventory, activated, signaled);
        };
        std::filesystem::create_directories("power-captures");
        RelayGame::ScientistPose scientist;
        auto environment = RelayGame::build_world_environment(level);
        RelayGame::update_world_environment(environment, activated, 2.0F);
        int profile_view_index = 0;
        const auto render = [&](const char* name,
                                const glm::vec3 position,
                                const glm::vec3 target,
                                const std::span<const RelayCore::gameplay::RuntimeTurret> scene_turrets = {},
                                const RelayCore::gameplay::PlayerCombatState* player_combat = nullptr,
                                const std::span<const RelayGame::RuntimeEnemy> scene_enemies = {}) {
            renderer.camera().set_position(position);
            renderer.camera().set_target(target);
            renderer.begin_frame();
            scene.render(
                renderer,
                renderer.camera(),
                0.0F,
                doors,
                scene_turrets,
                usables,
                -1,
                {},
                false,
                activated,
                scientist,
                environment,
                scene_enemies,
                player_combat
            );
            const std::array<std::string_view, 6> profile_names{
                "intake-before.bmp",
                "maintenance-open.bmp",
                "intake-after.bmp",
                "filter-turret.bmp",
                "pump-after.bmp",
                "return-threat.bmp"
            };
            if (profile_views && std::ranges::find(profile_names, name) != profile_names.end())
            {
                std::vector<double> submit_ms, completed_ms;
                renderer.finish_scene();
                for (int frame = 0; frame < 200; ++frame)
                {
                    RELAYCORE_PROFILE_ZONE_N("Playtest::view_frame");
                    const auto offset = glm::vec3{0.05F * std::sin(frame * 0.05F), 0, 0};
                    renderer.camera().set_position(position + offset);
                    renderer.camera().set_target(target + offset);
                    const auto start = std::chrono::steady_clock::now();
                    renderer.begin_frame();
                    scene.render(
                        renderer,
                        renderer.camera(),
                        frame / 60.0F,
                        doors,
                        scene_turrets,
                        usables,
                        -1,
                        {},
                        false,
                        activated,
                        scientist,
                        environment,
                        scene_enemies,
                        player_combat
                    );
                    const auto submitted = std::chrono::steady_clock::now();
                    renderer.finish_scene();
                    const auto completed = std::chrono::steady_clock::now();
                    const double cpu = std::chrono::duration<double, std::milli>(submitted - start).count();
                    const double full = std::chrono::duration<double, std::milli>(completed - start).count();
                    if (frame >= 20)
                    {
                        submit_ms.push_back(cpu);
                        completed_ms.push_back(full);
                        RELAYCORE_PROFILE_PLOT("view_index", profile_view_index);
                        RELAYCORE_PROFILE_PLOT("view_submit_ms", cpu);
                        RELAYCORE_PROFILE_PLOT("view_completed_ms", full);
                    }
                    RELAYCORE_PROFILE_FRAME();
                }
                std::ranges::sort(submit_ms);
                std::ranges::sort(completed_ms);
                std::cout << "profile_view=" << profile_view_index++ << "; name=" << name
                          << "; samples=" << completed_ms.size() << "; submit_p50_ms=" << submit_ms[90]
                          << "; submit_p95_ms=" << submit_ms[171] << "; completed_p50_ms=" << completed_ms[90]
                          << "; completed_p95_ms=" << completed_ms[171] << "; completed_max_ms=" << completed_ms.back()
                          << "; local_lights=" << scene.diagnostics().local_light_count << '\n';
                renderer.camera().set_position(position);
                renderer.camera().set_target(target);
                renderer.begin_frame();
                scene.render(
                    renderer,
                    renderer.camera(),
                    0,
                    doors,
                    scene_turrets,
                    usables,
                    -1,
                    {},
                    false,
                    activated,
                    scientist,
                    environment,
                    scene_enemies,
                    player_combat
                );
            }
            return capture(renderer, std::filesystem::path{"power-captures"} / name);
        };
        if (config.renderer.shadow_enabled && config.renderer.shadow_light_intensity > 0.0F)
        {
            RelayGame::PreparedWorldSceneData caster_data{};
            const auto add_chunk = [&](const glm::vec3 center, const bool transparent) {
                auto mesh = RelayCore::render::make_color_box_mesh_data();
                for (auto& vertex : mesh.vertices)
                    vertex.position += center;
                caster_data.blockout.chunks.push_back(
                    {.mesh = std::move(mesh),
                     .mins = center - glm::vec3{0.5F},
                     .maxs = center + glm::vec3{0.5F},
                     .triangle_count = 12,
                     .transparent = transparent}
                );
            };
            add_chunk({0.0F, 0.0F, 5.0F}, true);
            add_chunk({8.0F, 0.0F, 5.0F}, false);
            RelayGame::WorldSceneRenderer caster_scene;
            caster_scene.initialize(caster_data, renderer.static_models());
            renderer.camera().set_position({0.0F, 0.0F, 0.0F});
            renderer.camera().set_target({0.0F, 0.0F, 1.0F});
            renderer.begin_frame();
            caster_scene.render(renderer, renderer.camera(), 0.0F, {}, {}, {}, -1, {}, false);
            if (caster_scene.diagnostics().culled_chunks != 1
                || caster_scene.diagnostics().transparent_visible_chunks != 1)
                throw std::runtime_error("Off-camera shadow fixture has unexpected camera visibility");
            std::vector<float> shadow_depth;
            const auto nearest_shadow_depth = [&] {
                shadow_depth = renderer.capture_shadow_depth(0);
                return std::ranges::min(shadow_depth);
            };
            if (nearest_shadow_depth() >= 1.0F)
                throw std::runtime_error("Off-camera opaque chunk did not reach the shadow map");
            caster_scene.shutdown();

            RelayGame::WorldSceneRenderer dynamic_scene;
            dynamic_scene.initialize({.color_pipeline_mode = renderer.color_pipeline_mode()}, renderer.static_models());
            const auto render_dynamic = [&](std::span<const RelayCore::gameplay::RuntimeDoor> doors = {},
                                            std::span<const RelayCore::gameplay::RuntimeTurret> turrets = {},
                                            std::span<const RelayCore::gameplay::RuntimeUsable> usables = {}) {
                renderer.begin_frame();
                dynamic_scene.render(renderer, renderer.camera(), 0.0F, doors, turrets, usables, -1, {}, false);
                return nearest_shadow_depth();
            };
            if (render_dynamic() < 0.999F)
                throw std::runtime_error("Shadow map retained a caster after an empty frame");

            RelayCore::gameplay::RuntimeDoor door{};
            door.closed_bounds = {{7.5F, -0.5F, 4.5F}, {8.5F, 0.5F, 5.5F}};
            const std::array off_camera_doors{door};
            if (render_dynamic(off_camera_doors) >= 1.0F || dynamic_scene.diagnostics().door_draw_calls != 0)
                throw std::runtime_error("Off-camera door did not reach the shadow map");
            const auto closed_door_shadow = shadow_depth;
            auto open_off_camera_doors = off_camera_doors;
            open_off_camera_doors[0].open_fraction = 1.0F;
            if (render_dynamic(open_off_camera_doors) >= 1.0F || shadow_depth == closed_door_shadow)
                throw std::runtime_error("Off-camera door shadow did not move with the door");

            RelayCore::gameplay::RuntimeTurret turret{};
            turret.bounds = door.closed_bounds;
            const std::array turrets{turret};
            if (render_dynamic({}, turrets) >= 1.0F || dynamic_scene.diagnostics().opaque_draw_calls != 0)
                throw std::runtime_error("Off-camera turret did not reach the shadow map");

            RelayCore::gameplay::RuntimeUsable pickup{};
            pickup.visual_bounds = door.closed_bounds;
            pickup.has_visual_bounds = true;
            pickup.visual_kind = RelayCore::gameplay::UsableVisualKind::Pickup;
            const std::array pickups{pickup};
            if (render_dynamic({}, {}, pickups) >= 1.0F || dynamic_scene.diagnostics().usable_draw_calls != 0)
                throw std::runtime_error("Off-camera pickup did not reach the shadow map");

            pickup.visual_kind = RelayCore::gameplay::UsableVisualKind::Panel;
            const std::array panels{pickup};
            if (render_dynamic({}, {}, panels) >= 1.0F || dynamic_scene.diagnostics().usable_draw_calls != 0)
                throw std::runtime_error("Off-camera panel did not reach the shadow map");
            dynamic_scene.shutdown();
        }
        {
            RelayCore::BlockoutLevel glass_level{};
            RelayCore::BlockoutSolid glass{};
            glass.mins = {-1.0F, 0.0F, 4.98F};
            glass.maxs = {1.0F, 3.0F, 5.02F};
            glass.material = RelayCore::SurfaceMaterial::Glass;
            glass_level.solids.push_back(glass);
            RelayGame::WorldSceneRenderer glass_scene;
            glass_scene.initialize(
                RelayGame::build_prepared_world_scene_data(
                    glass_level,
                    renderer.authoring_config(),
                    renderer.color_pipeline_mode()
                ),
                renderer.static_models()
            );
            RelayGame::EnvironmentEffect steam{};
            steam.kind = RelayGame::EnvironmentKind::Steam;
            steam.origin = {0.0F, 1.0F, 6.0F};
            steam.scale = 2.0F;
            steam.strength = 1.0F;
            steam.age = 0.4F;
            const std::array effects{steam};
            const auto render_glass_and_steam = [&](const char* name, const glm::vec3 eye, const glm::vec3 target) {
                renderer.camera().set_position(eye);
                renderer.camera().set_target(target);
                renderer.begin_frame();
                glass_scene.render(renderer, renderer.camera(), 0.0F, {}, {}, {}, -1, {}, false, {}, {}, effects);
                if (glass_scene.diagnostics().transparent_draw_calls != 13)
                    throw std::runtime_error("Glass and steam did not enter the transparent pass together");
                (void) capture(renderer, std::filesystem::path{"power-captures"} / name);
            };
            render_glass_and_steam("glass-steam-back.bmp", {0.0F, 1.5F, 0.0F}, {0.0F, 1.5F, 7.0F});
            render_glass_and_steam("glass-steam-front.bmp", {0.0F, 1.5F, 10.0F}, {0.0F, 1.5F, 3.0F});
            glass_scene.shutdown();

            glass_level.solids[0].mins.x = -5.0F;
            glass_level.solids[0].maxs.x = 5.0F;
            RelayGame::WorldSceneRenderer side_glass_scene;
            side_glass_scene.initialize(
                RelayGame::build_prepared_world_scene_data(
                    glass_level,
                    renderer.authoring_config(),
                    renderer.color_pipeline_mode()
                ),
                renderer.static_models()
            );
            RelayGame::WorldSceneRenderer side_steam_scene;
            side_steam_scene.initialize(
                {.color_pipeline_mode = renderer.color_pipeline_mode()},
                renderer.static_models()
            );
            steam.origin = {3.5F, 1.0F, 3.5F};
            const std::array side_effects{steam};
            renderer.camera().set_position({0.0F, 1.5F, 0.0F});
            renderer.camera().set_target({0.0F, 1.5F, 7.0F});
            const auto pixels = [&] {
                auto result = read_rgb(renderer);
                return result;
            };
            const auto render_side_glass = [&] {
                side_glass_scene.render(renderer, renderer.camera(), 0.0F, {}, {}, {}, -1, {}, false);
            };
            const auto render_side_steam = [&] {
                side_steam_scene
                    .render(renderer, renderer.camera(), 0.0F, {}, {}, {}, -1, {}, false, {}, {}, side_effects);
            };
            renderer.begin_frame();
            side_glass_scene.render(renderer, renderer.camera(), 0.0F, {}, {}, {}, -1, {}, false, {}, {}, side_effects);
            if (side_glass_scene.diagnostics().transparent_draw_calls != 13)
                throw std::runtime_error("Side-view glass and steam did not enter the transparent pass together");
            const auto sorted_pixels = pixels();
            (void) capture(renderer, "power-captures/glass-steam-side.bmp");

            renderer.begin_frame();
            render_side_glass();
            render_side_steam();
            const auto expected_pixels = pixels();

            renderer.begin_frame();
            render_side_steam();
            render_side_glass();
            const auto wrong_order_pixels = pixels();
            if (sorted_pixels != expected_pixels || sorted_pixels == wrong_order_pixels)
                throw std::runtime_error("Side-view glass and steam blend in the wrong order");
            side_steam_scene.shutdown();
            side_glass_scene.shutdown();
        }
        const auto before = render("intake-before.bmp", {0.0F, 1.65F, -3.5F}, {0.0F, 1.65F, 5.0F});
        (void) render("spawn-before.bmp", {-2.0F, 1.65F, -3.5F}, {-2.0F, 1.65F, 5.0F});
        if (scene.diagnostics().local_light_count != 8)
        {
            throw std::runtime_error("Unpowered sector must use emergency lights only");
        }
        (void) render("corridor-cap.bmp", {12.5F, 1.65F, 1.0F}, {12.5F, 1.65F, -1.4F});
        (void) render("button-locked.bmp", {2.35F, 1.65F, 3.9F}, {2.1F, 1.4F, 4.76F});
        (void) render("service-before.bmp", {12.5F, 1.65F, 1.0F}, {12.5F, 1.65F, 17.0F});
        (void) render("switch-room-before.bmp", {12.5F, 1.65F, 16.0F}, {14.3F, 1.5F, 22.0F});
        if (config.renderer.shadow_enabled)
        {
            const auto shadow_projection = [&] {
                const auto& rc = config.renderer;
                const auto matrix = RelayCore::render::shadow_light_view_projection(
                    renderer.camera(),
                    rc,
                    rc.shadow_focus_distance,
                    rc.shadow_projection_radius,
                    rc.shadow_projection_depth
                );
                return matrix;
            };
            // These two viewpoints stay within one shadow texel with the shipped cascade settings.
            (void) render("switch-room-shadow-anchor.bmp", {12.51F, 1.65F, 16.0F}, {14.31F, 1.5F, 22.0F});
            const auto before_move = shadow_projection();
            (void) render("switch-room-small-move.bmp", {12.509F, 1.65F, 16.0F}, {14.309F, 1.5F, 22.0F});
            const auto after_move = shadow_projection();
            for (int column = 0; column < 4; ++column)
                for (int row = 0; row < 2; ++row)
                    if (std::abs(before_move[column][row] - after_move[column][row]) > 0.000001F)
                        throw std::runtime_error("Switch-room shadows move under sub-texel camera translation");
        }
        (void) render("switch-room-close-left.bmp", {12.0F, 1.65F, 19.0F}, {14.3F, 1.5F, 22.0F});
        (void) render("switch-room-close-right.bmp", {15.5F, 1.65F, 19.0F}, {14.3F, 1.5F, 22.0F});
        (void) render("pump-before.bmp", {10.0F, 1.65F, 8.0F}, {6.5F, 2.5F, 8.6F});
        (void) render("pump-fan-mount.bmp", {8.0F, 1.65F, 8.5F}, {10.0F, 2.55F, 10.77F});
        RelayGame::update_world_environment(environment, activated, 1.25F);
        (void) render("sparks.bmp", {13.2F, 1.65F, 20.0F}, {14.05F, 1.68F, 21.60F});
        (void) render("service-wing-entrance.bmp", {12.5F, 1.65F, 20.0F}, {13.0F, 1.65F, 27.0F});
        (void) render("service-floor-edge.bmp", {13.0F, 1.65F, 27.0F}, {13.0F, 0.9F, 31.0F});
        (void) render("intake-corridor-edge.bmp", {9.72F, 1.65F, -0.99F}, {11.5F, 0.9F, 0.0F});
        (void) render("maintenance-before.bmp", {13.0F, 1.65F, 33.0F}, {15.5F, 1.4F, 38.85F});
        if (!use("spare_fuse").primary_applied || !use("maintenance_isolator").primary_applied)
            throw std::runtime_error("Maintenance isolator interaction failed");
        static_cast<void>(RelayCore::gameplay::update_doors(5, {}, doors));
        (void
        ) render("maintenance-open.bmp", {15.5F, 1.65F, 34.0F}, {21.5F, 1.0F, 34.0F}, {}, nullptr, reserve_enemies);
        auto destroyed_enemies = reserve_enemies;
        auto winding_enemies = reserve_enemies;
        winding_enemies.front().state.mode = RelayGame::EnemyMode::Windup;
        winding_enemies.front().state.timer = .225F;
        (void) render("maintenance-windup.bmp", {19.5F, 1.65F, 34}, {21.5F, .5F, 34}, {}, nullptr, winding_enemies);
        destroyed_enemies.front().state.mode = RelayGame::EnemyMode::Dead;
        (void
        ) render("maintenance-wreck.bmp", {15.5F, 1.65F, 34.0F}, {21.5F, 0.5F, 34.0F}, {}, nullptr, destroyed_enemies);
        (void) render("reserve-bus.bmp", {22.0F, 1.65F, 34.0F}, {24.85F, 1.5F, 34.0F});
        if (!use("bus_relay").chain_applied || !use("power_breaker").chain_applied)
        {
            throw std::runtime_error("Power restoration interaction failed");
        }
        RelayGame::update_world_environment(environment, activated, 0);
        RelayGame::update_world_environment(environment, activated, 2);
        const auto after = render("intake-after.bmp", {0.0F, 1.65F, -3.5F}, {0.0F, 1.65F, 5.0F});
        (void) render("spawn-after.bmp", {-2.0F, 1.65F, -3.5F}, {-2.0F, 1.65F, 5.0F});
        if (scene.diagnostics().local_light_count != 8 || after < before + 1.0)
        {
            throw std::runtime_error("Restoring power must visibly illuminate the sector");
        }
        (void) render("switch-room-after.bmp", {12.5F, 1.65F, 16.0F}, {14.3F, 1.5F, 22.0F});
        (void) render("filter-passage.bmp", {7.8F, 1.65F, 13.0F}, {7.8F, 1.65F, 22.0F});
        (void) render("filter-turret.bmp", {7.5F, 1.65F, 26.0F}, {7.5F, 1.55F, 30.7F}, combat.turrets);
        (void) render("filter-panel.bmp", {3.4F, 1.65F, 28.3F}, {3.0F, 1.45F, 30.62F}, combat.turrets);
        if (!use("filter_purge").chain_applied)
            throw std::runtime_error("Filter purge must release the cooling control");
        if (!use("cooling_control").chain_applied)
        {
            throw std::runtime_error("Cooling must release the airlock interlock");
        }
        (void) render("pump-room.bmp", {10.0F, 1.65F, 8.0F}, {6.5F, 1.3F, 7.0F});
        RelayGame::update_world_environment(environment, activated, 0);
        for (int i = 0; i < 360; ++i)
            RelayGame::update_world_environment(environment, activated, 1.0F / 60);
        for (const auto& effect : environment)
        {
            if (effect.kind == RelayGame::EnvironmentKind::Fan && effect.strength < 0.97F)
                throw std::runtime_error("Cooling must spin up ventilation");
            if (effect.kind == RelayGame::EnvironmentKind::Steam && effect.strength > 0.01F)
                throw std::runtime_error("Cooling must suppress the steam leak");
            if (effect.kind == RelayGame::EnvironmentKind::Sparks && effect.strength != 0)
                throw std::runtime_error("Repairing the breaker must stop electrical discharge");
        }
        (void) render("pump-after.bmp", {10.0F, 1.65F, 8.0F}, {6.5F, 2.5F, 8.6F});
        static_cast<void>(RelayCore::gameplay::update_doors(5, {}, doors));
        (void) render("return-hatch-open.bmp", {7, 1.65F, 7.8F}, {3.5F, 1.65F, 8});
        (void) render("cooling-panel-route.bmp", {8.5F, 1.65F, 8}, {6.2F, 1.5F, 6.9F});
        (void) render("return-threat.bmp", {7, 1.65F, 8.5F}, {2.48F, 1.55F, 8.5F}, combat.turrets);
        auto winding_turrets = combat.turrets;
        winding_turrets[0].warming_up = true;
        winding_turrets[0].warmup_remaining_seconds = 0.3F;
        (void) render("return-threat-windup.bmp", {7, 1.65F, 8.5F}, {2.48F, 1.55F, 8.5F}, winding_turrets);
        winding_turrets[0].warmup_remaining_seconds = 0.2F;
        (void) render("return-threat-windup-off.bmp", {7, 1.65F, 8.5F}, {2.48F, 1.55F, 8.5F}, winding_turrets);
        (void) render("return-threat-side.bmp", {4.5F, 1.65F, 7.0F}, {2.48F, 1.55F, 8.5F}, combat.turrets);
        auto destroyed_turrets = combat.turrets;
        destroyed_turrets[0].destroyed = true;
        destroyed_turrets[0].hit_flash_remaining_seconds = 0.18F;
        (void) render("return-threat-destroyed.bmp", {4.5F, 1.65F, 7.0F}, {2.48F, 1.55F, 8.5F}, destroyed_turrets);
        auto firing_turrets = combat.turrets;
        firing_turrets[0].shot_trace = {{2.85F, 1.55F, 8.5F}, {4.5F, 1.65F, 7.0F}, 0.08F};
        (void) render("return-threat-shot.bmp", {4.5F, 1.65F, 7.0F}, {2.48F, 1.55F, 8.5F}, firing_turrets);
        (void) render("return-threat-shot-side.bmp", {3.0F, 1.65F, 6.2F}, {3.5F, 1.55F, 8.0F}, firing_turrets);
        firing_turrets[0].shot_trace.remaining_seconds = 0.04F;
        (void) render("return-threat-shot-fading.bmp", {3.0F, 1.65F, 6.2F}, {3.5F, 1.55F, 8.0F}, firing_turrets);
        RelayCore::gameplay::PlayerCombatState firing_player{};
        firing_player.shot_trace = {{4.5F, 1.65F, 7.0F}, {2.85F, 1.55F, 8.5F}, 0.12F};
        (void
        ) render("return-player-shot.bmp", {4.5F, 1.65F, 7.0F}, {2.48F, 1.55F, 8.5F}, combat.turrets, &firing_player);
        (void) render("return-passage.bmp", {3.5F, 1.65F, 9.5F}, {3.5F, 1.65F, 3.5F});
        (void) render("button-ready.bmp", {2.35F, 1.65F, 3.9F}, {2.1F, 1.4F, 4.76F});
        const auto button_draws = scene.diagnostics().usable_draw_calls;
        if (!use("airlock_reader").primary_applied)
        {
            throw std::runtime_error("Airlock button failed");
        }
        (void) render("button-pressed.bmp", {2.35F, 1.65F, 3.9F}, {2.1F, 1.4F, 4.76F});
        if (scene.diagnostics().usable_draw_calls != button_draws)
        {
            throw std::runtime_error("Used button housing must remain visible");
        }
        static_cast<void>(RelayCore::gameplay::update_doors(5, {}, doors));
        (void) render("airlock-exit-route.bmp", {2.1F, 1.65F, 3}, {1, 1.5F, 5});
        const glm::vec3 eye{0, 1.62F, -3.5F}, down{0, 0, -3.25F};
        const auto no_body = render("body-hidden.bmp", eye, down);
        const auto world_draws = scene.diagnostics().opaque_draw_calls;
        scientist = {.position = {0, 0, -3.5F}, .visible = true};
        const auto idle_body = render("scientist-idle.bmp", eye, down);
        if (scene.diagnostics().opaque_draw_calls != world_draws + 7 || std::abs(idle_body - no_body) < 0.1)
        {
            throw std::runtime_error("Scientist body must be visible looking down");
        }
        scientist.stride_phase = 1.57F;
        scientist.stride_amount = 1.0F;
        (void) render("scientist-step.bmp", eye, down);
        scientist.forward = {1, 0, 0};
        (void) render("scientist-turned.bmp", eye, {0.25F, 0, -3.5F});
        std::cout << "power_restoration=passed; mean brightness " << before << " -> " << after << '\n';
        config.audio.master_volume = 0;
        RelayCore::AudioSystem audio{config.audio};
        audio.initialize();
        bool running = true;
        RelayCore::EngineServices services{config, window, renderer, audio, running};
        if (audio.is_initialized())
        {
            for (const auto kind :
                 {RelayGame::EnvironmentKind::Fan, RelayGame::EnvironmentKind::Steam, RelayGame::EnvironmentKind::Sparks
                 })
            {
                const auto path = RelayCore::core::asset_path(RelayGame::environment_sound(kind));
                audio.play_loop(path, 0);
                if (!audio.has_active_loop() || audio.current_loop_path() != path)
                    throw std::runtime_error("Equipment audio did not decode");
            }
            audio.stop_loop();
            RelayGame::GameWorldRuntime equipment;
            equipment.load_level_session(config, RelayGame::LevelSessionBuilder{config.gameplay}.build());
            equipment.prepare_level_session_activation(services, false);
            equipment.activate_level_session(services);
            const RelayCore::FrameInfo frame{.delta_seconds = 1.0 / 60.0};
            for (int i = 0; i < 180; ++i)
                equipment.update(frame, services);
            for (const auto& effect : equipment.environment())
            {
                if (effect.kind == RelayGame::EnvironmentKind::Fan && effect.audible_gain != 0)
                    throw std::runtime_error("Ventilation was audible before cooling started");
                if (effect.kind == RelayGame::EnvironmentKind::Steam && effect.audible_gain <= 0)
                    throw std::runtime_error("Active steam did not reach spatial audio");
            }
            equipment.set_paused(true, services);
            const auto paused = equipment.capture_encounter_snapshot(0, 0, false);
            for (int i = 0; i < 60; ++i)
                equipment.update(frame, services);
            for (std::size_t i = 0; i < paused.environment.size(); ++i)
            {
                const auto& actual = equipment.environment()[i];
                if (actual.phase != paused.environment[i].phase
                    || actual.audible_gain != paused.environment[i].audible_gain)
                    throw std::runtime_error("Pause advanced equipment presentation");
            }
            equipment.set_paused(false, services);
            for (const auto name :
                 {"spare_fuse", "maintenance_isolator", "bus_relay", "power_breaker", "filter_purge", "cooling_control"
                 })
            {
                const auto found =
                    std::ranges::find(equipment.usables(), name, &RelayCore::gameplay::RuntimeUsable::name);
                if (found == equipment.usables().end())
                    throw std::runtime_error("Missing equipment interaction");
                static_cast<void>(equipment.debug_apply_usable_interaction(
                    static_cast<std::size_t>(found - equipment.usables().begin())
                ));
            }
            for (int i = 0; i < 600; ++i)
                equipment.update(frame, services);
            for (const auto& effect : equipment.environment())
            {
                if (effect.kind == RelayGame::EnvironmentKind::Fan
                    && (effect.strength < 0.99F || effect.audible_gain <= 0))
                    throw std::runtime_error("Running ventilation did not reach spatial audio");
                if ((effect.kind == RelayGame::EnvironmentKind::Steam
                     || effect.kind == RelayGame::EnvironmentKind::Sparks)
                    && effect.audible_gain > 0.001F)
                    throw std::runtime_error("Stopped equipment retained audible feedback");
            }
            equipment.shutdown(services);
            if (audio.has_active_loop())
                throw std::runtime_error("Level shutdown retained ambient audio");
            std::cout << "equipment_audio=passed; decode, power/cooling, pause, spatial gains, shutdown\n";
        } else
            std::cout << "equipment_audio=unavailable; backend check not performed\n";
        renderer.set_diagnostics_visible(false);
        renderer.camera().set_position({4.5F, 1.65F, 7.0F});
        renderer.camera().set_target({2.48F, 1.55F, 8.5F});
        renderer.begin_frame();
        scene.render(renderer, renderer.camera(), 0.0F, doors, combat.turrets, usables, -1, {}, false);
        RelayCore::render::PlayerDiagnostics feedback{};
        feedback.health = 75.0F;
        feedback.max_health = 100.0F;
        feedback.hit_confirmation_strength = 1.0F;
        feedback.hurt_flash_strength = 1.0F;
        renderer.set_player_diagnostics(feedback);
        if (!renderer.begin_ui({.delta_seconds = 1.0 / 60.0}, audio))
            throw std::runtime_error("Combat feedback HUD unavailable");
        renderer.end_ui();
        (void) capture(renderer, "power-captures/combat-feedback.bmp");
        const auto capture_status = [&](const char* name) {
            renderer.begin_frame();
            scene.render(renderer, renderer.camera(), 0.0F, doors, combat.turrets, usables, -1, {}, false);
            renderer.set_player_diagnostics(feedback);
            if (!renderer.begin_ui({.delta_seconds = 1.0 / 60.0}, audio))
                throw std::runtime_error("Player status HUD unavailable");
            renderer.end_ui();
            (void) capture(renderer, std::filesystem::path{"power-captures"} / name);
        };
        feedback.hit_confirmation_strength = 0;
        feedback.hurt_flash_strength = 0;
        feedback.active_hazard_contacts = 1;
        capture_status("hud-hazard.bmp");
        feedback.active_hazard_contacts = 0;
        feedback.health = 0;
        feedback.alive = false;
        feedback.respawn_remaining_seconds = 1.25F;
        capture_status("hud-respawn.bmp");
        renderer.set_player_diagnostics({});
        RelayGame::LevelEditor editor;
        renderer.camera().set_position({0, 1.65F, -3.5F});
        renderer.camera().set_target({0, 1.65F, 5});
        editor.enter(level, services);
        const auto editor_frame = [&] {
            const RelayCore::FrameInfo frame{.delta_seconds = 1.0 / 60.0};
            editor.update(frame, services);
            renderer.begin_frame();
            editor.render(frame, services);
            if (!renderer.begin_ui(frame, audio, true))
                throw std::runtime_error("Editor UI unavailable");
            editor.draw_ui(services);
            renderer.end_ui();
        };
        editor_frame();
        ImGui::GetIO().AddMousePosEvent(900, 360);
        ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, true);
        editor_frame();
        ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, false);
        editor_frame();
        if (!editor.document().selection())
            throw std::runtime_error("Editor viewport picking failed");
        const bool selected_entity = editor.document().selection()->entity;
        const auto object_count = [&] {
            return selected_entity ? editor.document().level().entities.size()
                                   : editor.document().level().solids.size();
        };
        const auto original_count = object_count();
        ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, true);
        ImGui::GetIO().AddKeyEvent(ImGuiKey_D, true);
        editor_frame();
        ImGui::GetIO().AddKeyEvent(ImGuiKey_D, false);
        editor_frame();
        if (object_count() != original_count + 1)
        {
            (void) capture(renderer, "power-captures/editor.bmp");
            throw std::runtime_error(
                "Editor duplicate shortcut failed; selected entity="
                + std::to_string(editor.document().selection()->entity)
                + " index=" + std::to_string(editor.document().selection()->index) + " ctrl="
                + std::to_string(ImGui::GetIO().KeyCtrl) + " text=" + std::to_string(ImGui::GetIO().WantTextInput)
            );
        }
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Z, true);
        editor_frame();
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Z, false);
        ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, false);
        editor_frame();
        if (object_count() != original_count)
            throw std::runtime_error("Editor undo shortcut failed");
        const auto selected = *editor.document().selection();
        const auto bounds = editor.document().bounds(selected);
        const auto center = (bounds.mins + bounds.maxs) * 0.5F;
        renderer.camera().set_position(center + glm::vec3{-5, 3, -5});
        renderer.camera().set_target(center);
        editor_frame();
        const auto screen = [&](glm::vec3 point) {
            const auto clip = renderer.camera().view_projection_matrix() * glm::vec4{point, 1};
            return glm::vec2{(clip.x / clip.w + 1) * width * 0.5F, (1 - clip.y / clip.w) * height * 0.5F};
        };
        const float handle_length = glm::length(renderer.camera().position() - center) * 0.12F;
        const auto base = screen(center);
        const auto tip = screen(center + glm::vec3{handle_length, 0, 0});
        const auto drag_start = base + (tip - base) * 0.85F;
        const auto drag_end = drag_start + (tip - base) / handle_length * 0.5F;
        ImGui::GetIO().AddMousePosEvent(drag_start.x, drag_start.y);
        editor_frame();
        ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, true);
        editor_frame();
        ImGui::GetIO().AddMousePosEvent(drag_end.x, drag_end.y);
        editor_frame();
        ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, false);
        editor_frame();
        const auto moved = editor.document().bounds(selected);
        if (std::abs(moved.mins.x - bounds.mins.x - 0.5F) > 0.001F || std::abs(moved.mins.y - bounds.mins.y) > 0.001F
            || std::abs(moved.mins.z - bounds.mins.z) > 0.001F)
            throw std::runtime_error("Editor axis drag did not move the selection by two grid cells");
        (void) capture(renderer, "power-captures/editor.bmp");
        editor.leave(services);
        editor.enter(level, services);
        if (!editor.document().dirty() || editor.document().bounds(selected).mins != moved.mins)
            throw std::runtime_error("Editor must retain unsaved work when returning from play mode");
        editor.leave(services);
        std::cout
            << "editor=passed; viewport picking, duplicate, undo, snapped axis drag, retained work, UI rendering\n";

        RelayCore::BlockoutLevel light_room;
        light_room.solids.push_back({{-3, 0, -6.2F}, {3, 3, -6}, RelayCore::SurfaceMaterial::Concrete});
        RelayGame::WorldSceneRenderer light_scene;
        const auto prepare_light_scene = [&] {
            light_scene.initialize(
                RelayGame::build_prepared_world_scene_data(
                    light_room,
                    renderer.authoring_config(),
                    renderer.color_pipeline_mode()
                ),
                renderer.static_models()
            );
        };
        prepare_light_scene();
        renderer.camera().set_position({4, 1.5F, 0});
        renderer.camera().set_target({0, 1.5F, -6});
        RelayCore::render::SpotLight light{.position = {0, 1.5F, 0}, .direction = {0, 0, -1}};
        const auto light_frame = [&](const char* name) {
            renderer.set_spot_light(light);
            renderer.begin_frame();
            light_scene.render(renderer, renderer.camera(), 0, {}, {}, {}, -1, {}, false, {});
            const auto image = renderer.capture_frame();
            double mean = 0;
            for (int y = height / 2 - 1; y <= height / 2 + 1; ++y)
                for (int x = width / 2 - 1; x <= width / 2 + 1; ++x)
                    for (size_t c = 0; c < 3; ++c)
                        mean += image.pixels[(size_t(y) * width + x) * 4 + c];
            (void) capture(renderer, std::filesystem::path{"power-captures"} / name);
            return mean / 27;
        };
        const auto unlit = light_frame("flashlight-off.bmp");
        light.enabled = true;
        const auto lit = light_frame("flashlight-on.bmp");
        light.direction = {0, 0, 1};
        const auto away = light_frame("flashlight-away.bmp");
        light.direction = {0, 0, -1};
        light_room.solids.push_back({{-0.65F, 0, -3.1F}, {0.65F, 2.7F, -3}, RelayCore::SurfaceMaterial::Concrete});
        light_scene.shutdown();
        prepare_light_scene();
        const auto occluded = light_frame("flashlight-blocked.bmp");
        if (lit < unlit + 20 || std::abs(away - unlit) > 2 || occluded > unlit + 3)
            throw std::runtime_error(
                "Flashlight direction/occlusion regression: " + std::to_string(unlit) + ", " + std::to_string(lit)
                + ", " + std::to_string(away) + ", " + std::to_string(occluded)
            );
        light_scene.shutdown();
        std::cout << "flashlight=passed; off=" << unlit << " on=" << lit << " away=" << away << " blocked=" << occluded
                  << '\n';

        RelayCore::test::ScopedTempDirectory player_data;
        const auto user_data = player_data.path() / "player";
        RelayGame::DemoGame game{user_data};
        RelayGame::DemoGame* active_game = &game;
        game.on_startup(services);
        const RelayCore::FrameInfo game_frame{.delta_seconds = 1.0 / 60.0};
        const auto key = [&](SDL_Scancode code) {
            SDL_Event event{};
            event.type = SDL_EVENT_KEY_DOWN;
            event.key.scancode = code;
            active_game->on_event(event, services);
        };
        const auto draw_game = [&](const char* name, bool log_text = false) {
            renderer.begin_frame();
            active_game->on_render(game_frame, services);
            if (renderer.begin_ui(game_frame, audio, active_game->has_custom_ui()))
            {
                if (log_text)
                    ImGui::LogToFile(-1, (std::string{"power-captures/"} + name + ".txt").c_str());
                active_game->on_ui(services);
                if (log_text)
                    ImGui::LogFinish();
                renderer.end_ui();
            }
            const auto extent = window.pixel_extent();
            return capture(renderer, std::filesystem::path{"power-captures"} / name, extent.width, extent.height);
        };
        (void) draw_game("main-menu.bmp");
        if (!game.has_custom_ui() || window.relative_mouse_mode())
            throw std::runtime_error("Startup must show a menu and release the cursor");
        const auto resize = [&](const int requested_width, const int requested_height) {
            if (!SDL_SetWindowSize(window.handle(), requested_width, requested_height)
                || !SDL_SyncWindow(window.handle()))
                throw std::runtime_error(SDL_GetError());
            const auto extent = window.pixel_extent();
            renderer.on_resize(extent.width, extent.height);
        };
        resize(960, 540);
        (void) draw_game("small-main-menu.bmp", true);
        if (ImGui::GetIO().DisplaySize.x != 960 || ImGui::GetIO().DisplaySize.y != 540)
            throw std::runtime_error("UI viewport did not follow the smaller window");
        key(SDL_SCANCODE_RETURN);
        if (game.has_custom_ui())
            throw std::runtime_error("Enter did not start the episode");
        game.on_update(game_frame, services);
        (void) draw_game("small-gameplay.bmp");
        const auto* hud = ImGui::GetForegroundDrawList();
        if (hud->VtxBuffer.empty())
            throw std::runtime_error("Small viewport HUD is missing");
        for (const auto& vertex : hud->VtxBuffer)
            if (vertex.pos.x < 0 || vertex.pos.y < 0 || vertex.pos.x > 960 || vertex.pos.y > 540)
                throw std::runtime_error("Small viewport HUD extends beyond the client area");
        resize(width, height);
        renderer.set_diagnostics_visible(true);
        game.on_update(game_frame, services);
        renderer.set_diagnostics_visible(false);
        (void) draw_game("gameplay.bmp");
        const auto tick = renderer.runtime_diagnostics().simulation_tick;
        key(SDL_SCANCODE_ESCAPE);
        if (!game.has_custom_ui() || !running || window.relative_mouse_mode())
            throw std::runtime_error("Escape must pause and release the cursor without exiting");
        renderer.set_diagnostics_visible(true);
        for (int i = 0; i < 10; ++i)
            game.on_update(game_frame, services);
        if (renderer.runtime_diagnostics().simulation_tick != tick)
            throw std::runtime_error("Pause advanced simulation");
        (void) draw_game("pause-menu.bmp");
        key(SDL_SCANCODE_ESCAPE);
        game.on_update(game_frame, services);
        if (game.has_custom_ui() || renderer.runtime_diagnostics().simulation_tick <= tick)
            throw std::runtime_error("Resume did not advance simulation");
        renderer.set_diagnostics_visible(false);
        ImGui::GetIO().WantCaptureKeyboard = false;
        const auto dark_game = draw_game("game-flashlight-off.bmp");
        ImGui::GetIO().WantCaptureKeyboard = false;
        key(SDL_SCANCODE_F);
        const auto bright_game = draw_game("game-flashlight-on.bmp");
        if (bright_game < dark_game + 0.05)
            throw std::runtime_error("F did not switch on the in-game flashlight");
        key(SDL_SCANCODE_F11);
        const auto campaign_save = RelayGame::load_saved_game(user_data / "progress.toml");
        key(SDL_SCANCODE_ESCAPE);
        (void) draw_game("editor-paused.bmp");
        key(SDL_SCANCODE_ESCAPE);
        key(SDL_SCANCODE_F11);
        game.on_update(game_frame, services);
        if (game.has_custom_ui())
            throw std::runtime_error("Editor failed to return to play after pause");
        game.on_shutdown(services);
        const auto after_editor = RelayGame::load_saved_game(user_data / "progress.toml");
        if (after_editor.played_seconds != campaign_save.played_seconds
            || after_editor.player.position != campaign_save.player.position)
            throw std::runtime_error("Editor playtest overwrote campaign progress");
        RelayGame::DemoGame resumed{user_data};
        active_game = &resumed;
        resumed.on_startup(services);
        (void) draw_game("continue-menu.bmp");
        key(SDL_SCANCODE_RETURN);
        if (resumed.has_custom_ui() || !window.relative_mouse_mode()
            || glm::distance(renderer.camera().position(), campaign_save.player.position + glm::vec3{0, 1.62F, 0})
                   > 0.01F)
            throw std::runtime_error("Continue did not restore player camera and input");
        renderer.set_diagnostics_visible(true);
        resumed.on_update(game_frame, services);
        if (renderer.runtime_diagnostics().simulation_tick <= campaign_save.world.simulation_tick)
            throw std::runtime_error("Continue did not resume simulation ticks");
        renderer.set_diagnostics_visible(false);
        resumed.on_shutdown(services);

        RelayGame::GameWorldRuntime finished;
        finished.load_level_session(config, RelayGame::LevelSessionBuilder{config.gameplay}.build());
        const auto at = [&](glm::vec3 position) {
            finished.debug_set_player_position(position);
            finished.debug_update_mission(RelayCore::physics::make_player_aabb(position, 0.3F, 1.8F));
        };
        for (const auto name :
             {"spare_fuse",
              "maintenance_isolator",
              "bus_relay",
              "power_breaker",
              "filter_purge",
              "cooling_control",
              "airlock_reader"})
        {
            const auto found = std::ranges::find(finished.usables(), name, &RelayCore::gameplay::RuntimeUsable::name);
            if (found == finished.usables().end())
                throw std::runtime_error("Missing final-scene interaction");
            static_cast<void>(
                finished.debug_apply_usable_interaction(static_cast<std::size_t>(found - finished.usables().begin()))
            );
            at({3.5F, 0.01F, 7});
        }
        at({0, 0.01F, 9});
        if (!finished.mission_status().completed)
            throw std::runtime_error("Episode could not complete through the return route");
        auto completed_save = finished.capture_saved_game(327, false);
        completed_save.player.view_angles = {1.5707963F, 0};
        completed_save.world.doors[0].state = RelayCore::gameplay::DoorState::Open;
        completed_save.world.doors[0].open_fraction = 1;
        RelayGame::write_saved_game(user_data / "progress.toml", completed_save);
        RelayGame::DemoGame finale{user_data};
        active_game = &finale;
        finale.on_startup(services);
        key(SDL_SCANCODE_RETURN);
        for (int i = 0; i < 150; ++i)
            finale.on_update(game_frame, services);
        (void) draw_game("episode-finale.bmp");
        for (int i = 0; i < 220; ++i)
            finale.on_update(game_frame, services);
        (void) draw_game("episode-complete.bmp");
        if (!finale.has_custom_ui() || window.relative_mouse_mode()
            || glm::distance(renderer.camera().position(), finished.ending()->camera_position) > 0.01F)
            throw std::runtime_error("Final scene failed to reach its authored camera");
        key(SDL_SCANCODE_RETURN);
        (void) draw_game("completed-main-menu.bmp");
        key(SDL_SCANCODE_RETURN);
        key(SDL_SCANCODE_ESCAPE);
        finale.on_update(game_frame, services);
        (void) draw_game("episode-skipped.bmp");
        finale.on_shutdown(services);
        if (!SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "RELAYCORE_UPDATE_READY_EVENT", "test-update", true))
            throw std::runtime_error(SDL_GetError());
        RelayGame::DemoGame updated{player_data.path() / "updated-player"};
        active_game = &updated;
        updated.on_startup(services);
        SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(), "RELAYCORE_UPDATE_READY_EVENT");
        resize(960, 540);
        (void) draw_game("updated-main-menu.bmp", true);
        const auto read_ui_log = [](const char* path) {
            std::ifstream file{path};
            if (!file)
                throw std::runtime_error("Missing UI log");
            return std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
        };
        if (read_ui_log("power-captures/small-main-menu.bmp.txt").find("Игра обновлена") != std::string::npos
            || read_ui_log("power-captures/updated-main-menu.bmp.txt")
                       .find("Игра обновлена до версии " RELAYCORE_GAME_VERSION)
                   == std::string::npos)
            throw std::runtime_error("Update notification must appear only after an updated launch");
        updated.on_shutdown(services);
        {
            RelayGame::DemoGame sandbox_game{player_data.path() / "sandbox-player", true};
            active_game = &sandbox_game;
            sandbox_game.on_startup(services);
            (void) draw_game("sandbox-menu.bmp", true);
            if (read_ui_log("power-captures/sandbox-menu.bmp.txt").find("ИСПЫТАТЕЛЬНЫЙ ПОЛИГОН") == std::string::npos)
                throw std::runtime_error("Sandbox launcher did not select its own menu");
            sandbox_game.begin_episode(false, services);
            sandbox_game.on_update(game_frame, services);
            sandbox_game.return_to_menu(services);
            const auto sandbox_save = RelayGame::load_saved_game(player_data.path() / "sandbox-player/progress.toml");
            if (std::filesystem::path{sandbox_save.world.level_path}.filename() != "development_sandbox.toml"
                || sandbox_save.world.mission.active)
                throw std::runtime_error("Sandbox launch selected an episode or forced a mission");
            sandbox_game.enter_editor(services);
            sandbox_game.begin_editor_preview(services);
            sandbox_game.return_from_editor(services);
            sandbox_game.on_shutdown(services);

            RelayGame::GameWorldRuntime weapon_fixture;
            weapon_fixture.load_level_session(
                config,
                RelayGame::LevelSessionBuilder{config.gameplay}.build(
                    RelayCore::core::asset_path("levels/development_sandbox.toml")
                )
            );
            const auto shotgun = std::ranges::find(
                weapon_fixture.usables(),
                RelayGame::shotgun_item_id,
                &RelayCore::gameplay::RuntimeUsable::item_id
            );
            if (shotgun == weapon_fixture.usables().end())
                throw std::runtime_error("Sandbox has no shotgun pickup");
            static_cast<void>(weapon_fixture.debug_apply_usable_interaction(
                static_cast<std::size_t>(shotgun - weapon_fixture.usables().begin())
            ));
            weapon_fixture.prepare_level_session_activation(services, false);
            weapon_fixture.activate_level_session(services);
            const glm::vec3 hazard_position{-25, .02F, 0};
            static_cast<void>(weapon_fixture.debug_apply_combat_threats(
                RelayCore::physics::make_player_aabb(hazard_position, .3F, 1.8F),
                hazard_position + glm::vec3{0, 1.65F, 0},
                10
            ));
            if (weapon_fixture.player_combat_state().alive)
                throw std::runtime_error("Weapon input death fixture did not kill the player");
            SDL_Event dead_selection{};
            dead_selection.type = SDL_EVENT_KEY_DOWN;
            dead_selection.key.scancode = SDL_SCANCODE_2;
            weapon_fixture.on_event(dead_selection, services);
            weapon_fixture.update(game_frame, services);
            if (!weapon_fixture.debug_advance_player_respawn(2))
                throw std::runtime_error("Weapon input fixture did not respawn");
            weapon_fixture.update(game_frame, services);
            if (weapon_fixture.selected_weapon() != RelayGame::PlayerWeapon::Precise)
                throw std::runtime_error("Dead weapon input leaked into respawn");
            const auto weapon_data = player_data.path() / "weapon-player";
            RelayGame::write_saved_game(weapon_data / "progress.toml", weapon_fixture.capture_saved_game(0, false));
            weapon_fixture.shutdown(services);
            RelayGame::DemoGame weapon_game{weapon_data, true};
            active_game = &weapon_game;
            weapon_game.on_startup(services);
            key(SDL_SCANCODE_2);
            weapon_game.begin_episode(true, services);
            weapon_game.on_update(game_frame, services);
            if (renderer.player_diagnostics().weapon_label != "Точное оружие")
                throw std::runtime_error("Menu weapon input leaked into gameplay");
            renderer.set_diagnostics_visible(true);
            ImGui::GetIO().WantTextInput = true;
            key(SDL_SCANCODE_2);
            weapon_game.on_update(game_frame, services);
            ImGui::GetIO().WantTextInput = false;
            renderer.set_diagnostics_visible(false);
            weapon_game.on_update(game_frame, services);
            if (renderer.player_diagnostics().weapon_label != "Точное оружие")
                throw std::runtime_error("Text input weapon key leaked into gameplay");
            key(SDL_SCANCODE_2);
            weapon_game.on_update(game_frame, services);
            if (renderer.player_diagnostics().weapon_label != "Дробовик")
                throw std::runtime_error("Weapon selection did not reach fixed simulation");
            (void) draw_game("shotgun-ready.bmp");
            const auto ready_draws = renderer.world_diagnostics().overlay_draw_calls;
            if (ready_draws != 2)
                throw std::runtime_error("Shotgun viewmodel did not submit its body and pump");
            const auto fire = [&]() {
                SDL_Event event{};
                event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
                event.button.button = SDL_BUTTON_LEFT;
                weapon_game.on_event(event, services);
            };
            fire();
            weapon_game.on_update(game_frame, services);
            if (renderer.player_diagnostics().shots_fired != 1
                || renderer.player_diagnostics().weapon_cooldown_remaining_seconds < .8F)
                throw std::runtime_error("Shotgun did not count one attack and start its cooldown");
            (void) draw_game("shotgun-fired.bmp");
            if (renderer.world_diagnostics().overlay_draw_calls != ready_draws + 7)
                throw std::runtime_error("Shotgun did not render seven separate pellet traces");
            for (int frame_index = 0; frame_index < 5; ++frame_index)
                weapon_game.on_update(game_frame, services);
            (void) draw_game("shotgun-recoil.bmp");
            for (int frame_index = 0; frame_index < 18; ++frame_index)
                weapon_game.on_update(game_frame, services);
            (void) draw_game("shotgun-pump.bmp");
            key(SDL_SCANCODE_1);
            fire();
            weapon_game.on_update(game_frame, services);
            if (renderer.player_diagnostics().weapon_label != "Точное оружие"
                || renderer.player_diagnostics().shots_fired != 1)
                throw std::runtime_error("Switching weapon bypassed cooldown");
            (void) draw_game("shotgun-hidden.bmp");
            if (renderer.world_diagnostics().overlay_draw_calls != 0)
                throw std::runtime_error("Shotgun viewmodel remained visible with the precise weapon");
            key(SDL_SCANCODE_ESCAPE);
            key(SDL_SCANCODE_2);
            weapon_game.on_update(game_frame, services);
            key(SDL_SCANCODE_ESCAPE);
            weapon_game.on_update(game_frame, services);
            if (renderer.player_diagnostics().weapon_label != "Точное оружие")
                throw std::runtime_error("Pause weapon input leaked into resumed gameplay");
            weapon_game.enter_editor(services);
            key(SDL_SCANCODE_2);
            weapon_game.return_from_editor(services);
            weapon_game.on_update(game_frame, services);
            if (renderer.player_diagnostics().weapon_label != "Точное оружие")
                throw std::runtime_error("Editor weapon input leaked into gameplay");
            key(SDL_SCANCODE_2);
            weapon_game.on_update(game_frame, services);
            key(SDL_SCANCODE_F9);
            weapon_game.on_update(game_frame, services);
            key(SDL_SCANCODE_1);
            weapon_game.on_update(game_frame, services);
            key(SDL_SCANCODE_F10);
            weapon_game.on_update(game_frame, services);
            if (renderer.player_diagnostics().weapon_label != "Дробовик")
                throw std::runtime_error("F10 did not restore the selected weapon");
            weapon_game.return_to_menu(services);
            if (RelayGame::load_saved_game(weapon_data / "progress.toml").world.selected_weapon
                != RelayGame::PlayerWeapon::Shotgun)
                throw std::runtime_error("Menu save lost the selected weapon");
            weapon_game.on_shutdown(services);
            active_game = &game;
            std::cout << "weapons=passed; selection, menu/text/pause/editor/death ownership, cooldown, F9/F10 and disk "
                         "save\n";
            resize(width, height);
            const auto sandbox_session = RelayGame::LevelSessionBuilder{config.gameplay}.build(
                RelayCore::core::asset_path("levels/development_sandbox.toml")
            );
            RelayGame::WorldSceneRenderer sandbox_scene;
            sandbox_scene.initialize(
                RelayGame::build_prepared_world_scene_data(
                    sandbox_session.level,
                    renderer.authoring_config(),
                    renderer.color_pipeline_mode()
                ),
                renderer.static_models()
            );
            const auto view = [&](const char* name, glm::vec3 eye, glm::vec3 target) {
                renderer.camera().set_position(eye);
                renderer.camera().set_target(target);
                renderer.begin_frame();
                sandbox_scene.render(
                    renderer,
                    renderer.camera(),
                    0,
                    sandbox_session.doors,
                    sandbox_session.combat.turrets,
                    sandbox_session.usables,
                    -1,
                    {},
                    false,
                    {},
                    {},
                    sandbox_session.environment,
                    sandbox_session.enemies
                );
                (void) capture(renderer, std::filesystem::path{"power-captures"} / name);
            };
            view("sandbox-hub.bmp", {0, 1.65F, -1}, {0, 1.65F, 6});
            view("sandbox-shotgun-pickup.bmp", {.4F, 1.65F, -2.5F}, {.7F, 1.105F, -4});
            view("sandbox-movement.bmp", {-10, 1.65F, 0}, {-25, 1, -2});
            view("sandbox-devices.bmp", {10, 1.65F, 0}, {23, 1.4F, -2});
            view("sandbox-native-model.bmp", {28, .85F, 5}, {29, .85F, 2.5F});
            view("sandbox-range.bmp", {-20, 1.65F, 10}, {-20, 1.25F, 23});
            view("sandbox-arena.bmp", {5, 1.65F, 10}, {17, 1, 19});
            view("sandbox-rover.bmp", {11, 1.7F, 18}, {11, .75F, 20});
            view("sandbox-mixed.bmp", {28, 1.7F, 19}, {28, 1.5F, 25});
            view("sandbox-mixed-cover.bmp", {22, 1.65F, 20}, {28, 1.5F, 25});
            sandbox_scene.shutdown();
            std::cout
                << "sandbox_flow=passed; own map, separate progress, editor preview, native model and authored views\n";
        }
        {
            const auto lab_path = RelayCore::core::asset_path("levels/native_lab.toml");
            const auto lab_data = player_data.path() / "native-lab";
            std::filesystem::create_directories(lab_data);
            RelayGame::LevelEditor native_editor;
            native_editor.enter(level, services);
            native_editor.open_map(lab_path, services);
            const auto editor_saved = lab_data / "editor-roundtrip.toml";
            native_editor.save_map(editor_saved, services);
            native_editor.leave(services);
            const auto bench_path = lab_data / "lab_bench.rcmodel";
            std::filesystem::copy_file(RelayCore::core::asset_path("models/lab_bench.rcmodel"), bench_path);
            auto lab_level = RelayCore::load_blockout_level(editor_saved);
            const auto probe_path = lab_data / "native_lab.rcprobe";
            std::filesystem::copy_file(RelayCore::core::asset_path("lighting/native_lab.rcprobe"), probe_path);
            for (auto& entity : lab_level.entities)
            {
                if (entity.classname == "prop_static" && entity.properties.at("model") == "models/lab_bench.rcmodel")
                    entity.properties["model"] = bench_path.generic_string();
                if (entity.classname == "irradiance_volume")
                    entity.properties["product"] = probe_path.generic_string();
            }
            RelayGame::LevelEditorDocument lab_document;
            lab_document.reset(std::move(lab_level));
            const auto preview_path = lab_data / "native_lab.toml";
            lab_document.save(preview_path);
            auto lab_config = config;
            lab_config.gameplay.level_path = preview_path.string();
            RelayCore::EngineServices lab_services{lab_config, window, renderer, audio, running};
            RelayGame::DemoGame lab_game{lab_data / "player"};
            renderer.set_diagnostics_visible(true);
            lab_game.on_startup(lab_services);
            lab_game.begin_episode(false, lab_services);
            lab_game.on_update(game_frame, lab_services);
            const auto lab_view = [&](const char* name, bool editor_view = false) {
                renderer.camera().set_position({0, 1.65F, -3.4F});
                renderer.camera().set_target({0, 1.35F, 3.8F});
                renderer.begin_frame();
                lab_game.on_render(game_frame, lab_services);
                if (editor_view && renderer.begin_ui(game_frame, audio, true))
                {
                    lab_game.on_ui(lab_services);
                    renderer.end_ui();
                }
                return capture(renderer, std::filesystem::path{"power-captures"} / name);
            };
            const auto require_lab = [&] {
                if (renderer.runtime_diagnostics().level_path != preview_path.string())
                    throw std::runtime_error("Native lab session was not activated");
            };
            require_lab();
            (void) lab_view("native-lab.bmp");
            lab_game.enter_editor(lab_services);
            (void) lab_view("native-lab-editor.bmp", true);
            lab_game.begin_editor_preview(lab_services);
            lab_game.on_update(game_frame, lab_services);
            if (lab_game.has_custom_ui())
                throw std::runtime_error("Native lab editor preview did not enter gameplay");
            (void) lab_view("native-lab-preview.bmp");
            lab_game.return_from_editor(lab_services);
            (void) lab_view("native-lab.bmp");
            const auto lab_before = read_rgb(renderer);
            auto bench = RelayCore::render::read_model_asset(bench_path);
            for (auto& material : bench.materials)
                for (std::size_t i = 0; i < material.base_color.pixels.size(); i += 4)
                {
                    material.base_color.pixels[i] = 50;
                    material.base_color.pixels[i + 1] = 90;
                    material.base_color.pixels[i + 2] = 180;
                }
            RelayCore::render::write_model_asset(bench_path, bench);
            const auto reload_lab = [&] {
                SDL_Event event{};
                event.type = SDL_EVENT_KEY_DOWN;
                event.key.scancode = SDL_SCANCODE_F7;
                lab_game.on_event(event, lab_services);
                lab_game.on_update(game_frame, lab_services);
            };
            reload_lab();
            if (renderer.iteration_diagnostics().last_reload_succeeded)
                throw std::runtime_error("Native lab accepted stale irradiance after material edit");
            require_lab();
            (void) lab_view("native-lab-stale-probe-rollback.bmp");
            const auto same_channel = [](unsigned char a, unsigned char b) {
                return std::abs(static_cast<int>(a) - static_cast<int>(b)) <= 1;
            };
            if (!std::ranges::equal(lab_before, read_rgb(renderer), same_channel))
                throw std::runtime_error("Stale irradiance reload changed the working scene");
            auto bake_level = RelayCore::load_blockout_level(preview_path);
            for (auto& entity : bake_level.entities)
                if (entity.classname == "irradiance_volume")
                    entity.properties["rays"] = "64";
            const auto bake_source = RelayGame::world_irradiance_source(bake_level);
            RelayGame::LevelEditorDocument bake_document;
            bake_document.reset(bake_level);
            bake_document.save(preview_path);
            std::erase_if(bake_level.entities, [](const auto& e) {
                return e.classname == "irradiance_volume";
            });
            const auto bake_scene = RelayGame::build_prepared_world_scene_data(
                bake_level,
                renderer.authoring_config(),
                RelayCore::render::ColorPipelineMode::hardware_srgb
            );
            RelayCore::render::write_irradiance_probes(
                probe_path,
                RelayCore::render::bake_irradiance_probes(
                    RelayGame::build_world_irradiance_scene(bake_scene, renderer.authoring_config()),
                    bake_source->settings
                )
            );
            reload_lab();
            if (!renderer.iteration_diagnostics().last_reload_succeeded)
                throw std::runtime_error("Native lab F7 did not reload the changed model");
            (void) lab_view("native-lab-reloaded.bmp");
            const auto reloaded = read_rgb(renderer);
            if (std::ranges::equal(lab_before, reloaded, same_channel))
                throw std::runtime_error("Native lab material update did not reach the rendered frame");
            std::ofstream(bench_path, std::ios::binary) << "broken native model";
            reload_lab();
            if (renderer.iteration_diagnostics().last_reload_succeeded)
                throw std::runtime_error("Native lab accepted a broken product during F7");
            require_lab();
            (void) lab_view("native-lab-rollback.bmp");
            if (!std::ranges::equal(read_rgb(renderer), reloaded, same_channel))
                throw std::runtime_error("Failed native lab reload changed the working scene");
            RelayCore::render::write_model_asset(bench_path, bench);
            reload_lab();
            if (!renderer.iteration_diagnostics().last_reload_succeeded)
                throw std::runtime_error("Native lab could not recover after a repaired product");
            lab_game.on_shutdown(lab_services);
            renderer.set_diagnostics_visible(false);
            std::cout << "native_lab=passed; models, editor/preview, stale-probe rollback, rebake/F7, failed model "
                         "rollback, recovery\n";
        }
        {
            const auto transfer_path = RelayCore::core::asset_path("levels/pressure_transfer.toml");
            const auto transfer_session = RelayGame::LevelSessionBuilder{config.gameplay}.build(transfer_path);
            RelayGame::WorldSceneRenderer transfer_scene;
            transfer_scene.initialize(
                RelayGame::build_prepared_world_scene_data(
                    transfer_session.level,
                    renderer.authoring_config(),
                    renderer.color_pipeline_mode()
                ),
                renderer.static_models()
            );
            const auto transfer_view = [&](const char* name, glm::vec3 eye, glm::vec3 target) {
                renderer.camera().set_position(eye);
                renderer.camera().set_target(target);
                renderer.begin_frame();
                transfer_scene.render(
                    renderer,
                    renderer.camera(),
                    0,
                    transfer_session.doors,
                    transfer_session.combat.turrets,
                    transfer_session.usables,
                    -1,
                    {},
                    false,
                    {},
                    {},
                    transfer_session.environment,
                    transfer_session.enemies
                );
                (void) capture(renderer, std::filesystem::path{"power-captures"} / name);
            };
            transfer_view("transfer-equipment.bmp", {4, 1.65F, 2}, {2, 1.1F, 4});
            transfer_view("transfer-rover.bmp", {4, 1.65F, 13}, {4, .45F, 19});
            transfer_view("transfer-gallery.bmp", {10, 1.65F, 25}, {30, 1.55F, 22});
            transfer_view("transfer-mixed.bmp", {28, 1.65F, 29}, {32, 1.1F, 38});
            transfer_scene.shutdown();

            RelayGame::GameWorldRuntime fixture;
            fixture.load_level_session(config, transfer_session);
            const auto use_transfer = [&](std::string_view name) {
                const auto found =
                    std::ranges::find(fixture.usables(), name, &RelayCore::gameplay::RuntimeUsable::name);
                if (found == fixture.usables().end())
                    throw std::runtime_error("Missing pressure transfer interaction");
                return fixture.debug_apply_usable_interaction(
                    static_cast<std::size_t>(found - fixture.usables().begin())
                );
            };
            static_cast<void>(use_transfer("transfer_shotgun"));
            static_cast<void>(use_transfer("service_reader"));
            fixture.prepare_level_session_activation(services, false);
            fixture.activate_level_session(services);
            fixture.debug_set_player_position({4, .01F, 8});
            for (int i = 0; i < 150; ++i)
                fixture.update(game_frame, services);
            fixture.debug_update_mission(RelayCore::physics::make_player_aabb({11, .01F, 22}, .3F, 1.8F));
            fixture.debug_update_mission(RelayCore::physics::make_player_aabb({30, .01F, 28}, .3F, 1.8F));
            if (!fixture.select_weapon(RelayGame::PlayerWeapon::Shotgun))
                throw std::runtime_error("Pressure transfer lost its weapon pickup");
            auto transfer_save = fixture.capture_saved_game(30, false);
            fixture.shutdown(services);
            transfer_save.player.position = {38, .01F, 40};
            transfer_save.player.view_angles = {0, 0};
            const auto transfer_data = player_data.path() / "transfer-player";
            RelayGame::write_saved_game(transfer_data / "progress.toml", transfer_save);
            RelayGame::DemoGame transfer_game{transfer_data};
            active_game = &transfer_game;
            transfer_game.on_startup(services);
            transfer_game.begin_episode(true, services);
            transfer_game.on_update(game_frame, services);
            (void) draw_game("transfer-control-before.bmp");
            ImGui::GetIO().WantCaptureKeyboard = false;
            key(SDL_SCANCODE_E);
            transfer_game.on_update(game_frame, services);
            (void) draw_game("transfer-control-after.bmp");
            transfer_game.return_to_menu(services);
            const auto activated_save = RelayGame::load_saved_game(transfer_data / "progress.toml");
            if (std::ranges::find(activated_save.world.activated_target_ids, "transfer_running")
                    == activated_save.world.activated_target_ids.end()
                || activated_save.world.selected_weapon != RelayGame::PlayerWeapon::Shotgun
                || std::filesystem::path(activated_save.world.level_path) != transfer_path)
                throw std::runtime_error("Pressure transfer continue/use/menu save did not preserve device and weapon");
            transfer_game.on_shutdown(services);

            fixture.load_level_session(config, transfer_session);
            fixture.restore_saved_game(activated_save);
            fixture.prepare_level_session_activation(services, false);
            fixture.activate_level_session(services);
            fixture.debug_set_player_position({28, .01F, 42});
            static_cast<void>(use_transfer("exit_reader"));
            for (int i = 0; i < 150; ++i)
                fixture.update(game_frame, services);
            fixture.debug_set_player_position({28, .01F, 49});
            fixture.update(game_frame, services);
            if (!fixture.mission_status().completed)
                throw std::runtime_error("Pressure transfer exit did not complete the mission");
            RelayGame::write_saved_game(transfer_data / "progress.toml", fixture.capture_saved_game(40, false));
            fixture.shutdown(services);
            RelayGame::DemoGame transfer_finale{transfer_data};
            active_game = &transfer_finale;
            transfer_finale.on_startup(services);
            transfer_finale.begin_episode(true, services);
            transfer_finale.on_update(game_frame, services);
            for (int i = 0; i < 150; ++i)
                transfer_finale.on_update(game_frame, services);
            (void) draw_game("transfer-finale.bmp", true);
            for (int i = 0; i < 220; ++i)
                transfer_finale.on_update(game_frame, services);
            (void) draw_game("transfer-complete.bmp", true);
            const auto completed_text = read_ui_log("power-captures/transfer-complete.bmp.txt");
            if (completed_text.find("ПЕРЕПУСК ДАВЛЕНИЯ") == std::string::npos
                || completed_text.find("Давление выровнено") == std::string::npos)
                throw std::runtime_error("Pressure transfer completion UI showed the wrong episode");
            if (!transfer_finale.has_custom_ui()
                || glm::distance(renderer.camera().position(), transfer_session.ending->camera_position) > .01F)
                throw std::runtime_error("Pressure transfer did not reach its authored finale");
            transfer_finale.on_shutdown(services);
            std::cout << "pressure_transfer=passed; authored views, disk continue, SDL use, menu save and finale\n";
        }
        std::cout
            << "episode_flow=passed; startup menu, disk continue, editor isolation, cinematic, completion, skip\n";
        std::cout << "player_experience=passed; pause, frozen simulation, resume, flashlight key, editor round trip\n";
        scene.shutdown();
    } catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    RelayCore::log::shutdown();
    SDL_Quit();
    return result;
}

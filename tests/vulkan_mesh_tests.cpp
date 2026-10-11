#include "relaycore/audio/audio_system.h"
#include "relaycore/core/engine_config.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/core/profiling.h"
#include "relaycore/debug/imgui_overlay.h"
#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/authored_color.h"
#include "relaycore/render/blockout_texture_atlas.h"
#include "relaycore/render/camera.h"
#include "relaycore/render/irradiance_probes.h"
#include "relaycore/render/player_ui.h"
#include "relaycore/render/reflection_probe.h"
#include "relaycore/render/shadow_math.h"
#include "relaycore/render/vulkan_device.h"
#include "relaycore/render/vulkan_mesh_renderer.h"
#include "relaycore/render/vulkan_swapchain.h"
#include "relaycore/world/blockout_loader.h"
#include "relaygame/world_scene_renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <unordered_map>

#include "scoped_temp_directory.h"
#include "vulkan_queue_gate.h"

namespace
{
    using namespace RelayCore::render;
    constexpr int width = 640, height = 360;

    void require(const bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    auto display_reference(glm::vec3 color, const bool linear, const float contrast = 1) -> glm::vec3
    {
        for (int c = 0; c < 3; ++c)
        {
            if (color[c] > .65F)
                color[c] = .65F + (color[c] - .65F) / (1 + (color[c] - .65F) / .35F);
            if (linear)
                color[c] = color[c] <= .0031308F ? color[c] * 12.92F : 1.055F * std::pow(color[c], 1 / 2.4F) - .055F;
        }
        const float luminance = glm::dot(color, glm::vec3{.2126F, .7152F, .0722F});
        const float t = glm::clamp((luminance - .18F) / .6F, 0.0F, 1.0F);
        color = (color - .5F) * contrast + .5F;
        color *= glm::mix(glm::vec3{.97F, .99F, 1.01F}, glm::vec3{1.01F, 1, .98F}, t * t * (3 - 2 * t));
        return glm::clamp(color, glm::vec3{0}, glm::vec3{1});
    }
    template<typename F>
    void expect_failure(F&& operation, const std::string_view expected)
    {
        try
        {
            operation();
        } catch (const std::exception& error)
        {
            if (std::string_view{error.what()}.find(expected) != std::string_view::npos)
                return;
            throw;
        }
        throw std::runtime_error("Invalid mesh/shader state was accepted");
    }

    void save(const CpuRgbaTextureData& image, const std::filesystem::path& path)
    {
        const std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface{
            SDL_CreateSurfaceFrom(
                image.width,
                image.height,
                SDL_PIXELFORMAT_RGBA32,
                const_cast<uint8_t*>(image.pixels.data()),
                image.width * 4
            ),
            SDL_DestroySurface
        };
        require(surface && SDL_SaveBMP(surface.get(), path.string().c_str()), "Could not save Vulkan mesh comparison");
    }

    void compare(const CpuRgbaTextureData& image, const std::string& name, const std::filesystem::path& captures)
    {
        save(image, captures / (name + "-vk.bmp"));
        const auto path = std::filesystem::path{RELAYCORE_TEST_BASELINE_DIRECTORY} / (name + ".rcgb");
        std::ifstream file{path, std::ios::binary};
        std::array<uint32_t, 4> header{};
        file.read(reinterpret_cast<char*>(header.data()), sizeof(header));
        require(
            file && header[0] == 0x31474352 && header[1] == uint32_t(image.width) && header[2] == uint32_t(image.height)
                && header[3] == 8,
            "Missing/invalid accepted graphics baseline"
        );
        const size_t count = size_t(image.width / 8) * (image.height / 8);
        std::vector<uint16_t> baseline(count * 4);
        file.read(reinterpret_cast<char*>(baseline.data()), baseline.size() * sizeof(uint16_t));
        require(file && file.peek() == std::char_traits<char>::eof(), "Truncated/oversized graphics baseline");
        size_t coverage_diff = 0, color_diff = 0, covered = 0, tile_index = 0;
        double sum = 0;
        for (int ty = 0; ty < image.height; ty += 8)
            for (int tx = 0; tx < image.width; tx += 8, ++tile_index)
            {
                std::array<unsigned, 4> rgba{};
                for (int y = ty; y < ty + 8; ++y)
                    for (int x = tx; x < tx + 8; ++x)
                        for (size_t c = 0; c < 4; ++c)
                            rgba[c] += image.pixels[(size_t(y) * image.width + x) * 4 + c];
                const auto* expected = baseline.data() + tile_index * 4;
                const bool a = expected[0] || expected[1] || expected[2];
                const bool b = rgba[0] || rgba[1] || rgba[2];
                covered += a;
                double max_delta = 0;
                for (size_t c = 0; c < 3; ++c)
                {
                    const double delta = std::abs(double(expected[c]) - rgba[c]) / 64;
                    sum += delta;
                    max_delta = std::max(delta, max_delta);
                }
                coverage_diff += a != b && max_delta > 3;
                color_diff += max_delta > 8;
                require(std::abs(double(expected[3]) - rgba[3]) / 64 <= 2, "Vulkan blend alpha differs from baseline");
            }
        std::cout << name << ": mask_diff=" << double(coverage_diff) / count
                  << " color_diff=" << double(color_diff) / count << " mean_rgb_delta=" << sum / (count * 3) << '\n';
        require(covered > count * .01, "Baseline has no meaningful visible geometry");
        require(double(coverage_diff) / count < .002, "Vulkan placement differs from accepted baseline");
        require(
            double(color_diff) / count < (name.starts_with("shadows-") ? .0005 : .01)
                && sum / (count * 3) < (name.starts_with("shadows-") ? .1 : 1.0),
            "Vulkan depth/UV/texture/lighting differs from accepted baseline"
        );
    }

    auto allocation_count(const VulkanDevice& device) -> uint32_t
    {
        VmaTotalStatistics stats{};
        vmaCalculateStatistics(device.allocator(), &stats);
        return stats.total.statistics.allocationCount;
    }

    void check_pistol_viewmodel(
        const VulkanDevice& device,
        const RenderAuthoringConfig& authoring,
        const CpuRgbaTextureData& atlas,
        const std::filesystem::path& captures
    )
    {
        const auto level = RelayCore::load_blockout_level(RelayCore::core::asset_path("levels/pressure_transfer.toml"));
        for (const bool hdr : {false, true})
        {
            RendererConfig config{};
            config.hdr_enabled = hdr;
            StaticModelCache models;
            RelayGame::WorldSceneRenderer scene;
            scene.initialize(
                RelayGame::build_prepared_world_scene_data(level, authoring, ColorPipelineMode::hardware_srgb),
                models
            );
            VulkanMeshRenderer renderer{
                device,
                RELAYCORE_TEST_SPIRV_DIRECTORY,
                authoring,
                atlas,
                TextureEncoding::srgb,
                config,
                ColorPipelineMode::hardware_srgb
            };
            renderer.prepare_resources(scene.resource_items());
            for (const float fov : {50.0F, 70.0F, 90.0F})
                for (const auto size : {glm::ivec2{640, 360}, glm::ivec2{800, 600}})
                {
                    PerspectiveCamera camera{
                        {.position = {4, 1.65F, 2}, .target = {4, 1.65F, 10}, .vertical_fov_radians = glm::radians(fov)}
                    };
                    camera.set_viewport(size.x, size.y);
                    scene.prepare_render_queue(camera, 0, {}, {}, {}, -1, {}, false);
                    renderer.set_local_lights(scene.local_lights());
                    const auto base = renderer.render_image(scene.render_items(), camera, size.x, size.y);
                    const auto prefix = std::string{"pistol-"} + (hdr ? "hdr-" : "sdr-") + std::to_string(int(fov))
                                        + "-" + std::to_string(size.x);
                    const auto resting = scene.prepare_pistol_viewmodel(camera, 0, .24F);
                    const auto idle =
                        renderer.render_image(scene.render_items(), camera, size.x, size.y, resting).pixels;
                    for (const float elapsed : {0.0F, .04F, .09F, .18F, .24F})
                    {
                        const auto weapon = scene.prepare_pistol_viewmodel(camera, .24F - elapsed, .24F);
                        require(!weapon.empty(), "Pistol asset produced no render parts");
                        const auto image = renderer.render_image(scene.render_items(), camera, size.x, size.y, weapon);
                        save(image, captures / (prefix + "-" + std::to_string(int(elapsed * 1000)) + ".bmp"));
                        require(image.pixels != base.pixels, "Pistol is invisible at a supported FOV/window size");
                        for (int y = size.y / 2 - 8; y <= size.y / 2 + 8; ++y)
                            for (int x = size.x / 2 - 8; x <= size.x / 2 + 8; ++x)
                                for (int c = 0; c < 3; ++c)
                                {
                                    const auto pixel = (y * size.x + x) * 4 + c;
                                    require(image.pixels[pixel] == base.pixels[pixel], "Pistol obscures the reticle");
                                }
                        if (elapsed == 0)
                        {
                            const auto muzzle = glm::vec3{weapon[0].model_matrix * glm::vec4{0, .047F, -.172F, 1}};
                            const auto bore =
                                glm::normalize(glm::vec3{weapon[0].model_matrix * glm::vec4{0, 0, -1, 0}});
                            const auto target = camera.position() + camera.forward() * 12.0F;
                            const auto travel =
                                glm::dot(target - muzzle, camera.forward()) / glm::dot(bore, camera.forward());
                            require(
                                glm::distance(muzzle + bore * travel, target) < .002F,
                                "Pistol bore misses the eye ray"
                            );
                        } else if (elapsed >= .18F)
                            require(image.pixels == idle, "Pistol did not recover to its idle pose");
                        else
                            require(image.pixels != idle, "Successful shot has no visible pistol cycle");
                    }
                    // One target resize may allocate; both weapon assets must already be uploaded.
                    const auto prepared = allocation_count(device);
                    const auto shotgun = scene.prepare_shotgun_viewmodel(camera, 0);
                    static_cast<void>(renderer.render_image(scene.render_items(), camera, size.x, size.y, shotgun));
                    const auto pistol = scene.prepare_pistol_viewmodel(camera, 0, .24F);
                    static_cast<void>(renderer.render_image(scene.render_items(), camera, size.x, size.y, pistol));
                    require(allocation_count(device) == prepared, "Weapon switch lazily uploaded a viewmodel");
                }
            scene.shutdown();
        }
        std::cout
            << "pistol_viewmodel=passed; HDR/SDR, 3 FOVs, 2 sizes, 5 poses, reticle and prepared weapon switching\n";
    }

    void check_shotgun_viewmodel(
        const VulkanDevice& device,
        const RenderAuthoringConfig& authoring,
        const CpuRgbaTextureData& atlas,
        const std::filesystem::path& captures
    )
    {
        const auto level = RelayCore::load_blockout_level(RelayCore::core::asset_path("levels/pressure_transfer.toml"));
        for (const bool hdr : {false, true})
        {
            RendererConfig config{};
            config.hdr_enabled = hdr;
            StaticModelCache models;
            RelayGame::WorldSceneRenderer scene;
            scene.initialize(
                RelayGame::build_prepared_world_scene_data(level, authoring, ColorPipelineMode::hardware_srgb),
                models
            );
            VulkanMeshRenderer renderer{
                device,
                RELAYCORE_TEST_SPIRV_DIRECTORY,
                authoring,
                atlas,
                TextureEncoding::srgb,
                config,
                ColorPipelineMode::hardware_srgb
            };
            renderer.prepare_resources(scene.resource_items());
            for (const float fov : {50.0F, 70.0F, 90.0F})
                for (const auto size : {glm::ivec2{640, 360}, glm::ivec2{800, 600}})
                {
                    PerspectiveCamera camera{
                        {.position = {4, 1.65F, 2}, .target = {4, 1.65F, 10}, .vertical_fov_radians = glm::radians(fov)}
                    };
                    camera.set_viewport(size.x, size.y);
                    scene.prepare_render_queue(camera, 0, {}, {}, {}, -1, {}, false);
                    renderer.set_local_lights(scene.local_lights());
                    const auto base = renderer.render_image(scene.render_items(), camera, size.x, size.y);
                    const auto prefix = std::string{"shotgun-"} + (hdr ? "hdr-" : "sdr-") + std::to_string(int(fov))
                                        + "-" + std::to_string(size.x);
                    const auto resting = scene.prepare_shotgun_viewmodel(camera, 0);
                    const auto idle =
                        renderer.render_image(scene.render_items(), camera, size.x, size.y, resting).pixels;
                    std::vector<glm::mat4> idle_models;
                    for (const auto& item : resting)
                        idle_models.push_back(item.model_matrix);
                    for (const float elapsed : {0.0F, .07F, .18F, .45F, .72F, .85F})
                    {
                        const auto weapon = scene.prepare_shotgun_viewmodel(camera, .85F - elapsed);
                        require(!weapon.empty(), "Shotgun asset produced no render parts");
                        const auto image = renderer.render_image(scene.render_items(), camera, size.x, size.y, weapon);
                        save(image, captures / (prefix + "-" + std::to_string(int(elapsed * 1000)) + ".bmp"));
                        require(image.pixels != base.pixels, "Shotgun is invisible at a supported FOV/window size");
                        for (int y = size.y / 2 - 8; y <= size.y / 2 + 8; ++y)
                            for (int x = size.x / 2 - 8; x <= size.x / 2 + 8; ++x)
                                for (int c = 0; c < 3; ++c)
                                {
                                    const auto pixel = (y * size.x + x) * 4 + c;
                                    require(image.pixels[pixel] == base.pixels[pixel], "Shotgun obscures the reticle");
                                }
                        if (elapsed == 0)
                        {
                            const auto muzzle = glm::vec3{weapon[0].model_matrix * glm::vec4{0, -.01645F, -.678F, 1}};
                            const auto bore =
                                glm::normalize(glm::vec3{weapon[0].model_matrix * glm::vec4{0, 0, -1, 0}});
                            const auto target = camera.position() + camera.forward() * 12.0F;
                            const auto travel =
                                glm::dot(target - muzzle, camera.forward()) / glm::dot(bore, camera.forward());
                            require(
                                glm::distance(muzzle + bore * travel, target) < .002F,
                                "Shotgun bore misses the eye ray"
                            );
                        } else if (elapsed >= .72F)
                            require(image.pixels == idle, "Shotgun did not recover to its idle pose");
                        else if (elapsed != .18F)
                            require(image.pixels != idle, "Shotgun recoil/pump cycle is invisible");
                        if (elapsed == .45F)
                        {
                            std::size_t stationary = 0, moving = 0;
                            for (std::size_t i = 0; i < weapon.size(); ++i)
                            {
                                const auto delta =
                                    glm::vec3{(glm::inverse(idle_models[i]) * weapon[i].model_matrix)[3]};
                                if (glm::length(delta) < .0001F)
                                    ++stationary;
                                else
                                {
                                    require(
                                        glm::distance(delta, glm::vec3{0, 0, .07F}) < .0001F,
                                        "Pump left its longitudinal guide"
                                    );
                                    ++moving;
                                }
                            }
                            require(stationary > 0 && moving > 0, "Shotgun body and pump lost independent motion");
                        }
                    }
                    const auto prepared = allocation_count(device);
                    const auto pistol = scene.prepare_pistol_viewmodel(camera, 0, .24F);
                    static_cast<void>(renderer.render_image(scene.render_items(), camera, size.x, size.y, pistol));
                    const auto shotgun = scene.prepare_shotgun_viewmodel(camera, 0);
                    static_cast<void>(renderer.render_image(scene.render_items(), camera, size.x, size.y, shotgun));
                    require(allocation_count(device) == prepared, "Weapon switch lazily uploaded a viewmodel");
                    if (fov == 70 && size.x == 640)
                    {
                        renderer.set_local_lights({});
                        const auto dim_base = renderer.render_image(scene.render_items(), camera, size.x, size.y);
                        const auto dim = renderer.render_image(scene.render_items(), camera, size.x, size.y, shotgun);
                        require(dim.pixels != dim_base.pixels, "Shotgun disappeared without local lights");
                        save(dim, captures / (prefix + "-no-local.bmp"));
                        renderer.set_spot_light(
                            {.position = camera.position(), .direction = camera.forward(), .enabled = true}
                        );
                        const auto lit = renderer.render_image(scene.render_items(), camera, size.x, size.y, shotgun);
                        save(lit, captures / (prefix + "-flashlight.bmp"));
                        renderer.set_spot_light({});
                    }
                }
        }
        std::cout << "shotgun_viewmodel=passed; HDR/SDR, 3 FOVs, 2 sizes, 6 poses, reticle and prepared switching\n";
    }

    void check_resource_preparation(
        VulkanMeshRenderer& renderer,
        const VulkanDevice& device,
        const RenderAuthoringConfig& authoring
    )
    {
        const auto statistics = [&] {
            VmaTotalStatistics stats{};
            vmaCalculateStatistics(device.allocator(), &stats);
            return stats.total.statistics;
        };
        StaticModelCache models;
        const auto level = RelayCore::load_blockout_level(RelayCore::core::asset_path("levels/pressure_transfer.toml"));
        const auto data = RelayGame::build_prepared_world_scene_data(level, authoring, ColorPipelineMode::legacy_gamma);
        PerspectiveCamera camera{{.position = {4, 1.65F, 2}, .target = {4, 1.65F, 10}}};
        camera.set_viewport(width, height);
        uint32_t previous_allocations = 0;
        VkDeviceSize previous_bytes = 0;
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            RelayGame::WorldSceneRenderer scene;
            scene.initialize(data, models);
            scene.prepare_render_queue(camera, 0, {}, {}, {}, -1, {}, false);
            const auto viewmodel = scene.prepare_shotgun_viewmodel(camera, 0);
            std::vector<RenderItem> casters{scene.render_items().begin(), scene.render_items().end()};
            for (auto& caster : casters)
            {
                caster.normal_texture = nullptr;
                caster.metallic_roughness.reset();
            }
            const auto lazy = renderer.render_image(scene.render_items(), camera, width, height, viewmodel);
            renderer.prepare_frame(scene.render_items(), casters, camera, width, height, viewmodel);
            require(renderer.capture_prepared_image().pixels == lazy.pixels, "Second scene slot changed pixels");
            const auto lazy_stats = statistics();
            renderer.prepare_resources(scene.resource_items());
            const auto prepared = statistics();
            std::cout << "pressure_resources: cycle=" << cycle << "; lazy_bytes=" << lazy_stats.allocationBytes
                      << "; prepared_bytes=" << prepared.allocationBytes << "; allocations=" << prepared.allocationCount
                      << '\n';
            {
                RelayCore::test::BlockGraphicsQueue gate{device};
                gate.block();
                // Both prepared frames must return while the GPU is still blocked.
                renderer.prepare_frame(scene.render_items(), casters, camera, width, height, viewmodel);
                renderer.prepare_frame(scene.render_items(), casters, camera, width, height, viewmodel);
                gate.release();
                if (cycle == 0)
                    renderer.reload_shaders(RELAYCORE_TEST_SPIRV_DIRECTORY);
            }
            require(
                renderer.capture_prepared_image().pixels == lazy.pixels,
                "Resource preparation changed first-frame pixels"
            );
            require(
                statistics().allocationCount == prepared.allocationCount
                    && statistics().allocationBytes == prepared.allocationBytes,
                "Prepared scene/viewmodel still allocated GPU resources while rendering"
            );
            if (cycle > 0)
                require(
                    prepared.allocationCount == previous_allocations && prepared.allocationBytes == previous_bytes,
                    "Repeated scene preparation retained obsolete GPU resources"
                );
            previous_allocations = prepared.allocationCount;
            previous_bytes = prepared.allocationBytes;
            {
                StaticColorMesh uploaded, invalid;
                uploaded.initialize(make_color_box_mesh_data());
                const std::array broken{RenderItem{.mesh = &uploaded}, RenderItem{.mesh = &invalid}};
                expect_failure(
                    [&] {
                        renderer.prepare_resources(broken);
                    },
                    "initialized CPU mesh"
                );
            }
            renderer.prepare_resources(scene.resource_items());
            require(
                statistics().allocationCount == prepared.allocationCount
                    && statistics().allocationBytes == prepared.allocationBytes,
                "Failed resource preparation retained abandoned GPU resources"
            );
            require(
                renderer.render_image(scene.render_items(), camera, width, height, viewmodel).pixels == lazy.pixels,
                "Failed resource preparation changed the active scene"
            );
        }
    }

    void check_pipeline(VulkanMeshRenderer& renderer, const std::filesystem::path& captures, const VulkanDevice& device)
    {
        PerspectiveCamera camera{{.position = {0, 0, 3}, .target = {0, 0, 0}}};
        camera.set_viewport(width, height);
        auto box = make_color_box_mesh_data();
        for (auto& v : box.vertices)
        {
            v.color = {1, 1, 1};
            v.uv = {v.position.x + .5F, v.position.y + .5F};
        }
        StaticColorMesh mesh;
        mesh.initialize(box);
        CpuRgbaTextureData data{2, 2, {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 0, 255}};
        Texture2D texture;
        uint32_t previous_allocations = 0;
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            texture.initialize_rgba(2, 2, data.pixels, cycle == 1 ? TextureEncoding::srgb : TextureEncoding::linear);
            mesh.initialize(box);
            RenderQueue queue;
            queue.push(
                mesh,
                glm::translate(glm::mat4{1}, {-.4F, .2F, 0}) * glm::scale(glm::mat4{1}, {1.3F, .8F, 1}),
                RenderPass::Opaque,
                &texture
            );
            queue.push(mesh, glm::translate(glm::mat4{1}, {.7F, -.35F, -1}), RenderPass::Opaque, &texture);
            compare(
                renderer.render_image(queue.items(), camera, width, height),
                "mesh-reload-" + std::to_string(cycle),
                captures
            );
            if (cycle == 0)
            {
                const auto before = renderer.render_image(queue.items(), camera, width, height);
                RelayCore::test::ScopedTempDirectory missing;
                expect_failure(
                    [&] {
                        renderer.reload_shaders(missing.path());
                    },
                    "vertex_main"
                );
                require(
                    renderer.render_image(queue.items(), camera, width, height).pixels == before.pixels,
                    "Failed shader reload changed working pipeline"
                );
                std::filesystem::copy_file(
                    std::filesystem::path(RELAYCORE_TEST_SPIRV_DIRECTORY) / "mesh.vert.spv",
                    missing.path() / "mesh.vert.spv"
                );
                expect_failure(
                    [&] {
                        renderer.reload_shaders(missing.path());
                    },
                    "fragment_main"
                );
                std::ofstream(missing.path() / "mesh.frag.spv", std::ios::binary) << "invalid";
                expect_failure(
                    [&] {
                        renderer.reload_shaders(missing.path());
                    },
                    "malformed SPIR-V"
                );
                renderer.reload_shaders(RELAYCORE_TEST_SPIRV_DIRECTORY);
                require(
                    renderer.render_image(queue.items(), camera, width, height).pixels == before.pixels,
                    "Successful shader reload changed mesh output"
                );
            }
            const auto allocations = allocation_count(device);
            if (cycle != 0)
                require(allocations == previous_allocations, "Mesh/texture reload retained obsolete VMA allocations");
            previous_allocations = allocations;
        }
        expect_failure(
            [&] {
                static_cast<void>(renderer.render_image({}, camera, 0, height));
            },
            "positive dimensions"
        );
        expect_failure(
            [&] {
                static_cast<void>(renderer.render_image({}, camera, std::numeric_limits<int>::max(), height));
            },
            "device framebuffer/image limits"
        );
        auto invalid = box;
        invalid.indices[0] = static_cast<uint32_t>(invalid.vertices.size());
        expect_failure(
            [&] {
                mesh.initialize(invalid);
            },
            "outside its vertex"
        );
        {
            RelayCore::test::ScopedTempDirectory broken;
            for (const auto* name :
                 {"mesh.vert.spv",
                  "mesh.frag.spv",
                  "shadow.vert.spv",
                  "shadow.frag.spv",
                  "point_shadow.vert.spv",
                  "point_shadow.frag.spv"})
                std::filesystem::copy_file(
                    std::filesystem::path(RELAYCORE_TEST_SPIRV_DIRECTORY) / name,
                    broken.path() / name
                );
            std::ofstream(broken.path() / "point_shadow.frag.spv", std::ios::binary | std::ios::trunc) << "invalid";
            RenderQueue queue;
            queue.push(mesh, glm::mat4{1});
            const auto before = renderer.render_image(queue.items(), camera, width, height);
            expect_failure(
                [&] {
                    renderer.reload_shaders(broken.path());
                },
                "point_shadow.frag.spv"
            );
            require(
                renderer.render_image(queue.items(), camera, width, height).pixels == before.pixels,
                "Failed shadow reload changed the published mesh/shadow pipeline set"
            );
        }
        const auto resized = renderer.render_image({}, camera, 127, 63);
        require(
            resized.pixels.size() == 127 * 63 * 4 && resized.width == 127 && resized.height == 63,
            "Vulkan mesh target resize failed"
        );
    }

    void check_passes(VulkanMeshRenderer& renderer, const std::filesystem::path& captures)
    {
        PerspectiveCamera camera{{.position = {0, 0, 4}, .target = {0, 0, 0}}};
        camera.set_viewport(width, height);
        const auto make_quad = [](const glm::vec3 color, const float material, const bool reversed = false) {
            CpuColorMeshData data{
                .vertices =
                    {{{-.5F, -.5F, 0}, color, {0, 0, 1}, material},
                     {{.5F, -.5F, 0}, color, {0, 0, 1}, material},
                     {{.5F, .5F, 0}, color, {0, 0, 1}, material},
                     {{-.5F, .5F, 0}, color, {0, 0, 1}, material}},
                .indices = reversed ? std::vector<uint32_t>{0, 2, 1, 0, 3, 2} : std::vector<uint32_t>{0, 1, 2, 0, 2, 3}
            };
            StaticColorMesh mesh;
            mesh.initialize(data);
            return mesh;
        };
        auto back = make_quad({.24F, .12F, .08F}, 33);
        auto green = make_quad({.1F, .85F, .2F}, static_cast<float>(RelayCore::SurfaceMaterial::Glass), true);
        auto blue = make_quad({.1F, .2F, .9F}, static_cast<float>(RelayCore::SurfaceMaterial::Glass));
        auto decal = make_quad({.9F, .7F, .1F}, 33, true);
        auto steam = make_quad({.8F, .8F, .85F}, static_cast<float>(RelayCore::surface_material_count() + 1), true);
        Texture2D white;
        const std::array<uint8_t, 4> pixels{255, 255, 255, 255};
        white.initialize_rgba(1, 1, pixels, TextureEncoding::linear);
        const auto transform = [](const float z, const float scale) {
            return glm::translate(glm::mat4{1}, glm::vec3{0, 0, z})
                   * glm::scale(glm::mat4{1}, glm::vec3{scale, scale, 1});
        };
        const auto check =
            [&](const RenderQueue& queue, const std::string& name, const std::span<const RenderItem> viewmodel = {}) {
                compare(renderer.render_image(queue.items(), camera, width, height, viewmodel), name, captures);
            };
        RenderQueue queue;
        // Queue order is deliberately interleaved; pass order dominates, submission order inside each pass remains intact.
        queue.push(green, transform(1, 2), RenderPass::Transparent, &white);
        queue.push(decal, transform(.5F, 1), RenderPass::Overlay);
        queue.push(back, transform(-2, 5));
        queue.push(blue, transform(-.5F, 2), RenderPass::Transparent, &white);
        check(queue, "passes-order-depth-cull");
        const auto before = renderer.render_image(queue.items(), camera, width, height);
        RenderQueue reverse;
        reverse.push(blue, transform(-.5F, 2), RenderPass::Transparent, &white);
        reverse.push(decal, transform(.5F, 1), RenderPass::Overlay);
        reverse.push(back, transform(-2, 5));
        reverse.push(green, transform(1, 2), RenderPass::Transparent, &white);
        check(reverse, "passes-reversed-transparency");
        require(
            before.pixels != renderer.render_image(reverse.items(), camera, width, height).pixels,
            "Transparent submission order was silently changed"
        );
        queue.push(back, transform(2, .5F));
        check(queue, "passes-opaque-occlusion");
        queue.push(steam, transform(2.5F, 1.2F), RenderPass::Transparent);
        check(queue, "passes-steam");
        RenderQueue weapon;
        weapon.push(blue, transform(-3, .8F), RenderPass::Transparent, &white);
        check(queue, "passes-viewmodel-restores-state", weapon.items());
        renderer.reload_shaders(RELAYCORE_TEST_SPIRV_DIRECTORY);
        check(queue, "passes-reload");
        check(reverse, "passes-next-frame-restores-state");
        RenderQueue distant;
        distant.push(back, transform(-21, 20));
        distant.push(green, transform(-20, 12), RenderPass::Transparent, &white);
        check(distant, "passes-glass-fog-alpha");
        RenderQueue glass_overlay;
        glass_overlay.push(back, transform(-2, 5));
        glass_overlay.push(green, transform(1, 2), RenderPass::Overlay, &white);
        check(glass_overlay, "passes-overlay-forces-alpha");
        glass_overlay.push(steam, transform(2, 1), RenderPass::Overlay);
        check(glass_overlay, "passes-overlay-steam-alpha");
        auto smooth_data = *blue.cpu_data();
        smooth_data.vertices[0].normal = {1, 0, 1};
        smooth_data.vertices[1].normal = {-1, 0, 1};
        smooth_data.vertices[2].normal = {-1, 1, 1};
        smooth_data.vertices[3].normal = {1, -1, 1};
        StaticColorMesh smooth_glass;
        smooth_glass.initialize(smooth_data);
        RenderQueue smooth;
        smooth.push(back, transform(-2, 5));
        smooth.push(smooth_glass, glm::scale(glm::mat4{1}, glm::vec3{2, .5F, 3}), RenderPass::Transparent, &white);
        check(smooth, "passes-smooth-glass-transform");
    }

    void check_mesh_pipeline(
        const VulkanDevice& device,
        const CpuRgbaTextureData& atlas,
        const std::filesystem::path& captures
    )
    {
        const auto authoring = load_render_authoring_config(
            std::filesystem::path{RELAYCORE_TEST_BASELINE_DIRECTORY} / "render_authoring.toml"
        );
        VulkanMeshRenderer renderer{
            device,
            RELAYCORE_TEST_SPIRV_DIRECTORY,
            authoring,
            atlas,
            TextureEncoding::linear,
            RendererConfig{}
        };
        check_pipeline(renderer, captures, device);
        check_passes(renderer, captures);
    }

    void check_lighting(
        const VulkanDevice& device,
        const RenderAuthoringConfig& defaults,
        const CpuRgbaTextureData& atlas,
        const std::filesystem::path& captures
    )
    {
        StaticColorMesh panel, cloth, model;
        auto box = make_color_box_mesh_data();
        for (auto& vertex : box.vertices)
        {
            vertex.color = {.24F, .31F, .38F};
            vertex.material_kind = static_cast<float>(RelayCore::SurfaceMaterial::WallPanel);
            vertex.uv = {vertex.position.x + .5F, vertex.position.y + .5F};
        }
        panel.initialize(box);
        for (auto& vertex : box.vertices)
            vertex.material_kind = static_cast<float>(RelayCore::SurfaceMaterial::Cotton);
        cloth.initialize(box);
        for (auto& vertex : box.vertices)
        {
            vertex.material_kind = static_cast<float>(RelayCore::SurfaceMaterial::Metal);
            vertex.normal = glm::normalize(vertex.normal + glm::vec3{.2F, .15F, .1F});
        }
        model.initialize(box);
        PerspectiveCamera camera{{.position = {0, 1.2F, 4}, .target = {0, 0, 0}}};
        camera.set_viewport(width, height);
        for (const auto pipeline : {ColorPipelineMode::legacy_gamma, ColorPipelineMode::hardware_srgb})
        {
            const auto encoding =
                pipeline == ColorPipelineMode::hardware_srgb ? TextureEncoding::srgb : TextureEncoding::linear;
            Texture2D texture;
            const std::array<uint8_t, 16>
                pixels{155, 75, 35, 255, 80, 180, 100, 255, 40, 95, 200, 255, 190, 160, 80, 255};
            texture.initialize_rgba(2, 2, pixels, encoding);
            RenderQueue queue;
            queue.push(panel, glm::translate(glm::mat4{1}, {-1.1F, 0, 0}));
            queue.push(cloth, glm::rotate(glm::mat4{1}, .4F, glm::vec3{0, 0, 1}));
            queue.push(
                model,
                glm::translate(glm::mat4{1}, {1.1F, 0, 0})
                    * glm::rotate(glm::scale(glm::mat4{1}, {.8F, 1.4F, .6F}), .4F, glm::vec3{0, 1, 0}),
                RenderPass::Opaque,
                &texture
            );
            for (int variant = 0; variant < 3; ++variant)
            {
                auto authoring = defaults;
                RendererConfig config{};
                config.shadow_enabled = false;
                if (variant == 1)
                {
                    config.shadow_light_intensity = 0;
                    config.indirect_bounce_strength = 0;
                }
                if (variant == 2)
                {
                    config.shadow_light_direction = {};
                    config.shadow_light_color = {.65F, .25F, .15F};
                    config.shadow_light_intensity = .3F;
                    config.indirect_bounce_strength = 1.7F;
                    authoring.lighting.ambient_floor = .22F;
                    authoring.lighting.ambient_zenith = .61F;
                    authoring.lighting.ambient_min = .12F;
                    authoring.lighting.ambient_max = .84F;
                    authoring.lighting.cool_ambient_color = {.17F, .21F, .32F};
                    authoring.lighting.cool_ambient_floor = .21F;
                    authoring.lighting.cool_ambient_zenith = .51F;
                    authoring.lighting.warm_bounce_color = {.25F, .12F, .07F};
                    authoring.lighting.warm_bounce_floor = .19F;
                    authoring.lighting.warm_bounce_underside = .31F;
                    authoring.lighting.bounce_neutral_color = {.23F, .12F, .18F};
                    authoring.lighting.bounce_floor = .13F;
                    authoring.lighting.bounce_underside = .27F;
                    authoring.lighting.bounce_darkness = .12F;
                    authoring.lighting.cavity_broad_strength = .23F;
                    authoring.lighting.cavity_micro_strength = .14F;
                    authoring.lighting.cavity_underside_strength = .21F;
                    authoring.lighting.cavity_ambient_darken = .52F;
                    authoring.lighting.local_light_wrap = .41F;
                    authoring.lighting.local_light_fill = .59F;
                    authoring.lighting.local_light_gain = 1.61F;
                    authoring.materials[static_cast<size_t>(RelayCore::SurfaceMaterial::WallPanel)].ambient_bias = .71F;
                    authoring.materials[static_cast<size_t>(RelayCore::SurfaceMaterial::Cotton)].bounce_mix = .17F;
                }
                VulkanMeshRenderer
                    renderer{device, RELAYCORE_TEST_SPIRV_DIRECTORY, authoring, atlas, encoding, config, pipeline};
                const auto prefix =
                    std::string("lighting-") + color_pipeline_mode_name(pipeline) + "-" + std::to_string(variant);
                const auto check = [&](const std::span<const LocalLight> lights, const std::string& suffix) {
                    renderer.set_local_lights(lights);
                    auto image = renderer.render_image(queue.items(), camera, width, height);
                    compare(image, prefix + suffix, captures);
                    return image;
                };
                const auto without_local = check({}, "-ambient-directional");
                std::array<LocalLight, 9> lights{};
                for (size_t i = 0; i < lights.size(); ++i)
                    lights[i] =
                        {{float(i) * .3F - 1, .8F, 1}, visual_rgb_for_pipeline({.3F, .2F, .1F}, pipeline), 2.5F};
                const auto single = check(std::span{lights}.first(1), "-single");
                require(single.pixels != without_local.pixels, "Local light had no observable contribution");
                const auto eight = check(std::span{lights}.first(8), "-eight");
                require(eight.pixels != single.pixels, "Eight-light budget was not exercised");
                lights[8].color = {100, 100, 100};
                require(
                    check(lights, "-over-budget").pixels == eight.pixels,
                    "Renderer did not preserve the first-eight light budget"
                );
                lights[0].radius = .00001F;
                require(
                    check(std::span{lights}.first(1), "-tiny-radius").pixels == without_local.pixels,
                    "Out-of-radius light affected the scene"
                );
                renderer.reload_shaders(RELAYCORE_TEST_SPIRV_DIRECTORY);
                require(
                    check({}, "-cleared-reload").pixels == without_local.pixels,
                    "Cleared lights leaked through buffer reuse or shader reload"
                );
                camera.set_position({0, -1.2F, 4});
                camera.set_target({0, 0, 0});
                check(std::span{lights}.first(8), "-underside");
                camera.set_position({0, 1.2F, 4});
                camera.set_target({0, 0, 0});
            }
        }
    }

    void check_shadows(
        const VulkanDevice& device,
        const RelayCore::Window& window,
        const RenderAuthoringConfig& authoring,
        const CpuRgbaTextureData& atlas,
        const std::filesystem::path& captures
    )
    {
        auto box = make_color_box_mesh_data();
        for (auto& vertex : box.vertices)
        {
            vertex.color = {.28F, .31F, .35F};
            vertex.material_kind = static_cast<float>(RelayCore::SurfaceMaterial::Metal);
        }
        StaticColorMesh mesh;
        mesh.initialize(box);
        RenderQueue queue;
        queue.push(mesh, glm::translate(glm::mat4{1}, {0, -.6F, -2}) * glm::scale(glm::mat4{1}, {12, .2F, 18}));
        queue.push(mesh, glm::mat4{1});
        queue.push(mesh, glm::translate(glm::mat4{1}, {-1.5F, .4F, -3}) * glm::scale(glm::mat4{1}, {1.2F, 2, 1.5F}));
        queue.push(mesh, glm::translate(glm::mat4{1}, {2, .2F, -7}) * glm::scale(glm::mat4{1}, {1.6F, 1.6F, 1.2F}));
        StaticColorMesh thin_wall;
        auto wall = make_color_double_sided_quad_mesh_data();
        for (auto& vertex : wall.vertices)
        {
            vertex.color = {.3F, .3F, .3F};
            vertex.material_kind = 1;
        }
        thin_wall.initialize(wall);
        queue.push(
            thin_wall,
            glm::translate(glm::mat4{1}, {1, 1, -2}) * glm::scale(glm::mat4{1}, {2, 2, 1}),
            RenderPass::Opaque,
            nullptr,
            false
        );
        PerspectiveCamera camera{{.position = {4, 3, 8}, .target = {0, 0, -2}}};
        camera.set_viewport(width, height);
        for (const auto pipeline : {ColorPipelineMode::legacy_gamma, ColorPipelineMode::hardware_srgb})
        {
            const auto encoding =
                pipeline == ColorPipelineMode::hardware_srgb ? TextureEncoding::srgb : TextureEncoding::linear;
            for (int mode = 0; mode < 3; ++mode)
            {
                RendererConfig config{};
                config.shadow_enabled = mode != 2;
                config.shadow_map_resolution = 512;
                config.shadow_focus_distance = 4;
                config.shadow_projection_radius = 6;
                config.shadow_projection_depth = 18;
                config.shadow_far_focus_distance = 10;
                config.shadow_far_projection_radius = 12;
                config.shadow_far_projection_depth = 28;
                config.shadow_cascade_split_distance = 9;
                config.shadow_cascade_blend_range = 5;
                config.shadow_strength = .9F;
                config.shadow_light_intensity = mode == 0 ? 1.8F : 0;
                config.shadow_light_color = {.8F, .6F, .4F};
                config.shadow_light_direction = {.5F, -1, .25F};
                config.shadow_bias_near = .0003F;
                config.shadow_bias_far = .0005F;
                config.shadow_normal_offset_near = .006F;
                config.shadow_normal_offset_far = .01F;
                VulkanMeshRenderer
                    renderer{device, RELAYCORE_TEST_SPIRV_DIRECTORY, authoring, atlas, encoding, config, pipeline};
                auto scene_queue = queue;
                if (mode == 1)
                {
                    constexpr std::array directions{
                        glm::vec3{1, 0, 0},
                        glm::vec3{-1, 0, 0},
                        glm::vec3{0, 1, 0},
                        glm::vec3{0, -1, 0},
                        glm::vec3{0, 0, 1},
                        glm::vec3{0, 0, -1}
                    };
                    for (const auto& direction : directions)
                        scene_queue.push(
                            mesh,
                            glm::translate(glm::mat4{1}, glm::vec3{0, 2, 2} + direction * 1.5F)
                                * glm::scale(glm::mat4{1}, glm::vec3{.5F})
                        );
                }
                const auto prefix =
                    std::string("shadows-") + color_pipeline_mode_name(pipeline) + "-" + std::to_string(mode);
                const auto check = [&](std::span<const RenderItem> items, const std::string& suffix) {
                    auto image = renderer.render_image(items, camera, width, height);
                    compare(image, prefix + suffix, captures);
                    return image;
                };
                const auto baseline = check(scene_queue.items(), "-baseline");
                if (mode == 0)
                {
                    auto excluded = std::vector<RenderItem>(scene_queue.items().begin(), scene_queue.items().end());
                    for (auto& item : excluded)
                        item.casts_directional_shadow = false;
                    require(
                        check(excluded, "-excluded-casters").pixels != baseline.pixels,
                        "Directional caster exclusion had no observable effect"
                    );
                    for (int step = 0; step < 3; ++step)
                    {
                        camera.set_position({4, 3, float(3 + step * 3)});
                        check(scene_queue.items(), "-cascade-" + std::to_string(step));
                    }
                    camera.set_position({4, 3, 8});
                } else if (mode == 1)
                {
                    const std::array light{
                        LocalLight{{0, 2, 2}, visual_rgb_for_pipeline({1.0F, .7F, .4F}, pipeline) * 1.3F, 12}
                    };
                    renderer.set_local_lights(light);

                    const auto shadowed = check(scene_queue.items(), "-point");
                    require(
                        shadowed.pixels != baseline.pixels,
                        "Point light/shadow path had no observable contribution"
                    );
                    auto unshadowed_config = config;
                    unshadowed_config.shadow_enabled = false;
                    VulkanMeshRenderer unshadowed_renderer{
                        device,
                        RELAYCORE_TEST_SPIRV_DIRECTORY,
                        authoring,
                        atlas,
                        encoding,
                        unshadowed_config,
                        pipeline
                    };

                    unshadowed_renderer.set_local_lights(light);
                    const auto unshadowed =
                        unshadowed_renderer.render_image(scene_queue.items(), camera, width, height);
                    compare(unshadowed, prefix + "-point-shadows-disabled", captures);
                    require(unshadowed.pixels != shadowed.pixels, "Point shadows had no observable occlusion");
                    const std::array positions{
                        glm::vec3{7, 2, 0},
                        glm::vec3{-7, 2, 0},
                        glm::vec3{0, 7, 0},
                        glm::vec3{0, -7, 0},
                        glm::vec3{0, 2, 7},
                        glm::vec3{0, 2, -9}
                    };
                    for (size_t face = 0; face < positions.size(); ++face)
                    {
                        camera.set_position(positions[face]);
                        camera.set_target({0, 0, -2});
                        check(scene_queue.items(), "-point-face-" + std::to_string(face));
                    }
                    camera.set_position({4, 3, 8});
                    camera.set_target({0, 0, -2});
                    std::array<LocalLight, 8> lights{};
                    for (size_t i = 0; i < 8; ++i)
                        lights[i] = {{float(i) - 3, 2, 2}, visual_rgb_for_pipeline({.15F, .11F, .08F}, pipeline), 14};
                    renderer.set_local_lights(lights);

                    check(scene_queue.items(), "-eight-points");
                    lights[0].radius = .05F;
                    lights[1].color = {};
                    lights[2].position = {1000, 0, 0};
                    renderer.set_local_lights(lights);

                    check(scene_queue.items(), "-point-mask-changed");
                    renderer.set_local_lights({});

                    require(
                        check(scene_queue.items(), "-points-cleared").pixels == baseline.pixels,
                        "Point mask/atlas retained stale shadows"
                    );
                } else
                {
                    const SpotLight
                        spot{{0, 2, 5}, glm::normalize(glm::vec3{.12F, -.3F, -1}), {1.0F, .75F, .5F}, 18, true};
                    renderer.set_spot_light(spot);

                    require(
                        check(scene_queue.items(), "-spot").pixels != baseline.pixels,
                        "Spot light with directional shadows disabled had no contribution"
                    );
                    auto removed = std::vector<RenderItem>(scene_queue.items().begin(), scene_queue.items().end() - 1);
                    check(removed, "-spot-thin-wall-removed");
                    auto disabled = spot;
                    disabled.enabled = false;
                    renderer.set_spot_light(disabled);

                    require(
                        check(scene_queue.items(), "-spot-cleared").pixels == baseline.pixels,
                        "Disabled spot retained stale lighting"
                    );
                }
                renderer.reload_shaders(RELAYCORE_TEST_SPIRV_DIRECTORY);
                check(scene_queue.items(), "-reload");
                if (mode == 1)
                {
                    const std::array light{
                        LocalLight{{0, 2, 2}, visual_rgb_for_pipeline({1.0F, .7F, .4F}, pipeline) * 1.3F, 12}
                    };
                    renderer.set_local_lights(light);
                }
                if (mode == 2)
                    renderer.set_spot_light(
                        {{0, 2, 5}, glm::normalize(glm::vec3{.12F, -.3F, -1}), {1.0F, .75F, .5F}, 18, true}
                    );
                VulkanSwapchain swapchain{device, window, false};
                require(
                    swapchain.draw_scene(renderer, scene_queue.items(), camera),
                    "Shadowed scene failed to present"
                );
                require(
                    swapchain.draw_scene(renderer, scene_queue.items(), camera),
                    "Repeated shadowed scene failed to present"
                );
            }
        }
    }

    void check_point_shadow_receiver(
        const VulkanDevice& device,
        const RenderAuthoringConfig& authoring,
        const CpuRgbaTextureData& atlas,
        const std::filesystem::path& captures
    )
    {
        auto box = make_color_box_mesh_data();
        for (auto& vertex : box.vertices)
        {
            vertex.color = {.5F, .5F, .5F};
            vertex.material_kind = 1;
        }
        StaticColorMesh blocker;
        blocker.initialize(box);
        PerspectiveCamera camera{{.position = {0, 1.65F, -1.7F}, .target = {0, 3.6F, 0}}};
        camera.set_viewport(width, height);
        for (const bool smooth_normals : {false, true})
        {
            auto receiver = box;
            for (auto& vertex : receiver.vertices)
            {
                vertex.position *= glm::vec3{12, .2F, 12};
                if (smooth_normals && vertex.normal.y < -.5F)
                    vertex.normal = glm::normalize(glm::vec3{vertex.position.x * .18F, -1, vertex.position.z * .18F});
            }
            StaticColorMesh ceiling;
            ceiling.initialize(receiver);
            RenderQueue queue;
            queue.push(ceiling, glm::translate(glm::mat4{1}, {0, 3.6F, 0}));
            for (const auto pipeline : {ColorPipelineMode::legacy_gamma, ColorPipelineMode::hardware_srgb})
            {
                RendererConfig config{};
                config.shadow_light_intensity = 0;
                const auto encoding =
                    pipeline == ColorPipelineMode::hardware_srgb ? TextureEncoding::srgb : TextureEncoding::linear;
                VulkanMeshRenderer
                    renderer{device, RELAYCORE_TEST_SPIRV_DIRECTORY, authoring, atlas, encoding, config, pipeline};
                const std::array lights{LocalLight{{0, 2.6F, 0}, visual_rgb_for_pipeline({.6F, .5F, .4F}, pipeline), 5}
                };
                renderer.set_local_lights(lights);
                const auto shadowed = renderer.render_image(queue.items(), camera, width, height);
                config.shadow_enabled = false;
                renderer.set_config(config);
                const auto lit = renderer.render_image(queue.items(), camera, width, height);
                const auto prefix = std::string{"point-receiver-"} + color_pipeline_mode_name(pipeline)
                                    + (smooth_normals ? "-smooth" : "-flat");
                save(shadowed, captures / (prefix + "-shadowed.bmp"));
                save(lit, captures / (prefix + "-lit.bmp"));
                size_t darkened = 0;
                for (size_t i = 0; i < lit.pixels.size(); i += 4)
                    if (int(lit.pixels[i]) - int(shadowed.pixels[i]) > 3)
                        ++darkened;
                std::cout << prefix << " self_shadow_pixels=" << darkened << '\n';
                require(darkened < size_t(width * height) / 100, "Unoccluded ceiling self-shadows under a point light");
                auto blocked_queue = queue;
                blocked_queue.push(
                    blocker,
                    glm::translate(glm::mat4{1}, {.8F, 3.05F, 0}) * glm::scale(glm::mat4{1}, {.3F, .15F, .3F})
                );
                const auto blocked_lit = renderer.render_image(blocked_queue.items(), camera, width, height);
                config.shadow_enabled = true;
                renderer.set_config(config);
                const auto blocked = renderer.render_image(blocked_queue.items(), camera, width, height);
                darkened = 0;
                for (size_t i = 0; i < blocked.pixels.size(); i += 4)
                    if (int(blocked_lit.pixels[i]) - int(blocked.pixels[i]) > 3)
                        ++darkened;
                require(darkened > 100, "Point receiver correction removed real blocker shadows");
                save(blocked, captures / (prefix + "-blocked.bmp"));
            }
        }
    }

    void check_light_selection(
        const VulkanDevice& device,
        const RenderAuthoringConfig& authoring,
        const CpuRgbaTextureData& atlas,
        const std::filesystem::path& captures
    )
    {
        const auto level = RelayCore::load_blockout_level(RelayCore::core::asset_path("levels/pressure_transfer.toml"));
        auto prepared = RelayGame::build_prepared_world_scene_data(level, authoring, ColorPipelineMode::legacy_gamma);
        prepared.local_lights.clear();
        for (int i = 0; i < 10; ++i)
            prepared.local_lights.push_back({{{float(i) + 4, 3, 2}, {.16F, .12F, .08F}, 5}, {}});
        prepared.local_lights.push_back({{{4, 1.65F, 2}, {.1F, .6F, .2F}, 9}, "lighting-test"});
        StaticModelCache models{};
        RelayGame::WorldSceneRenderer scene;
        scene.initialize(prepared, models);
        VulkanMeshRenderer renderer{
            device,
            RELAYCORE_TEST_SPIRV_DIRECTORY,
            authoring,
            atlas,
            TextureEncoding::linear,
            RendererConfig{}
        };
        PerspectiveCamera camera{{.position = {4, 1.65F, 2}, .target = {4, 1.65F, 10}}};
        camera.set_viewport(width, height);
        const auto prepare = [&](const std::span<const std::string> targets = {},
                                 const std::span<const RelayGame::EnvironmentEffect> effects = {}) {
            scene.prepare_render_queue(camera, 0, {}, {}, {}, -1, {}, false, targets, {}, effects);
            require(scene.local_lights().size() == 8, "World selection did not retain its eight-light budget");
            renderer.set_local_lights(scene.local_lights());
        };
        const auto check = [&](const char* name) {
            auto image = renderer.render_image(scene.render_items(), camera, width, height);
            compare(image, name, captures);
            return image;
        };
        prepare();
        for (size_t i = 0; i < 8; ++i)
            require(scene.local_lights()[i].position.x == float(i) + 4, "Nearest-light selection order changed");
        const auto initial = check("lighting-world-nearest-eight");
        const std::array targets{std::string{"lighting-test"}};
        prepare(targets);
        require(scene.local_lights()[0].radius == 9, "Target-gated source was not selected");
        require(
            check("lighting-world-target-active").pixels != initial.pixels,
            "Target activation did not change lighting"
        );
        prepare();
        require(
            check("lighting-world-target-cleared").pixels == initial.pixels,
            "Deactivated target left stale GPU light data"
        );
        const std::array effects{
            RelayGame::EnvironmentEffect{.kind = RelayGame::EnvironmentKind::Lamp, .light_index = 0, .strength = .25F}
        };
        prepare({}, effects);
        require(
            scene.local_lights()[0].color.x == .16F * .25F,
            "Environment lamp strength was not applied before submission"
        );
        require(
            check("lighting-world-lamp-strength").pixels != initial.pixels,
            "Lamp strength did not change lighting"
        );
        camera.set_position({13, 1.65F, 2});
        camera.set_target({4, 1.65F, 10});
        prepare();
        require(scene.local_lights()[0].position.x == 13, "Camera movement did not update selected nearest lights");
        check("lighting-world-camera-switch");
        scene.shutdown();
        renderer.set_local_lights({});
    }

    void check_color_baseline(
        const VulkanDevice& device,
        const RelayCore::Window& window,
        const RenderAuthoringConfig& authoring,
        const CpuRgbaTextureData& atlas,
        const std::filesystem::path& captures
    )
    {
        const auto encode = [](const float linear) {
            return linear <= .0031308F ? linear * 12.92F : 1.055F * std::pow(linear, 1 / 2.4F) - .055F;
        };
        const auto decode = [](const float encoded) {
            return encoded <= .04045F ? encoded / 12.92F : std::pow((encoded + .055F) / 1.055F, 2.4F);
        };
        PerspectiveCamera camera{{.position = {0, 0, 3}, .target = {0, 0, 0}}};
        camera.set_viewport(width, height);
        auto data = make_color_double_sided_quad_mesh_data();
        const glm::vec3 gray{.5F, .18F, .0031308F};
        for (auto& v : data.vertices)
        {
            v.color = gray;
            v.material_kind = float(RelayCore::surface_material_count() + 2);
        }
        StaticColorMesh lamp;
        lamp.initialize(data);
        RenderQueue queue;
        queue.push(lamp, glm::mat4{1});
        for (const auto mode : {ColorPipelineMode::legacy_gamma, ColorPipelineMode::hardware_srgb})
        {
            RendererConfig config{};
            config.shadow_enabled = false;
            config.fog_enabled = false;
            const auto encoding =
                mode == ColorPipelineMode::hardware_srgb ? TextureEncoding::srgb : TextureEncoding::linear;
            VulkanMeshRenderer
                renderer{device, RELAYCORE_TEST_SPIRV_DIRECTORY, authoring, atlas, encoding, config, mode};
            const auto check_pixel = [&](const CpuRgbaTextureData& image, const glm::vec3 expected, const char* message
                                     ) {
                const auto offset = size_t((height / 2) * width + width / 2) * 4;
                for (int c = 0; c < 3; ++c)
                    require(
                        std::abs(int(image.pixels[offset + c]) - int(std::lround(expected[c] * 255))) <= 1,
                        message
                    );
                require(image.pixels[offset + 3] == 255, "Opaque Vulkan color target changed alpha");
            };
            const auto convert = [&](glm::vec3 value) {
                if (mode == ColorPipelineMode::hardware_srgb)
                    for (int c = 0; c < 3; ++c)
                        value[c] = encode(value[c]);
                return value;
            };
            const auto image = renderer.render_image(queue.items(), camera, width, height);
            check_pixel(image, convert(gray), "Vulkan target does not obey UNORM/sRGB encoding");
            save(image, captures / (std::string("color-baseline-") + color_pipeline_mode_name(mode) + ".bmp"));
            for (const float value : {0.0F, .18F, .5F, 1.0F})
            {
                auto reference_data = data;
                for (auto& vertex : reference_data.vertices)
                {
                    vertex.color = glm::vec3{value};
                    vertex.material_kind = float(RelayCore::surface_material_count() + 2);
                }
                StaticColorMesh reference_mesh;
                reference_mesh.initialize(reference_data);
                RenderQueue reference_queue;
                reference_queue.push(reference_mesh, glm::mat4{1});
                check_pixel(
                    renderer.render_image(reference_queue.items(), camera, width, height),
                    convert(glm::vec3{value}),
                    "Unlit SDR reference received missing/double sRGB encoding"
                );
            }
            const auto cleared = renderer.render_image({}, camera, width, height);
            check_pixel(
                cleared,
                convert(visual_rgb_for_pipeline({config.clear_r, config.clear_g, config.clear_b}, mode)),
                "Vulkan clear color is encoded twice or not encoded"
            );
            VulkanSwapchain swapchain{device, window, false};
            require(swapchain.draw_scene(renderer, queue.items(), camera), "Vulkan encoded color did not present");
            RelayCore::debug::ImGuiOverlay overlay{{.log_fps_to_console = false}};
            overlay.initialize(window, device);
            ImGui::GetIO().IniFilename = nullptr;
            overlay.begin_frame();
            ImGui::GetForegroundDrawList()
                ->AddRectFilled({0, 0}, {float(width), float(height)}, IM_COL32(128, 64, 32, 255));
            auto* draw = overlay.finish_frame();
            check_pixel(
                renderer.render_image(queue.items(), camera, width, height, {}, draw),
                {128 / 255.0F, 64 / 255.0F, 32 / 255.0F},
                "Vulkan UI received unintended sRGB encoding"
            );
            require(
                swapchain.draw_scene(renderer, queue.items(), camera, {}, draw),
                "Vulkan encoded UI did not present"
            );
            overlay.shutdown();
            check_pixel(
                renderer.render_image(queue.items(), camera, width, height),
                convert(gray),
                "UI pass leaked UNORM state into the next sRGB scene"
            );
            require(
                swapchain.draw_scene(renderer, queue.items(), camera),
                "Vulkan scene did not present after UI teardown"
            );

            auto neutral = authoring;
            neutral.lighting.ambient_floor = neutral.lighting.ambient_zenith = 1;
            neutral.lighting.ambient_min = neutral.lighting.ambient_max = 1;
            neutral.lighting.cool_ambient_color = neutral.lighting.warm_bounce_color = {};
            config.indirect_bounce_strength = 0;
            config.shadow_light_intensity = 0;
            neutral.materials[0].render_profile = MaterialRenderProfile::Default;
            neutral.materials[0].ambient_bias = 1;
            for (auto& v : data.vertices)
            {
                v.color = {1, 1, 1};
                v.material_kind = 0;
                v.uv = {.5F, .5F};
            }
            StaticColorMesh textured;
            textured.initialize(data);
            VulkanMeshRenderer
                texture_renderer{device, RELAYCORE_TEST_SPIRV_DIRECTORY, neutral, atlas, encoding, config, mode};
            for (const auto texture_encoding : {TextureEncoding::linear, TextureEncoding::srgb})
            {
                Texture2D texture;
                const std::array<uint8_t, 4> pixel{128, 128, 128, 255};
                texture.initialize_rgba(1, 1, pixel, texture_encoding);
                RenderQueue textured_queue;
                textured_queue.push(textured, glm::mat4{1}, RenderPass::Overlay, &texture);
                const float value = texture_encoding == TextureEncoding::srgb ? decode(128 / 255.0F) : 128 / 255.0F;
                check_pixel(
                    texture_renderer.render_image(textured_queue.items(), camera, width, height),
                    glm::clamp(convert(glm::vec3{value}) + .05F, glm::vec3{0}, glm::vec3{1}),
                    "Vulkan sampled texture decode or overlay lift changed"
                );
                Texture2D filtered;
                const std::array<uint8_t, 16>
                    checker{64, 64, 64, 255, 192, 192, 192, 255, 192, 192, 192, 255, 64, 64, 64, 255};
                filtered.initialize_rgba(2, 2, checker, texture_encoding);
                RenderQueue filtered_queue;
                filtered_queue.push(textured, glm::mat4{1}, RenderPass::Overlay, &filtered);
                const float average = texture_encoding == TextureEncoding::srgb
                                          ? (decode(64 / 255.0F) + decode(192 / 255.0F)) * .5F
                                          : 128 / 255.0F;
                check_pixel(
                    texture_renderer.render_image(filtered_queue.items(), camera, width, height),
                    glm::clamp(convert(glm::vec3{average}) + .05F, glm::vec3{0}, glm::vec3{1}),
                    "Vulkan texture filtering must decode sRGB before interpolation"
                );
            }
            config.postfx_vignette_strength = config.postfx_grain_strength = config.postfx_scanline_strength = 0;
            config.postfx_contrast = config.postfx_saturation = 1;
            texture_renderer.set_config(config);
            Texture2D white;
            white.initialize_rgba(1, 1, std::array<uint8_t, 4>{255, 255, 255, 255}, encoding);
            bool transfer_correct = true;
            for (const float contrast : {1.0F, 1.2F})
            {
                config.postfx_contrast = contrast;
                texture_renderer.set_config(config);
                for (const float gray_value : {0.0F, .002F, .0031308F, .04F, .18F, .5F})
                {
                    for (auto& vertex : data.vertices)
                        vertex.color = glm::vec3{gray_value};
                    StaticColorMesh ramp;
                    ramp.initialize(data);
                    RenderQueue ramp_queue;
                    ramp_queue.push(ramp, glm::mat4{1}, RenderPass::Opaque, &white);
                    const auto ramp_image = texture_renderer.render_image(ramp_queue.items(), camera, width, height);
                    const float display = mode == ColorPipelineMode::hardware_srgb ? encode(gray_value) : gray_value;
                    const float t = glm::clamp((display - .18F) / .6F, 0.0F, 1.0F);
                    const float green_grade = .99F + .01F * t * t * (3 - 2 * t);
                    const auto reference =
                        std::lround(glm::clamp(((display - .5F) * contrast + .5F) * green_grade, 0.0F, 1.0F) * 255);
                    const int actual = ramp_image.pixels[(height / 2 * width + width / 2) * 4 + 1];
                    std::cout << "sdr_reference: mode=" << color_pipeline_mode_name(mode) << " input=" << gray_value
                              << " contrast=" << contrast << " actual=" << actual << " expected=" << reference << '\n';
                    transfer_correct &= std::abs(actual - reference) <= 1;
                }
            }
            require(transfer_correct, "SDR postfx must operate on display brightness, with exactly one sRGB encoding");
            config.postfx_contrast = 1;
            texture_renderer.set_config(config);
            {
                auto light_authoring = neutral;
                light_authoring.lighting.ambient_floor = light_authoring.lighting.ambient_zenith = 0;
                light_authoring.lighting.ambient_min = light_authoring.lighting.ambient_max = 0;
                light_authoring.lighting.local_light_wrap = light_authoring.lighting.local_light_gain = 1;
                light_authoring.lighting.local_light_fill = 0;
                VulkanMeshRenderer light_renderer{
                    device,
                    RELAYCORE_TEST_SPIRV_DIRECTORY,
                    light_authoring,
                    atlas,
                    encoding,
                    config,
                    mode
                };
                const auto light_color = visual_rgb_for_pipeline(glm::vec3{.5F}, mode);
                const std::array lights{
                    LocalLight{{0, 0, 1}, light_color, 100},
                    LocalLight{{0, 0, 1}, light_color, 100}
                };
                light_renderer.set_local_lights(lights);
                for (auto& vertex : data.vertices)
                    vertex.color = glm::vec3{1};
                StaticColorMesh receiver;
                receiver.initialize(data);
                RenderQueue receiver_queue;
                receiver_queue.push(receiver, glm::mat4{1}, RenderPass::Opaque, &white);
                const auto light_image = light_renderer.render_image(receiver_queue.items(), camera, width, height);
                const auto reference = display_reference(
                    light_color * (2 * .99F * .99F * (3 - 2 * .99F)),
                    mode == ColorPipelineMode::hardware_srgb
                );
                check_pixel(
                    light_image,
                    reference,
                    "Local lights must accumulate in the selected pipeline's color space"
                );
                std::cout << "light_accumulation: mode=" << color_pipeline_mode_name(mode) << " two_srgb_half_lights="
                          << int(light_image.pixels[(height / 2 * width + width / 2) * 4 + 1]) << '\n';
            }
            for (auto& v : data.vertices)
                v.color = {.6F, 1, 2};
            StaticColorMesh highlights;
            highlights.initialize(data);
            RenderQueue highlight_queue;
            highlight_queue.push(highlights, glm::mat4{1}, RenderPass::Opaque, &white);
            const auto highlight_image = texture_renderer.render_image(highlight_queue.items(), camera, width, height);
            const auto center = size_t((height / 2) * width + width / 2) * 4;
            require(
                highlight_image.pixels[center] + 5 < highlight_image.pixels[center + 1]
                    && highlight_image.pixels[center + 1] + 5 < highlight_image.pixels[center + 2]
                    && highlight_image.pixels[center + 2] < 250,
                "SDR clipped distinct bright surface values instead of retaining highlight detail"
            );
            save(
                highlight_image,
                captures / (std::string{"sdr-highlights-"} + color_pipeline_mode_name(mode) + ".bmp")
            );
        }
        std::cout << "vulkan_color_baseline=passed (UNORM/sRGB, texture decode, clear, UI, presentation)\n";
    }

    void check_color_post(
        const VulkanDevice& device,
        const RenderAuthoringConfig& defaults,
        const CpuRgbaTextureData& atlas,
        const std::filesystem::path& captures
    )
    {
        PerspectiveCamera camera{{.position = {0, 0, 8}, .target = {0, 0, 0}}};
        camera.set_viewport(width, height);
        std::array<StaticColorMesh, 11> meshes;
        auto authoring = defaults;
        RenderQueue queue;
        for (size_t i = 0; i < meshes.size(); ++i)
        {
            authoring.materials[i].render_profile = static_cast<MaterialRenderProfile>(i);
            authoring.materials[i].emissive_strength = .4F;
            authoring.materials[i].emissive_pulse_speed = 2;
            auto data = make_color_double_sided_quad_mesh_data();
            for (auto& v : data.vertices)
            {
                v.color = {.35F, .5F, .65F};
                v.material_kind = float(i);
            }
            meshes[i].initialize(data);
            queue.push(
                meshes[i],
                glm::translate(glm::mat4{1}, {float(i % 4) * 1.2F - 1.8F, float(i / 4) * 1.2F - 1.2F, 0})
            );
        }
        for (const auto mode : {ColorPipelineMode::legacy_gamma, ColorPipelineMode::hardware_srgb})
            for (int variant = 0; variant < 4; ++variant)
            {
                RendererConfig config{};
                config.shadow_enabled = variant == 3;
                config.shadow_map_resolution = 512;
                config.fog_enabled = variant == 1;
                config.fog_start = .5F;
                config.fog_end = 12;
                config.fog_color = {.2F, .07F, .04F};
                config.shadow_debug_cascades = variant == 3;
                if (variant == 2)
                {
                    config.postfx_vignette_strength = .6F;
                    config.postfx_grain_strength = .08F;
                    config.postfx_scanline_strength = .7F;
                    config.postfx_contrast = 1.3F;
                    config.postfx_saturation = .3F;
                }
                const auto encoding =
                    mode == ColorPipelineMode::hardware_srgb ? TextureEncoding::srgb : TextureEncoding::linear;
                VulkanMeshRenderer
                    renderer{device, RELAYCORE_TEST_SPIRV_DIRECTORY, authoring, atlas, encoding, config, mode};
                const auto prefix =
                    std::string("color-post-") + color_pipeline_mode_name(mode) + "-" + std::to_string(variant);
                const auto check = [&](std::span<const RenderItem> items, const std::string& suffix) {
                    auto image = renderer.render_image(items, camera, width, height);
                    compare(image, prefix + suffix, captures);
                    return image;
                };
                const auto before = check(queue.items(), "-time-zero");
                renderer.set_scene_time(.43F);

                require(
                    check(queue.items(), "-animated").pixels != before.pixels,
                    "Scene time did not reach emissive/postfx"
                );
                for (const auto pass : {RenderPass::Overlay, RenderPass::Transparent})
                {
                    std::vector<RenderItem> items(queue.items().begin(), queue.items().end());
                    for (auto& item : items)
                        item.pass = pass;
                    check(items, pass == RenderPass::Overlay ? "-overlay" : "-transparent");
                }
            }
    }

    void check_color_world(
        const VulkanDevice& device,
        const RenderAuthoringConfig& authoring,
        const CpuRgbaTextureData& atlas,
        const std::filesystem::path& captures
    )
    {
        for (const auto mode : {ColorPipelineMode::legacy_gamma, ColorPipelineMode::hardware_srgb})
        {
            RendererConfig config{};
            const auto encoding =
                mode == ColorPipelineMode::hardware_srgb ? TextureEncoding::srgb : TextureEncoding::linear;
            VulkanMeshRenderer
                renderer{device, RELAYCORE_TEST_SPIRV_DIRECTORY, authoring, atlas, encoding, config, mode};

            renderer.set_scene_time(.4F);
            StaticModelCache models{};
            for (const auto* name : {"power_restoration", "pressure_transfer"})
            {
                const auto level =
                    RelayCore::load_blockout_level(RelayCore::core::asset_path(std::string("levels/") + name + ".toml")
                    );
                RelayGame::WorldSceneRenderer scene;
                scene.initialize(RelayGame::build_prepared_world_scene_data(level, authoring, mode), models);
                PerspectiveCamera camera{{.position = {4, 1.65F, 2}, .target = {4, 1.65F, 10}}};
                camera.set_viewport(width, height);
                scene.prepare_render_queue(camera, .4F, {}, {}, {}, -1, {}, false);

                renderer.set_local_lights(scene.local_lights());
                const auto weapon = scene.prepare_shotgun_viewmodel(camera, .4F);
                compare(
                    renderer.render_image(scene.render_items(), camera, width, height, weapon),
                    std::string("color-world-") + name + "-" + color_pipeline_mode_name(mode),
                    captures
                );
            }
        }
    }

    void check_model_products(
        const VulkanDevice& device,
        const RenderAuthoringConfig& authoring,
        const CpuRgbaTextureData& atlas,
        const std::filesystem::path& captures
    )
    {
        for (const auto mode : {ColorPipelineMode::legacy_gamma, ColorPipelineMode::hardware_srgb})
        {
            const auto encoding =
                mode == ColorPipelineMode::hardware_srgb ? TextureEncoding::srgb : TextureEncoding::linear;
            VulkanMeshRenderer
                renderer{device, RELAYCORE_TEST_SPIRV_DIRECTORY, authoring, atlas, encoding, RendererConfig{}, mode};
            RelayCore::test::ScopedTempDirectory temporary;
            const auto path = temporary.path() / "cylinder.rcmodel";
            auto data = read_model_asset(RelayCore::core::asset_path("models/service_cylinder.rcmodel"));
            write_model_asset(path, data);
            StaticModelCache cache;
            auto model = cache.load(path, encoding);
            require(model->parts.size() == 2 && model->materials.size() == 2, "Native model lost parts/materials");
            require(cache.load(path, encoding) == model, "Native model cache did not share CPU resources");
            PerspectiveCamera camera{{.position = {0, .85F, 3}, .target = {0, .85F, 0}}};
            camera.set_viewport(width, height);
            RenderQueue queue;
            const auto prepare = [&] {
                queue.clear();
                for (const auto& part : model->parts)
                    queue.push(
                        model->meshes[part.mesh],
                        part.transform,
                        RenderPass::Opaque,
                        &model->materials[part.material].base_color
                    );
            };
            prepare();
            const auto before = renderer.render_image(queue.items(), camera, width, height);
            const auto allocations = allocation_count(device);
            require(
                renderer.render_image(queue.items(), camera, width, height).pixels == before.pixels,
                "Cached native model changed its output"
            );
            require(
                allocation_count(device) == allocations,
                "Settled native model rendering allocated new GPU resources"
            );
            save(before, captures / (std::string("native-cylinder-") + color_pipeline_mode_name(mode) + ".bmp"));
            std::ofstream(path) << "broken";
            expect_failure(
                [&] {
                    static_cast<void>(cache.load(path, encoding));
                },
                "cylinder.rcmodel"
            );
            require(
                renderer.render_image(queue.items(), camera, width, height).pixels == before.pixels,
                "Failed native model reload changed the working image"
            );
            for (std::size_t i = 0; i < data.materials[1].base_color.pixels.size(); i += 4)
            {
                data.materials[1].base_color.pixels[i] = 255;
                data.materials[1].base_color.pixels[i + 1] = 30;
                data.materials[1].base_color.pixels[i + 2] = 20;
            }
            data.parts.push_back(data.parts[0]);
            write_model_asset(path, data);
            model = cache.load(path, encoding);
            prepare();
            require(
                renderer.render_image(queue.items(), camera, width, height).pixels != before.pixels,
                "Native model material replacement was not rendered"
            );
            require(allocation_count(device) == allocations, "Native model reload retained obsolete GPU allocations");
        }
    }

    void check_normal_maps(
        const VulkanDevice& device,
        const RenderAuthoringConfig& authoring,
        const CpuRgbaTextureData& atlas,
        const std::filesystem::path& captures
    )
    {
        for (const auto mode : {ColorPipelineMode::legacy_gamma, ColorPipelineMode::hardware_srgb})
        {
            const auto encoding =
                mode == ColorPipelineMode::hardware_srgb ? TextureEncoding::srgb : TextureEncoding::linear;
            RendererConfig config{};
            config.shadow_enabled = false;
            VulkanMeshRenderer
                renderer{device, RELAYCORE_TEST_SPIRV_DIRECTORY, authoring, atlas, encoding, config, mode};
            PerspectiveCamera camera{{.position = {0, 0, 3}, .target = {0, 0, 0}}};
            camera.set_viewport(width, height);
            CpuColorMeshData quad{
                {{{-.5F, -.5F, 0}, {1, 1, 1}, {0, 0, 1}, 0, {0, 0}, {1, 0, 0, 1}},
                 {{.5F, -.5F, 0}, {1, 1, 1}, {0, 0, 1}, 0, {1, 0}, {1, 0, 0, 1}},
                 {{.5F, .5F, 0}, {1, 1, 1}, {0, 0, 1}, 0, {1, 1}, {1, 0, 0, 1}},
                 {{-.5F, .5F, 0}, {1, 1, 1}, {0, 0, 1}, 0, {0, 1}, {1, 0, 0, 1}}},
                {0, 1, 2, 0, 2, 3}
            };
            StaticColorMesh mesh;
            mesh.initialize(quad);
            Texture2D color, normal, other_normal, invalid_normal;
            color.initialize_rgba(1, 1, std::array<std::uint8_t, 4>{90, 120, 140, 255}, encoding);
            const std::array<std::uint8_t, 4> normal_bytes{180, 200, 230, 255};
            normal.initialize_rgba(1, 1, normal_bytes, TextureEncoding::linear);
            other_normal.initialize_rgba(1, 1, std::array<std::uint8_t, 4>{128, 60, 240, 255}, TextureEncoding::linear);
            invalid_normal.initialize_rgba(1, 1, normal_bytes, TextureEncoding::srgb);
            RenderQueue queue;
            const auto mapped_image = [&](const glm::mat4& transform, const Texture2D& map, const float strength) {
                queue.clear();
                queue.push(mesh, transform, RenderPass::Overlay, &color, false, &map, strength);
                return renderer.render_image(queue.items(), camera, width, height);
            };
            for (const float mirror : {1.0F, -1.0F})
            {
                const auto transform =
                    glm::rotate(glm::mat4{1}, .3F, {0, 0, 1}) * glm::scale(glm::mat4{1}, {1.7F * mirror, .7F, 1.2F});
                const auto image = mapped_image(transform, normal, .6F);
                auto reference = quad;
                const auto n = glm::normalize(glm::transpose(glm::inverse(glm::mat3{transform})) * glm::vec3{0, 0, 1});
                auto t = glm::mat3{transform} * glm::vec3{1, 0, 0};
                t = glm::normalize(t - n * glm::dot(n, t));
                const auto b = glm::cross(n, t) * mirror;
                auto decoded = glm::vec3{180, 200, 230} / 127.5F - 1.0F;
                decoded.x *= .6F;
                decoded.y *= .6F;
                for (auto& vertex : reference.vertices)
                {
                    vertex.position = glm::vec3{transform * glm::vec4{vertex.position, 1}};
                    vertex.normal = glm::normalize(t * decoded.x + b * decoded.y + n * decoded.z);
                }
                StaticColorMesh expected_mesh;
                expected_mesh.initialize(reference);
                RenderQueue expected_queue;
                expected_queue.push(expected_mesh, glm::mat4{1}, RenderPass::Overlay, &color, false);
                const auto expected = renderer.render_image(expected_queue.items(), camera, width, height);
                std::size_t different = 0;
                for (std::size_t i = 0; i < image.pixels.size(); ++i)
                    different += std::abs(int(image.pixels[i]) - int(expected.pixels[i])) > 2;
                require(
                    different < 100,
                    "Normal map differs from explicit world-normal reference (encoding/transform/handedness)"
                );
                require(mapped_image(transform, normal, 0).pixels != image.pixels, "Normal scale was ignored");
                require(
                    mapped_image(transform, other_normal, .6F).pixels != image.pixels,
                    "Material cache confused two normal maps sharing one color"
                );
                const auto restored = mapped_image(transform, normal, .6F);
                const auto allocations = allocation_count(device);
                require(
                    mapped_image(transform, normal, .6F).pixels == restored.pixels,
                    "Normal material cache changed its image"
                );
                require(allocation_count(device) == allocations, "Settled normal material allocated GPU resources");
            }
            expect_failure(
                [&] {
                    static_cast<void>(mapped_image(glm::mat4{1}, invalid_normal, 1));
                },
                "linear texture"
            );

            StaticModelCache cache;
            const auto cabinet = cache.load(RelayCore::core::asset_path("models/power_cabinet.rcmodel"), encoding);
            PerspectiveCamera cabinet_camera{{.position = {1.5F, 1.5F, 3}, .target = {0, 1.15F, 0}}};
            cabinet_camera.set_viewport(width, height);
            RenderQueue cabinet_queue;
            for (const auto& part : cabinet->parts)
            {
                const auto& surface = cabinet->materials[part.material];
                cabinet_queue.push(
                    cabinet->meshes[part.mesh],
                    part.transform,
                    RenderPass::Opaque,
                    &surface.base_color,
                    true,
                    surface.normal.cpu_data() ? &surface.normal : nullptr,
                    surface.normal_scale,
                    surface.metallic_roughness,
                    surface.metallic_roughness_map.valid() ? &surface.metallic_roughness_map : nullptr,
                    surface.occlusion.valid() ? &surface.occlusion : nullptr,
                    surface.occlusion_strength,
                    surface.emissive.valid() ? &surface.emissive : nullptr,
                    surface.emissive_factor
                );
            }
            const auto mapped = renderer.render_image(cabinet_queue.items(), cabinet_camera, width, height);
            save(mapped, captures / (std::string("native-cabinet-normal-") + color_pipeline_mode_name(mode) + ".bmp"));
            RenderQueue flat_queue;
            for (const auto& part : cabinet->parts)
                flat_queue.push(
                    cabinet->meshes[part.mesh],
                    part.transform,
                    RenderPass::Opaque,
                    &cabinet->materials[part.material].base_color,
                    true,
                    nullptr,
                    1,
                    cabinet->materials[part.material].metallic_roughness
                );
            const auto flat = renderer.render_image(flat_queue.items(), cabinet_camera, width, height);
            save(flat, captures / (std::string("native-cabinet-flat-") + color_pipeline_mode_name(mode) + ".bmp"));
            require(mapped.pixels != flat.pixels, "Cabinet authored normal map did not affect rendering");
        }
    }

    void check_emission(
        const VulkanDevice& device,
        const RenderAuthoringConfig& authoring,
        const CpuRgbaTextureData& atlas,
        const std::filesystem::path& captures
    )
    {
        auto dark = authoring;
        dark.lighting.ambient_floor = dark.lighting.ambient_zenith = dark.lighting.ambient_min = 0;
        dark.lighting.cool_ambient_color = dark.lighting.warm_bounce_color = {};
        dark.lighting.bounce_floor = dark.lighting.bounce_underside = dark.lighting.bounce_darkness = 0;
        StaticColorMesh mesh;
        mesh.initialize(make_color_double_sided_quad_mesh_data());
        Texture2D black, emission, alpha_variant, ao;
        black.initialize_rgba(1, 1, std::array<std::uint8_t, 4>{0, 0, 0, 255}, TextureEncoding::srgb);
        emission.initialize_rgba(1, 1, std::array<std::uint8_t, 4>{128, 192, 240, 0}, TextureEncoding::srgb);
        alpha_variant.initialize_rgba(1, 1, std::array<std::uint8_t, 4>{128, 192, 240, 255}, TextureEncoding::srgb);
        ao.initialize_rgba(1, 1, std::array<std::uint8_t, 4>{0, 0, 0, 255}, TextureEncoding::linear);
        PerspectiveCamera camera{{.position = {0, 0, 3}, .target = {0, 0, 0}}};
        camera.set_viewport(width, height);
        for (const bool hdr : {false, true})
            for (const auto mode : {ColorPipelineMode::legacy_gamma, ColorPipelineMode::hardware_srgb})
            {
                if (hdr && mode == ColorPipelineMode::legacy_gamma)
                    continue;
                RendererConfig config;
                config.hdr_enabled = hdr;
                config.shadow_enabled = config.fog_enabled = false;
                config.shadow_light_intensity = 0;
                config.postfx_vignette_strength = config.postfx_grain_strength = config.postfx_scanline_strength = 0;
                config.postfx_contrast = config.postfx_saturation = 1;
                VulkanMeshRenderer renderer{
                    device,
                    RELAYCORE_TEST_SPIRV_DIRECTORY,
                    dark,
                    atlas,
                    mode == ColorPipelineMode::legacy_gamma ? TextureEncoding::linear : TextureEncoding::srgb,
                    config,
                    mode
                };
                for (const bool pbr : {false, true})
                {
                    const auto render = [&](const Texture2D* map, glm::vec3 factor, const Texture2D* occlusion = nullptr
                                        ) {
                        RenderQueue queue;
                        queue.push(
                            mesh,
                            glm::mat4{1},
                            RenderPass::Opaque,
                            &black,
                            false,
                            nullptr,
                            1,
                            pbr ? std::optional{MetallicRoughness{1, 0}} : std::nullopt,
                            nullptr,
                            occlusion,
                            1,
                            map,
                            factor
                        );
                        return renderer.render_image(queue.items(), camera, width, height);
                    };
                    const glm::vec3 factor{.25F, .5F, .75F};
                    const auto image = render(&emission, factor);
                    const auto off = render(&emission, glm::vec3{0});
                    const auto center = (height / 2 * width + width / 2) * 4;
                    require(
                        image.pixels[center + 2] > image.pixels[center + 1]
                            && image.pixels[center + 1] > image.pixels[center] && image.pixels[center] > 5,
                        "Emission channels/factors failed in an unlit scene"
                    );
                    require(
                        off.pixels[center] == 0 && off.pixels[center + 1] == 0 && off.pixels[center + 2] == 0,
                        "Powered-off native material still emits"
                    );
                    require(render(&alpha_variant, factor).pixels == image.pixels, "Emission alpha affected RGB");
                    require(render(&emission, factor, &ao).pixels == image.pixels, "AO attenuated emission");
                    glm::vec3 decoded;
                    for (int c = 0; c < 3; ++c)
                    {
                        const auto value = emission.cpu_data()->pixels[c] / 255.F;
                        decoded[c] =
                            (value <= .04045F ? value / 12.92F : std::pow((value + .055F) / 1.055F, 2.4F)) * factor[c];
                    }
                    const auto reference = render(nullptr, decoded);
                    for (std::size_t i = center; i < center + 3; ++i)
                        require(
                            std::abs(int(reference.pixels[i]) - int(image.pixels[i])) <= 1,
                            "Emission texture was not decoded from sRGB before linear factor multiplication"
                        );
                    const auto allocations = allocation_count(device);
                    require(
                        render(&emission, factor).pixels == image.pixels && allocation_count(device) == allocations,
                        "Emission cache failed to restore powered state"
                    );
                    save(
                        image,
                        captures
                            / (std::string{"img-emission-"} + color_pipeline_mode_name(mode) + (hdr ? "-hdr" : "-sdr")
                               + (pbr ? "-pbr.bmp" : "-legacy.bmp"))
                    );
                    expect_failure(
                        [&] {
                            static_cast<void>(render(&ao, factor));
                        },
                        "sRGB"
                    );
                    expect_failure(
                        [&] {
                            static_cast<void>(render(&emission, {-1, 0, 0}));
                        },
                        "factor"
                    );
                }
            }
    }

    void check_msaa(
        const VulkanDevice& device,
        const RenderAuthoringConfig& authoring,
        const CpuRgbaTextureData& atlas,
        const std::filesystem::path& captures
    )
    {
        PerspectiveCamera camera{{.position = {0, 0, 3}, .target = {0, 0, 0}}};
        camera.set_viewport(width, height);
        CpuColorMeshData triangle{
            {{{-.8F, -.7F, 0}, {1, 1, 1}, {0, 0, 1}, 40},
             {{.8F, -.53F, 0}, {1, 1, 1}, {0, 0, 1}, 40},
             {{.17F, .8F, 0}, {1, 1, 1}, {0, 0, 1}, 40}},
            {0, 1, 2}
        };
        StaticColorMesh mesh;
        mesh.initialize(triangle);
        RenderQueue queue;
        queue.push(mesh, glm::mat4{1});
        for (const bool hdr : {false, true})
        {
            std::size_t aliased_edges = 0;
            for (const int samples : {1, 2, 4})
            {
                RendererConfig config;
                config.msaa_samples = samples;
                config.hdr_enabled = hdr;
                config.shadow_enabled = config.fog_enabled = false;
                config.clear_r = config.clear_g = config.clear_b = 0;
                config.postfx_vignette_strength = config.postfx_grain_strength = config.postfx_scanline_strength = 0;
                config.postfx_contrast = config.postfx_saturation = 1;
                VulkanMeshRenderer renderer{
                    device,
                    RELAYCORE_TEST_SPIRV_DIRECTORY,
                    authoring,
                    atlas,
                    TextureEncoding::srgb,
                    config,
                    ColorPipelineMode::hardware_srgb
                };
                const auto image = renderer.render_image(queue.items(), camera, width, height);
                const auto center = (height / 2 * width + width / 2) * 4;
                const auto solid = image.pixels[center];
                std::size_t partial_edges = 0;
                for (std::size_t p = 0; p < image.pixels.size(); p += 4)
                    partial_edges += image.pixels[p] > 2 && image.pixels[p] + 2 < solid;
                if (samples == 1)
                    aliased_edges = partial_edges;
                else
                    require(partial_edges > aliased_edges + 100, "MSAA did not resolve diagonal geometry coverage");
                const auto allocations = allocation_count(device);
                require(
                    renderer.render_image(queue.items(), camera, width, height).pixels == image.pixels,
                    "MSAA changed a settled scene"
                );
                require(allocation_count(device) == allocations, "MSAA allocated resources for a settled frame");
                require(
                    renderer.render_image({}, camera, width, height, queue.items()).pixels == image.pixels,
                    "MSAA viewmodel resolve lost coverage or retained world depth"
                );
                static_cast<void>(renderer.render_image(queue.items(), camera, width / 2, height / 2));
                require(
                    renderer.render_image(queue.items(), camera, width, height).pixels == image.pixels,
                    "MSAA target recreation changed rendering"
                );
                save(image, captures / ("msaa-" + std::to_string(samples) + (hdr ? "-hdr.bmp" : "-sdr.bmp")));
                auto changed = config;
                changed.msaa_samples = samples == 1 ? 4 : 1;
                expect_failure(
                    [&] {
                        renderer.set_config(changed);
                    },
                    "MSAA"
                );
            }
        }
    }

    void check_metallic_roughness(
        const VulkanDevice& device,
        const RenderAuthoringConfig& authoring,
        const CpuRgbaTextureData& atlas,
        const std::filesystem::path& captures
    )
    {
        auto unlit_ambient = authoring;
        unlit_ambient.lighting.ambient_floor = 0;
        unlit_ambient.lighting.ambient_zenith = 0;
        RendererConfig config{};
        config.shadow_enabled = false;
        config.fog_enabled = false;
        config.postfx_vignette_strength = config.postfx_grain_strength = config.postfx_scanline_strength = 0;
        config.postfx_contrast = config.postfx_saturation = 1;
        config.shadow_light_direction = {0, 0, -1};
        config.shadow_light_color = {.5F, .5F, .5F};
        config.shadow_light_intensity = 1;
        auto quad = make_color_double_sided_quad_mesh_data();
        for (auto& v : quad.vertices)
            v.color = {1, 1, 1};
        StaticColorMesh mesh;
        mesh.initialize(quad);
        PerspectiveCamera camera{{.position = {0, 0, 3}, .target = {0, 0, 0}}};
        camera.set_viewport(width, height);
        std::vector<std::uint8_t> previous;
        for (const auto mode : {ColorPipelineMode::legacy_gamma, ColorPipelineMode::hardware_srgb})
        {
            const auto encoding =
                mode == ColorPipelineMode::hardware_srgb ? TextureEncoding::srgb : TextureEncoding::linear;
            VulkanMeshRenderer
                renderer{device, RELAYCORE_TEST_SPIRV_DIRECTORY, unlit_ambient, atlas, encoding, config, mode};
            Texture2D color;
            const std::array<std::uint8_t, 4> bytes{180, 110, 70, 255};
            color.initialize_rgba(1, 1, bytes, encoding);
            RenderQueue queue;
            const auto render = [&](MetallicRoughness factors,
                                    const Texture2D* mr = nullptr,
                                    const Texture2D* ao = nullptr,
                                    float strength = 1) {
                queue.clear();
                queue
                    .push(mesh, glm::mat4{1}, RenderPass::Opaque, &color, false, nullptr, 1, factors, mr, ao, strength);
                return renderer.render_image(queue.items(), camera, width, height);
            };
            const auto center = (height / 2 * width + width / 2) * 4;
            for (float metallic : {0.0F, 1.0F})
            {
                const auto image = render({.5F, metallic});
                // Normal incidence: D=16/pi, visibility=1/4, Fresnel=F0.
                glm::vec3 radiance{};
                for (int c = 0; c < 3; ++c)
                {
                    const auto base = srgb_channel_to_linear(bytes[c] / 255.0F);
                    const float f0 = metallic == 0 ? .04F : base;
                    radiance[c] =
                        ((1 - f0) * (1 - metallic) * base + 4 * f0) / 3.14159265F * srgb_channel_to_linear(.5F);
                }
                const auto reference = display_reference(radiance, true);
                for (int c = 0; c < 3; ++c)
                {
                    const auto visual = reference[c];
                    if (std::abs(int(image.pixels[center + c]) - int(std::lround(visual * 255))) > 2)
                        throw std::runtime_error(
                            std::string("Metallic/roughness reference failed: mode=") + color_pipeline_mode_name(mode)
                            + " metallic=" + std::to_string(metallic) + " channel=" + std::to_string(c)
                            + " actual=" + std::to_string(image.pixels[center + c])
                            + " expected=" + std::to_string(std::lround(visual * 255))
                        );
                }
                const auto smooth = render({.25F, metallic});
                const auto rough = render({.8F, metallic});
                require(
                    smooth.pixels[center] > rough.pixels[center] + 10,
                    "Roughness did not broaden/lower the highlight"
                );
                const auto restored = render({.5F, metallic});
                const auto allocations = allocation_count(device);
                require(restored.pixels == image.pixels, "Material cache confused roughness/metalness factors");
                require(
                    render({.5F, metallic}).pixels == image.pixels && allocation_count(device) == allocations,
                    "Settled metallic/roughness draw changed/allocated GPU resources"
                );
                save(
                    image,
                    captures
                        / (std::string("mr-") + (metallic == 0 ? "paint-" : "metal-") + color_pipeline_mode_name(mode)
                           + ".bmp")
                );
                if (metallic == 0)
                {
                    if (!previous.empty())
                        for (std::size_t i = 0; i < image.pixels.size(); ++i)
                            require(
                                std::abs(int(previous[i]) - int(image.pixels[i])) <= 1,
                                "Metallic/roughness color conversion differs between modes"
                            );
                    previous = image.pixels;
                }
            }
            Texture2D mr, mr_unused, ao;
            mr.initialize_rgba(1, 1, std::array<std::uint8_t, 4>{3, 128, 64, 7}, TextureEncoding::linear);
            mr_unused.initialize_rgba(1, 1, std::array<std::uint8_t, 4>{240, 128, 64, 255}, TextureEncoding::linear);
            ao.initialize_rgba(1, 1, std::array<std::uint8_t, 4>{0, 37, 98, 7}, TextureEncoding::linear);
            const auto mapped = render({.8F, .6F}, &mr);
            const auto equivalent = render({.8F * 128.F / 255.F, .6F * 64.F / 255.F});
            require(
                mapped.pixels == equivalent.pixels,
                "Material maps must use linear G roughness/B metallic multiplied by factors"
            );
            require(
                render({.8F, .6F}, &mr_unused).pixels == mapped.pixels,
                "Unused MR R/alpha channels changed shading"
            );
            require(render({.8F, .6F}, &mr, &ao).pixels == mapped.pixels, "AO incorrectly attenuated direct lighting");
            config.shadow_light_intensity = 0;
            renderer.set_config(config);
            auto volume = std::make_shared<IrradianceProbeVolume>();
            volume->settings = {{-2, -2, -2}, {2, 2, 2}, {2, 2, 2}, 64};
            volume->probes.resize(8);
            for (auto& coefficients : volume->probes)
                coefficients[0] = glm::vec3{.8F / .282094792F};
            renderer.set_irradiance_probes(volume);
            const auto indirect = render({.5F, 0});
            const auto occluded = render({.5F, 0}, nullptr, &ao);
            require(
                indirect.pixels[center] > occluded.pixels[center] + 10,
                "AO did not attenuate indirect illumination"
            );
            require(render({.5F, 0}, nullptr, &ao, 0).pixels == indirect.pixels, "Zero AO strength changed shading");
            const auto halfway = render({.5F, 0}, nullptr, &ao, .5F);
            require(
                halfway.pixels[center] > occluded.pixels[center] && halfway.pixels[center] < indirect.pixels[center],
                "AO strength interpolation failed"
            );
            save(indirect, captures / (std::string{"img-ao-before-"} + color_pipeline_mode_name(mode) + ".bmp"));
            save(halfway, captures / (std::string{"img-ao-half-"} + color_pipeline_mode_name(mode) + ".bmp"));
            renderer.set_irradiance_probes({});
            auto probe = std::make_shared<ReflectionProbe>();
            probe->settings = {{0, 0, 0}, {-2, -2, -2}, {2, 2, 2}, 8, 32};
            for (uint32_t size = 8; size; size >>= 1)
            {
                auto& mip = probe->mips.emplace_back(6 * size * size, glm::vec4{.08F, .08F, .08F, 1});
                for (uint32_t texel = 0; texel < size * size; ++texel)
                    mip[4 * size * size + texel] =
                        size == 8 ? glm::vec4{1.5F, .05F, .02F, 1} : glm::vec4{.02F, .05F, .7F, 1};
            }
            renderer.set_reflection_probe(probe);
            const auto reflected = render({0, 1});
            require(
                reflected.pixels[center] > reflected.pixels[center + 2] + 10,
                "Reflection cube face orientation/linear HDR sampling failed"
            );
            const auto blurred = render({1, 1});
            require(
                blurred.pixels[center + 2] > blurred.pixels[center] + 10,
                "Reflection roughness did not select baked mips"
            );
            save(
                reflected,
                captures / (std::string{"img-reflection-sharp-"} + color_pipeline_mode_name(mode) + ".bmp")
            );
            save(blurred, captures / (std::string{"img-reflection-rough-"} + color_pipeline_mode_name(mode) + ".bmp"));
            renderer.set_irradiance_probes(volume);
            require(render({0, 1}).pixels == reflected.pixels, "Reflection and old SH specular were added twice");
            renderer.set_irradiance_probes({});
            camera.set_position({3, 0, 3});
            camera.set_target({0, 0, 0});
            const auto rotated = render({0, 1});
            require(rotated.pixels != reflected.pixels, "Reflections ignored camera rotation");
            camera.set_position({0, 0, 3});
            camera.set_target({0, 0, 0});
            auto gradient_probe = std::make_shared<ReflectionProbe>(*probe);
            for (uint32_t y = 0; y < 8; ++y)
                for (uint32_t x = 0; x < 8; ++x)
                    gradient_probe->mips[0][4 * 64 + y * 8 + x] = {.15F + static_cast<float>(x) * .12F, .05F, .02F, 1};
            renderer.set_reflection_probe(gradient_probe);
            const auto translated = [&](float x) {
                camera.set_position({x, 0, 3});
                camera.set_target({x, 0, 0});
                queue.clear();
                queue.push(
                    mesh,
                    glm::translate(glm::mat4{1}, {x, 0, 0}),
                    RenderPass::Opaque,
                    &color,
                    false,
                    nullptr,
                    1,
                    MetallicRoughness{0, 1}
                );
                return renderer.render_image(queue.items(), camera, width, height);
            };
            const auto left = translated(-1.F), right = translated(1.F);
            require(
                right.pixels[center] > left.pixels[center] + 8,
                "Reflection box projection ignored receiver position"
            );
            camera.set_position({0, 0, 3});
            camera.set_target({0, 0, 0});
            auto outside = std::make_shared<ReflectionProbe>(*probe);
            outside->settings.position = {10, 0, 0};
            outside->settings.mins = {8, -2, -2};
            outside->settings.maxs = {12, 2, 2};
            renderer.set_reflection_probe(outside);
            const auto outside_image = render({0, 1});
            renderer.set_reflection_probe({});
            require(
                render({0, 1}).pixels == outside_image.pixels,
                "Reflection probe affected receivers outside its bounds"
            );
            config.shadow_light_intensity = 1;
            renderer.set_config(config);
            require(render({0, 0}).pixels[center] > 0, "Zero roughness produced invalid output");
            require(render({1, 1}).pixels[center] > 0, "Fully rough metal produced invalid output");
            expect_failure(
                [&] {
                    static_cast<void>(render({-1, 0}));
                },
                "factors"
            );
            config.shadow_light_intensity = 0;
            renderer.set_config(config);
            const std::array local{LocalLight{{0, 0, 2}, {.5F, .5F, .5F}, 4}};
            renderer.set_local_lights(local);
            require(render({.5F, 0}).pixels[center] > 0, "Local light did not illuminate metallic/roughness material");
            renderer.set_local_lights({});
            config.shadow_enabled = true;
            renderer.set_config(config);
            renderer.set_spot_light({{0, 0, 2}, {0, 0, -1}, {.5F, .5F, .5F}, 4, true});
            require(render({.5F, 0}).pixels[center] > 0, "Spot light did not illuminate metallic/roughness material");
            config.shadow_light_intensity = 1;
        }
    }

    void check_irradiance(
        const VulkanDevice& device,
        const RenderAuthoringConfig& authoring,
        const CpuRgbaTextureData& atlas,
        const std::filesystem::path& captures
    )
    {
        auto lighting = authoring;
        lighting.lighting.ambient_floor = lighting.lighting.ambient_zenith = lighting.lighting.ambient_min = 0;
        lighting.lighting.cool_ambient_color = lighting.lighting.warm_bounce_color = {};
        lighting.lighting.bounce_floor = lighting.lighting.bounce_underside = lighting.lighting.bounce_darkness = 0;
        RendererConfig config;
        config.hdr_enabled = true;
        config.shadow_enabled = config.fog_enabled = false;
        config.shadow_light_intensity = 0;
        config.indirect_bounce_strength = 0;
        config.postfx_vignette_strength = config.postfx_grain_strength = config.postfx_scanline_strength = 0;
        config.postfx_contrast = config.postfx_saturation = 1;
        VulkanMeshRenderer renderer{
            device,
            RELAYCORE_TEST_SPIRV_DIRECTORY,
            lighting,
            atlas,
            TextureEncoding::srgb,
            config,
            ColorPipelineMode::hardware_srgb
        };
        IrradianceProbeVolume volume{{{-1, -1, -1}, {1, 1, 1}, {2, 2, 2}, 64}, 0, {}};
        volume.probes.resize(8);
        for (std::size_t i = 0; i < 8; ++i)
        {
            volume.probes[i][0] = glm::vec3{.25F + static_cast<float>(i) * .2F, .3F, .1F} / .282094792F;
            volume.probes[i][2] = {.35F, .1F, 0};
        }
        auto probes = std::make_shared<const IrradianceProbeVolume>(volume);
        const std::array<std::uint8_t, 4> white{255, 255, 255, 255};
        Texture2D texture;
        texture.initialize_rgba(1, 1, white, TextureEncoding::srgb);
        PerspectiveCamera camera{{.position = {0, 0, 3}, .target = {0, 0, 0}}};
        camera.set_viewport(width, height);
        const auto render =
            [&](glm::vec3 normal, std::optional<MetallicRoughness> mr, RenderPass pass, bool viewmodel = false) {
                auto quad = make_color_double_sided_quad_mesh_data();
                for (auto& vertex : quad.vertices)
                {
                    vertex.material_kind = 0;
                    vertex.color = {1, 1, 1};
                    vertex.normal = normal;
                }
                StaticColorMesh mesh;
                mesh.initialize(quad);
                RenderQueue queue;
                queue.push(mesh, glm::mat4{1}, pass, &texture, false, nullptr, 1, mr);
                return viewmodel ? renderer.render_image({}, camera, width, height, queue.items())
                                 : renderer.render_image(queue.items(), camera, width, height);
            };
        for (const auto normal : {glm::vec3{0, 0, 1}, glm::vec3{0, 0, -1}})
            for (const int material : {0, 1, 2})
            {
                const auto mr = material == 0 ? std::optional<MetallicRoughness>{}
                                              : std::optional{MetallicRoughness{.7F, material == 2 ? 1.F : 0.F}};
                renderer.set_irradiance_probes(probes);
                const auto actual = render(normal, mr, RenderPass::Opaque);
                auto expected = sample_irradiance_probes(volume, {}, normal);
                if (mr)
                {
                    expected *= .96F * (1 - mr->metallic);
                    // This fixture contains only SH bands 0 and 1. Reflection is +Z for both normals.
                    const auto& a = volume.probes;
                    glm::vec3 radiance{};
                    for (const auto& probe : a)
                        radiance +=
                            probe[0] * .282094792F + probe[2] * (.488602512F * std::exp(-2.F * .7F * .7F) * 1.5F);
                    radiance /= 8;
                    const float nv = normal.z > 0 ? 1.F : 0.F;
                    const float r_x = 1 - .7F, r_y = .0425F - .0275F * .7F;
                    const float a004 = std::min(r_x * r_x, std::exp2(-9.28F * nv)) * r_x + r_y;
                    const float scale = 1.04F - .572F * .7F - 1.04F * a004;
                    const float bias = .022F * .7F - .04F + 1.04F * a004;
                    expected += radiance * std::max((mr->metallic == 1 ? 1.F : .04F) * scale + bias, 0.F);
                }
                auto reference_quad = make_color_double_sided_quad_mesh_data();
                for (auto& vertex : reference_quad.vertices)
                {
                    vertex.material_kind = 40;
                    vertex.color = expected;
                }
                StaticColorMesh reference_mesh;
                reference_mesh.initialize(reference_quad);
                RenderQueue reference_queue;
                reference_queue.push(reference_mesh, glm::mat4{1});
                renderer.set_irradiance_probes({});
                const auto reference = renderer.render_image(reference_queue.items(), camera, width, height);
                const auto center = (height / 2 * width + width / 2) * 4;
                for (int c = 0; c < 3; ++c)
                    require(
                        std::abs(int(actual.pixels[center + c]) - int(reference.pixels[center + c])) <= 2,
                        "GPU probe interpolation/normal/material differs from CPU irradiance reference"
                    );
                save(
                    actual,
                    captures
                        / ("irradiance-material-" + std::to_string(material)
                           + (normal.z > 0 ? "-front.bmp" : "-back.bmp"))
                );
            }
        renderer.set_irradiance_probes(probes);
        const auto overlay = render({0, 0, 1}, {}, RenderPass::Overlay);
        renderer.set_irradiance_probes({});
        require(
            overlay.pixels == render({0, 0, 1}, {}, RenderPass::Overlay).pixels,
            "Baked world probes affected overlay shading"
        );
        renderer.set_irradiance_probes(probes);
        const auto weapon = render({0, 0, 1}, {}, RenderPass::Opaque, true);
        renderer.set_irradiance_probes({});
        require(
            weapon.pixels == render({0, 0, 1}, {}, RenderPass::Opaque, true).pixels,
            "Baked world probes affected viewmodel shading"
        );

        const auto level = RelayCore::load_blockout_level(RelayCore::core::asset_path("levels/native_lab.toml"));
        const auto prepared =
            RelayGame::build_prepared_world_scene_data(level, authoring, ColorPipelineMode::hardware_srgb);
        require(bool(prepared.irradiance), "Native scene did not load its baked product");
        StaticModelCache models;
        RelayGame::WorldSceneRenderer scene;
        scene.initialize(prepared, models);
        PerspectiveCamera native_camera{{.position = {0, 1.65F, -3.4F}, .target = {1.5F, 1.45F, 3.9F}}};
        native_camera.set_viewport(width, height);
        VulkanMeshRenderer native{
            device,
            RELAYCORE_TEST_SPIRV_DIRECTORY,
            authoring,
            atlas,
            TextureEncoding::srgb,
            config,
            ColorPipelineMode::hardware_srgb
        };
        scene.prepare_render_queue(native_camera, 0, {}, {}, {}, -1, {}, false);
        native.set_local_lights(scene.local_lights());
        const auto before = native.render_image(scene.render_items(), native_camera, width, height);
        native.set_irradiance_probes(scene.irradiance());
        const auto after = native.render_image(scene.render_items(), native_camera, width, height);
        std::uint64_t difference = 0;
        for (std::size_t i = 0; i < before.pixels.size(); ++i)
            difference += std::uint64_t(std::abs(int(before.pixels[i]) - int(after.pixels[i])));
        require(difference > 1000, "Native baked diffuse contribution was not visible");
        save(before, captures / "irradiance-native-before.bmp");
        save(after, captures / "irradiance-native-after.bmp");
        std::cout << "irradiance_native_difference=" << difference << '\n';
        native.set_irradiance_probes({});
        require(
            before.pixels == native.render_image(scene.render_items(), native_camera, width, height).pixels,
            "Probe removal left stale GPU lighting"
        );
    }

    void check_hdr(
        const VulkanDevice& device,
        const RelayCore::Window& window,
        const RenderAuthoringConfig& authoring,
        const CpuRgbaTextureData& atlas,
        const std::filesystem::path& captures
    )
    {
        RendererConfig config{};
        config.hdr_enabled = true;
        config.shadow_enabled = config.fog_enabled = false;
        config.postfx_vignette_strength = config.postfx_grain_strength = config.postfx_scanline_strength = 0;
        config.postfx_contrast = config.postfx_saturation = 1;
        expect_failure(
            [&] {
                VulkanMeshRenderer
                    invalid{device, RELAYCORE_TEST_SPIRV_DIRECTORY, authoring, atlas, TextureEncoding::linear, config};
            },
            "hardware_srgb"
        );
        VulkanMeshRenderer renderer{
            device,
            RELAYCORE_TEST_SPIRV_DIRECTORY,
            authoring,
            atlas,
            TextureEncoding::srgb,
            config,
            ColorPipelineMode::hardware_srgb
        };
        const glm::vec3 radiance{.25F, 1, 4};
        auto quad = make_color_double_sided_quad_mesh_data();
        for (auto& vertex : quad.vertices)
        {
            vertex.color = radiance;
            vertex.material_kind = 40;
        }
        StaticColorMesh mesh;
        mesh.initialize(quad);
        RenderQueue queue;
        queue.push(mesh, glm::mat4{1});
        PerspectiveCamera camera{{.position = {0, 0, 3}, .target = {0, 0, 0}}};
        camera.set_viewport(width, height);
        const auto expected = [](glm::vec3 source, float ev, float contrast = 1) {
            return display_reference(source * std::exp2(ev), true, contrast);
        };
        const auto check_center = [&](const CpuRgbaTextureData& image, glm::vec3 reference) {
            const auto center = (image.height / 2 * image.width + image.width / 2) * 4;
            for (int c = 0; c < 3; ++c)
                if (std::abs(int(image.pixels[center + c]) - int(std::lround(reference[c] * 255))) > 2)
                    throw std::runtime_error(
                        "HDR reference: channel=" + std::to_string(c)
                        + " actual=" + std::to_string(image.pixels[center + c])
                        + " expected=" + std::to_string(std::lround(reference[c] * 255))
                    );
        };
        for (const float ev : {-1.0F, 0.0F, 1.0F})
        {
            config.exposure_ev = ev;
            renderer.set_config(config);
            const auto image = renderer.render_image(queue.items(), camera, width, height);
            check_center(image, expected(radiance, ev));
            save(image, captures / ("hdr-exposure-" + std::to_string(int(ev)) + ".bmp"));
        }
        config.exposure_ev = 0;
        renderer.set_config(config);
        {
            int previous = -1;
            bool shadows_preserved = true;
            for (const float gray : {0.0F, .002F, .0031308F, .04F, .18F, .5F, 1.0F, 2.0F})
            {
                auto ramp_data = quad;
                for (auto& vertex : ramp_data.vertices)
                    vertex.color = glm::vec3{gray};
                StaticColorMesh ramp;
                ramp.initialize(ramp_data);
                RenderQueue ramp_queue;
                ramp_queue.push(ramp, glm::mat4{1});
                const auto image = renderer.render_image(ramp_queue.items(), camera, width, height);
                const int value = image.pixels[(height / 2 * width + width / 2) * 4 + 1];
                require(value > previous && value < 250, "HDR lost tonal separation or clipped diffuse highlights");
                if (gray <= .5F)
                {
                    const float display = gray <= .0031308F ? gray * 12.92F : 1.055F * std::pow(gray, 1 / 2.4F) - .055F;
                    const float t = glm::clamp((display - .18F) / .6F, 0.0F, 1.0F);
                    const auto reference = std::lround(display * (.99F + .01F * t * t * (3 - 2 * t)) * 255);
                    std::cout << "hdr_reference: linear=" << gray << " actual=" << value << " expected=" << reference
                              << '\n';
                    shadows_preserved &= std::abs(value - reference) <= 2;
                }
                previous = value;
            }
            require(shadows_preserved, "HDR tone mapping changed shadows/midtones at zero exposure");
        }
        {
            auto dark_data = quad;
            const glm::vec3 dark{.002F, .004F, .008F};
            for (auto& vertex : dark_data.vertices)
                vertex.color = dark;
            StaticColorMesh dark_mesh;
            dark_mesh.initialize(dark_data);
            RenderQueue dark_queue;
            dark_queue.push(dark_mesh, glm::mat4{1});
            config.postfx_contrast = 1.01F;
            renderer.set_config(config);
            check_center(renderer.render_image(dark_queue.items(), camera, width, height), expected(dark, 0, 1.01F));
            config.postfx_contrast = 1;
            renderer.set_config(config);

            auto hemisphere = authoring;
            auto ambient_config = config;
            ambient_config.shadow_light_intensity = 0;
            hemisphere.lighting.ambient_floor = hemisphere.lighting.ambient_zenith = .5F;
            hemisphere.lighting.cool_ambient_color = hemisphere.lighting.warm_bounce_color = {};
            hemisphere.lighting.bounce_floor = hemisphere.lighting.bounce_underside =
                hemisphere.lighting.bounce_darkness = 0;
            hemisphere.materials[0].ambient_bias = 1;
            VulkanMeshRenderer ambient_renderer{
                device,
                RELAYCORE_TEST_SPIRV_DIRECTORY,
                hemisphere,
                atlas,
                TextureEncoding::srgb,
                ambient_config,
                ColorPipelineMode::hardware_srgb
            };
            for (auto& vertex : dark_data.vertices)
            {
                vertex.color = {.1F, .1F, .1F};
                vertex.material_kind = 0;
            }
            StaticColorMesh ambient_mesh;
            ambient_mesh.initialize(dark_data);
            Texture2D white_diffuse;
            white_diffuse.initialize_rgba(1, 1, std::array<std::uint8_t, 4>{255, 255, 255, 255}, TextureEncoding::srgb);
            RenderQueue ambient_queue;
            ambient_queue.push(ambient_mesh, glm::mat4{1}, RenderPass::Opaque, &white_diffuse);
            check_center(
                ambient_renderer.render_image(ambient_queue.items(), camera, width, height),
                expected(glm::vec3{.05F}, 0)
            );
            RenderQueue mr_ambient;
            mr_ambient.push(
                ambient_mesh,
                glm::mat4{1},
                RenderPass::Opaque,
                &white_diffuse,
                false,
                nullptr,
                1,
                MetallicRoughness{1, 0}
            );
            check_center(
                ambient_renderer.render_image(mr_ambient.items(), camera, width, height),
                expected(glm::vec3{.048F + .5F * (.04F * .4524F - .0024F)}, 0)
            );
            auto sdr_config = ambient_config;
            sdr_config.hdr_enabled = false;
            VulkanMeshRenderer sdr_renderer{
                device,
                RELAYCORE_TEST_SPIRV_DIRECTORY,
                hemisphere,
                atlas,
                TextureEncoding::srgb,
                sdr_config,
                ColorPipelineMode::hardware_srgb
            };
            for (const float contrast : {1.0F, 1.2F})
            {
                ambient_config.postfx_contrast = sdr_config.postfx_contrast = contrast;
                ambient_renderer.set_config(ambient_config);
                sdr_renderer.set_config(sdr_config);
                for (const float value : {0.0F, .004F, .08F, .36F, 1.0F, 2.0F, 4.0F})
                {
                    for (auto& vertex : dark_data.vertices)
                        vertex.color = glm::vec3{value};
                    StaticColorMesh ramp;
                    ramp.initialize(dark_data);
                    for (const bool mr : {false, true})
                    {
                        RenderQueue ramp_queue;
                        ramp_queue.push(
                            ramp,
                            glm::mat4{1},
                            RenderPass::Opaque,
                            &white_diffuse,
                            false,
                            nullptr,
                            1,
                            mr ? std::optional{MetallicRoughness{1, 0}} : std::nullopt
                        );
                        const auto sdr = sdr_renderer.render_image(ramp_queue.items(), camera, width, height);
                        const auto hdr = ambient_renderer.render_image(ramp_queue.items(), camera, width, height);
                        const auto center = (height / 2 * width + width / 2) * 4;
                        std::cout << "sdr_hdr_reference: input=" << value << " mr=" << mr << " contrast=" << contrast
                                  << " sdr=" << int(sdr.pixels[center + 1]) << " hdr=" << int(hdr.pixels[center + 1])
                                  << '\n';
                        for (int c = 0; c < 3; ++c)
                            require(
                                std::abs(int(sdr.pixels[center + c]) - int(hdr.pixels[center + c])) <= 2,
                                "HDR toggle changed linear ambient response or SDR display mapping"
                            );
                    }
                }
            }
        }
        check_center(
            renderer.render_image({}, camera, width, height),
            expected(
                visual_rgb_for_pipeline(
                    {config.clear_r, config.clear_g, config.clear_b},
                    ColorPipelineMode::hardware_srgb
                ),
                0
            )
        );
        const auto original = renderer.render_image(queue.items(), camera, width, height);
        const auto allocations = allocation_count(device);
        require(
            renderer.render_image(queue.items(), camera, width, height).pixels == original.pixels
                && allocation_count(device) == allocations,
            "Settled HDR frame changed or allocated"
        );
        check_center(renderer.render_image(queue.items(), camera, width / 2, height / 2), expected(radiance, 0));
        check_center(renderer.render_image(queue.items(), camera, width, height), expected(radiance, 0));
        RelayCore::test::ScopedTempDirectory broken;
        for (const auto& file : std::filesystem::directory_iterator(RELAYCORE_TEST_SPIRV_DIRECTORY))
            if (file.path().extension() == ".spv" && file.path().filename() != "tone_mapping.frag.spv")
                std::filesystem::copy_file(file.path(), broken.path() / file.path().filename());
        expect_failure(
            [&] {
                renderer.reload_shaders(broken.path());
            },
            "tone_mapping.frag.spv"
        );
        require(
            renderer.render_image(queue.items(), camera, width, height).pixels == original.pixels,
            "Failed HDR shader reload replaced the working pipelines"
        );
        renderer.reload_shaders(RELAYCORE_TEST_SPIRV_DIRECTORY);
        require(
            renderer.render_image(queue.items(), camera, width, height).pixels == original.pixels,
            "HDR shader reload changed the image"
        );
        auto steam_data = quad;
        steam_data.indices.resize(6);
        for (auto& v : steam_data.vertices)
        {
            v.material_kind = float(RelayCore::surface_material_count()) + 1;
            v.color = {4, 1, .25F};
        }
        StaticColorMesh steam;
        steam.initialize(steam_data);
        RenderQueue blended;
        blended.push(mesh, glm::mat4{1});
        blended.push(steam, glm::translate(glm::mat4{1}, {0, 0, .1F}), RenderPass::Overlay);
        const auto transparent = renderer.render_image(blended.items(), camera, width, height);
        check_center(transparent, expected(radiance * .84F + glm::vec3{4, 1, .25F} * .16F, 0));
        const auto center = (height / 2 * width + width / 2) * 4;
        require(
            std::abs(int(transparent.pixels[center + 3]) - int(std::lround((.16F * .16F + .84F) * 255))) <= 1,
            "HDR composition changed scene alpha"
        );
        VulkanSwapchain swapchain{device, window, false};
        RelayCore::debug::ImGuiOverlay overlay{{.log_fps_to_console = false}};
        overlay.initialize(window, device);
        ImGui::GetIO().IniFilename = nullptr;
        overlay.begin_frame();
        ImGui::GetForegroundDrawList()
            ->AddRectFilled({0, 0}, {float(width), float(height)}, IM_COL32(128, 64, 32, 255));
        ImGui::GetForegroundDrawList()->AddRectFilled({0, 0}, {80, 80}, IM_COL32(64, 128, 192, 128));
        auto* ui = overlay.finish_frame();
        const auto check_ui = [&](const CpuRgbaTextureData& image) {
            for (int c = 0; c < 3; ++c)
                require(
                    std::abs(
                        int(image.pixels[center + c])
                        - (c == 0   ? 128
                           : c == 1 ? 64
                                    : 32)
                    ) <= 1,
                    "HDR exposure/tone mapping/sRGB conversion affected display-space UI"
                );
            const auto pixel = (10 * image.width + 10) * 4;
            for (int c = 0; c < 3; ++c)
            {
                const float front = c == 0 ? 64.0F : c == 1 ? 128.0F : 192.0F;
                const float back = c == 0 ? 128.0F : c == 1 ? 64.0F : 32.0F;
                const auto blended = std::lround(front * (128.0F / 255) + back * (127.0F / 255));
                require(
                    std::abs(int(image.pixels[pixel + c]) - int(blended)) <= 1,
                    "HDR changed display-space UI alpha blending"
                );
            }
        };
        check_ui(renderer.render_image(queue.items(), camera, width, height, {}, ui));
        config.exposure_ev = 2;
        renderer.set_config(config);
        renderer.prepare_frame(queue.items(), {}, camera, width, height);
        renderer.append_ui(ui);
        check_ui(renderer.capture_prepared_image());
        require(swapchain.present_prepared_scene(renderer), "HDR prepared UI did not present");
        overlay.shutdown();
        auto metal_data = quad;
        for (auto& vertex : metal_data.vertices)
        {
            vertex.color = {1, 1, 1};
            vertex.material_kind = float(RelayCore::SurfaceMaterial::Metal);
        }
        StaticColorMesh metal;
        metal.initialize(metal_data);
        Texture2D white;
        white.initialize_rgba(1, 1, std::array<std::uint8_t, 4>{255, 255, 255, 255}, TextureEncoding::srgb);
        RenderQueue specular;
        specular.push(metal, glm::mat4{1}, RenderPass::Opaque, &white, false, nullptr, 1, MetallicRoughness{.5F, 1});
        config.exposure_ev = 0;
        config.shadow_light_color = {1, 1, 1};
        config.shadow_light_intensity = 4;
        config.shadow_light_direction = {0, 0, -1};
        renderer.set_config(config);
        check_center(
            renderer.render_image(specular.items(), camera, width, height),
            expected(glm::vec3{16 / 3.14159265F}, 0)
        );
        config.hdr_enabled = false;
        expect_failure(
            [&] {
                renderer.set_config(config);
            },
            "reload"
        );
        config.hdr_enabled = true;
        config.exposure_ev = std::numeric_limits<float>::quiet_NaN();
        expect_failure(
            [&] {
                renderer.set_config(config);
            },
            "Exposure"
        );
    }

    void profile_views(
        const VulkanDevice& device,
        const RelayCore::Window& window,
        const RenderAuthoringConfig& authoring,
        const CpuRgbaTextureData& atlas,
        const bool native_view = false,
        const bool hdr_view = false
    )
    {
        RendererConfig config{};
        if (hdr_view)
            config =
                RelayCore::core::load_engine_config(RelayCore::core::asset_path("config/native_lab_hdr.toml")).renderer;
        const auto mode = ColorPipelineMode::hardware_srgb;
        VulkanMeshRenderer
            renderer{device, RELAYCORE_TEST_SPIRV_DIRECTORY, authoring, atlas, TextureEncoding::srgb, config, mode};
        VulkanSwapchain swapchain{device, window, false};
        StaticModelCache models{};
        RelayCore::debug::ImGuiOverlay overlay{{.log_fps_to_console = false}};
        overlay.initialize(window, device);
        ImGui::GetIO().IniFilename = nullptr;
        for (const auto* name : native_view ? std::vector<const char*>{"native_lab"}
                                            : std::vector<const char*>{"power_restoration", "pressure_transfer"})
        {
            const auto level =
                RelayCore::load_blockout_level(RelayCore::core::asset_path(std::string("levels/") + name + ".toml"));
            RelayGame::WorldSceneRenderer scene;
            scene.initialize(RelayGame::build_prepared_world_scene_data(level, authoring, mode), models);
            PerspectiveCamera camera{{.position = {4, 1.65F, 2}, .target = {4, 1.65F, 10}}};
            if (native_view)
                camera = PerspectiveCamera{{.position = {1.5F, 1.45F, 1.7F}, .target = {1.5F, 1.45F, 4}}};
            camera.set_viewport(width, height);
            for (int frame = 0; frame < (native_view ? 120 : 240); ++frame)
            {
                RELAYCORE_PROFILE_ZONE_N("Vulkan profile view");
                SDL_PumpEvents();
                const float time = frame / 60.0F;
                scene.prepare_render_queue(camera, time, {}, {}, {}, -1, {}, false);
                renderer.set_local_lights(scene.local_lights());
                renderer.set_irradiance_probes(scene.irradiance());
                renderer.set_scene_time(time);
                renderer.set_spot_light({camera.position(), camera.forward(), {1, .94F, .84F}, 18, true});
                const auto weapon = scene.prepare_shotgun_viewmodel(camera, time);
                overlay.begin_frame();
                ImGui::GetForegroundDrawList()->AddRectFilled({12, 12}, {160, 20}, IM_COL32(128, 64, 32, 255));
                require(
                    swapchain.draw_scene(renderer, scene.render_items(), camera, weapon, overlay.finish_frame()),
                    "Vulkan profiling workload did not present"
                );
                RELAYCORE_PROFILE_FRAME();
                // Pace this capture workload independently of driver VSync; it is not a throughput benchmark.
                SDL_Delay(16);
            }
        }
        overlay.shutdown();
        std::cout
            << (native_view
                    ? "vulkan_profile_native=passed (120 frames, normal map, shadows, UI, presentation)\n"
                    : "vulkan_profile_views=passed (480 frames, two maps, shadows, viewmodel, UI, presentation)\n");
    }

    void check_ui(
        const VulkanDevice& device,
        const RelayCore::Window& window,
        const RenderAuthoringConfig& authoring,
        const CpuRgbaTextureData& atlas,
        const std::filesystem::path& captures
    )
    {
        RelayCore::AudioSystem audio{{.enabled = false}};
        RendererConfig config{};
        VulkanMeshRenderer
            renderer{device, RELAYCORE_TEST_SPIRV_DIRECTORY, authoring, atlas, TextureEncoding::linear, config};
        VulkanSwapchain swapchain{device, window, false};
        StaticModelCache models{};
        RelayGame::WorldSceneRenderer scene;
        const auto level = RelayCore::load_blockout_level(RelayCore::core::asset_path("levels/pressure_transfer.toml"));
        scene.initialize(
            RelayGame::build_prepared_world_scene_data(level, authoring, ColorPipelineMode::legacy_gamma),
            models
        );
        PerspectiveCamera camera{{.position = {4, 1.65F, 2}, .target = {4, 1.65F, 10}}};
        RelayCore::debug::ImGuiOverlay overlay{{.log_fps_to_console = false}};
        PlayerDiagnostics player{};
        player.health = 72;
        player.max_health = 100;
        player.weapon_label = "Дробовик";
        player.weapon_reticle_radius = 12;
        InteractionDiagnostics interaction{};
        interaction.has_objective = true;
        interaction.objective_current = 5;
        interaction.objective_total = 7;
        interaction.objective_title = "Восстановить оборудование перепускного зала";
        interaction.objective_detail =
            "Робот проходит под верстаком; высокий шкаф закрывает огонь турели. Пульт — на правой стене.";
        interaction.has_inventory = true;
        interaction.inventory_summary = "Дробовик получен";
        interaction.has_focused_usable = true;
        interaction.focused_label = "Пульт перепуска";
        interaction.focused_detail = "Включить перепуск давления и разблокировать выход в технический сектор.";
        interaction.use_binding_label = "E";
        interaction.has_transient_event = true;
        interaction.transient_event_text = "Перепуск работает. Пульт выхода на дальней стене разблокирован.";
        interaction.transient_event_strength = 1.0F;
        player.weapon_selection_hint = "[1] Пистолет · [2] Дробовик";
        const RelayCore::FrameInfo frame{.delta_seconds = 1.0 / 60};
        for (int cycle = 0; cycle < 4; ++cycle)
        {
            PlayerUi hud{window.handle(), device};
            hud.set_screen(PlayerUiScreen::Hud);
            HudPresentation presentation{
                .visible = true,
                .health = 72,
                .max_health = 100,
                .reticle_radius = 12,
                .weapon = "Дробовик",
                .hint = "[1] Пистолет · [2] Дробовик",
                .objective_header = "ЗАДАЧА 5/7",
                .objective_title = interaction.objective_title,
                .objective_detail = interaction.objective_detail,
                .inventory = interaction.inventory_summary,
                .prompt = "[E] " + interaction.focused_label,
                .prompt_detail = interaction.focused_detail,
                .event = interaction.transient_event_text,
                .event_strength = 1
            };
            overlay.initialize(window, device);
            require(overlay.is_initialized(), "Vulkan ImGui did not initialize");
            ImGui::GetIO().IniFilename = nullptr;
            const int w = cycle == 1 ? 1920 : cycle == 2 ? 2560 : cycle == 3 ? 800 : 1280;
            const int h = cycle == 1 ? 1080 : cycle == 2 ? 1440 : cycle == 3 ? 600 : 720;
            require(SDL_SetWindowSize(window.handle(), w, h), "Vulkan UI window resize failed");
            SDL_PumpEvents();
            camera.set_viewport(w, h);
            scene.prepare_render_queue(camera, 0, {}, {}, {}, -1, {}, false);
            const auto weapon = scene.prepare_shotgun_viewmodel(camera, .4F);
            const auto base = renderer.render_image(scene.render_items(), camera, w, h, weapon);
            overlay.set_diagnostics_visible(false);
            for (int sample = 0; sample < 7; ++sample)
            {
                SDL_Event event{};
                event.type = SDL_EVENT_KEY_DOWN;
                event.key.scancode = SDL_SCANCODE_F2;
                overlay.process_event(event);
                if (sample == 1 || sample == 3)
                    overlay.toggle_diagnostics_visible();
                overlay.begin_frame();
                overlay.update_diagnostics(frame);
                overlay.draw_diagnostics(
                    frame,
                    camera,
                    audio,
                    config,
                    scene.diagnostics(),
                    {},
                    player,
                    interaction,
                    {},
                    {},
                    {}
                );
                presentation.hit = sample == 1 ? 1.0F : 0;
                presentation.hurt = sample == 2 ? 0.8F : 0;
                presentation.health = sample == 2 ? 20.0F : 72.0F;
                presentation.alive = sample != 4;
                presentation.weapon = sample == 4 ? "Ко входу через 2 с" : "Дробовик";
                presentation.prompt = sample == 5 ? "" : "[E] " + interaction.focused_label;
                presentation.event = sample == 5 ? "" : interaction.transient_event_text;
                presentation.inventory = sample == 5 ? "" : interaction.inventory_summary;
                presentation.objective_detail =
                    sample == 6 ? "Длинная русская цель: осмотрите аварийную линию и оборудование технического "
                                  "сектора, восстановите питание и вентиляцию, проверьте изолятор, откройте служебный "
                                  "выход. <Не разметка> & текст игрока."
                                : interaction.objective_detail;
                hud.set_hud(presentation);
                auto* draw = overlay.finish_frame();
                renderer.prepare_frame(scene.render_items(), scene.render_items(), camera, w, h, weapon);
                renderer.append_player_ui(hud);
                renderer.append_ui(draw);
                const auto image = renderer.capture_prepared_image();
                require(image.pixels != base.pixels, "Vulkan HUD/diagnostics did not reach the color target");
                save(image, captures / ("ui-" + std::to_string(cycle) + "-" + std::to_string(sample) + ".bmp"));
                require(swapchain.present_prepared_scene(renderer), "Vulkan UI did not present");
            }
            require(!overlay.diagnostics_visible(), "F2 diagnostics toggle did not restore hidden state");
            overlay.begin_frame();
            auto* list = ImGui::GetForegroundDrawList();
            list->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
            list->PushClipRect({12, 12}, {28, 28}, true);
            list->AddRectFilled({0, 0}, {80, 80}, IM_COL32(0, 255, 0, 255));
            list->PopClipRect();
            // Simulate a 2x framebuffer while retaining logical SDL coordinates.
            ImGui::GetIO().DisplayFramebufferScale = {2, 2};
            auto* draw = overlay.finish_frame();
            const auto clipped = renderer.render_image({}, camera, w * 2, h * 2, {}, draw);
            const auto green_at = [&](const int x, const int y) {
                const auto offset = size_t(y * (w * 2) + x) * 4;
                return clipped.pixels[offset] == 0 && clipped.pixels[offset + 1] == 255
                       && clipped.pixels[offset + 2] == 0;
            };
            require(
                green_at(30, 30) && !green_at(23, 30) && !green_at(56, 30),
                "ImGui DPI viewport/scissor is incorrect"
            );
            save(clipped, captures / ("ui-scissor-" + std::to_string(cycle) + ".bmp"));
            auto atmosphere = config;
            atmosphere.fog_start = 0;
            atmosphere.fog_end = 1;
            atmosphere.postfx_vignette_strength = .9F;
            atmosphere.postfx_scanline_strength = .8F;
            atmosphere.postfx_contrast = 1.5F;
            atmosphere.postfx_saturation = .1F;
            renderer.set_config(atmosphere);
            const auto graded_ui = renderer.render_image({}, camera, w * 2, h * 2, {}, draw);
            const auto ui_offset = size_t(30 * (w * 2) + 30) * 4;
            require(
                std::equal(
                    clipped.pixels.begin() + ui_offset,
                    clipped.pixels.begin() + ui_offset + 4,
                    graded_ui.pixels.begin() + ui_offset
                ),
                "Scene atmosphere changed an opaque HUD pixel"
            );
            renderer.set_config(config);
            overlay.shutdown();
            const auto without = renderer.render_image(scene.render_items(), camera, w, h, weapon);
            require(without.pixels == base.pixels, "UI teardown changed core scene rendering");
        }
        {
            auto hud = std::make_unique<PlayerUi>(window.handle(), device);
            hud->set_screen(PlayerUiScreen::Hud);
            hud->set_hud({.visible = true, .health = 72, .max_health = 100, .weapon = "Дробовик"});
            camera.set_viewport(width, height);
            renderer.prepare_frame(scene.render_items(), scene.render_items(), camera, width, height);
            renderer.append_player_ui(*hud);
            const auto expected = renderer.capture_prepared_image();
            {
                RelayCore::test::BlockGraphicsQueue gate{device};
                gate.block();
                renderer.prepare_frame(scene.render_items(), scene.render_items(), camera, width, height);
                renderer.append_player_ui(*hud);
                hud.reset();
                gate.release();
            }
            require(
                renderer.capture_prepared_image().pixels == expected.pixels,
                "Destroying an in-flight RmlUi context changed its retained draw"
            );
        }
        RelayCore::debug::ImGuiOverlay disabled{{.enabled = false}};
        disabled.initialize(window, device);
        require(!disabled.is_initialized() && disabled.finish_frame() == nullptr, "Disabled UI created backend state");
        require(SDL_SetWindowSize(window.handle(), width, height), "Could not restore Vulkan window size");
        SDL_PumpEvents();
    }

    void check_world(
        VulkanMeshRenderer& renderer,

        const RenderAuthoringConfig& authoring,
        const std::filesystem::path& captures,
        VulkanSwapchain& swapchain,
        const RelayCore::Window& window,
        const bool pressure_slice_only = false,
        const bool power_slice_only = false
    )
    {
        StaticModelCache models{};
        const auto cylinder_path = RelayCore::core::asset_path("models/service_cylinder.obj");
        const auto cylinder_texture = RelayCore::core::asset_path("models/service_cylinder.bmp");
        const auto cylinder = models.load(cylinder_path, cylinder_texture, TextureEncoding::linear);
        require(
            cylinder == models.load(cylinder_path, cylinder_texture, TextureEncoding::linear),
            "OBJ cache missed unchanged model"
        );
        for (const auto* level_name : {"power_restoration", "pressure_transfer"})
        {
            if (pressure_slice_only && std::string_view{level_name} != "pressure_transfer")
                continue;
            if (power_slice_only && std::string_view{level_name} != "power_restoration")
                continue;
            const auto level = RelayCore::load_blockout_level(
                RelayCore::core::asset_path(std::string("levels/") + level_name + ".toml")
            );
            const auto prepared =
                RelayGame::build_prepared_world_scene_data(level, authoring, ColorPipelineMode::legacy_gamma);
            RelayGame::WorldSceneRenderer scene;
            scene.initialize(prepared, models);
            std::vector<RelayCore::gameplay::RuntimeDoor> doors;
            std::vector<RelayCore::gameplay::RuntimeTrigger> triggers;
            std::vector<RelayCore::gameplay::RuntimeUsable> usables;
            RelayCore::gameplay::build_runtime_interactions(level, doors, triggers, usables);
            const bool pressure = std::string_view{level_name} == "pressure_transfer";
            const std::array positions = pressure ? std::array{glm::vec3{4, 1.65F, 2}, glm::vec3{32, 1.65F, 29}}
                                                  : std::array{glm::vec3{4.5F, 1.65F, 7}, glm::vec3{0, 1.65F, -3.5F}};
            for (size_t view = 0; view < positions.size(); ++view)
            {
                if (pressure_slice_only && view != 1)
                    continue;
                PerspectiveCamera camera{{.position = positions[view], .target = positions[view] + glm::vec3{0, 0, 1}}};
                camera.set_viewport(width, height);
                scene.prepare_render_queue(camera, 0, doors, {}, usables, -1, {}, false);
                require(
                    scene.diagnostics().visible_chunks > 0 && scene.diagnostics().culled_chunks > 0,
                    "Level comparison did not exercise visible and culled chunks"
                );
                renderer.set_local_lights(scene.local_lights());

                const auto viewmodel = scene.prepare_shotgun_viewmodel(camera, .4F);
                const auto name = std::string(level_name) + "-" + std::to_string(view);
                compare(renderer.render_image(scene.render_items(), camera, width, height), name, captures);
                compare(
                    renderer.render_image(scene.render_items(), camera, width, height, viewmodel),
                    name + "-shotgun",
                    captures
                );
                for (int frame = 0; frame < 3; ++frame)
                {
                    SDL_PumpEvents();
                    require(
                        swapchain.draw_scene(renderer, scene.render_items(), camera, viewmodel),
                        "Vulkan mesh scene did not present"
                    );
                }
            }
            scene.initialize(prepared, models);
            PerspectiveCamera reload_camera{{.position = positions[0], .target = positions[0] + glm::vec3{0, 0, 1}}};
            reload_camera.set_viewport(width, height);
            scene.prepare_render_queue(reload_camera, 0, doors, {}, usables, -1, {}, false);
            renderer.set_local_lights(scene.local_lights());

            require(
                swapchain.draw_scene(renderer, scene.render_items(), reload_camera),
                "Reloaded level did not present"
            );
            scene.shutdown();
        }
        renderer.set_local_lights({});
        if (pressure_slice_only || power_slice_only)
            return;

        // An OBJ prop under a rotated/nonuniform transform also checks model matrix and normal/atlas conventions.
        PerspectiveCamera camera{{.position = {0, 1.5F, 3}, .target = {0, .5F, 0}}};
        camera.set_viewport(width, height);
        RenderQueue props;
        props.push(
            cylinder->mesh,
            glm::rotate(glm::scale(glm::mat4{1}, {1.2F, .8F, 1.1F}), .5F, glm::vec3{0, 1, 0}),
            RenderPass::Opaque,
            &cylinder->texture
        );
        compare(renderer.render_image(props.items(), camera, width, height), "obj-prop", captures);
        RelayCore::test::ScopedTempDirectory reload;
        const auto mesh_path = reload.path() / "model.obj", texture_path = reload.path() / "model.bmp";
        std::filesystem::copy_file(cylinder_path, mesh_path);
        std::filesystem::copy_file(cylinder_texture, texture_path);
        const auto old = models.load(mesh_path, texture_path, TextureEncoding::linear);
        std::ofstream(mesh_path, std::ios::app) << "\n# changed mesh source\n";
        const auto changed = models.load(mesh_path, texture_path, TextureEncoding::linear);
        require(
            old != changed && changed == models.load(mesh_path, texture_path, TextureEncoding::linear),
            "OBJ cache did not replace and reuse the changed model"
        );
        const auto encoded = models.load(mesh_path, texture_path, TextureEncoding::srgb);
        require(
            encoded != changed && encoded->texture.encoding() == TextureEncoding::srgb,
            "OBJ cache aliased linear and sRGB textures"
        );
        save(CpuRgbaTextureData{1, 1, {255, 64, 32, 255}}, texture_path);
        const auto retextured = models.load(mesh_path, texture_path, TextureEncoding::linear);
        require(retextured != changed, "OBJ cache ignored changed texture source");
        for (const auto& resource : {old, changed, encoded, retextured})
        {
            RenderQueue queue;
            queue.push(resource->mesh, glm::mat4{1}, RenderPass::Opaque, &resource->texture);
            compare(
                renderer.render_image(queue.items(), camera, width, height),
                "obj-cache-"
                    + std::to_string(
                        resource == old       ? 0
                        : resource == changed ? 1
                        : resource == encoded ? 2
                                              : 3
                    ),
                captures
            );
        }
        require(SDL_SetWindowSize(window.handle(), 480, 270), "Could not resize Vulkan mesh window");
        SDL_PumpEvents();
        camera.set_viewport(480, 270);
        require(swapchain.draw_scene(renderer, props.items(), camera), "Resized Vulkan mesh scene did not present");
        require(
            swapchain.extent().width == 480 && swapchain.extent().height == 270,
            "Mesh swapchain resize extent differs"
        );
    }
} // namespace

int main(int argc, char** argv)
{
    const bool shadows_only = argc == 2 && std::string_view{argv[1]} == "--shadows-only";
    const bool point_receiver_only = argc == 2 && std::string_view{argv[1]} == "--point-receiver-only";
    const bool color_baseline = argc == 2 && std::string_view{argv[1]} == "--color-baseline";
    const bool profiling = argc == 2 && std::string_view{argv[1]} == "--profile-views";
    const bool native_view = argc == 2 && std::string_view{argv[1]} == "--profile-native";
    const bool mr_only = argc == 2 && std::string_view{argv[1]} == "--metallic-roughness";
    const bool emission_only = argc == 2 && std::string_view{argv[1]} == "--emission";
    const bool msaa_only = argc == 2 && std::string_view{argv[1]} == "--msaa";
    const bool hdr_only = argc == 2 && std::string_view{argv[1]} == "--hdr";
    const bool probes_only = argc == 2 && std::string_view{argv[1]} == "--irradiance";
    const bool resources_only = argc == 2 && std::string_view{argv[1]} == "--resource-preparation";
    const bool pistol_only = argc == 2 && std::string_view{argv[1]} == "--pistol-viewmodel";
    const bool shotgun_only = argc == 2 && std::string_view{argv[1]} == "--shotgun-viewmodel";
    const bool pressure_slice_only = argc == 2 && std::string_view{argv[1]} == "--pressure-transfer-slice";
    const bool power_slice_only = argc == 2 && std::string_view{argv[1]} == "--power-restoration-slice";
    const bool ui_only = argc == 2 && std::string_view{argv[1]} == "--ui-only";
    const bool pipeline_only = argc == 2 && std::string_view{argv[1]} == "--pipeline-only";
    const bool native_hdr = argc == 2 && std::string_view{argv[1]} == "--profile-native-hdr";
    if (argc > 1 && !shadows_only && !color_baseline && !profiling && !native_view && !mr_only && !emission_only
        && !hdr_only && !msaa_only && !native_hdr && !probes_only && !point_receiver_only && !resources_only
        && !pressure_slice_only && !ui_only && !pipeline_only && !power_slice_only && !pistol_only && !shotgun_only)
        return 1;
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr << SDL_GetError() << '\n';
        return 77;
    }
    int result = 0;
    RelayCore::test::ScopedTempDirectory temp;
    RelayCore::log::init({.logs_directory = temp.path().string(), .enable_timestamped_log = false});
    try
    {
        const auto captures = std::filesystem::current_path() / "vulkan-mesh-captures";
        std::filesystem::create_directories(captures);
        RelayCore::Window vk_window{{.title = "Vulkan mesh parity", .width = width, .height = height}};
        SDL_ShowWindow(vk_window.handle());
        VulkanDevice device{vk_window};
        const auto authoring = load_default_render_authoring_config();
        const auto loaded_atlas =
            load_blockout_texture_atlas(RelayCore::core::asset_path("textures/blockout_atlas.bmp"));
        const auto atlas = loaded_atlas ? *loaded_atlas : build_blockout_texture_atlas(authoring);
        if (pistol_only)
            check_pistol_viewmodel(device, authoring, atlas, captures);
        else if (shotgun_only)
            check_shotgun_viewmodel(device, authoring, atlas, captures);
        else if (pipeline_only)
            check_mesh_pipeline(device, atlas, captures);
        else if (ui_only)
            check_ui(device, vk_window, authoring, atlas, captures);
        else if (resources_only)
        {
            VulkanMeshRenderer renderer{
                device,
                RELAYCORE_TEST_SPIRV_DIRECTORY,
                authoring,
                atlas,
                TextureEncoding::linear,
                RendererConfig{}
            };
            check_resource_preparation(renderer, device, authoring);
        } else if (point_receiver_only)
            check_point_shadow_receiver(device, authoring, atlas, captures);
        else if (native_hdr)
            profile_views(device, vk_window, authoring, atlas, true, true);
        else if (native_view)
            profile_views(device, vk_window, authoring, atlas, true);
        else if (profiling)
            profile_views(device, vk_window, authoring, atlas);
        else if (msaa_only)
            check_msaa(device, authoring, atlas, captures);
        else if (mr_only)
            check_metallic_roughness(device, authoring, atlas, captures);
        else if (emission_only)
            check_emission(device, authoring, atlas, captures);
        else if (hdr_only)
            check_hdr(device, vk_window, authoring, atlas, captures);
        else if (probes_only)
            check_irradiance(device, authoring, atlas, captures);
        else if (color_baseline)
            check_color_baseline(device, vk_window, authoring, atlas, captures);
        else if (pressure_slice_only || power_slice_only)
        {
            VulkanMeshRenderer renderer{
                device,
                RELAYCORE_TEST_SPIRV_DIRECTORY,
                authoring,
                atlas,
                TextureEncoding::linear,
                RendererConfig{}
            };
            VulkanSwapchain swapchain{device, vk_window, false};
            check_world(renderer, authoring, captures, swapchain, vk_window, pressure_slice_only, power_slice_only);
        } else
        {
            if (!shadows_only)
                check_ui(device, vk_window, authoring, atlas, captures);
            require(allocation_count(device) == 0, "Vulkan UI scene leaked VMA resources");
            // CPU assets prepare independently of GPU upload and render cache lifetime.
            {
                StaticModelCache models{};
                RelayGame::WorldSceneRenderer scene;
                const auto level =
                    RelayCore::load_blockout_level(RelayCore::core::asset_path("levels/pressure_transfer.toml"));
                scene.initialize(
                    RelayGame::build_prepared_world_scene_data(level, authoring, ColorPipelineMode::legacy_gamma),
                    models
                );
                PerspectiveCamera camera{{.position = {4, 1.65F, 2}, .target = {4, 1.65F, 10}}};
                camera.set_viewport(width, height);
                scene.prepare_render_queue(camera, 0, {}, {}, {}, -1, {}, false);
                require(
                    !scene.render_items().empty(),
                    "CPU-mode world did not prepare a render queue without GPU upload"
                );
                const auto shotgun = scene.prepare_shotgun_viewmodel(camera, 0);
                require(
                    shotgun[0].mesh->cpu_data() && shotgun[0].diffuse_texture->valid(),
                    "CPU-mode OBJ resources are incomplete"
                );
            }
            if (!shadows_only)
            {
                check_mesh_pipeline(device, atlas, captures);
                VulkanMeshRenderer renderer{
                    device,
                    RELAYCORE_TEST_SPIRV_DIRECTORY,
                    authoring,
                    atlas,
                    TextureEncoding::linear,
                    RendererConfig{}
                };
                VulkanSwapchain swapchain{device, vk_window, false};
                check_world(renderer, authoring, captures, swapchain, vk_window);
            }
            if (!shadows_only)
            {
                check_lighting(device, authoring, atlas, captures);
                check_light_selection(device, authoring, atlas, captures);
                check_color_post(device, authoring, atlas, captures);
                check_color_world(device, authoring, atlas, captures);
                check_model_products(device, authoring, atlas, captures);
                check_normal_maps(device, authoring, atlas, captures);
                check_metallic_roughness(device, authoring, atlas, captures);
            }
            if (!shadows_only)
                check_point_shadow_receiver(device, authoring, atlas, captures);
            check_shadows(device, vk_window, authoring, atlas, captures);
        }
        require(allocation_count(device) == 0, "Vulkan mesh renderer leaked VMA resources");
        std::cout << "vulkan_mesh=passed\n";
    } catch (const std::exception& error)
    {
        std::cerr << "Vulkan mesh failed: " << error.what() << '\n';
        result = 1;
    }
    RelayCore::log::shutdown();
    SDL_Quit();
    return result;
}

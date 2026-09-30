#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/blockout_texture_atlas.h"
#include "relaycore/render/camera.h"
#include "relaycore/render/gl_loader.h"
#include "relaycore/render/shader_program.h"
#include "relaycore/render/shader_source_loader.h"
#include "relaycore/render/vulkan_device.h"
#include "relaycore/render/vulkan_mesh_renderer.h"
#include "relaycore/render/vulkan_swapchain.h"
#include "relaycore/world/blockout_loader.h"
#include "relaygame/world_scene_renderer.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <unordered_map>

#include "scoped_temp_directory.h"

namespace
{
    using namespace RelayCore::render;
    constexpr int width = 640, height = 360;

    void require(const bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
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

    // Uses the existing GL vertex/surface/atlas source; excludes lighting, overlays and postfx owned by later gates.
    class GlMeshReference {
    public:
        GlMeshReference(
            const std::filesystem::path& temporary,
            const RenderAuthoringConfig& authoring,
            const CpuRgbaTextureData& atlas,
            const TextureEncoding encoding
        )
        {
            const auto fragment = temporary / "mesh-reference.frag";
            std::ofstream source(fragment);
            source << R"(#version 460 core
in vec3 vertex_color, world_position, world_normal, object_position, object_normal;
in vec2 model_uv;
flat in float material_kind;
layout(location=0) out vec4 out_color;
layout(location=10) uniform int model_textured;
layout(binding=0) uniform sampler2D u_blockout_atlas;
layout(location=20) uniform vec2 atlas_parameters[32];
float rc_material_atlas_world_scale(int id) { return atlas_parameters[id].x; }
)";
            for (const auto* file : {"scene_math.glsl", "scene_surface.glsl", "scene_atlas.glsl"})
                source << load_shader_source(RelayCore::core::asset_path(std::string("shaders/") + file)) << '\n';
            source << R"(
void main() {
    if (model_textured != 0) { out_color=vec4(texture(u_blockout_atlas, model_uv).rgb*vertex_color, 1); return; }
    int id=int(material_kind+0.5);
    if (id>=32) { out_color=vec4(vertex_color,1); return; }
    bool clothing=id>=23 && id<=26;
    vec2 uv=clothing ? surface_uv(object_position,normalize(object_normal)) : surface_uv(world_position,normalize(world_normal));
    out_color=vec4(mix(vertex_color,vertex_color*sample_blockout_atlas(id,uv),atlas_parameters[id].y),1);
})";
            source.close();
            program_.load_from_files(RelayCore::core::asset_path("shaders/scene_opaque.vert"), fragment);
            program_.bind();
            std::array<glm::vec2, 32> parameters{};
            for (size_t i = 0; i < authoring.materials.size(); ++i)
                parameters[i] = {authoring.materials[i].atlas_world_scale, authoring.materials[i].atlas_mix};
            glUniform2fv(20, 32, &parameters.front().x);
            atlas_.initialize_rgba(atlas.width, atlas.height, atlas.pixels, encoding);
            glGenFramebuffers(1, &framebuffer_);
            glGenTextures(1, &color_);
            glGenRenderbuffers(1, &depth_);
            glBindTexture(GL_TEXTURE_2D, color_);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glBindRenderbuffer(GL_RENDERBUFFER, depth_);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, width, height);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_, 0);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_);
            require(
                glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                "GL parity framebuffer incomplete"
            );
        }
        ~GlMeshReference()
        {
            glDeleteFramebuffers(1, &framebuffer_);
            glDeleteTextures(1, &color_);
            glDeleteRenderbuffers(1, &depth_);
        }
        void clear_cache()
        {
            meshes_.clear();
            textures_.clear();
        }
        auto render(
            const std::span<const RenderItem> items,
            const PerspectiveCamera& camera,
            const std::span<const RenderItem> viewmodel = {}
        ) -> CpuRgbaTextureData
        {
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
            glViewport(0, 0, width, height);
            glDisable(GL_FRAMEBUFFER_SRGB);
            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glDepthFunc(GL_LEQUAL);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            glFrontFace(GL_CCW);
            glClearColor(0, 0, 0, 1);
            glClearDepth(1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            program_.bind();
            draw(items, camera);
            if (!viewmodel.empty())
            {
                glClear(GL_DEPTH_BUFFER_BIT);
                draw(viewmodel, camera);
            }
            CpuRgbaTextureData output{width, height, std::vector<uint8_t>(width * height * 4)};
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, output.pixels.data());
            require(glGetError() == GL_NO_ERROR, "OpenGL mesh comparison failed");
            for (int y = 0; y < height / 2; ++y)
                std::swap_ranges(
                    output.pixels.begin() + y * width * 4,
                    output.pixels.begin() + (y + 1) * width * 4,
                    output.pixels.begin() + (height - 1 - y) * width * 4
                );
            return output;
        }

    private:
        void draw(const std::span<const RenderItem> items, const PerspectiveCamera& camera)
        {
            for (const auto& item : items)
            {
                if (!item.mesh || item.pass != RenderPass::Opaque)
                    continue;
                const auto& cpu = item.mesh->cpu_data();
                auto [geometry, inserted] = meshes_.try_emplace(cpu);
                if (inserted)
                    geometry->second.initialize(*cpu);
                if (item.diffuse_texture)
                {
                    const auto& texture = item.diffuse_texture->cpu_data();
                    auto [entry, new_texture] = textures_.try_emplace(texture);
                    if (new_texture)
                        entry->second.initialize_rgba(
                            texture->width,
                            texture->height,
                            texture->pixels,
                            item.diffuse_texture->encoding()
                        );
                    entry->second.bind(0);
                } else
                    atlas_.bind(0);
                program_.set_mat4(0, camera.view_projection_matrix() * item.model_matrix);
                program_.set_mat4(1, item.model_matrix);
                program_.set_int(10, item.diffuse_texture ? 1 : 0);
                geometry->second.draw();
            }
        }
        ShaderProgram program_;
        Texture2D atlas_;
        unsigned framebuffer_ = 0, color_ = 0, depth_ = 0;
        std::unordered_map<std::shared_ptr<const CpuColorMeshData>, StaticColorMesh> meshes_;
        std::unordered_map<std::shared_ptr<const CpuRgbaTextureData>, Texture2D> textures_;
    };

    void compare(
        const CpuRgbaTextureData& gl,
        const CpuRgbaTextureData& vk,
        const std::string& name,
        const std::filesystem::path& captures
    )
    {
        save(gl, captures / (name + "-gl.bmp"));
        save(vk, captures / (name + "-vk.bmp"));
        require(gl.width == vk.width && gl.height == vk.height, "Mesh image extent differs");
        size_t coverage_diff = 0, color_diff = 0, covered = 0;
        double sum = 0;
        for (size_t i = 0; i < gl.pixels.size(); i += 4)
        {
            const bool a = gl.pixels[i] || gl.pixels[i + 1] || gl.pixels[i + 2];
            const bool b = vk.pixels[i] || vk.pixels[i + 1] || vk.pixels[i + 2];
            coverage_diff += a != b;
            covered += a;
            int max_delta = 0;
            for (size_t c = 0; c < 3; ++c)
            {
                const auto delta = std::abs(int(gl.pixels[i + c]) - int(vk.pixels[i + c]));
                sum += delta;
                max_delta = std::max(delta, max_delta);
            }
            color_diff += max_delta > 8;
            require(vk.pixels[i + 3] == 255, "Vulkan mesh target lost opaque alpha");
        }
        const double count = double(gl.width) * gl.height;
        std::cout << name << ": coverage=" << covered << " mask_diff=" << coverage_diff / count
                  << " color_diff=" << color_diff / count << " mean_rgb_delta=" << sum / (count * 3) << '\n';
        require(covered > count * .01, "Reference scene has no meaningful visible geometry");
        require(coverage_diff / count < .002, "Vulkan placement/winding/clipping differs from OpenGL");
        require(
            color_diff / count < .01 && sum / (count * 3) < 1.0,
            "Vulkan depth/UV/texture color differs from OpenGL"
        );
    }

    auto allocation_count(const VulkanDevice& device) -> uint32_t
    {
        VmaTotalStatistics stats{};
        vmaCalculateStatistics(device.allocator(), &stats);
        return stats.total.statistics.allocationCount;
    }

    void check_pipeline(
        VulkanMeshRenderer& renderer,
        GlMeshReference& reference,
        const std::filesystem::path& captures,
        const VulkanDevice& device
    )
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
        mesh.initialize(box, ResourceUpload::cpu);
        CpuRgbaTextureData data{2, 2, {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 0, 255}};
        Texture2D texture;
        uint32_t previous_allocations = 0;
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            texture.initialize_rgba(
                2,
                2,
                data.pixels,
                cycle == 1 ? TextureEncoding::srgb : TextureEncoding::linear,
                ResourceUpload::cpu
            );
            mesh.initialize(box, ResourceUpload::cpu);
            RenderQueue queue;
            queue.push(
                mesh,
                glm::translate(glm::mat4{1}, {-.4F, .2F, 0}) * glm::scale(glm::mat4{1}, {1.3F, .8F, 1}),
                RenderPass::Opaque,
                &texture
            );
            queue.push(mesh, glm::translate(glm::mat4{1}, {.7F, -.35F, -1}), RenderPass::Opaque, &texture);
            compare(
                reference.render(queue.items(), camera),
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
            reference.clear_cache();
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
                mesh.initialize(invalid, ResourceUpload::cpu);
            },
            "outside its vertex"
        );
        const auto resized = renderer.render_image({}, camera, 127, 63);
        require(
            resized.pixels.size() == 127 * 63 * 4 && resized.width == 127 && resized.height == 63,
            "Vulkan mesh target resize failed"
        );
    }

    void check_world(
        VulkanMeshRenderer& renderer,
        GlMeshReference& reference,
        const RenderAuthoringConfig& authoring,
        const std::filesystem::path& captures,
        VulkanSwapchain& swapchain,
        const RelayCore::Window& window
    )
    {
        StaticModelCache models{ResourceUpload::cpu};
        const auto cylinder_path = RelayCore::core::asset_path("models/service_cylinder.obj");
        const auto cylinder_texture = RelayCore::core::asset_path("models/service_cylinder.bmp");
        const auto cylinder = models.load(cylinder_path, cylinder_texture, TextureEncoding::linear);
        require(
            cylinder == models.load(cylinder_path, cylinder_texture, TextureEncoding::linear),
            "OBJ cache missed unchanged model"
        );
        for (const auto* level_name : {"power_restoration", "pressure_transfer"})
        {
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
                PerspectiveCamera camera{{.position = positions[view], .target = positions[view] + glm::vec3{0, 0, 1}}};
                camera.set_viewport(width, height);
                scene.prepare_render_queue(camera, 0, doors, {}, usables, -1, {}, false);
                require(
                    scene.diagnostics().visible_chunks > 0 && scene.diagnostics().culled_chunks > 0,
                    "Level comparison did not exercise visible and culled chunks"
                );
                const auto viewmodel = scene.prepare_shotgun_viewmodel(camera, .4F);
                const auto name = std::string(level_name) + "-" + std::to_string(view);
                compare(
                    reference.render(scene.render_items(), camera),
                    renderer.render_image(scene.render_items(), camera, width, height),
                    name,
                    captures
                );
                compare(
                    reference.render(scene.render_items(), camera, viewmodel),
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
            require(
                swapchain.draw_scene(renderer, scene.render_items(), reload_camera),
                "Reloaded level did not present"
            );
            scene.shutdown();
        }
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
        compare(
            reference.render(props.items(), camera),
            renderer.render_image(props.items(), camera, width, height),
            "obj-prop",
            captures
        );
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
                reference.render(queue.items(), camera),
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

int main()
{
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
        RelayCore::Window vk_window{
            {.title = "Vulkan mesh parity", .width = width, .height = height},
            ColorPipelineMode::legacy_gamma,
            RelayCore::WindowGraphicsApi::vulkan
        };
        SDL_ShowWindow(vk_window.handle());
        VulkanDevice device{vk_window};
        const auto authoring = load_default_render_authoring_config();
        const auto loaded_atlas =
            load_blockout_texture_atlas(RelayCore::core::asset_path("textures/blockout_atlas.bmp"));
        const auto atlas = loaded_atlas ? *loaded_atlas : build_blockout_texture_atlas(authoring);
        // World/OBJ/mesh/texture lifecycle must also work before GL context creation and entry-point loading.
        {
            StaticModelCache models{ResourceUpload::cpu};
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
            require(!scene.render_items().empty(), "CPU-mode world did not prepare a render queue without OpenGL");
            const auto shotgun = scene.prepare_shotgun_viewmodel(camera, 0);
            require(
                shotgun[0].mesh->cpu_data() && shotgun[0].diffuse_texture->valid(),
                "CPU-mode OBJ resources are incomplete"
            );
        }
        RelayCore::Window gl_window{{.title = "OpenGL mesh reference", .width = width, .height = height}};
        SDL_HideWindow(gl_window.handle());
        gl::load();
        {
            GlMeshReference reference{temp.path(), authoring, atlas, TextureEncoding::linear};
            VulkanMeshRenderer
                renderer{device, RELAYCORE_TEST_SPIRV_DIRECTORY, authoring, atlas, TextureEncoding::linear};
            VulkanSwapchain swapchain{device, vk_window, false};
            check_pipeline(renderer, reference, captures, device);
            check_world(renderer, reference, authoring, captures, swapchain, vk_window);
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

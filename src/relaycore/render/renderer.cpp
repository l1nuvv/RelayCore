#include "relaycore/render/renderer.h"

#include "relaycore/audio/audio_system.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/core/profiling.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/blockout_texture_atlas.h"
#include "relaycore/render/vulkan_device.h"
#include "relaycore/render/vulkan_mesh_renderer.h"
#include "relaycore/render/vulkan_swapchain.h"

#include <algorithm>
#include <cmath>
#include <glm/trigonometric.hpp>
#include <imgui.h>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace RelayCore
{
    namespace
    {
        void draw_shadow_debug_banner()
        {
            RELAYCORE_PROFILE_FUNCTION();
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImDrawList* draw_list = ImGui::GetForegroundDrawList();

            const char* label = "SHADOW CASCADE DEBUG";
            const char* detail = "Cascade tint is enabled. Disable it in Diagnostics -> Shadow Tuning.";
            const ImVec2 label_size = ImGui::CalcTextSize(label);
            const ImVec2 detail_size = ImGui::CalcTextSize(detail);
            const float width = std::max(label_size.x, detail_size.x) + 40.0F;
            const float height = label_size.y + detail_size.y + 28.0F;
            const ImVec2 min{
                viewport->WorkPos.x + (viewport->WorkSize.x - width) * 0.5F,
                viewport->WorkPos.y + 18.0F,
            };
            const ImVec2 max{min.x + width, min.y + height};

            draw_list
                ->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(ImVec4(0.17F, 0.09F, 0.05F, 0.92F)), 10.0F);
            draw_list
                ->AddRect(min, max, ImGui::ColorConvertFloat4ToU32(ImVec4(0.98F, 0.66F, 0.24F, 0.85F)), 10.0F, 0, 1.5F);
            draw_list->AddText(
                ImVec2(min.x + 20.0F, min.y + 10.0F),
                ImGui::ColorConvertFloat4ToU32(ImVec4(1.0F, 0.93F, 0.76F, 1.0F)),
                label
            );
            draw_list->AddText(
                ImVec2(min.x + 20.0F, min.y + 10.0F + label_size.y + 6.0F),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.96F, 0.84F, 0.68F, 1.0F)),
                detail
            );
        }

    } // namespace

    struct Renderer::State {
        const Window* window = nullptr;
        std::unique_ptr<render::VulkanDevice> device;
        std::filesystem::path pipeline_cache_file;
        std::optional<render::VulkanMemoryUsage> memory_snapshot;
        render::VulkanResourceUsage resource_snapshot{};
        render::VulkanMaterialDescriptorUsage descriptor_snapshot{};
        std::unique_ptr<render::VulkanSwapchain> swapchain;
        std::shared_ptr<render::VulkanMeshRenderer> backend;
        // UI reload may replace backend after scene submission. Keep that frame alive through present.
        std::shared_ptr<render::VulkanMeshRenderer> frame_backend;
        std::vector<render::RenderItem> items;
        std::vector<render::RenderItem> shadow_casters;
        std::vector<render::RenderItem> viewmodel;

        ~State() noexcept
        {
            swapchain.reset();
            frame_backend.reset();
            backend.reset();
        }
    };

    Renderer::Renderer(const render::RendererConfig& config, render::ColorPipelineMode color_pipeline_mode) :
        config_(config), color_pipeline_mode_(color_pipeline_mode), camera_(config.camera),
        imgui_overlay_(config.imgui), state_(std::make_unique<State>())
    {}

    Renderer::~Renderer() noexcept
    {
        // Drain presentation before destroying descriptors, pipelines or the ImGui backend.
        state_->swapchain.reset();
        state_->frame_backend.reset();
        state_->backend.reset();
        player_ui_.reset();
        imgui_overlay_.shutdown();
        if (state_->device && !state_->pipeline_cache_file.empty()
            && !state_->device->save_pipeline_cache(state_->pipeline_cache_file, core::asset_path("shaders/vulkan")))
            log::warn("Could not persist Vulkan pipeline cache: {}", state_->pipeline_cache_file.string());
    }

    void Renderer::initialize(const Window& window)
    {
        if (initialized_)
            return;
        state_->window = &window;
        state_->device = std::make_unique<render::VulkanDevice>(window);
        if (auto* preferences = SDL_GetPrefPath("RelayCore", "RelayCore"))
        {
            state_->pipeline_cache_file = std::filesystem::path{preferences} / "vulkan-pipelines.cache";
            SDL_free(preferences);
            log::info(
                "Vulkan pipeline cache: {}",
                state_->device->load_pipeline_cache(state_->pipeline_cache_file, core::asset_path("shaders/vulkan"))
                    ? "loaded"
                    : "fresh (missing, incompatible or corrupt)"
            );
        }
        state_->swapchain = std::make_unique<render::VulkanSwapchain>(*state_->device, window, window.vsync_enabled());
        commit_live_content_reload(prepare_live_content_reload(config_, render::load_default_render_authoring_config())
        );
        camera_.set_viewport(viewport_width_, viewport_height_);
        imgui_overlay_.initialize(window, *state_->device);
        player_ui_ = std::make_unique<render::PlayerUi>(window.handle(), *state_->device);
        initialized_ = true;
        const auto& capabilities = state_->device->capabilities();
        log::info(
            "Renderer initialized: Vulkan {}.{}, {}",
            VK_API_VERSION_MAJOR(capabilities.effective_api_version),
            VK_API_VERSION_MINOR(capabilities.effective_api_version),
            capabilities.device_name
        );
    }

    auto Renderer::prepare_live_content_reload(
        const render::RendererConfig& config,
        const render::RenderAuthoringConfig& authoring_config
    ) -> PreparedLiveContentReload
    {
        return prepare_live_content_reload(config, authoring_config, color_pipeline_mode_);
    }

    auto Renderer::prepare_live_content_reload(
        const render::RendererConfig& config,
        const render::RenderAuthoringConfig& authoring_config,
        const render::ColorPipelineMode color_pipeline_mode
    ) -> PreparedLiveContentReload
    {
        PreparedLiveContentReload staged_reload{};
        staged_reload.config = config;
        staged_reload.authoring_config = authoring_config;
        staged_reload.color_pipeline_mode = color_pipeline_mode;
        const std::filesystem::path atlas_path = core::asset_path("textures/blockout_atlas.bmp");
        render::CpuRgbaTextureData atlas{};
        if (const auto loaded_atlas = render::load_blockout_texture_atlas(atlas_path); loaded_atlas.has_value())
        {
            atlas = *loaded_atlas;
            staged_reload.atlas_source_label = atlas_path.string();
        } else
        {
            atlas = render::build_blockout_texture_atlas(authoring_config);
            staged_reload.atlas_source_label = "generated fallback";
        }

        staged_reload.backend = std::make_shared<render::VulkanMeshRenderer>(
            *state_->device,
            core::asset_path("shaders/vulkan"),
            authoring_config,
            atlas,
            color_pipeline_mode == render::ColorPipelineMode::hardware_srgb ? render::TextureEncoding::srgb
                                                                            : render::TextureEncoding::linear,
            config,
            color_pipeline_mode
        );
        return staged_reload;
    }

    void Renderer::PreparedLiveContentReload::prepare_resources(std::span<const render::RenderItem> items)
    {
        if (!backend)
            throw std::logic_error("Resource preparation requires a prepared Vulkan backend");
        backend->prepare_resources(items);
    }

    void Renderer::prepare_resources(std::span<const render::RenderItem> items)
    {
        state_->backend->prepare_resources(items);
    }

    void Renderer::commit_live_content_reload(PreparedLiveContentReload staged_reload)
    {
        static_assert(std::is_nothrow_move_assignable_v<render::RendererConfig>);
        static_assert(std::is_nothrow_move_assignable_v<render::RenderAuthoringConfig>);
        if (!staged_reload.backend)
            throw std::invalid_argument("Live content reload requires a prepared Vulkan backend");
        camera_.apply_projection_config(staged_reload.config.camera);
        state_->backend = std::move(staged_reload.backend);
        config_ = std::move(staged_reload.config);
        color_pipeline_mode_ = staged_reload.color_pipeline_mode;
        authoring_config_ = std::move(staged_reload.authoring_config);
        camera_.set_viewport(viewport_width_, viewport_height_);
        imgui_overlay_.apply_config(config_.imgui);
    }

    void Renderer::process_event(const SDL_Event& event)
    {
        imgui_overlay_.process_event(event);
        if (player_ui_)
            player_ui_->process_event(event);
    }

    void Renderer::on_resize(int width, int height) noexcept
    {
        if (width <= 0 || height <= 0)
            return;
        viewport_width_ = width;
        viewport_height_ = height;
        camera_.set_viewport(width, height);
    }

    void Renderer::begin_frame()
    {
        irradiance_probes_.reset();
        reflection_probe_.reset();
        state_->frame_backend.reset();
        state_->items.clear();
        state_->shadow_casters.clear();
        state_->viewmodel.clear();
    }

    void Renderer::draw_render_queue(
        std::span<const render::RenderItem> items,
        std::span<const render::RenderItem> shadow_casters
    )
    {
        state_->frame_backend.reset();
        state_->items.insert(state_->items.end(), items.begin(), items.end());
        state_->shadow_casters.insert(state_->shadow_casters.end(), shadow_casters.begin(), shadow_casters.end());
    }

    void Renderer::draw_viewmodel(std::span<const render::RenderItem> items)
    {
        state_->viewmodel.assign(items.begin(), items.end());
    }

    void Renderer::finish_scene()
    {
        if (state_->frame_backend)
            return;
        state_->backend->set_config(config_);
        state_->backend->set_local_lights(local_lights_);
        state_->backend->set_irradiance_probes(irradiance_probes_);
        state_->backend->set_reflection_probe(reflection_probe_);
        state_->backend->set_spot_light(spot_light_);
        state_->backend->set_scene_time(scene_time_seconds_);
        state_->backend->prepare_frame(
            state_->items,
            state_->shadow_casters,
            camera_,
            viewport_width_,
            viewport_height_,
            state_->viewmodel
        );
        world_diagnostics_.shadow_enabled = config_.shadow_enabled;
        world_diagnostics_.shadow_cascade_count = config_.shadow_enabled ? static_cast<std::uint32_t>(2) : 0U;
        world_diagnostics_.indirect_bounce_strength = std::max(config_.indirect_bounce_strength, 0.0F);
        world_diagnostics_.shadow_map_resolution =
            config_.shadow_enabled ? static_cast<std::uint32_t>(std::max(config_.shadow_map_resolution, 256)) : 0U;
        world_diagnostics_.shadow_contact_strength =
            config_.shadow_enabled ? std::max(config_.shadow_contact_strength, 0.0F) : 0.0F;
        world_diagnostics_.shadow_bias_near = config_.shadow_enabled ? std::max(config_.shadow_bias_near, 0.0F) : 0.0F;
        world_diagnostics_.shadow_bias_far = config_.shadow_enabled ? std::max(config_.shadow_bias_far, 0.0F) : 0.0F;
        world_diagnostics_.shadow_normal_offset_near =
            config_.shadow_enabled ? std::max(config_.shadow_normal_offset_near, 0.0F) : 0.0F;
        world_diagnostics_.shadow_normal_offset_far =
            config_.shadow_enabled ? std::max(config_.shadow_normal_offset_far, 0.0F) : 0.0F;
        world_diagnostics_.shadow_pcf_radius_near =
            config_.shadow_enabled ? std::max(config_.shadow_pcf_radius_near, 0.0F) : 0.0F;
        world_diagnostics_.shadow_pcf_radius_far =
            config_.shadow_enabled ? std::max(config_.shadow_pcf_radius_far, 0.0F) : 0.0F;
        world_diagnostics_.shadow_cascade_split_distance =
            config_.shadow_enabled ? std::max(config_.shadow_cascade_split_distance, 0.0F) : 0.0F;
        world_diagnostics_.shadow_cascade_blend_range =
            config_.shadow_enabled ? std::max(config_.shadow_cascade_blend_range, 0.0F) : 0.0F;
        world_diagnostics_.shadow_projection_radius =
            config_.shadow_enabled ? std::max(config_.shadow_projection_radius, 1.0F) : 0.0F;
        world_diagnostics_.shadow_far_projection_radius =
            config_.shadow_enabled ? std::max(config_.shadow_far_projection_radius, 1.0F) : 0.0F;
        world_diagnostics_.shadow_focus_distance =
            config_.shadow_enabled ? std::max(config_.shadow_focus_distance, 0.0F) : 0.0F;
        world_diagnostics_.shadow_far_focus_distance =
            config_.shadow_enabled ? std::max(config_.shadow_far_focus_distance, 0.0F) : 0.0F;
        world_diagnostics_.shadow_texel_world_size =
            config_.shadow_enabled ? (world_diagnostics_.shadow_projection_radius * 2.0F)
                                         / static_cast<float>(std::max(config_.shadow_map_resolution, 256))
                                   : 0.0F;
        world_diagnostics_.shadow_far_texel_world_size =
            config_.shadow_enabled ? (world_diagnostics_.shadow_far_projection_radius * 2.0F)
                                         / static_cast<float>(std::max(config_.shadow_map_resolution, 256))
                                   : 0.0F;

        world_diagnostics_.shadow_preview_texture_id =
            config_.shadow_enabled && developer_ui_available() ? state_->backend->shadow_preview(0) : 0;
        world_diagnostics_.shadow_far_preview_texture_id =
            config_.shadow_enabled && developer_ui_available() ? state_->backend->shadow_preview(1) : 0;
        state_->frame_backend = state_->backend;
        state_->items.clear();
        state_->shadow_casters.clear();
        state_->viewmodel.clear();
    }

    auto Renderer::capture_frame() -> render::CpuRgbaTextureData
    {
        finish_scene();
        return state_->frame_backend->capture_prepared_image();
    }

    auto Renderer::capture_shadow_depth(size_t cascade) -> std::vector<float>
    {
        finish_scene();
        return state_->frame_backend->capture_shadow_depth(cascade);
    }

    auto Renderer::present() -> bool
    {
        finish_scene();
        state_->swapchain->set_vsync(state_->window->vsync_enabled());
        return state_->swapchain->present_prepared_scene(*state_->frame_backend);
    }

    void Renderer::set_local_lights(const std::span<const render::LocalLight> lights)
    {
        local_lights_.assign(lights.begin(), lights.end());
    }

    void Renderer::set_scene_time(const float seconds) noexcept
    {
        scene_time_seconds_ = seconds;
    }

    void Renderer::set_reflection_probe(std::shared_ptr<const render::ReflectionProbe> probe)
    {
        reflection_probe_ = std::move(probe);
    }

    void Renderer::set_irradiance_probes(std::shared_ptr<const render::IrradianceProbeVolume> probes)
    {
        irradiance_probes_ = std::move(probes);
    }

    void Renderer::set_spot_light(const render::SpotLight& light) noexcept
    {
        spot_light_ = light;
    }

    void Renderer::set_field_of_view(const float degrees)
    {
        if (!std::isfinite(degrees) || degrees < 50 || degrees > 100)
            throw std::invalid_argument("Field of view must be between 50 and 100 degrees");
        config_.camera.vertical_fov_radians = glm::radians(degrees);
        camera_.apply_projection_config(config_.camera);
    }

    void Renderer::draw_ui(const FrameInfo& frame, const AudioSystem& audio_system)
    {
        if (begin_ui(frame, audio_system))
        {
            end_ui();
        }
    }

    auto Renderer::begin_ui(const FrameInfo& frame, const AudioSystem& audio_system, const bool exclusive) -> bool
    {
        RELAYCORE_PROFILE_FUNCTION();
        finish_scene();
        developer_frame_active_ = false;
        if (!developer_ui_available())
            return ui_available();

        imgui_overlay_.update_diagnostics(frame);
        const bool show_shadow_debug_banner = config_.shadow_debug_cascades;

        if (!exclusive && !imgui_overlay_.should_render() && !show_shadow_debug_banner)
        {
            return ui_available();
        }
        {
            RELAYCORE_PROFILE_ZONE_N("imgui.begin_frame");
            imgui_overlay_.begin_frame();
            developer_frame_active_ = true;
        }
        if (exclusive)
        {
            return true;
        }
        if (imgui_overlay_.diagnostics_visible())
        {
            RELAYCORE_PROFILE_ZONE_N("imgui.draw_diagnostics");
            imgui_overlay_.draw_diagnostics(
                frame,
                camera_,
                audio_system,
                config_,
                world_diagnostics_,
                runtime_diagnostics_,
                player_diagnostics_,
                interaction_diagnostics_,
                inspector_diagnostics_,
                audio_diagnostics_,
                iteration_diagnostics_
            );
            if (imgui_overlay_.advanced_diagnostics_visible() && ImGui::Begin("Vulkan memory"))
            {
                if (ImGui::Button("Refresh GPU memory"))
                {
                    state_->memory_snapshot = state_->device->memory_usage();
                    state_->resource_snapshot = state_->backend->resource_usage();
                    state_->descriptor_snapshot = state_->backend->material_descriptor_usage();
                    if (state_->frame_backend && state_->frame_backend != state_->backend)
                    {
                        const auto previous = state_->frame_backend->resource_usage();
                        state_->resource_snapshot.staging_bytes += previous.staging_bytes;
                        state_->resource_snapshot.upload_batches += previous.upload_batches;
                        state_->resource_snapshot.upload_retirements += previous.upload_retirements;
                        state_->resource_snapshot.scene_retirements += previous.scene_retirements;
                        state_->resource_snapshot.material_retirements += previous.material_retirements;
                        const auto descriptors = state_->frame_backend->material_descriptor_usage();
                        state_->descriptor_snapshot.pools += descriptors.pools;
                        state_->descriptor_snapshot.materials += descriptors.materials;
                    }
                }
                if (state_->memory_snapshot)
                {
                    ImGui::TextUnformatted(
                        state_->memory_snapshot->driver_budget ? "Driver heap budgets (VK_EXT_memory_budget)"
                                                               : "Estimated heap budgets (VMA fallback)"
                    );
                    constexpr double mib = 1024.0 * 1024.0;
                    for (size_t i = 0; i < state_->memory_snapshot->heaps.size(); ++i)
                    {
                        const auto& heap = state_->memory_snapshot->heaps[i];
                        ImGui::Text(
                            "Heap %zu (%s): usage %.1f / budget %.1f MiB",
                            i,
                            heap.device_local ? "device local" : "host",
                            heap.usage_bytes / mib,
                            heap.budget_bytes / mib
                        );
                        ImGui::Text(
                            "VMA: %u allocations, %.1f MiB; %u blocks, %.1f MiB",
                            heap.allocations,
                            heap.allocation_bytes / mib,
                            heap.blocks,
                            heap.block_bytes / mib
                        );
                    }
                    const auto& resources = state_->resource_snapshot;
                    ImGui::Text(
                        "Retained staging: %.2f MiB, %zu batches",
                        resources.staging_bytes / mib,
                        resources.upload_batches
                    );
                    ImGui::Text(
                        "Pending retirement: upload %zu, scene %zu, materials %zu",
                        resources.upload_retirements,
                        resources.scene_retirements,
                        resources.material_retirements
                    );
                    ImGui::Text(
                        "Materials: %zu; descriptor pools: %zu",
                        state_->descriptor_snapshot.materials,
                        state_->descriptor_snapshot.pools
                    );
                } else
                    ImGui::TextUnformatted("Press Refresh GPU memory to capture a snapshot.");
                ImGui::End();
            } else if (imgui_overlay_.advanced_diagnostics_visible())
                ImGui::End();
        }
        if (show_shadow_debug_banner)
        {
            RELAYCORE_PROFILE_ZONE_N("imgui.draw_shadow_debug_banner");
            draw_shadow_debug_banner();
        }
        return true;
    }

    void Renderer::end_ui()
    {
        finish_scene();
        if (player_ui_ && player_ui_->visible())
            state_->frame_backend->append_player_ui(*player_ui_);
        if (developer_frame_active_)
            state_->frame_backend->append_ui(imgui_overlay_.finish_frame());
    }

    void Renderer::set_world_diagnostics(const render::WorldDiagnostics& diagnostics) noexcept
    {
        world_diagnostics_ = diagnostics;
    }

    auto Renderer::world_diagnostics() const noexcept -> const render::WorldDiagnostics&
    {
        return world_diagnostics_;
    }

    void Renderer::set_runtime_diagnostics(render::RuntimeDiagnostics diagnostics)
    {
        runtime_diagnostics_ = std::move(diagnostics);
    }

    auto Renderer::runtime_diagnostics() const noexcept -> const render::RuntimeDiagnostics&
    {
        return runtime_diagnostics_;
    }

    void Renderer::set_player_diagnostics(render::PlayerDiagnostics diagnostics)
    {
        player_diagnostics_ = std::move(diagnostics);
    }

    auto Renderer::player_diagnostics() const noexcept -> const render::PlayerDiagnostics&
    {
        return player_diagnostics_;
    }

    void Renderer::set_interaction_diagnostics(render::InteractionDiagnostics diagnostics)
    {
        interaction_diagnostics_ = std::move(diagnostics);
    }

    auto Renderer::interaction_diagnostics() const noexcept -> const render::InteractionDiagnostics&
    {
        return interaction_diagnostics_;
    }

    void Renderer::set_inspector_diagnostics(render::InspectorDiagnostics diagnostics)
    {
        inspector_diagnostics_ = std::move(diagnostics);
    }

    auto Renderer::inspector_diagnostics() const noexcept -> const render::InspectorDiagnostics&
    {
        return inspector_diagnostics_;
    }

    void Renderer::set_audio_diagnostics(render::AudioDiagnostics diagnostics)
    {
        audio_diagnostics_ = std::move(diagnostics);
    }

    auto Renderer::audio_diagnostics() const noexcept -> const render::AudioDiagnostics&
    {
        return audio_diagnostics_;
    }

    void Renderer::set_iteration_diagnostics(const render::IterationDiagnostics& diagnostics)
    {
        iteration_diagnostics_ = diagnostics;
    }

    auto Renderer::iteration_diagnostics() const noexcept -> const render::IterationDiagnostics&
    {
        return iteration_diagnostics_;
    }

    auto Renderer::authoring_config() const noexcept -> const render::RenderAuthoringConfig&
    {
        return authoring_config_;
    }

    auto Renderer::color_pipeline_mode() const noexcept -> render::ColorPipelineMode
    {
        return color_pipeline_mode_;
    }

    auto Renderer::camera() noexcept -> render::PerspectiveCamera&
    {
        return camera_;
    }

    auto Renderer::camera() const noexcept -> const render::PerspectiveCamera&
    {
        return camera_;
    }

    auto Renderer::diagnostics_visible() const noexcept -> bool
    {
        return imgui_overlay_.diagnostics_visible();
    }

    auto Renderer::wants_keyboard_capture() const noexcept -> bool
    {
        return (player_ui_ && player_ui_->captures_input()) || imgui_overlay_.wants_keyboard_capture();
    }

    auto Renderer::wants_mouse_capture() const noexcept -> bool
    {
        return (player_ui_ && player_ui_->captures_input()) || imgui_overlay_.wants_mouse_capture();
    }

    auto Renderer::wants_text_input() const noexcept -> bool
    {
        return (player_ui_ && player_ui_->wants_text_input()) || imgui_overlay_.wants_text_input();
    }

    void Renderer::set_diagnostics_visible(const bool visible) noexcept
    {
        imgui_overlay_.set_diagnostics_visible(visible);
    }

    void Renderer::toggle_diagnostics_visible() noexcept
    {
        imgui_overlay_.toggle_diagnostics_visible();
    }

    auto Renderer::advanced_diagnostics_visible() const noexcept -> bool
    {
        return imgui_overlay_.advanced_diagnostics_visible();
    }

    void Renderer::set_advanced_diagnostics_visible(const bool visible) noexcept
    {
        imgui_overlay_.set_advanced_diagnostics_visible(visible);
    }

    void Renderer::toggle_advanced_diagnostics_visible() noexcept
    {
        imgui_overlay_.toggle_advanced_diagnostics_visible();
    }

    auto Renderer::inspector_visualization_enabled() const noexcept -> bool
    {
        return inspector_visualization_enabled_;
    }

    void Renderer::set_inspector_visualization_enabled(const bool enabled) noexcept
    {
        inspector_visualization_enabled_ = enabled;
    }

    void Renderer::toggle_inspector_visualization_enabled() noexcept
    {
        inspector_visualization_enabled_ = !inspector_visualization_enabled_;
    }

    auto Renderer::consume_iteration_command_requests() noexcept -> IterationCommandRequests
    {
        const debug::ImGuiOverlay::IterationCommandRequests overlay_requests =
            imgui_overlay_.consume_iteration_command_requests();
        IterationCommandRequests requests{};
        requests.save_runtime_snapshot = overlay_requests.save_runtime_snapshot;
        requests.restore_runtime_snapshot = overlay_requests.restore_runtime_snapshot;
        requests.open_first_authoring_warning = overlay_requests.open_first_authoring_warning;
        requests.dump_authoring_warnings = overlay_requests.dump_authoring_warnings;
        requests.open_first_runtime_registry = overlay_requests.open_first_runtime_registry;
        requests.dump_runtime_registry = overlay_requests.dump_runtime_registry;
        requests.open_first_runtime_binding = overlay_requests.open_first_runtime_binding;
        requests.dump_runtime_bindings = overlay_requests.dump_runtime_bindings;
        requests.dump_render_authoring_contract = overlay_requests.dump_render_authoring_contract;
        requests.authoring_warning_source_filter = overlay_requests.authoring_warning_source_filter;
        requests.runtime_registry_source_filter = overlay_requests.runtime_registry_source_filter;
        requests.runtime_binding_source_filter = overlay_requests.runtime_binding_source_filter;
        return requests;
    }
} // namespace RelayCore

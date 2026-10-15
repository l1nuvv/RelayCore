/**
 * @file src/relaycore/debug/imgui_overlay.cpp
 * @brief Dear ImGui overlay implementation.
 */

#include "relaycore/debug/imgui_overlay.h"

#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/debug/imgui_overlay_gameplay_panel.h"
#include "relaycore/debug/imgui_overlay_inspector_panel.h"
#include "relaycore/debug/imgui_overlay_iteration_panel.h"
#include "relaycore/debug/imgui_overlay_summary_panel.h"
#include "relaycore/debug/imgui_overlay_system_panel.h"
#include "relaycore/debug/imgui_overlay_widgets.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/vulkan_device.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <span>
#include <stdexcept>

namespace RelayCore::debug
{
    ImGuiOverlay::ImGuiOverlay(const ImGuiOverlayConfig& config) : config_(config) {}

    ImGuiOverlay::~ImGuiOverlay() noexcept
    {
        shutdown();
    }

    void ImGuiOverlay::initialize(const Window& window, const render::VulkanDevice& device)
    {
        if (initialized_ || !config_.enabled)
            return;
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        configure_imgui_font(io);
        ImGui::StyleColorsDark();
        if (!ImGui_ImplSDL3_InitForVulkan(window.handle()))
        {
            ImGui::DestroyContext();
            log::warn("ImGui overlay disabled: SDL3 Vulkan backend initialization failed");
            return;
        }
        try
        {
            constexpr VkFormat color = VK_FORMAT_R8G8B8A8_UNORM;
            ImGui_ImplVulkan_InitInfo info{};
            info.ApiVersion = render::kVulkanApiVersion;
            info.Instance = device.instance();
            info.PhysicalDevice = device.physical_device();
            info.Device = device.handle();
            info.PipelineCache = device.pipeline_cache();
            info.QueueFamily = device.capabilities().graphics_queue_family;
            info.Queue = device.graphics_queue();
            info.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE;
            // The mesh renderer completes each submission before reuse; the UI buffer ring is independent of presentation.
            info.MinImageCount = 2;
            info.ImageCount = 2;
            info.UseDynamicRendering = true;
            info.PipelineInfoMain.PipelineRenderingCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                .colorAttachmentCount = 1,
                .pColorAttachmentFormats = &color
            };
            info.CheckVkResultFn = [](const VkResult result) {
                if (result != VK_SUCCESS)
                    throw std::runtime_error("ImGui Vulkan backend: VkResult " + std::to_string(result));
            };
            if (!ImGui_ImplVulkan_Init(&info))
                throw std::runtime_error("ImGui Vulkan backend initialization failed");
        } catch (...)
        {
            if (io.BackendRendererUserData)
                ImGui_ImplVulkan_Shutdown();
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
            throw;
        }
        window_ = &window;
        initialized_ = true;
        log::info("ImGui Vulkan overlay initialized");
    }

    void ImGuiOverlay::shutdown() noexcept
    {
        if (!initialized_)
        {
            return;
        }

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        window_ = nullptr;
        frame_time_sample_count_ = 0;
        frame_time_sample_cursor_ = 0;
        frame_time_samples_.fill(0.0F);
        initialized_ = false;
    }

    void ImGuiOverlay::apply_config(const ImGuiOverlayConfig& config) noexcept
    {
        config_ = config;
        if (!config_.enabled)
        {
            diagnostics_visible_ = false;
            advanced_diagnostics_visible_ = false;
        }
    }

    void ImGuiOverlay::process_event(const SDL_Event& event)
    {
        if (!initialized_)
        {
            return;
        }
        ImGui_ImplSDL3_ProcessEvent(&event);
    }

    void ImGuiOverlay::begin_frame()
    {
        if (!initialized_)
        {
            return;
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiOverlay::push_frame_time_sample(const float frame_time_ms) noexcept
    {
        frame_time_samples_[frame_time_sample_cursor_] = frame_time_ms;
        frame_time_sample_cursor_ = (frame_time_sample_cursor_ + 1) % frame_time_samples_.size();

        if (frame_time_sample_count_ < frame_time_samples_.size())
        {
            ++frame_time_sample_count_;
        }
    }

    auto ImGuiOverlay::average_frame_time_ms() const noexcept -> float
    {
        if (frame_time_sample_count_ == 0)
        {
            return 0.0F;
        }

        float sum = 0.0F;
        for (std::size_t i = 0; i < frame_time_sample_count_; ++i)
        {
            sum += frame_time_samples_[i];
        }

        return sum / static_cast<float>(frame_time_sample_count_);
    }

    void ImGuiOverlay::draw_diagnostics(
        const FrameInfo& frame,
        const render::PerspectiveCamera& camera,
        const AudioSystem& audio_system,
        render::RendererConfig& renderer_config,
        const render::WorldDiagnostics& world,
        const render::RuntimeDiagnostics& runtime,
        const render::PlayerDiagnostics& player,
        const render::InteractionDiagnostics& interaction,
        const render::InspectorDiagnostics& inspector,
        const render::AudioDiagnostics& audio,
        const render::IterationDiagnostics& iteration
    )
    {
        RELAYCORE_PROFILE_ZONE_N("ImGuiOverlay::draw_diagnostics");
        (void) frame;

        if (!initialized_ || !diagnostics_visible_)
        {
            return;
        }

        const float refresh_rate_hz = window_ != nullptr ? window_->refresh_rate_hz() : 0.0F;
        const float target_frame_ms = refresh_rate_hz > 0.0F ? 1000.0F / refresh_rate_hz : 16.667F;

        std::array<float, kFrameTimeSampleCount> ordered_frame_times{};
        float frame_plot_max = std::max(target_frame_ms * 2.0F, 20.0F);
        if (frame_time_sample_count_ > 0)
        {
            const std::size_t start =
                frame_time_sample_count_ == frame_time_samples_.size() ? frame_time_sample_cursor_ : 0;
            for (std::size_t i = 0; i < frame_time_sample_count_; ++i)
            {
                const float sample = frame_time_samples_[(start + i) % frame_time_samples_.size()];
                ordered_frame_times[i] = sample;
                frame_plot_max = std::max(frame_plot_max, sample);
            }
        }

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 12.0F, viewport->WorkPos.y + 12.0F), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(540.0F, 560.0F), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.92F);
        constexpr ImGuiWindowFlags kWindowFlags = ImGuiWindowFlags_NoSavedSettings;

        ImGui::Begin("RelayCore Diagnostics", nullptr, kWindowFlags);
        (void) audio_system;

        const ImGuiOverlaySummaryFrameStats summary_frame_stats{
            .frame_time_ms = frame_time_ms_,
            .avg_frame_time_ms = avg_frame_time_ms_,
            .fps = fps_,
            .target_frame_ms = target_frame_ms,
            .refresh_rate_hz = refresh_rate_hz,
            .frame_plot_max = frame_plot_max,
            .vsync_label = window_ != nullptr && window_->vsync_enabled() ? "on" : "off",
            .ordered_frame_times = std::span<const float>{ordered_frame_times.data(), frame_time_sample_count_},
        };
        draw_summary_diagnostics_section(summary_frame_stats, runtime, player, interaction, audio, iteration);

        if (advanced_diagnostics_visible_)
        {
            draw_world_diagnostics_section(world);
            draw_runtime_diagnostics_section(runtime);
        }

        if (advanced_diagnostics_visible_)
        {
            ImGuiOverlayIterationPanelState iteration_panel_state{
                .save_runtime_snapshot_requested = save_runtime_snapshot_requested_,
                .restore_runtime_snapshot_requested = restore_runtime_snapshot_requested_,
                .open_first_authoring_warning_requested = open_first_authoring_warning_requested_,
                .dump_authoring_warnings_requested = dump_authoring_warnings_requested_,
                .open_first_runtime_registry_requested = open_first_runtime_registry_requested_,
                .dump_runtime_registry_requested = dump_runtime_registry_requested_,
                .open_first_runtime_binding_requested = open_first_runtime_binding_requested_,
                .dump_runtime_bindings_requested = dump_runtime_bindings_requested_,
                .dump_render_authoring_contract_requested = dump_render_authoring_contract_requested_,
                .authoring_warning_source_filter = authoring_warning_source_filter_,
                .runtime_registry_source_filter = runtime_registry_source_filter_,
                .runtime_binding_source_filter = runtime_binding_source_filter_,
            };
            draw_iteration_diagnostics_section(iteration, iteration_panel_state);
        }

        draw_player_diagnostics_section(player, camera);
        draw_interaction_diagnostics_section(interaction);

        if (advanced_diagnostics_visible_)
        {
            ImGuiOverlayInspectorPanelState inspector_panel_state{
                .inspector_source_filter = inspector_source_filter_,
            };
            draw_inspector_diagnostics_section(inspector, inspector_panel_state);
        }

        if (advanced_diagnostics_visible_)
        {
            draw_audio_diagnostics_section(audio);
            draw_render_tuning_section(world, renderer_config);
        }
        ImGui::End();

        if (config_.show_demo_window)
        {
            ImGui::ShowDemoWindow(&config_.show_demo_window);
        }
    }

    auto ImGuiOverlay::finish_frame() const -> ImDrawData*
    {
        if (!initialized_)
            return nullptr;
        RELAYCORE_PROFILE_ZONE_N("ImGui::Render");
        ImGui::Render();
        return ImGui::GetDrawData();
    }

    auto ImGuiOverlay::is_initialized() const noexcept -> bool
    {
        return initialized_;
    }

    auto ImGuiOverlay::enabled() const noexcept -> bool
    {
        return config_.enabled;
    }

    auto ImGuiOverlay::wants_keyboard_capture() const noexcept -> bool
    {
        return config_.enabled && should_render() && ImGui::GetIO().WantCaptureKeyboard;
    }

    auto ImGuiOverlay::wants_mouse_capture() const noexcept -> bool
    {
        return config_.enabled && should_render() && ImGui::GetIO().WantCaptureMouse;
    }

    auto ImGuiOverlay::wants_text_input() const noexcept -> bool
    {
        return config_.enabled && should_render() && ImGui::GetIO().WantTextInput;
    }

    auto ImGuiOverlay::diagnostics_visible() const noexcept -> bool
    {
        return diagnostics_visible_;
    }

    void ImGuiOverlay::set_diagnostics_visible(bool visible) noexcept
    {
        diagnostics_visible_ = visible;
        if (!diagnostics_visible_)
        {
            advanced_diagnostics_visible_ = false;
        }
    }

    void ImGuiOverlay::toggle_diagnostics_visible() noexcept
    {
        diagnostics_visible_ = !diagnostics_visible_;
        if (!diagnostics_visible_)
        {
            advanced_diagnostics_visible_ = false;
        }
    }

    auto ImGuiOverlay::advanced_diagnostics_visible() const noexcept -> bool
    {
        return advanced_diagnostics_visible_;
    }

    void ImGuiOverlay::set_advanced_diagnostics_visible(const bool visible) noexcept
    {
        advanced_diagnostics_visible_ = visible;
        if (advanced_diagnostics_visible_)
        {
            diagnostics_visible_ = true;
        }
    }

    void ImGuiOverlay::toggle_advanced_diagnostics_visible() noexcept
    {
        advanced_diagnostics_visible_ = !advanced_diagnostics_visible_;
        if (advanced_diagnostics_visible_)
        {
            diagnostics_visible_ = true;
        }
    }

    auto ImGuiOverlay::should_render() const noexcept -> bool
    {
        return initialized_ && (diagnostics_visible_ || advanced_diagnostics_visible_ || config_.show_demo_window);
    }

    auto ImGuiOverlay::consume_iteration_command_requests() noexcept -> IterationCommandRequests
    {
        IterationCommandRequests requests{};
        requests.save_runtime_snapshot = save_runtime_snapshot_requested_;
        requests.restore_runtime_snapshot = restore_runtime_snapshot_requested_;
        requests.open_first_authoring_warning = open_first_authoring_warning_requested_;
        requests.dump_authoring_warnings = dump_authoring_warnings_requested_;
        requests.open_first_runtime_registry = open_first_runtime_registry_requested_;
        requests.dump_runtime_registry = dump_runtime_registry_requested_;
        requests.open_first_runtime_binding = open_first_runtime_binding_requested_;
        requests.dump_runtime_bindings = dump_runtime_bindings_requested_;
        requests.dump_render_authoring_contract = dump_render_authoring_contract_requested_;
        requests.authoring_warning_source_filter = authoring_warning_source_filter_;
        requests.runtime_registry_source_filter = runtime_registry_source_filter_;
        requests.runtime_binding_source_filter = runtime_binding_source_filter_;

        save_runtime_snapshot_requested_ = false;
        restore_runtime_snapshot_requested_ = false;
        open_first_authoring_warning_requested_ = false;
        dump_authoring_warnings_requested_ = false;
        open_first_runtime_registry_requested_ = false;
        dump_runtime_registry_requested_ = false;
        open_first_runtime_binding_requested_ = false;
        dump_runtime_bindings_requested_ = false;
        dump_render_authoring_contract_requested_ = false;
        return requests;
    }

    void ImGuiOverlay::update_diagnostics(const FrameInfo& frame) noexcept
    {
        frame_time_ms_ = static_cast<float>(frame.wall_delta_seconds) * 1000.0F;
        push_frame_time_sample(frame_time_ms_);

        avg_frame_time_ms_ = average_frame_time_ms();
        fps_ = frame.wall_delta_seconds > 0.0 ? static_cast<float>(1.0 / frame.wall_delta_seconds) : 0.0F;
        avg_fps_ = avg_frame_time_ms_ > 0.0F ? (1000.0F / avg_frame_time_ms_) : 0.0F;
    }
} // namespace RelayCore::debug

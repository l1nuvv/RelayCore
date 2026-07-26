/**
 * @file src/relaycore/debug/imgui_overlay.h
 * @brief Dear ImGui debug overlay wrapper.
 */

#pragma once

#include "relaycore/app/frame_timer.h"
#include "relaycore/render/render_diagnostics.h"

#include <SDL3/SDL.h>
#include <array>
#include <cstddef>
#include <cstdint>

namespace RelayCore
{
    class Window;
    class AudioSystem;
} // namespace RelayCore

namespace RelayCore::render
{
    class PerspectiveCamera;
    struct RendererConfig;
} // namespace RelayCore::render

namespace RelayCore::debug
{
    struct ImGuiOverlayConfig {
        bool enabled = true;
        bool show_demo_window = false;
        bool log_fps_to_console = true;
        float fps_log_interval_seconds = 1.0F;
    };

    class ImGuiOverlay {
    public:
        struct IterationCommandRequests {
            bool save_runtime_snapshot = false;
            bool restore_runtime_snapshot = false;
            bool open_first_authoring_warning = false;
            bool dump_authoring_warnings = false;
            bool open_first_runtime_registry = false;
            bool dump_runtime_registry = false;
            bool open_first_runtime_binding = false;
            bool dump_runtime_bindings = false;
            bool dump_render_authoring_contract = false;
            render::DiagnosticsSourceFilter authoring_warning_source_filter = render::DiagnosticsSourceFilter::All;
            render::DiagnosticsSourceFilter runtime_registry_source_filter = render::DiagnosticsSourceFilter::All;
            render::DiagnosticsSourceFilter runtime_binding_source_filter = render::DiagnosticsSourceFilter::All;
        };

        explicit ImGuiOverlay(const ImGuiOverlayConfig& config = ImGuiOverlayConfig{});

        ~ImGuiOverlay() noexcept;

        ImGuiOverlay(const ImGuiOverlay&) = delete;

        auto operator=(const ImGuiOverlay&) -> ImGuiOverlay& = delete;

        void initialize(const Window& window);

        void shutdown() noexcept;

        void apply_config(const ImGuiOverlayConfig& config) noexcept;

        void process_event(const SDL_Event& event);

        void begin_frame();

        void draw_diagnostics(
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
        );

        void render() const;

        [[nodiscard]] auto is_initialized() const noexcept -> bool;

        [[nodiscard]] auto enabled() const noexcept -> bool;

        [[nodiscard]] auto wants_keyboard_capture() const noexcept -> bool;

        [[nodiscard]] auto wants_mouse_capture() const noexcept -> bool;

        [[nodiscard]] auto wants_text_input() const noexcept -> bool;

        [[nodiscard]] auto diagnostics_visible() const noexcept -> bool;

        void set_diagnostics_visible(bool visible) noexcept;

        void toggle_diagnostics_visible() noexcept;

        [[nodiscard]] auto advanced_diagnostics_visible() const noexcept -> bool;

        void set_advanced_diagnostics_visible(bool visible) noexcept;

        void toggle_advanced_diagnostics_visible() noexcept;

        void update_diagnostics(const FrameInfo& frame) noexcept;

        [[nodiscard]] auto should_render() const noexcept -> bool;

        [[nodiscard]] auto consume_iteration_command_requests() noexcept -> IterationCommandRequests;

    private:
        static constexpr std::size_t kFrameTimeSampleCount = 120;

        void push_frame_time_sample(float frame_time_ms) noexcept;

        [[nodiscard]] auto average_frame_time_ms() const noexcept -> float;

        [[nodiscard]] static auto vsync_label(int swap_interval) -> const char*;

        ImGuiOverlayConfig config_{};
        const Window* window_ = nullptr;
        bool initialized_ = false;
        float fps_log_accumulator_ = 0.0F;
        std::array<float, kFrameTimeSampleCount> frame_time_samples_{};
        std::size_t frame_time_sample_count_ = 0;
        std::size_t frame_time_sample_cursor_ = 0;
        bool diagnostics_visible_ = true;
        bool advanced_diagnostics_visible_ = false;
        render::DiagnosticsSourceFilter inspector_source_filter_ = render::DiagnosticsSourceFilter::All;
        render::DiagnosticsSourceFilter authoring_warning_source_filter_ = render::DiagnosticsSourceFilter::All;
        render::DiagnosticsSourceFilter runtime_registry_source_filter_ = render::DiagnosticsSourceFilter::All;
        render::DiagnosticsSourceFilter runtime_binding_source_filter_ = render::DiagnosticsSourceFilter::All;
        float frame_time_ms_ = 0.0F;
        float avg_frame_time_ms_ = 0.0F;
        float fps_ = 0.0F;
        float avg_fps_ = 0.0F;
        bool save_runtime_snapshot_requested_ = false;
        bool restore_runtime_snapshot_requested_ = false;
        bool open_first_authoring_warning_requested_ = false;
        bool dump_authoring_warnings_requested_ = false;
        bool open_first_runtime_registry_requested_ = false;
        bool dump_runtime_registry_requested_ = false;
        bool open_first_runtime_binding_requested_ = false;
        bool dump_runtime_bindings_requested_ = false;
        bool dump_render_authoring_contract_requested_ = false;
    };
} // namespace RelayCore::debug

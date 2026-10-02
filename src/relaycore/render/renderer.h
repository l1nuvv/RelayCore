#pragma once

#include "relaycore/app/frame_timer.h"
#include "relaycore/core/engine_config.h"
#include "relaycore/debug/imgui_overlay.h"
#include "relaycore/render/camera.h"
#include "relaycore/render/color_pipeline_mode.h"
#include "relaycore/render/game_hud.h"
#include "relaycore/render/mesh.h"
#include "relaycore/render/render_authoring_config.h"
#include "relaycore/render/render_diagnostics.h"
#include "relaycore/render/render_queue.h"
#include "relaycore/render/static_model.h"
#include "relaycore/render/texture2d.h"

#include <SDL3/SDL.h>
#include <array>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <vector>

namespace RelayCore
{
    class Window;
    class AudioSystem;
} // namespace RelayCore

namespace RelayCore::render
{
    class VulkanMeshRenderer;
    struct IrradianceProbeVolume;
} // namespace RelayCore::render

namespace RelayCore
{
    class Renderer {
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

        struct PreparedLiveContentReload {
            PreparedLiveContentReload() = default;
            PreparedLiveContentReload(const PreparedLiveContentReload&) = delete;
            auto operator=(const PreparedLiveContentReload&) -> PreparedLiveContentReload& = delete;
            PreparedLiveContentReload(PreparedLiveContentReload&&) noexcept = default;
            auto operator=(PreparedLiveContentReload&&) noexcept -> PreparedLiveContentReload& = default;

            render::RendererConfig config{};
            render::RenderAuthoringConfig authoring_config{};
            render::ColorPipelineMode color_pipeline_mode = render::ColorPipelineMode::legacy_gamma;
            std::shared_ptr<render::VulkanMeshRenderer> backend{};
            std::string atlas_source_label{};
        };

        explicit Renderer(
            const render::RendererConfig& config = render::RendererConfig{},
            render::ColorPipelineMode color_pipeline_mode = render::ColorPipelineMode::legacy_gamma
        );

        ~Renderer() noexcept;

        void initialize(const Window& window);

        [[nodiscard]] auto prepare_live_content_reload(
            const render::RendererConfig& config,
            const render::RenderAuthoringConfig& authoring_config
        ) -> PreparedLiveContentReload;

        [[nodiscard]] auto prepare_live_content_reload(
            const render::RendererConfig& config,
            const render::RenderAuthoringConfig& authoring_config,
            render::ColorPipelineMode color_pipeline_mode
        ) -> PreparedLiveContentReload;

        [[nodiscard]] auto config() const noexcept -> const render::RendererConfig&
        {
            return config_;
        }

        void commit_live_content_reload(PreparedLiveContentReload staged_reload);

        void process_event(const SDL_Event& event);

        void on_resize(int width, int height) noexcept;

        void begin_frame();
        void finish_scene();
        [[nodiscard]] auto capture_frame() -> render::CpuRgbaTextureData;
        [[nodiscard]] auto capture_shadow_depth(size_t cascade) -> std::vector<float>;
        [[nodiscard]] auto present() -> bool;

        void draw_render_queue(
            std::span<const render::RenderItem> items,
            std::span<const render::RenderItem> shadow_casters
        );
        void draw_viewmodel(std::span<const render::RenderItem> items);

        [[nodiscard]] auto static_models() noexcept -> render::StaticModelCache&
        {
            return static_models_;
        }

        void set_local_lights(std::span<const render::LocalLight> lights);
        void set_irradiance_probes(std::shared_ptr<const render::IrradianceProbeVolume> probes);

        void set_scene_time(float seconds) noexcept;
        void set_spot_light(const render::SpotLight& light) noexcept;
        void set_field_of_view(float degrees);

        void draw_ui(const FrameInfo& frame, const AudioSystem& audio_system);

        [[nodiscard]] auto begin_ui(const FrameInfo& frame, const AudioSystem& audio_system, bool exclusive = false)
            -> bool;

        void end_ui();

        void set_world_diagnostics(const render::WorldDiagnostics& diagnostics) noexcept;

        [[nodiscard]] auto world_diagnostics() const noexcept -> const render::WorldDiagnostics&;

        void set_runtime_diagnostics(render::RuntimeDiagnostics diagnostics);

        [[nodiscard]] auto runtime_diagnostics() const noexcept -> const render::RuntimeDiagnostics&;

        void set_player_diagnostics(render::PlayerDiagnostics diagnostics);

        [[nodiscard]] auto player_diagnostics() const noexcept -> const render::PlayerDiagnostics&;

        void set_interaction_diagnostics(render::InteractionDiagnostics diagnostics);

        [[nodiscard]] auto interaction_diagnostics() const noexcept -> const render::InteractionDiagnostics&;

        void set_inspector_diagnostics(render::InspectorDiagnostics diagnostics);

        [[nodiscard]] auto inspector_diagnostics() const noexcept -> const render::InspectorDiagnostics&;

        void set_audio_diagnostics(render::AudioDiagnostics diagnostics);

        [[nodiscard]] auto audio_diagnostics() const noexcept -> const render::AudioDiagnostics&;

        void set_iteration_diagnostics(const render::IterationDiagnostics& diagnostics);

        [[nodiscard]] auto iteration_diagnostics() const noexcept -> const render::IterationDiagnostics&;

        [[nodiscard]] auto authoring_config() const noexcept -> const render::RenderAuthoringConfig&;

        [[nodiscard]] auto color_pipeline_mode() const noexcept -> render::ColorPipelineMode;

        [[nodiscard]] auto camera() noexcept -> render::PerspectiveCamera&;

        [[nodiscard]] auto camera() const noexcept -> const render::PerspectiveCamera&;

        [[nodiscard]] auto diagnostics_visible() const noexcept -> bool;
        [[nodiscard]] auto ui_available() const noexcept -> bool
        {
            return imgui_overlay_.enabled() && imgui_overlay_.is_initialized();
        }

        [[nodiscard]] auto wants_keyboard_capture() const noexcept -> bool;

        [[nodiscard]] auto wants_mouse_capture() const noexcept -> bool;

        [[nodiscard]] auto wants_text_input() const noexcept -> bool;

        void set_diagnostics_visible(bool visible) noexcept;

        void toggle_diagnostics_visible() noexcept;

        [[nodiscard]] auto advanced_diagnostics_visible() const noexcept -> bool;

        void set_advanced_diagnostics_visible(bool visible) noexcept;

        void toggle_advanced_diagnostics_visible() noexcept;

        [[nodiscard]] auto inspector_visualization_enabled() const noexcept -> bool;

        void set_inspector_visualization_enabled(bool enabled) noexcept;

        void toggle_inspector_visualization_enabled() noexcept;

        [[nodiscard]] auto consume_iteration_command_requests() noexcept -> IterationCommandRequests;

    private:
        render::RendererConfig config_{};
        render::ColorPipelineMode color_pipeline_mode_ = render::ColorPipelineMode::legacy_gamma;
        render::RenderAuthoringConfig authoring_config_{};
        bool initialized_ = false;
        int viewport_width_ = 1280;
        int viewport_height_ = 720;
        render::PerspectiveCamera camera_{};
        debug::ImGuiOverlay imgui_overlay_{};
        render::GameHud game_hud_{};
        render::WorldDiagnostics world_diagnostics_{};
        render::RuntimeDiagnostics runtime_diagnostics_{};
        render::PlayerDiagnostics player_diagnostics_{};
        render::InteractionDiagnostics interaction_diagnostics_{};
        render::InspectorDiagnostics inspector_diagnostics_{};
        render::AudioDiagnostics audio_diagnostics_{};
        render::IterationDiagnostics iteration_diagnostics_{};
        std::vector<render::LocalLight> local_lights_{};
        std::shared_ptr<const render::IrradianceProbeVolume> irradiance_probes_;
        render::StaticModelCache static_models_{};
        render::SpotLight spot_light_{};
        float scene_time_seconds_ = 0.0F;
        bool inspector_visualization_enabled_ = false;
        struct State;
        std::unique_ptr<State> state_;
    };
} // namespace RelayCore

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
#include "relaycore/render/shader_program.h"
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
            render::RendererConfig config{};
            render::RenderAuthoringConfig authoring_config{};
            render::ShaderProgram opaque_shader_program{};
            render::ShaderProgram overlay_shader_program{};
            render::ShaderProgram transparent_shader_program{};
            render::ShaderProgram shadow_shader_program{};
            render::ShaderProgram point_shadow_shader_program{};
            render::Texture2D blockout_atlas{};
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

        void commit_live_content_reload(PreparedLiveContentReload staged_reload);

        void process_event(const SDL_Event& event);

        void on_resize(int width, int height) noexcept;

        void begin_frame();

        void draw_render_queue(
            std::span<const render::RenderItem> items,
            std::span<const render::RenderItem> shadow_casters
        );

        [[nodiscard]] auto static_models() noexcept -> render::StaticModelCache&
        {
            return static_models_;
        }

        void set_local_lights(std::span<const render::LocalLight> lights);

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
        static constexpr std::size_t kShadowCascadeCount = 2;
        static constexpr std::size_t kShadowMapCount = 3;
        static constexpr std::size_t kSpotShadowIndex = 2;

        struct ShadowResources {
            ~ShadowResources() noexcept;

            ShadowResources() = default;
            ShadowResources(const ShadowResources&) = delete;
            auto operator=(const ShadowResources&) -> ShadowResources& = delete;
            ShadowResources(ShadowResources&& other) noexcept;
            auto operator=(ShadowResources&& other) noexcept -> ShadowResources&;

            void reset() noexcept;

            [[nodiscard]] auto complete(bool directional) const noexcept -> bool;

            std::array<unsigned int, kShadowMapCount> framebuffer_ids{};
            std::array<unsigned int, kShadowMapCount> depth_texture_ids{};
            unsigned int point_framebuffer_id = 0;
            unsigned int point_depth_texture_id = 0;
        };

        [[nodiscard]] static auto create_shadow_resources(const render::RendererConfig& config) -> ShadowResources;

        void initialize_shadow_resources();

        void release_shadow_resources() noexcept;

        void render_shadow_map(std::span<const render::RenderItem> items);

        [[nodiscard]] auto build_shadow_light_view_projection(float focus_distance, float radius, float depth) const
            -> glm::mat4;

        [[nodiscard]] auto scene_shader_program(render::RenderPass pass) noexcept -> render::ShaderProgram&;

        [[nodiscard]] auto scene_shader_program(render::RenderPass pass) const noexcept -> const render::ShaderProgram&;

        void bind_scene_program_uniforms(render::ShaderProgram& program) const;

        void apply_scene_framebuffer_state() const noexcept;

        render::RendererConfig config_{};
        render::ColorPipelineMode color_pipeline_mode_ = render::ColorPipelineMode::legacy_gamma;
        render::RenderAuthoringConfig authoring_config_{};
        bool initialized_ = false;
        int viewport_width_ = 1280;
        int viewport_height_ = 720;
        render::PerspectiveCamera camera_{};
        render::ShaderProgram opaque_shader_program_{};
        render::ShaderProgram overlay_shader_program_{};
        render::ShaderProgram transparent_shader_program_{};
        render::ShaderProgram shadow_program_{};
        render::ShaderProgram point_shadow_program_{};
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
        std::vector<glm::vec4> shadow_caster_spheres_{};
        int local_shadow_mask_ = 0;
        render::Texture2D blockout_atlas_{};
        render::StaticModelCache static_models_{};
        ShadowResources shadow_resources_{};
        std::array<glm::mat4, kShadowCascadeCount> shadow_light_view_projections_{};
        render::SpotLight spot_light_{};
        glm::mat4 spot_view_projection_{1.0F};
        float scene_time_seconds_ = 0.0F;
        bool inspector_visualization_enabled_ = false;
    };
} // namespace RelayCore

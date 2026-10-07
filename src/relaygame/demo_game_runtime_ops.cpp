#include "relaygame/demo_game_runtime_ops.h"

#include "relaycore/audio/audio_system.h"
#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/renderer.h"
#include "relaygame/level_session_builder.h"

#include <exception>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace RelayGame::detail
{
    namespace
    {
        struct PreparedLevelReload {
            RelayCore::core::EngineConfig config{};
            RelayCore::Renderer::PreparedLiveContentReload renderer{};
            PreparedLevelSession session{};
            std::string atlas_source_label{};
        };

        void log_level_loaded(
            RelayCore::EngineServices& services,
            const RelayGame::GameWorldRuntime& runtime,
            const RelayGame::WorldSceneRenderer& world_scene
        )
        {
            RelayCore::log::info(
                "DemoGame loaded level {}: {} solids, {} entities, {} render chunks, {} triangles, {} doors, {} "
                "triggers, {} usables",
                services.renderer().runtime_diagnostics().level_name.empty()
                    ? std::string{"<unknown>"}
                    : services.renderer().runtime_diagnostics().level_name,
                runtime.level().solids.size(),
                runtime.level().entities.size(),
                world_scene.diagnostics().visible_solids,
                world_scene.diagnostics().triangles,
                runtime.doors().size(),
                runtime.triggers().size(),
                runtime.usables().size()
            );
        }

        [[nodiscard]] auto build_reload_success_summary(
            const RelayCore::render::RuntimeDiagnostics& runtime,
            const RelayGame::WorldSceneRenderer& world_scene,
            const std::string& atlas_source_label
        ) -> std::string
        {
            return "Reload ok: " + (runtime.level_name.empty() ? std::string{"<unknown>"} : runtime.level_name)
                   + " | shaders+atlas+engine/render_authoring+level" + " | atlas "
                   + (atlas_source_label.empty() ? std::string{"<unknown>"} : atlas_source_label) + " | "
                   + std::to_string(runtime.level_entity_count) + " entities" + " | "
                   + std::to_string(world_scene.diagnostics().triangles) + " triangles";
        }

        [[nodiscard]] auto prepare_level_reload(
            RelayCore::EngineServices& services,
            const std::string& current_level_path,
            const std::optional<RelayGame::GameWorldRuntime::InspectorRestoreState>& pinned_inspector
        ) -> PreparedLevelReload
        {
            if (current_level_path.empty())
                throw std::runtime_error("Cannot reload without an active level");
            RelayCore::core::EngineConfig reloaded_config =
                RelayCore::core::load_engine_config(services.config().source_path);
            reloaded_config.renderer.hdr_enabled = services.renderer().config().hdr_enabled;
            const RelayCore::render::RenderAuthoringConfig reloaded_authoring_config =
                RelayCore::render::load_default_render_authoring_config();

            PreparedLevelReload prepared{
                .config = reloaded_config,
                .renderer = services.renderer()
                                .prepare_live_content_reload(reloaded_config.renderer, reloaded_authoring_config),
            };

            prepared.session = prepare_level_session(
                prepared.config,
                LevelSessionBuilder{prepared.config.gameplay}.build(std::filesystem::path{current_level_path}),
                prepared.renderer.authoring_config,
                services.renderer().color_pipeline_mode(),
                services.renderer().static_models()
            );
            if (pinned_inspector.has_value())
            {
                prepared.session.runtime.restore_pinned_inspector(*pinned_inspector);
            }

            prepared.atlas_source_label = prepared.renderer.atlas_source_label;
            return prepared;
        }

        void publish_reload_result(
            RelayCore::render::IterationDiagnostics& iteration_diagnostics,
            const bool succeeded,
            std::string summary,
            std::string error = {}
        )
        {
            iteration_diagnostics.has_last_reload_result = true;
            iteration_diagnostics.last_reload_succeeded = succeeded;
            iteration_diagnostics.last_reload_summary = std::move(summary);
            iteration_diagnostics.last_reload_error = std::move(error);
        }
    } // namespace

    auto prepare_level_session(
        const RelayCore::core::EngineConfig& config,
        LevelSession level,
        const RelayCore::render::RenderAuthoringConfig& authoring,
        RelayCore::render::ColorPipelineMode pipeline,
        RelayCore::render::StaticModelCache& models,
        const SavedGame* save
    ) -> PreparedLevelSession
    {
        PreparedLevelSession prepared;
        prepared.runtime.load_level_session(config, std::move(level));
        if (save)
            prepared.runtime.restore_saved_game(*save);
        prepared.world_scene.initialize(
            build_prepared_world_scene_data(prepared.runtime.level(), authoring, pipeline),
            models
        );
        return prepared;
    }

    void replace_level_session(
        DemoGameRuntimeState state,
        PreparedLevelSession&& prepared,
        RelayCore::EngineServices& services
    )
    {
        static_assert(std::is_nothrow_move_assignable_v<GameWorldRuntime>);
        static_assert(std::is_nothrow_move_assignable_v<WorldSceneRenderer>);
        static_assert(std::is_nothrow_move_assignable_v<RelayCore::render::IterationDiagnostics>);
        state.runtime.retire_level_session(services);
        state.world_scene.shutdown();
        state.runtime = std::move(prepared.runtime);
        state.world_scene = std::move(prepared.world_scene);
        state.saved_runtime_snapshot.reset();
        state.iteration_diagnostics = {};
        state.runtime.activate_level_session(services);
        services.renderer().set_world_diagnostics(state.world_scene.diagnostics());
        try
        {
            state.runtime.publish_diagnostics(services.renderer());
            publish_iteration_diagnostics(state, services);
        } catch (const std::exception& error)
        {
            RelayCore::log::error("Level loaded, but diagnostics publication failed: {}", error.what());
        }
    }

    void start_level_session(DemoGameRuntimeState state, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::start_level_session");
        auto prepared = prepare_level_session(
            services.config(),
            LevelSessionBuilder{services.config().gameplay}.build(),
            services.renderer().authoring_config(),
            services.renderer().color_pipeline_mode(),
            services.renderer().static_models()
        );
        prepared.runtime.prepare_level_session_activation(services, !services.renderer().ui_available());
        services.renderer().prepare_resources(prepared.world_scene.resource_items());
        replace_level_session(state, std::move(prepared), services);
        log_level_loaded(services, state.runtime, state.world_scene);
    }

    auto reload_live_content(DemoGameRuntimeState state, RelayCore::EngineServices& services) -> bool
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::reload_live_content");
        const std::string level_name = services.renderer().runtime_diagnostics().level_name;
        RelayCore::log::info(
            "Reloading live content for {}",
            level_name.empty() ? std::string{"<unknown>"} : level_name
        );

        const std::optional<GameWorldRuntime::InspectorRestoreState> pinned_inspector =
            state.runtime.capture_pinned_inspector_restore_state();

        PreparedLevelReload staged{};
        const bool previous_capture = services.window().relative_mouse_mode();
        try
        {
            staged = prepare_level_reload(services, state.runtime.level_path(), pinned_inspector);
            staged.renderer.prepare_resources(staged.session.world_scene.resource_items());
            staged.session.runtime.prepare_level_session_activation(services, true, &staged.renderer.config.camera);

            services.renderer().commit_live_content_reload(std::move(staged.renderer));
        } catch (const std::exception& error)
        {
            static_cast<void>(SDL_SetWindowRelativeMouseMode(services.window().handle(), previous_capture));
            publish_reload_result(
                state.iteration_diagnostics,
                false,
                "Reload failed: live content for " + (level_name.empty() ? std::string{"<unknown>"} : level_name),
                error.what()
            );
            services.renderer().set_world_diagnostics(state.world_scene.diagnostics());
            state.runtime.publish_diagnostics(services.renderer());
            publish_iteration_diagnostics(state, services);
            RelayCore::log::error("Live content reload failed: {}", error.what());
            return false;
        }

        replace_level_session(state, std::move(staged.session), services);
        services.commit_live_reload_config(std::move(staged.config));
        try
        {
            publish_reload_result(
                state.iteration_diagnostics,
                true,
                build_reload_success_summary(
                    services.renderer().runtime_diagnostics(),
                    state.world_scene,
                    staged.atlas_source_label
                )
            );
            publish_iteration_diagnostics(state, services);
            RelayCore::log::info("{}", state.iteration_diagnostics.last_reload_summary);
            log_level_loaded(services, state.runtime, state.world_scene);
        } catch (const std::exception& error)
        {
            RelayCore::log::error("Live content reloaded, but diagnostics publication failed: {}", error.what());
        }
        return true;
    }
} // namespace RelayGame::detail

#include "relaygame/demo_game_runtime_ops.h"

#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/render/renderer.h"
#include "relaygame/level_session_builder.h"

#include <exception>
#include <string>

namespace RelayGame::detail
{
    namespace
    {
        struct PreparedLevelReload {
            RelayCore::core::EngineConfig config{};
            RelayCore::Renderer::PreparedLiveContentReload renderer{};
            RelayGame::GameWorldRuntime runtime{};
            RelayGame::WorldSceneRenderer world_scene{};
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
            const std::optional<RelayGame::GameWorldRuntime::InspectorRestoreState>& pinned_inspector
        ) -> PreparedLevelReload
        {
            const RelayCore::core::EngineConfig reloaded_config =
                RelayCore::core::load_engine_config(services.config().source_path);
            const RelayCore::render::RenderAuthoringConfig reloaded_authoring_config =
                RelayCore::render::load_default_render_authoring_config();

            PreparedLevelReload prepared{
                .config = reloaded_config,
                .renderer = services.renderer()
                                .prepare_live_content_reload(reloaded_config.renderer, reloaded_authoring_config),
            };

            prepared.runtime.load_level_session(prepared.config, LevelSessionBuilder{prepared.config.gameplay}.build());
            if (pinned_inspector.has_value())
            {
                prepared.runtime.restore_pinned_inspector(*pinned_inspector);
            }

            prepared.world_scene.initialize(
                build_prepared_world_scene_data(prepared.runtime.level(), prepared.renderer.authoring_config)
            );
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

    void start_level_session(DemoGameRuntimeState state, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::start_level_session");
        state.runtime.startup(services);
        state.world_scene.initialize(
            build_prepared_world_scene_data(state.runtime.level(), services.renderer().authoring_config())
        );
        services.renderer().set_world_diagnostics(state.world_scene.diagnostics());
        state.runtime.publish_diagnostics(services.renderer());
        publish_iteration_diagnostics(state, services);
        log_level_loaded(services, state.runtime, state.world_scene);
    }

    void reload_live_content(DemoGameRuntimeState state, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::reload_live_content");
        const std::string level_name = services.renderer().runtime_diagnostics().level_name;
        RelayCore::log::info(
            "Reloading live content for {}",
            level_name.empty() ? std::string{"<unknown>"} : level_name
        );

        const std::optional<GameWorldRuntime::InspectorRestoreState> pinned_inspector =
            state.runtime.capture_pinned_inspector_restore_state();

        try
        {
            PreparedLevelReload staged = prepare_level_reload(services, pinned_inspector);

            services.renderer().commit_live_content_reload(std::move(staged.renderer));
            state.runtime.shutdown(services);
            state.world_scene.shutdown();
            state.runtime = std::move(staged.runtime);
            state.world_scene = std::move(staged.world_scene);
            state.runtime.activate_level_session(services);

            services.renderer().set_world_diagnostics(state.world_scene.diagnostics());
            state.runtime.publish_diagnostics(services.renderer());

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
        }
    }
} // namespace RelayGame::detail

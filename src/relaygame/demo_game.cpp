#include "relaygame/demo_game.h"

#include "relaycore/app/engine_services.h"
#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/debug/inspector_source_reference.h"
#include "relaycore/render/renderer.h"
#include "relaygame/level_session_builder.h"

#include <exception>
#include <string>
#include <string_view>

namespace RelayGame
{
    namespace
    {
        void log_level_loaded(
            RelayCore::EngineServices& services,
            const RelayGame::GameWorldRuntime& runtime,
            const RelayGame::WorldSceneRenderer& world_scene
        )
        {
            RelayCore::log::info(
                "DemoGame loaded level {}: {} solids, {} entities, {} render chunks, {} triangles, {} doors, {} "
                "triggers, {} "
                "usables",
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

        void publish_snapshot_result(
            RelayCore::render::IterationDiagnostics& iteration_diagnostics,
            const bool succeeded,
            std::string summary,
            std::string error = {}
        )
        {
            iteration_diagnostics.has_last_snapshot_result = true;
            iteration_diagnostics.last_snapshot_succeeded = succeeded;
            iteration_diagnostics.last_snapshot_summary = std::move(summary);
            iteration_diagnostics.last_snapshot_error = std::move(error);
        }

        void publish_command_result(
            RelayCore::render::IterationDiagnostics& iteration_diagnostics,
            const bool succeeded,
            std::string summary,
            std::string error = {}
        )
        {
            iteration_diagnostics.has_last_command_result = true;
            iteration_diagnostics.last_command_succeeded = succeeded;
            iteration_diagnostics.last_command_summary = std::move(summary);
            iteration_diagnostics.last_command_error = std::move(error);
        }

        [[nodiscard]] auto diagnostics_filter_label(const RelayCore::render::DiagnosticsSourceFilter filter)
            -> std::string_view
        {
            switch (filter)
            {
                case RelayCore::render::DiagnosticsSourceFilter::Direct:
                    return "direct";
                case RelayCore::render::DiagnosticsSourceFilter::Instanced:
                    return "instanced";
                case RelayCore::render::DiagnosticsSourceFilter::Generated:
                    return "generated";
                case RelayCore::render::DiagnosticsSourceFilter::All:
                default:
                    return "all";
            }
        }

        [[nodiscard]] auto source_bucket_matches_filter(
            const std::string_view bucket,
            const RelayCore::render::DiagnosticsSourceFilter filter,
            const bool direct_matches_untracked = false
        ) -> bool
        {
            if (filter == RelayCore::render::DiagnosticsSourceFilter::All)
            {
                return true;
            }

            if (direct_matches_untracked && filter == RelayCore::render::DiagnosticsSourceFilter::Direct
                && bucket == "untracked")
            {
                return true;
            }

            switch (filter)
            {
                case RelayCore::render::DiagnosticsSourceFilter::Direct:
                    return bucket == "direct";
                case RelayCore::render::DiagnosticsSourceFilter::Instanced:
                    return bucket == "instanced";
                case RelayCore::render::DiagnosticsSourceFilter::Generated:
                    return bucket == "generated";
                case RelayCore::render::DiagnosticsSourceFilter::All:
                default:
                    return true;
            }
        }

        [[nodiscard]] auto warning_matches_filter(
            const RelayCore::render::AuthoringWarningDiagnostics& warning,
            const RelayCore::render::DiagnosticsSourceFilter filter
        ) -> bool
        {
            const std::string_view bucket =
                warning.source_bucket.empty() ? std::string_view{"direct"} : std::string_view{warning.source_bucket};
            return source_bucket_matches_filter(bucket, filter);
        }

        [[nodiscard]] auto source_reference_for_location(const std::string& file_path, const int line, const int column)
            -> std::string
        {
            return RelayCore::debug::build_source_reference(
                RelayCore::debug::SourceLocation{
                    .file_path = file_path,
                    .line = line,
                    .column = column,
                }
            );
        }
    } // namespace

    void DemoGame::start_level_session(RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::start_level_session");
        runtime_.startup(services);
        world_scene_.initialize(runtime_.level(), services.renderer().authoring_config());
        services.renderer().set_world_diagnostics(world_scene_.diagnostics());
        runtime_.publish_diagnostics(services.renderer());
        publish_iteration_diagnostics(services);
        log_level_loaded(services, runtime_, world_scene_);
    }

    void DemoGame::reload_live_content(RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::reload_live_content");
        const std::string level_name = services.renderer().runtime_diagnostics().level_name;
        RelayCore::log::info(
            "Reloading live content for {}",
            level_name.empty() ? std::string{"<unknown>"} : level_name
        );

        const std::optional<GameWorldRuntime::InspectorRestoreState> pinned_inspector =
            runtime_.capture_pinned_inspector_restore_state();

        try
        {
            const RelayCore::core::EngineConfig reloaded_config = RelayCore::core::load_default_engine_config();
            const RelayCore::render::RenderAuthoringConfig reloaded_authoring_config =
                RelayCore::render::load_default_render_authoring_config();
            RelayCore::Renderer::PreparedLiveContentReload staged_renderer =
                services.renderer().prepare_live_content_reload(reloaded_config.renderer, reloaded_authoring_config);

            GameWorldRuntime staged_runtime{};
            staged_runtime.load_level_session(reloaded_config, LevelSessionBuilder{reloaded_config.gameplay}.build());
            if (pinned_inspector.has_value())
            {
                staged_runtime.restore_pinned_inspector(*pinned_inspector);
            }

            WorldSceneRenderer staged_world_scene{};
            staged_world_scene.initialize(staged_runtime.level(), staged_renderer.authoring_config);
            const std::string atlas_source_label = staged_renderer.atlas_source_label;

            runtime_.shutdown(services);
            world_scene_.shutdown();
            services.renderer().commit_live_content_reload(std::move(staged_renderer));
            runtime_ = std::move(staged_runtime);
            world_scene_ = std::move(staged_world_scene);
            runtime_.activate_level_session(services);

            services.renderer().set_world_diagnostics(world_scene_.diagnostics());
            runtime_.publish_diagnostics(services.renderer());

            iteration_diagnostics_.has_last_reload_result = true;
            iteration_diagnostics_.last_reload_succeeded = true;
            iteration_diagnostics_.last_reload_summary = build_reload_success_summary(
                services.renderer().runtime_diagnostics(),
                world_scene_,
                atlas_source_label
            );
            iteration_diagnostics_.last_reload_error.clear();
            publish_iteration_diagnostics(services);

            RelayCore::log::info("{}", iteration_diagnostics_.last_reload_summary);
            log_level_loaded(services, runtime_, world_scene_);
        } catch (const std::exception& error)
        {
            iteration_diagnostics_.has_last_reload_result = true;
            iteration_diagnostics_.last_reload_succeeded = false;
            iteration_diagnostics_.last_reload_summary =
                "Reload failed: live content for " + (level_name.empty() ? std::string{"<unknown>"} : level_name);
            iteration_diagnostics_.last_reload_error = error.what();
            services.renderer().set_world_diagnostics(world_scene_.diagnostics());
            runtime_.publish_diagnostics(services.renderer());
            publish_iteration_diagnostics(services);
            RelayCore::log::error("Live content reload failed: {}", error.what());
        }
    }

    void DemoGame::handle_debug_iteration_tools(RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::handle_debug_iteration_tools");
        RelayCore::Renderer& renderer = services.renderer();

        if (runtime_.consume_reload_level_requested())
        {
            reload_live_content(services);
            return;
        }

        if (runtime_.consume_save_runtime_snapshot_requested() || renderer.consume_save_runtime_snapshot_requested())
        {
            saved_runtime_snapshot_ = runtime_.capture_runtime_snapshot();
            iteration_diagnostics_.has_saved_runtime_snapshot = saved_runtime_snapshot_.has_value();
            iteration_diagnostics_.saved_snapshot_summary =
                saved_runtime_snapshot_.has_value()
                    ? RelayCore::gameplay::build_runtime_snapshot_summary(*saved_runtime_snapshot_)
                    : std::string{};
            publish_snapshot_result(iteration_diagnostics_, true, iteration_diagnostics_.saved_snapshot_summary);
            publish_iteration_diagnostics(services);
            RelayCore::log::info("{}", iteration_diagnostics_.last_snapshot_summary);
            return;
        }

        if (runtime_.consume_restore_runtime_snapshot_requested()
            || renderer.consume_restore_runtime_snapshot_requested())
        {
            if (!saved_runtime_snapshot_.has_value())
            {
                publish_snapshot_result(
                    iteration_diagnostics_,
                    false,
                    "Snapshot restore failed",
                    "No saved runtime snapshot is available"
                );
                publish_iteration_diagnostics(services);
                RelayCore::log::warn("{}", iteration_diagnostics_.last_snapshot_error);
                return;
            }

            const RelayCore::gameplay::SnapshotRestoreResult restore_result =
                runtime_.restore_runtime_snapshot(*saved_runtime_snapshot_);
            publish_snapshot_result(
                iteration_diagnostics_,
                restore_result.restored,
                RelayCore::gameplay::build_runtime_restore_summary(restore_result),
                restore_result.error
            );
            runtime_.publish_diagnostics(services.renderer());
            publish_iteration_diagnostics(services);
            if (restore_result.restored)
            {
                RelayCore::log::info("{}", iteration_diagnostics_.last_snapshot_summary);
            } else
            {
                RelayCore::log::warn(
                    "{}",
                    iteration_diagnostics_.last_snapshot_error.empty() ? iteration_diagnostics_.last_snapshot_summary
                                                                       : iteration_diagnostics_.last_snapshot_error
                );
            }
            return;
        }

        const auto publish_and_log_command = [&](const bool succeeded, std::string summary, std::string error = {}) {
            publish_command_result(iteration_diagnostics_, succeeded, std::move(summary), std::move(error));
            publish_iteration_diagnostics(services);
            if (iteration_diagnostics_.last_command_succeeded)
            {
                RelayCore::log::info("{}", iteration_diagnostics_.last_command_summary);
            } else
            {
                RelayCore::log::warn(
                    "{}",
                    iteration_diagnostics_.last_command_error.empty() ? iteration_diagnostics_.last_command_summary
                                                                      : iteration_diagnostics_.last_command_error
                );
            }
        };

        if (renderer.consume_open_first_authoring_warning_requested())
        {
            const auto filter = renderer.authoring_warning_source_filter();
            for (const auto& warning : iteration_diagnostics_.authoring_warnings)
            {
                if (!warning_matches_filter(warning, filter))
                {
                    continue;
                }

                RelayCore::debug::SourceLocation location{
                    .file_path = warning.primary_file_path,
                    .line = warning.primary_line,
                    .column = warning.primary_column,
                };
                if (!location.valid())
                {
                    location = RelayCore::debug::SourceLocation{
                        .file_path = warning.secondary_file_path,
                        .line = warning.secondary_line,
                        .column = warning.secondary_column,
                    };
                }
                if (!location.valid())
                {
                    continue;
                }

                const std::string source_ref = RelayCore::debug::build_source_reference(location);
                const RelayCore::debug::SourceOpenResult result = RelayCore::debug::open_source_location(location);
                if (!result.launched)
                {
                    publish_and_log_command(
                        false,
                        "Open first authoring warning failed",
                        "Failed to open authoring warning source " + source_ref + ": "
                            + (result.error.empty() ? std::string{"unknown error"} : result.error)
                    );
                } else
                {
                    publish_and_log_command(
                        true,
                        "Opened first authoring warning (" + std::string(diagnostics_filter_label(filter)) + ") via "
                            + (result.strategy.empty() ? std::string{"editor launcher"} : result.strategy) + ": "
                            + source_ref
                    );
                }
                return;
            }

            publish_and_log_command(
                false,
                "Open first authoring warning failed",
                "No filtered authoring warning with a valid source location"
            );
            return;
        }

        if (renderer.consume_dump_authoring_warnings_requested())
        {
            const auto filter = renderer.authoring_warning_source_filter();
            std::size_t match_count = 0;
            RelayCore::log::info("Authoring warnings dump [{}]", diagnostics_filter_label(filter));
            for (const auto& warning : iteration_diagnostics_.authoring_warnings)
            {
                if (!warning_matches_filter(warning, filter))
                {
                    continue;
                }

                ++match_count;
                const std::string primary_ref = source_reference_for_location(
                    warning.primary_file_path,
                    warning.primary_line,
                    warning.primary_column
                );
                const std::string secondary_ref = source_reference_for_location(
                    warning.secondary_file_path,
                    warning.secondary_line,
                    warning.secondary_column
                );
                RelayCore::log::warn(
                    "[{}] {} | bucket={} | A={} | B={}",
                    match_count,
                    warning.message,
                    warning.source_bucket.empty() ? std::string{"direct"} : warning.source_bucket,
                    primary_ref.empty() ? std::string{"(none)"} : primary_ref,
                    secondary_ref.empty() ? std::string{"(none)"} : secondary_ref
                );
            }
            publish_and_log_command(
                true,
                "Dumped " + std::to_string(match_count) + " authoring warnings ("
                    + std::string(diagnostics_filter_label(filter)) + ")"
            );
            return;
        }

        if (renderer.consume_open_first_runtime_registry_requested())
        {
            const auto filter = renderer.runtime_registry_source_filter();
            for (const auto& entry : iteration_diagnostics_.runtime_registry)
            {
                const std::string_view bucket =
                    entry.source_bucket.empty() ? std::string_view{"untracked"} : std::string_view{entry.source_bucket};
                if (!source_bucket_matches_filter(bucket, filter, true))
                {
                    continue;
                }

                const RelayCore::debug::SourceLocation location{
                    .file_path = entry.source_file_path,
                    .line = entry.source_line,
                    .column = entry.source_column,
                };
                if (!location.valid())
                {
                    continue;
                }

                const std::string source_ref = RelayCore::debug::build_source_reference(location);
                const RelayCore::debug::SourceOpenResult result = RelayCore::debug::open_source_location(location);
                if (!result.launched)
                {
                    publish_and_log_command(
                        false,
                        "Open first runtime registry source failed",
                        "Failed to open runtime registry source " + source_ref + ": "
                            + (result.error.empty() ? std::string{"unknown error"} : result.error)
                    );
                } else
                {
                    publish_and_log_command(
                        true,
                        "Opened first runtime registry source (" + std::string(diagnostics_filter_label(filter))
                            + ") via " + (result.strategy.empty() ? std::string{"editor launcher"} : result.strategy)
                            + ": " + source_ref
                    );
                }
                return;
            }

            publish_and_log_command(
                false,
                "Open first runtime registry source failed",
                "No filtered runtime registry entry with a valid source location"
            );
            return;
        }

        if (renderer.consume_dump_runtime_registry_requested())
        {
            const auto filter = renderer.runtime_registry_source_filter();
            std::size_t match_count = 0;
            RelayCore::log::info("Runtime registry dump [{}]", diagnostics_filter_label(filter));
            for (const auto& entry : iteration_diagnostics_.runtime_registry)
            {
                const std::string_view bucket =
                    entry.source_bucket.empty() ? std::string_view{"untracked"} : std::string_view{entry.source_bucket};
                if (!source_bucket_matches_filter(bucket, filter, true))
                {
                    continue;
                }

                ++match_count;
                RelayCore::log::info(
                    "[{}] {} | {} | name={} | label={} | target={} | source={}",
                    match_count,
                    entry.runtime_handle,
                    entry.object_kind,
                    entry.name.empty() ? std::string{"(none)"} : entry.name,
                    entry.label.empty() ? std::string{"(none)"} : entry.label,
                    entry.target.empty() ? std::string{"(none)"} : entry.target,
                    entry.source_reference.empty() ? std::string{"(none)"} : entry.source_reference
                );
            }
            publish_and_log_command(
                true,
                "Dumped " + std::to_string(match_count) + " runtime registry entries ("
                    + std::string(diagnostics_filter_label(filter)) + ")"
            );
            return;
        }

        if (renderer.consume_open_first_runtime_binding_requested())
        {
            const auto filter = renderer.runtime_binding_source_filter();
            for (const auto& binding : iteration_diagnostics_.runtime_bindings)
            {
                const std::string_view bucket = binding.source_bucket.empty() ? std::string_view{"untracked"}
                                                                              : std::string_view{binding.source_bucket};
                if (!source_bucket_matches_filter(bucket, filter, true))
                {
                    continue;
                }

                const RelayCore::debug::SourceLocation location{
                    .file_path = binding.source_file_path,
                    .line = binding.source_line,
                    .column = binding.source_column,
                };
                if (!location.valid())
                {
                    continue;
                }

                const std::string source_ref = RelayCore::debug::build_source_reference(location);
                const RelayCore::debug::SourceOpenResult result = RelayCore::debug::open_source_location(location);
                if (!result.launched)
                {
                    publish_and_log_command(
                        false,
                        "Open first runtime binding source failed",
                        "Failed to open runtime binding source " + source_ref + ": "
                            + (result.error.empty() ? std::string{"unknown error"} : result.error)
                    );
                } else
                {
                    publish_and_log_command(
                        true,
                        "Opened first runtime binding source (" + std::string(diagnostics_filter_label(filter))
                            + ") via " + (result.strategy.empty() ? std::string{"editor launcher"} : result.strategy)
                            + ": " + source_ref
                    );
                }
                return;
            }

            publish_and_log_command(
                false,
                "Open first runtime binding source failed",
                "No filtered runtime binding with a valid source location"
            );
            return;
        }

        if (renderer.consume_dump_runtime_bindings_requested())
        {
            const auto filter = renderer.runtime_binding_source_filter();
            std::size_t match_count = 0;
            RelayCore::log::info("Runtime bindings dump [{}]", diagnostics_filter_label(filter));
            for (const auto& binding : iteration_diagnostics_.runtime_bindings)
            {
                const std::string_view bucket = binding.source_bucket.empty() ? std::string_view{"untracked"}
                                                                              : std::string_view{binding.source_bucket};
                if (!source_bucket_matches_filter(bucket, filter, true))
                {
                    continue;
                }

                ++match_count;
                RelayCore::log::info(
                    "[{}] {} ({}) -> {} | resolved={} | targets={} | source={}",
                    match_count,
                    binding.source_name.empty() ? std::string{"(none)"} : binding.source_name,
                    binding.source_kind.empty() ? std::string{"(none)"} : binding.source_kind,
                    binding.target.empty() ? std::string{"(none)"} : binding.target,
                    binding.resolved_target_count,
                    binding.resolved_targets_summary.empty() ? std::string{"(none)"} : binding.resolved_targets_summary,
                    binding.source_reference.empty() ? std::string{"(none)"} : binding.source_reference
                );
            }
            publish_and_log_command(
                true,
                "Dumped " + std::to_string(match_count) + " runtime bindings ("
                    + std::string(diagnostics_filter_label(filter)) + ")"
            );
            return;
        }

        if (renderer.consume_dump_render_authoring_contract_requested())
        {
            const auto& authoring_config = renderer.authoring_config();
            RelayCore::log::info("Render authoring contract: {}", authoring_config.source_path.string());
            for (const RelayCore::SurfaceMaterial material : RelayCore::all_surface_materials())
            {
                const auto& authored_material = RelayCore::render::material_authoring(authoring_config, material);
                RelayCore::log::info(
                    "Material {} | profile={} | transparent={} | bevel={} | atlas_scale={:.2f} | atlas_mix={:.2f} | "
                    "ambient_bias={:.2f} | bounce_mix={:.2f} | emissive={:.2f} | pulse={:.2f}",
                    authored_material.canonical_name,
                    RelayCore::render::material_render_profile_name(authored_material.render_profile),
                    authored_material.transparent ? "yes" : "no",
                    authored_material.bevel_enabled ? "yes" : "no",
                    authored_material.atlas_world_scale,
                    authored_material.atlas_mix,
                    authored_material.ambient_bias,
                    authored_material.bounce_mix,
                    authored_material.emissive_strength,
                    authored_material.emissive_pulse_speed
                );
            }

            const auto& lighting = authoring_config.lighting;
            RelayCore::log::info(
                "Lighting ambient | min={:.2f} max={:.2f} floor={:.2f} zenith={:.2f} | cool floor={:.2f} zenith={:.2f}",
                lighting.ambient_min,
                lighting.ambient_max,
                lighting.ambient_floor,
                lighting.ambient_zenith,
                lighting.cool_ambient_floor,
                lighting.cool_ambient_zenith
            );
            RelayCore::log::info(
                "Lighting bounce | warm floor={:.2f} underside={:.2f} | neutral floor={:.2f} underside={:.2f} | "
                "darkness={:.2f}",
                lighting.warm_bounce_floor,
                lighting.warm_bounce_underside,
                lighting.bounce_floor,
                lighting.bounce_underside,
                lighting.bounce_darkness
            );
            RelayCore::log::info(
                "Lighting cavity/local | broad={:.2f} micro={:.2f} underside={:.2f} darken={:.2f} | wrap={:.2f} "
                "fill={:.2f} gain={:.2f}",
                lighting.cavity_broad_strength,
                lighting.cavity_micro_strength,
                lighting.cavity_underside_strength,
                lighting.cavity_ambient_darken,
                lighting.local_light_wrap,
                lighting.local_light_fill,
                lighting.local_light_gain
            );

            const auto& render_light = authoring_config.render_light;
            RelayCore::log::info(
                "Render light contract | default_color=[{:.2f}, {:.2f}, {:.2f}] | default_intensity={:.2f} | "
                "default_radius={:.2f} | intensity=[{:.2f}, {:.2f}] | radius=[{:.2f}, {:.2f}]",
                render_light.default_color.x,
                render_light.default_color.y,
                render_light.default_color.z,
                render_light.default_intensity,
                render_light.default_radius,
                render_light.min_intensity,
                render_light.max_intensity,
                render_light.min_radius,
                render_light.max_radius
            );

            publish_and_log_command(true, "Dumped render authoring contract: " + authoring_config.source_path.string());
            return;
        }

        if (!runtime_.consume_open_inspector_source_requested())
        {
            return;
        }

        const RelayCore::render::InspectorDiagnostics& inspector = renderer.inspector_diagnostics();
        const RelayCore::debug::SourceLocation source_location =
            RelayCore::debug::build_primary_inspector_source_location(inspector);
        const std::string source_ref = RelayCore::debug::build_source_reference(source_location);
        if (!source_location.valid())
        {
            RelayCore::log::info("Inspector source open ignored: no authored source available");
            return;
        }

        const RelayCore::debug::SourceOpenResult result = RelayCore::debug::open_source_location(source_location);
        if (!result.launched)
        {
            RelayCore::log::warn(
                "Failed to open inspector source {}: {}",
                source_ref,
                result.error.empty() ? std::string{"unknown error"} : result.error
            );
            return;
        }

        RelayCore::log::info(
            "Opened inspector source via {}: {}",
            result.strategy.empty() ? std::string{"editor launcher"} : result.strategy,
            source_ref
        );
    }

    void DemoGame::publish_iteration_diagnostics(RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_FUNCTION();
        iteration_diagnostics_.authoring_warnings = runtime_.authoring_warnings();
        iteration_diagnostics_.authoring_warnings.insert(
            iteration_diagnostics_.authoring_warnings.end(),
            world_scene_.authoring_warnings().begin(),
            world_scene_.authoring_warnings().end()
        );
        iteration_diagnostics_.runtime_registry = runtime_.runtime_registry_diagnostics();
        iteration_diagnostics_.runtime_bindings = runtime_.runtime_binding_diagnostics();
        iteration_diagnostics_.has_saved_runtime_snapshot = saved_runtime_snapshot_.has_value();
        iteration_diagnostics_.saved_snapshot_summary =
            saved_runtime_snapshot_.has_value()
                ? RelayCore::gameplay::build_runtime_snapshot_summary(*saved_runtime_snapshot_)
                : std::string{};
        services.renderer().set_iteration_diagnostics(iteration_diagnostics_);
    }

    void DemoGame::on_startup(RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::on_startup");
        start_level_session(services);
    }

    void DemoGame::on_event(const SDL_Event& event, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_FUNCTION();
        runtime_.on_event(event, services);
    }

    void DemoGame::on_update(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::on_update");
        runtime_.update(frame, services);
        runtime_.publish_diagnostics(services.renderer());
        handle_debug_iteration_tools(services);
    }

    void DemoGame::on_render(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::on_render");
        services.renderer().set_scene_time(static_cast<float>(frame.elapsed_seconds));
        world_scene_.render(
            services.renderer(),
            services.camera(),
            static_cast<float>(frame.elapsed_seconds),
            runtime_.doors(),
            runtime_.turrets(),
            runtime_.usables(),
            runtime_.focused_usable_index(),
            services.renderer().inspector_diagnostics(),
            services.renderer().inspector_visualization_enabled()
        );
        services.renderer().set_world_diagnostics(world_scene_.diagnostics());
    }

    void DemoGame::on_shutdown(RelayCore::EngineServices& services) noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        runtime_.shutdown(services);
        world_scene_.shutdown();
        saved_runtime_snapshot_.reset();
        RelayCore::log::info("DemoGame shutdown");
    }

    auto make_demo_game() -> std::unique_ptr<RelayCore::AppDelegate>
    {
        RELAYCORE_PROFILE_FUNCTION();
        return std::make_unique<DemoGame>();
    }
} // namespace RelayGame

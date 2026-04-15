#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/debug/inspector_source_reference.h"
#include "relaycore/render/renderer.h"
#include "relaygame/demo_game_iteration_internal.h"

#include <string>
#include <string_view>

namespace RelayGame::detail
{
    namespace
    {
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

        void publish_and_log_command_result(
            DemoGameRuntimeState state,
            RelayCore::EngineServices& services,
            const bool succeeded,
            std::string summary,
            std::string error = {}
        )
        {
            publish_command_result(state.iteration_diagnostics, succeeded, std::move(summary), std::move(error));
            push_iteration_diagnostics(state, services);
            if (state.iteration_diagnostics.last_command_succeeded)
            {
                RelayCore::log::info("{}", state.iteration_diagnostics.last_command_summary);
            } else
            {
                RelayCore::log::warn(
                    "{}",
                    state.iteration_diagnostics.last_command_error.empty()
                        ? state.iteration_diagnostics.last_command_summary
                        : state.iteration_diagnostics.last_command_error
                );
            }
        }

        void save_runtime_snapshot(DemoGameRuntimeState state, RelayCore::EngineServices& services)
        {
            RELAYCORE_PROFILE_ZONE_N("DemoGame::save_runtime_snapshot");
            state.saved_runtime_snapshot = state.runtime.capture_runtime_snapshot();
            update_saved_snapshot_diagnostics(state);
            publish_snapshot_result(
                state.iteration_diagnostics,
                true,
                state.iteration_diagnostics.saved_snapshot_summary
            );
            publish_iteration_diagnostics(state, services);
            RelayCore::log::info("{}", state.iteration_diagnostics.last_snapshot_summary);
        }

        void restore_runtime_snapshot(DemoGameRuntimeState state, RelayCore::EngineServices& services)
        {
            RELAYCORE_PROFILE_ZONE_N("DemoGame::restore_runtime_snapshot");
            if (!state.saved_runtime_snapshot.has_value())
            {
                publish_snapshot_result(
                    state.iteration_diagnostics,
                    false,
                    "Snapshot restore failed",
                    "No saved runtime snapshot is available"
                );
                publish_iteration_diagnostics(state, services);
                RelayCore::log::warn("{}", state.iteration_diagnostics.last_snapshot_error);
                return;
            }

            const RelayCore::gameplay::SnapshotRestoreResult restore_result =
                state.runtime.restore_runtime_snapshot(*state.saved_runtime_snapshot);
            publish_snapshot_result(
                state.iteration_diagnostics,
                restore_result.restored,
                RelayCore::gameplay::build_runtime_restore_summary(restore_result),
                restore_result.error
            );
            state.runtime.publish_diagnostics(services.renderer());
            publish_iteration_diagnostics(state, services);
            if (restore_result.restored)
            {
                RelayCore::log::info("{}", state.iteration_diagnostics.last_snapshot_summary);
            } else
            {
                RelayCore::log::warn(
                    "{}",
                    state.iteration_diagnostics.last_snapshot_error.empty()
                        ? state.iteration_diagnostics.last_snapshot_summary
                        : state.iteration_diagnostics.last_snapshot_error
                );
            }
        }

        [[nodiscard]] auto
        handle_runtime_snapshot_commands(DemoGameRuntimeState state, RelayCore::EngineServices& services) -> bool
        {
            RELAYCORE_PROFILE_ZONE_N("DemoGame::handle_runtime_snapshot_commands");
            RelayCore::Renderer& renderer = services.renderer();

            if (state.runtime.consume_save_runtime_snapshot_requested()
                || renderer.consume_save_runtime_snapshot_requested())
            {
                save_runtime_snapshot(state, services);
                return true;
            }

            if (state.runtime.consume_restore_runtime_snapshot_requested()
                || renderer.consume_restore_runtime_snapshot_requested())
            {
                restore_runtime_snapshot(state, services);
                return true;
            }

            return false;
        }

        [[nodiscard]] auto
        handle_authoring_warning_commands(DemoGameRuntimeState state, RelayCore::EngineServices& services) -> bool
        {
            RelayCore::Renderer& renderer = services.renderer();

            if (renderer.consume_open_first_authoring_warning_requested())
            {
                const auto filter = renderer.authoring_warning_source_filter();
                for (const auto& warning : state.iteration_diagnostics.authoring_warnings)
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
                        publish_and_log_command_result(
                            state,
                            services,
                            false,
                            "Open first authoring warning failed",
                            "Failed to open authoring warning source " + source_ref + ": "
                                + (result.error.empty() ? std::string{"unknown error"} : result.error)
                        );
                    } else
                    {
                        publish_and_log_command_result(
                            state,
                            services,
                            true,
                            "Opened first authoring warning (" + std::string(diagnostics_filter_label(filter))
                                + ") via "
                                + (result.strategy.empty() ? std::string{"editor launcher"} : result.strategy) + ": "
                                + source_ref
                        );
                    }
                    return true;
                }

                publish_and_log_command_result(
                    state,
                    services,
                    false,
                    "Open first authoring warning failed",
                    "No filtered authoring warning with a valid source location"
                );
                return true;
            }

            if (renderer.consume_dump_authoring_warnings_requested())
            {
                const auto filter = renderer.authoring_warning_source_filter();
                std::size_t match_count = 0;
                RelayCore::log::info("Authoring warnings dump [{}]", diagnostics_filter_label(filter));
                for (const auto& warning : state.iteration_diagnostics.authoring_warnings)
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
                publish_and_log_command_result(
                    state,
                    services,
                    true,
                    "Dumped " + std::to_string(match_count) + " authoring warnings ("
                        + std::string(diagnostics_filter_label(filter)) + ")"
                );
                return true;
            }

            return false;
        }

        [[nodiscard]] auto
        handle_runtime_registry_commands(DemoGameRuntimeState state, RelayCore::EngineServices& services) -> bool
        {
            RelayCore::Renderer& renderer = services.renderer();
            const bool open_requested = renderer.consume_open_first_runtime_registry_requested();
            const bool dump_requested = renderer.consume_dump_runtime_registry_requested();
            if (!open_requested && !dump_requested)
            {
                return false;
            }

            if (open_requested)
            {
                const auto filter = renderer.runtime_registry_source_filter();
                for (const auto& entry : state.iteration_diagnostics.runtime_registry)
                {
                    const std::string_view bucket = entry.source_bucket.empty() ? std::string_view{"untracked"}
                                                                                : std::string_view{entry.source_bucket};
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
                        publish_and_log_command_result(
                            state,
                            services,
                            false,
                            "Open first runtime registry source failed",
                            "Failed to open runtime registry source " + source_ref + ": "
                                + (result.error.empty() ? std::string{"unknown error"} : result.error)
                        );
                    } else
                    {
                        publish_and_log_command_result(
                            state,
                            services,
                            true,
                            "Opened first runtime registry source (" + std::string(diagnostics_filter_label(filter))
                                + ") via "
                                + (result.strategy.empty() ? std::string{"editor launcher"} : result.strategy) + ": "
                                + source_ref
                        );
                    }
                    return true;
                }

                publish_and_log_command_result(
                    state,
                    services,
                    false,
                    "Open first runtime registry source failed",
                    "No filtered runtime registry entry with a valid source location"
                );
                return true;
            }

            const auto filter = renderer.runtime_registry_source_filter();
            std::size_t match_count = 0;
            RelayCore::log::info("Runtime registry dump [{}]", diagnostics_filter_label(filter));
            for (const auto& entry : state.iteration_diagnostics.runtime_registry)
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
            publish_and_log_command_result(
                state,
                services,
                true,
                "Dumped " + std::to_string(match_count) + " runtime registry entries ("
                    + std::string(diagnostics_filter_label(filter)) + ")"
            );
            return true;
        }

        [[nodiscard]] auto
        handle_runtime_binding_commands(DemoGameRuntimeState state, RelayCore::EngineServices& services) -> bool
        {
            RelayCore::Renderer& renderer = services.renderer();

            if (renderer.consume_open_first_runtime_binding_requested())
            {
                const auto filter = renderer.runtime_binding_source_filter();
                for (const auto& binding : state.iteration_diagnostics.runtime_bindings)
                {
                    const std::string_view bucket = binding.source_bucket.empty()
                                                        ? std::string_view{"untracked"}
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
                        publish_and_log_command_result(
                            state,
                            services,
                            false,
                            "Open first runtime binding source failed",
                            "Failed to open runtime binding source " + source_ref + ": "
                                + (result.error.empty() ? std::string{"unknown error"} : result.error)
                        );
                    } else
                    {
                        publish_and_log_command_result(
                            state,
                            services,
                            true,
                            "Opened first runtime binding source (" + std::string(diagnostics_filter_label(filter))
                                + ") via "
                                + (result.strategy.empty() ? std::string{"editor launcher"} : result.strategy) + ": "
                                + source_ref
                        );
                    }
                    return true;
                }

                publish_and_log_command_result(
                    state,
                    services,
                    false,
                    "Open first runtime binding source failed",
                    "No filtered runtime binding with a valid source location"
                );
                return true;
            }

            if (renderer.consume_dump_runtime_bindings_requested())
            {
                const auto filter = renderer.runtime_binding_source_filter();
                std::size_t match_count = 0;
                RelayCore::log::info("Runtime bindings dump [{}]", diagnostics_filter_label(filter));
                for (const auto& binding : state.iteration_diagnostics.runtime_bindings)
                {
                    const std::string_view bucket = binding.source_bucket.empty()
                                                        ? std::string_view{"untracked"}
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
                        binding.resolved_targets_summary.empty() ? std::string{"(none)"}
                                                                 : binding.resolved_targets_summary,
                        binding.source_reference.empty() ? std::string{"(none)"} : binding.source_reference
                    );
                }
                publish_and_log_command_result(
                    state,
                    services,
                    true,
                    "Dumped " + std::to_string(match_count) + " runtime bindings ("
                        + std::string(diagnostics_filter_label(filter)) + ")"
                );
                return true;
            }

            return false;
        }

        [[nodiscard]] auto
        handle_render_authoring_commands(DemoGameRuntimeState state, RelayCore::EngineServices& services) -> bool
        {
            RelayCore::Renderer& renderer = services.renderer();
            if (!renderer.consume_dump_render_authoring_contract_requested())
            {
                return false;
            }

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

            publish_and_log_command_result(
                state,
                services,
                true,
                "Dumped render authoring contract: " + authoring_config.source_path.string()
            );
            return true;
        }

        void handle_open_inspector_source_command(DemoGameRuntimeState state, RelayCore::EngineServices& services)
        {
            if (!state.runtime.consume_open_inspector_source_requested())
            {
                return;
            }

            RelayCore::Renderer& renderer = services.renderer();
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
    } // namespace

    void handle_debug_iteration_tools(DemoGameRuntimeState state, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::handle_debug_iteration_tools");

        if (state.runtime.consume_reload_level_requested())
        {
            reload_live_content(state, services);
            return;
        }

        if (handle_runtime_snapshot_commands(state, services))
        {
            return;
        }

        if (handle_authoring_warning_commands(state, services))
        {
            return;
        }

        if (handle_runtime_registry_commands(state, services))
        {
            return;
        }

        if (handle_runtime_binding_commands(state, services))
        {
            return;
        }

        if (handle_render_authoring_commands(state, services))
        {
            return;
        }

        handle_open_inspector_source_command(state, services);
    }
} // namespace RelayGame::detail

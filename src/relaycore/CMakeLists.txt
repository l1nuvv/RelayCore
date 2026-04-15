set(RELAYCORE_ENGINE_SOURCES
    app/application.cpp
    app/engine_services.cpp
    app/fly_camera_controller.cpp
    app/frame_timer.cpp
    app/run_application.cpp
    audio/audio_system.cpp
    audio/miniaudio_impl.cpp
    core/engine_config.cpp
    core/log.cpp
    core/paths.cpp
    debug/imgui_overlay.cpp
    debug/imgui_overlay_gameplay_panel.cpp
    debug/imgui_overlay_inspector_panel.cpp
    debug/imgui_overlay_iteration_panel.cpp
    debug/imgui_overlay_summary_panel.cpp
    debug/imgui_overlay_system_panel.cpp
    debug/imgui_overlay_widgets.cpp
    debug/inspector_source_reference.cpp
    gameplay/combat_runtime.cpp
    gameplay/interaction_descriptors.cpp
    gameplay/interaction_runtime_doors.cpp
    gameplay/interaction_runtime_factory.cpp
    gameplay/interaction_runtime_queries.cpp
    gameplay/mission_runtime.cpp
    gameplay/runtime_model.cpp
    physics/collision.cpp
    physics/player_motion.cpp
    platform/sdl_system.cpp
    platform/window.cpp
    render/blockout_mesh_builder.cpp
    render/blockout_texture_atlas.cpp
    render/camera.cpp
    render/game_hud.cpp
    render/gl_debug.cpp
    render/gl_loader.cpp
    render/mesh.cpp
    render/render_authoring_config.cpp
    render/renderer.cpp
    render/shader_program.cpp
    render/shader_source_loader.cpp
    render/texture2d.cpp
    world/blockout_loader.cpp
    world/blockout_loader_parse.cpp
    world/blockout_loader_transform.cpp
    world/blockout_loader_pipeline.cpp
    world/blockout_loader_validation.cpp
    world/surface_material_utils.cpp
)

set(RELAYCORE_ENGINE_HEADERS
    app/app_delegate.h
    app/application.h
    app/engine_services.h
    app/fly_camera_controller.h
    app/frame_timer.h
    app/run_application.h
    audio/audio_system.h
    core/engine_config.h
    core/log.h
    core/paths.h
    core/profiling.h
    core/sdl_error.h
    core/spatial_index_2d.h
    debug/imgui_overlay.h
    debug/imgui_overlay_gameplay_panel.h
    debug/imgui_overlay_inspector_panel.h
    debug/imgui_overlay_iteration_panel.h
    debug/imgui_overlay_summary_panel.h
    debug/imgui_overlay_system_panel.h
    debug/imgui_overlay_widgets.h
    debug/inspector_source_reference.h
    gameplay/combat_runtime.h
    gameplay/entity_spawn_parsing.h
    gameplay/interaction_descriptors.h
    gameplay/interaction_runtime.h
    gameplay/mission_runtime.h
    gameplay/runtime_model.h
    physics/collision.h
    physics/player_motion.h
    platform/sdl_system.h
    platform/window.h
    render/blockout_mesh_builder.h
    render/blockout_texture_atlas.h
    render/camera.h
    render/game_hud.h
    render/gl_debug.h
    render/gl_loader.h
    render/mesh.h
    render/render_authoring_config.h
    render/render_diagnostics.h
    render/render_queue.h
    render/renderer.h
    render/shader_program.h
    render/shader_source_loader.h
    render/texture2d.h
    world/blockout_level.h
    world/blockout_loader.h
    world/blockout_loader_entity_props.h
    world/blockout_loader_validation.h
    world/surface_material_utils.h
)

add_library(relaycore_engine STATIC)
add_library(RelayCore::engine ALIAS relaycore_engine)

target_sources(
    relaycore_engine
    PRIVATE
        ${RELAYCORE_ENGINE_SOURCES}
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${PROJECT_SOURCE_DIR}/src
        FILES ${RELAYCORE_ENGINE_HEADERS}
)

target_compile_features(relaycore_engine PUBLIC cxx_std_20)

target_include_directories(
    relaycore_engine
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/extern/glad/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_definitions(
    relaycore_engine
    PRIVATE
        RELAYCORE_ASSET_DIR="${PROJECT_SOURCE_DIR}/assets"
        RELAYCORE_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
)

if (NOT RELAYCORE_ENABLE_AUDIO)
    target_compile_definitions(relaycore_engine PRIVATE RELAYCORE_AUDIO_DISABLED)
endif()

if (NOT RELAYCORE_ENABLE_CONFIG)
    target_compile_definitions(relaycore_engine PRIVATE RELAYCORE_CONFIG_DISABLED)
endif()

if (NOT RELAYCORE_ENABLE_IMGUI)
    target_compile_definitions(relaycore_engine PRIVATE RELAYCORE_IMGUI_DISABLED)
endif()

target_link_libraries(
    relaycore_engine
    PUBLIC
        RelayCore::project_options
        RelayCore::warnings
        RelayCore::toml_headers
        SDL3::SDL3
        spdlog::spdlog
        glm::glm
        RelayCore::glad
    PRIVATE
        RelayCore::miniaudio_headers
)

if (RELAYCORE_ENABLE_IMGUI)
    target_link_libraries(relaycore_engine PRIVATE ${RELAYCORE_IMGUI_TARGET})
    target_compile_definitions(relaycore_engine PRIVATE RELAYCORE_ENABLE_IMGUI=1)
endif()

if (RELAYCORE_ENABLE_TRACY)
    target_link_libraries(relaycore_engine PRIVATE Tracy::TracyClient)
    target_compile_definitions(relaycore_engine PRIVATE TRACY_ENABLE)
endif()


set_target_properties(relaycore_engine PROPERTIES FOLDER "engine")
relaycore_apply_standard_target_settings(relaycore_engine)

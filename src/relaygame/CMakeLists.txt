set(RELAYCORE_DEMO_SOURCES
        ../relaycore/render/main.cpp
        demo_game.cpp
        demo_game_debug_command_helpers.cpp
        demo_game_debug_commands.cpp
        demo_game_iteration_tools.cpp
        demo_game_render_debug_commands.cpp
        demo_game_runtime_ops.cpp
        demo_game_snapshot_commands.cpp
        demo_game_source_debug_commands.cpp
        game_world_runtime.cpp
        game_world_runtime_diagnostics.cpp
        game_world_runtime_feedback.cpp
        game_world_runtime_update.cpp
        level_session_builder.cpp
        player_controller.cpp
        player_controller_config.cpp
        player_controller_motion.cpp
        world_audio_zones.cpp
        world_inspector.cpp
        world_scene_renderer.cpp
        world_scene_renderer_interactives.cpp
)

set(RELAYCORE_DEMO_HEADERS
        demo_game.h
        demo_game_debug_commands_internal.h
        demo_game_iteration_internal.h
        demo_game_runtime_ops.h
        fixed_simulation_loop.h
        game_world_runtime.h
        level_session_builder.h
        player_controller.h
        player_controller_config.h
        world_audio_zones.h
        world_inspector.h
        world_scene_renderer.h
)

add_executable(relaycore_demo)

target_sources(
        relaycore_demo
        PRIVATE
        ${RELAYCORE_DEMO_SOURCES}
        PRIVATE
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES ${RELAYCORE_DEMO_HEADERS}
)

target_compile_features(relaycore_demo PRIVATE cxx_std_20)

target_include_directories(relaycore_demo PRIVATE ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(
        relaycore_demo
        PRIVATE
        RelayCore::engine
        RelayCore::project_options
        RelayCore::warnings
)

set_target_properties(
        relaycore_demo
        PROPERTIES
        OUTPUT_NAME RelayCore
        FOLDER apps
)

relaycore_apply_standard_target_settings(relaycore_demo)

file(
        GLOB_RECURSE RELAYCORE_DEMO_ASSET_FILES
        CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/assets/*"
)

set(RELAYCORE_DEMO_ASSET_STAMP "${CMAKE_CURRENT_BINARY_DIR}/relaycore_demo_assets.stamp")

add_custom_command(
        OUTPUT "${RELAYCORE_DEMO_ASSET_STAMP}"
        COMMAND
        ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:relaycore_demo>/assets"
        COMMAND
        ${CMAKE_COMMAND} -E copy_directory_if_different
        "${PROJECT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:relaycore_demo>/assets"
        COMMAND
        ${CMAKE_COMMAND} -E touch "${RELAYCORE_DEMO_ASSET_STAMP}"
        DEPENDS
        ${RELAYCORE_DEMO_ASSET_FILES}
        COMMENT
        "Synchronizing runtime assets"
)

add_custom_target(relaycore_demo_assets ALL DEPENDS "${RELAYCORE_DEMO_ASSET_STAMP}")

add_dependencies(relaycore_demo relaycore_demo_assets)

add_custom_command(
        TARGET relaycore_demo
        POST_BUILD
        COMMAND
        ${CMAKE_COMMAND} -E copy -t "$<TARGET_FILE_DIR:relaycore_demo>" $<TARGET_RUNTIME_DLLS:relaycore_demo>
        COMMENT
        "Copying runtime DLLs"
        COMMAND_EXPAND_LISTS
)

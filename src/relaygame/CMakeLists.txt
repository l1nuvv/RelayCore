set(RELAYCORE_DEMO_SOURCES
        ../relaycore/render/main.cpp
        demo_game.cpp
        demo_game_debug_commands.cpp
        demo_game_iteration_tools.cpp
        demo_game_runtime_ops.cpp
        game_world_runtime.cpp
        game_world_runtime_diagnostics.cpp
        game_world_runtime_feedback.cpp
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

add_custom_command(
        TARGET relaycore_demo
        POST_BUILD
        COMMAND
        ${CMAKE_COMMAND} -E copy -t "$<TARGET_FILE_DIR:relaycore_demo>" $<TARGET_RUNTIME_DLLS:relaycore_demo>
        COMMAND
        ${CMAKE_COMMAND} -E copy_directory_if_different
        "${PROJECT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:relaycore_demo>/assets"
        COMMENT
        "Copying runtime assets"
        COMMAND_EXPAND_LISTS
)

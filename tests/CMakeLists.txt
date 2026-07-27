add_executable(relaycore_tests)
add_executable(relaycore_content_validation)
add_executable(relaycore_graphics_baseline)
set_target_properties(relaycore_tests PROPERTIES FOLDER "tests" UNITY_BUILD OFF)
set_target_properties(relaycore_content_validation PROPERTIES FOLDER "tests" UNITY_BUILD OFF)
set_target_properties(relaycore_graphics_baseline PROPERTIES FOLDER "tests" UNITY_BUILD OFF)

target_sources(
    relaycore_tests
    PRIVATE
        ../src/relaygame/level_session_builder.cpp
        ../src/relaygame/player_controller.cpp
        ../src/relaygame/player_controller_motion.cpp
        ../src/relaygame/world_audio_zones.cpp
        ../src/relaygame/world_inspector.cpp
        camera_tests.cpp
        diagnostics_publication_policy_tests.cpp
        audio_system_tests.cpp
        blockout_loader_tests.cpp
        blockout_mesh_tests.cpp
        blockout_texture_atlas_tests.cpp
        fixed_simulation_loop_tests.cpp
        gameplay_input_ownership_tests.cpp
        inspector_source_reference_tests.cpp
        level_session_builder_tests.cpp
        log_tests.cpp
        mission_runtime_tests.cpp
        player_motion_tests.cpp
        render_authoring_config_tests.cpp
        renderer_diagnostics_api_tests.cpp
        scoped_temp_directory_tests.cpp
        runtime_model_tests.cpp
        combat_runtime_tests.cpp
        engine_config_tests.cpp
        shader_source_loader_tests.cpp
        player_controller_tests.cpp
        window_presentation_tests.cpp
        world_audio_zones_tests.cpp
        world_inspector_tests.cpp
        interaction_runtime_tests.cpp
)

target_sources(
    relaycore_graphics_baseline
    PRIVATE
        color_pipeline_baseline.cpp
)

target_sources(
    relaycore_content_validation
    PRIVATE
        ../src/relaygame/game_world_runtime.cpp
        ../src/relaygame/game_world_runtime_diagnostics.cpp
        ../src/relaygame/game_world_runtime_feedback.cpp
        ../src/relaygame/game_world_runtime_update.cpp
        ../src/relaygame/level_session_builder.cpp
        ../src/relaygame/player_controller.cpp
        ../src/relaygame/player_controller_config.cpp
        ../src/relaygame/player_controller_motion.cpp
        ../src/relaygame/world_audio_zones.cpp
        ../src/relaygame/world_inspector.cpp
        content_validation_main.cpp
        shipped_content_validation_tests.cpp
)

target_link_libraries(
    relaycore_tests
    PRIVATE
        RelayCore::engine
        RelayCore::project_options
        RelayCore::warnings
        RelayCore::doctest_headers
)

target_link_libraries(
    relaycore_graphics_baseline
    PRIVATE
        RelayCore::engine
        RelayCore::project_options
        RelayCore::warnings
)

target_link_libraries(
    relaycore_content_validation
    PRIVATE
        RelayCore::engine
        RelayCore::project_options
        RelayCore::warnings
        RelayCore::doctest_headers
)

find_package(doctest CONFIG REQUIRED)
include(doctest)

foreach(test_target IN ITEMS relaycore_tests relaycore_content_validation relaycore_graphics_baseline)
    relaycore_apply_standard_target_settings(${test_target})

    add_custom_command(
        TARGET ${test_target}
        POST_BUILD
        COMMAND
        ${CMAKE_COMMAND} -E copy -t "$<TARGET_FILE_DIR:${test_target}>" $<TARGET_RUNTIME_DLLS:${test_target}>
        COMMENT
        "Copying runtime DLLs"
        COMMAND_EXPAND_LISTS
    )
endforeach()

add_test(
    NAME graphics.color_pipeline_baseline
    COMMAND relaycore_graphics_baseline
)
set_tests_properties(
    graphics.color_pipeline_baseline
    PROPERTIES
        LABELS "integration;graphics"
        RUN_SERIAL TRUE
        SKIP_RETURN_CODE 77
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_graphics_baseline>"
        ENVIRONMENT "RELAYCORE_ASSET_ROOT=${PROJECT_SOURCE_DIR}/assets"
)

doctest_discover_tests(
    relaycore_tests
    TEST_PREFIX "relaycore."
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_tests>"
    PROPERTIES ENVIRONMENT "RELAYCORE_ASSET_ROOT=${PROJECT_SOURCE_DIR}/assets"
)

doctest_discover_tests(
    relaycore_content_validation
    TEST_PREFIX "validation."
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_content_validation>"
    PROPERTIES ENVIRONMENT "RELAYCORE_ASSET_ROOT=${PROJECT_SOURCE_DIR}/assets"
)

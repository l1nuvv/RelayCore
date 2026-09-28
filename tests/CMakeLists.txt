add_executable(relaycore_tests)
if(WIN32)
    file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}/tools/updater/RelayCoreUpdater.cs" updater_source)
    file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/updater_tests.cs" updater_test_source)
    file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/relaycore_updater_tests.exe" updater_test_output)
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/relaycore_updater_tests.exe
        COMMAND "${RELAYCORE_CSC}" /nologo /target:exe /main:UpdaterTests /optimize+
            /reference:System.Net.Http.dll /reference:System.IO.Compression.dll
            /reference:System.IO.Compression.FileSystem.dll /reference:System.Web.Extensions.dll
            /reference:System.Windows.Forms.dll /reference:System.Drawing.dll
            "/out:${updater_test_output}"
            "${updater_source}"
            "${updater_test_source}"
        DEPENDS ${PROJECT_SOURCE_DIR}/tools/updater/RelayCoreUpdater.cs ${CMAKE_CURRENT_SOURCE_DIR}/updater_tests.cs
        VERBATIM)
    add_custom_target(relaycore_updater_tests DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/relaycore_updater_tests.exe)
    add_dependencies(relaycore_tests relaycore_updater_tests)
    add_test(NAME relaycore.updater COMMAND ${CMAKE_CURRENT_BINARY_DIR}/relaycore_updater_tests.exe)
endif()
add_executable(relaycore_content_validation)
add_executable(relaycore_audio_lifetime audio_lifetime_tests.cpp)
target_link_libraries(relaycore_audio_lifetime PRIVATE
    RelayCore::engine RelayCore::project_options RelayCore::warnings
)
set_target_properties(relaycore_audio_lifetime PROPERTIES FOLDER "tests" UNITY_BUILD OFF)
add_executable(relaycore_game_lifecycle game_lifecycle_tests.cpp)
target_link_libraries(relaycore_game_lifecycle PRIVATE RelayGame::runtime RelayCore::project_options RelayCore::warnings)
add_executable(relaycore_episode_profile EXCLUDE_FROM_ALL episode_profile.cpp)
target_link_libraries(relaycore_episode_profile PRIVATE RelayGame::runtime RelayCore::project_options RelayCore::warnings)
set_target_properties(relaycore_episode_profile PROPERTIES FOLDER "tests" UNITY_BUILD OFF)
add_executable(relaycore_graphics_baseline)
add_executable(relaycore_window_lifetime_tests window_lifetime_tests.cpp)
add_executable(relaycore_power_restoration_graphics
    power_restoration_graphics.cpp
)
target_link_libraries(relaycore_power_restoration_graphics PRIVATE
    RelayGame::runtime RelayCore::project_options RelayCore::warnings
)
set_target_properties(relaycore_power_restoration_graphics PROPERTIES FOLDER "tests" UNITY_BUILD OFF)
target_include_directories(relaycore_power_restoration_graphics PRIVATE ${PROJECT_BINARY_DIR}/src/relaygame/generated)
set_target_properties(relaycore_tests PROPERTIES FOLDER "tests" UNITY_BUILD OFF)
set_target_properties(relaycore_content_validation PROPERTIES FOLDER "tests" UNITY_BUILD OFF)
set_target_properties(relaycore_graphics_baseline PROPERTIES FOLDER "tests" UNITY_BUILD OFF)
set_target_properties(relaycore_window_lifetime_tests PROPERTIES FOLDER "tests" UNITY_BUILD OFF)

target_sources(
    relaycore_tests
    PRIVATE
        world_environment_tests.cpp
        static_model_tests.cpp
        enemy_runtime_tests.cpp
        save_game_tests.cpp
        level_editor_tests.cpp
        camera_tests.cpp
        diagnostics_publication_policy_tests.cpp
        audio_system_tests.cpp
        authored_color_tests.cpp
        startup_options_tests.cpp
        blockout_loader_tests.cpp
        blockout_mesh_tests.cpp
        blockout_texture_atlas_tests.cpp
        fixed_simulation_loop_tests.cpp
        gameplay_input_ownership_tests.cpp
        inspector_source_reference_tests.cpp
        level_session_builder_tests.cpp
        local_light_order_tests.cpp
        log_tests.cpp
        mission_runtime_tests.cpp
        player_motion_tests.cpp
        player_experience_tests.cpp
        render_authoring_config_tests.cpp
        pipeline_authoring_overlay_tests.cpp


        renderer_diagnostics_api_tests.cpp
        scoped_temp_directory_tests.cpp
        spatial_index_2d_tests.cpp
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
        content_validation_main.cpp
        shipped_content_validation_tests.cpp
)

target_link_libraries(
    relaycore_tests
    PRIVATE
        RelayGame::runtime
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
        RelayGame::runtime
        RelayCore::engine
        RelayCore::project_options
        RelayCore::warnings
        RelayCore::doctest_headers
)

find_package(doctest CONFIG REQUIRED)
include(doctest)

target_link_libraries(relaycore_window_lifetime_tests PRIVATE RelayCore::engine)

add_executable(relaycore_default_framebuffer_probe default_framebuffer_srgb_probe.cpp)
target_link_libraries(relaycore_default_framebuffer_probe PRIVATE
    RelayCore::engine RelayCore::project_options RelayCore::warnings
)
set_target_properties(relaycore_default_framebuffer_probe PROPERTIES FOLDER "tests" UNITY_BUILD OFF)

foreach(test_target IN ITEMS
    relaycore_tests
    relaycore_audio_lifetime
    relaycore_content_validation
    relaycore_graphics_baseline
    relaycore_window_lifetime_tests
    relaycore_power_restoration_graphics
    relaycore_game_lifecycle
    relaycore_episode_profile
    relaycore_default_framebuffer_probe
)
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

add_test(NAME relaycore.audio_lifetime COMMAND relaycore_audio_lifetime)
set_tests_properties(relaycore.audio_lifetime PROPERTIES
    LABELS "integration;audio"
    RUN_SERIAL TRUE
    SKIP_RETURN_CODE 77
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_audio_lifetime>"
    ENVIRONMENT "RELAYCORE_ASSET_ROOT=${PROJECT_SOURCE_DIR}/assets"
)

add_test(NAME graphics.window_lifetime COMMAND relaycore_window_lifetime_tests)
add_test(NAME graphics.game_lifecycle COMMAND relaycore_game_lifecycle)
set_tests_properties(graphics.game_lifecycle PROPERTIES
    LABELS "integration;graphics"
    RUN_SERIAL TRUE
    SKIP_RETURN_CODE 77
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_game_lifecycle>"
    ENVIRONMENT "RELAYCORE_ASSET_ROOT=${PROJECT_SOURCE_DIR}/assets"
)
add_test(NAME graphics.power_restoration COMMAND relaycore_power_restoration_graphics)
set_tests_properties(graphics.power_restoration PROPERTIES
    LABELS "integration;graphics"
    RUN_SERIAL TRUE
    SKIP_RETURN_CODE 77
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_power_restoration_graphics>"
    ENVIRONMENT "RELAYCORE_ASSET_ROOT=${PROJECT_SOURCE_DIR}/assets"
)
set_tests_properties(
    graphics.window_lifetime
    PROPERTIES
        LABELS "integration;graphics"
        RUN_SERIAL TRUE
        SKIP_RETURN_CODE 77
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_window_lifetime_tests>"
        ENVIRONMENT "RELAYCORE_ASSET_ROOT=${PROJECT_SOURCE_DIR}/assets"
)

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

add_test(
    NAME graphics.default_framebuffer_srgb_probe
    COMMAND relaycore_default_framebuffer_probe
)

set_tests_properties(
    graphics.default_framebuffer_srgb_probe
    PROPERTIES
        LABELS "integration;graphics"
        RUN_SERIAL TRUE
        SKIP_RETURN_CODE 77
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_default_framebuffer_probe>"
)

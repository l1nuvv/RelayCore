add_executable(relaycore_tests)
set_target_properties(relaycore_tests PROPERTIES FOLDER "tests")

target_sources(
    relaycore_tests
    PRIVATE
        ../src/relaygame/level_session_builder.cpp
        camera_tests.cpp
        blockout_loader_tests.cpp
        blockout_mesh_tests.cpp
        blockout_texture_atlas_tests.cpp
        level_session_builder_tests.cpp
        log_tests.cpp
        mission_runtime_tests.cpp
        player_motion_tests.cpp
        interaction_runtime_tests.cpp
)

target_link_libraries(
    relaycore_tests
    PRIVATE
        RelayCore::engine
        RelayCore::project_options
        RelayCore::warnings
        RelayCore::doctest_headers
)

relaycore_apply_standard_target_settings(relaycore_tests)

find_package(doctest CONFIG REQUIRED)
include(doctest)

add_custom_command(
    TARGET relaycore_tests
    POST_BUILD
    COMMAND
    ${CMAKE_COMMAND} -E copy -t "$<TARGET_FILE_DIR:relaycore_tests>" $<TARGET_RUNTIME_DLLS:relaycore_tests>
    COMMENT
    "Copying runtime DLLs"
    COMMAND_EXPAND_LISTS
)

doctest_discover_tests(
    relaycore_tests
    TEST_PREFIX "relaycore."
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_tests>"
)

add_executable(relaycore_tests)
set_target_properties(relaycore_tests PROPERTIES FOLDER "tests")

target_sources(
    relaycore_tests
    PRIVATE
        camera_tests.cpp
        blockout_loader_tests.cpp
        blockout_mesh_tests.cpp
        blockout_texture_atlas_tests.cpp
        log_tests.cpp
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

add_custom_command(
    TARGET relaycore_tests
    POST_BUILD
    COMMAND
    ${CMAKE_COMMAND} -E copy -t "$<TARGET_FILE_DIR:relaycore_tests>" $<TARGET_RUNTIME_DLLS:relaycore_tests>
    COMMENT
    "Copying runtime DLLs"
    COMMAND_EXPAND_LISTS
)

add_test(NAME relaycore.camera COMMAND relaycore_tests)
set_tests_properties(relaycore.camera PROPERTIES LABELS "unit;math;camera")

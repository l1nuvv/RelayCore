add_executable(relaycore_tests)
set_target_properties(relaycore_tests PROPERTIES FOLDER "tests")

target_sources(relaycore_tests PRIVATE camera_tests.cpp blockout_mesh_tests.cpp player_motion_tests.cpp)

target_link_libraries(
    relaycore_tests
    PRIVATE
        RelayCore::engine
        RelayCore::project_options
        RelayCore::warnings
        RelayCore::doctest_headers
)

relaycore_apply_standard_target_settings(relaycore_tests)

add_test(NAME relaycore.camera COMMAND relaycore_tests)
set_tests_properties(relaycore.camera PROPERTIES LABELS "unit;math;camera")

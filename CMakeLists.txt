cmake_minimum_required(VERSION 4.0...4.3)

project(
    RelayCore
    VERSION 0.1.0
    DESCRIPTION "Retro FPS engine prototype"
    LANGUAGES C CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(RelayCoreOptions)
include(RelayCoreDependencies)

relaycore_initialize_project()
relaycore_find_dependencies()
relaycore_create_dependency_targets()

add_subdirectory(src/relaycore)
add_subdirectory(src/relaygame)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

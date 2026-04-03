add_executable(relaycore_demo)

target_sources(
    relaycore_demo
    PRIVATE
        ../relaycore/render/main.cpp
        demo_game.cpp
        demo_game.h
)

target_include_directories(relaycore_demo PRIVATE ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(
    relaycore_demo
    PRIVATE
        RelayCore::engine
        RelayCore::project_options
        RelayCore::warnings
        SDL3::SDL3
)

set_target_properties(
    relaycore_demo
    PROPERTIES
        OUTPUT_NAME RelayCore
        FOLDER "apps"
)

relaycore_apply_standard_target_settings(relaycore_demo)

add_custom_command(
    TARGET relaycore_demo
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${PROJECT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:relaycore_demo>/assets"
    COMMENT "Copying runtime assets"
)

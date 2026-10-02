set(RELAYCORE_DEMO_SOURCES
        demo_game.cpp
        demo_game_session.cpp
        demo_game_transition.cpp
        mission_runtime.cpp
        runtime_model.cpp
        usable_interaction.cpp
        demo_game_menu.cpp
        game_updates.cpp
        demo_game_progress.cpp
        save_game.cpp
        player_feedback.cpp
        player_settings.cpp
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
        enemy_runtime.cpp
        static_props.cpp
        level_editor.cpp
        level_editor_document.cpp
        level_editor_authoring.cpp
        level_editor_tools.cpp
        player_controller.cpp
        player_controller_config.cpp
        player_controller_motion.cpp
        world_audio_zones.cpp
        world_environment.cpp
        world_scene_renderer_environment.cpp
        world_inspector.cpp
        world_scene_renderer.cpp
        world_irradiance.cpp
        world_scene_renderer_interactives.cpp
        world_scene_renderer_props.cpp
)

set(RELAYCORE_DEMO_HEADERS
        demo_game.h
        game_updates.h
        mission_runtime.h
        runtime_model.h
        usable_interaction.h
        save_game.h
        player_feedback.h
        player_weapon.h
        player_settings.h
        demo_game_debug_commands_internal.h
        demo_game_iteration_internal.h
        demo_game_runtime_ops.h
        fixed_simulation_loop.h
        gameplay_input_ownership.h
        game_world_runtime.h
        level_session_builder.h
        level_editor.h
        level_editor_document.h
        player_controller.h
        player_controller_config.h
        world_audio_zones.h
        world_inspector.h
        world_scene_renderer.h
        world_irradiance.h
        scientist_pose.h
)

add_library(relaygame_runtime STATIC)
add_library(RelayGame::runtime ALIAS relaygame_runtime)
add_executable(relaycore_demo main.cpp)

target_sources(
        relaygame_runtime
        PRIVATE
        ${RELAYCORE_DEMO_SOURCES}
        PRIVATE
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES ${RELAYCORE_DEMO_HEADERS}
)

target_compile_features(relaygame_runtime PUBLIC cxx_std_20)
configure_file(game_version.h.in generated/relaygame/game_version.h @ONLY)
target_include_directories(relaygame_runtime PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)

target_include_directories(relaygame_runtime PUBLIC ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(
        relaygame_runtime
        PUBLIC
        RelayCore::engine
        PRIVATE
        RelayCore::project_options
        RelayCore::warnings
)

target_link_libraries(relaycore_demo PRIVATE RelayGame::runtime RelayCore::project_options RelayCore::warnings)

if (RELAYCORE_ENABLE_TRACY)
    target_link_libraries(relaygame_runtime PUBLIC Tracy::TracyClient)
    target_compile_definitions(relaygame_runtime PUBLIC TRACY_ENABLE)
endif()

set_target_properties(
        relaycore_demo
        PROPERTIES
        OUTPUT_NAME RelayCore
        FOLDER apps
)

relaycore_apply_standard_target_settings(relaycore_demo)
relaycore_apply_standard_target_settings(relaygame_runtime)
set_target_properties(relaygame_runtime PROPERTIES FOLDER "game")

add_executable(relaycore_irradiance_bake ${PROJECT_SOURCE_DIR}/tools/irradiance_bake_main.cpp)
target_link_libraries(relaycore_irradiance_bake PRIVATE RelayGame::runtime)
relaycore_apply_standard_target_settings(relaycore_irradiance_bake)
set_target_properties(relaycore_irradiance_bake PROPERTIES FOLDER "tools")
add_custom_command(TARGET relaycore_irradiance_bake POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy -t "$<TARGET_FILE_DIR:relaycore_irradiance_bake>" $<TARGET_RUNTIME_DLLS:relaycore_irradiance_bake>
    COMMAND_EXPAND_LISTS VERBATIM)
include(${PROJECT_SOURCE_DIR}/cmake/RelayCoreIrradianceAssets.cmake)

add_custom_target(
        relaycore_demo_assets ALL
        COMMAND
        ${CMAKE_COMMAND}
        "-DRELAYCORE_ASSET_SOURCE=${PROJECT_SOURCE_DIR}/assets"
        "-DRELAYCORE_GENERATED_ASSETS=${RELAYCORE_MODEL_ASSET_ROOT}"
        "-DRELAYCORE_GENERATED_ASSET_MANIFEST=${PROJECT_BINARY_DIR}/generated/runtime-assets.txt"
        "-DRELAYCORE_ASSET_OUTPUT=$<TARGET_FILE_DIR:relaycore_demo>/assets"
        "-DRELAYCORE_ASSET_MANIFEST=${CMAKE_CURRENT_BINARY_DIR}/relaycore_demo_assets.manifest"
        -P "${PROJECT_SOURCE_DIR}/cmake/sync_runtime_assets.cmake"
        COMMENT
        "Synchronizing runtime assets"
        VERBATIM
)

add_dependencies(relaycore_demo relaycore_demo_assets)
add_dependencies(relaycore_demo_assets relaycore_vulkan_shaders)
add_dependencies(relaycore_demo_assets relaycore_model_assets)
add_dependencies(relaycore_demo_assets relaycore_irradiance_assets)
add_custom_command(TARGET relaycore_demo_assets POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RELAYCORE_MESH_SPIRV}
        "$<TARGET_FILE_DIR:relaycore_demo>/assets/shaders/vulkan"
    VERBATIM)
add_dependencies(relaycore_demo relaycore_vulkan_shaders)

add_executable(relaycore_level_check EXCLUDE_FROM_ALL ${PROJECT_SOURCE_DIR}/tools/level_check.cpp)
target_link_libraries(relaycore_level_check PRIVATE RelayGame::runtime RelayCore::toml_headers
    RelayCore::project_options RelayCore::warnings)
add_executable(relaycore_sandbox EXCLUDE_FROM_ALL ${PROJECT_SOURCE_DIR}/tools/sandbox_main.cpp)
target_link_libraries(relaycore_sandbox PRIVATE RelayGame::runtime RelayCore::project_options RelayCore::warnings)
foreach(development_target IN ITEMS relaycore_level_check relaycore_sandbox)
    relaycore_apply_standard_target_settings(${development_target})
    set_target_properties(${development_target} PROPERTIES FOLDER "tools")
    add_dependencies(${development_target} relaycore_demo_assets)
    add_custom_command(TARGET ${development_target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy -t "$<TARGET_FILE_DIR:${development_target}>"
            $<TARGET_RUNTIME_DLLS:${development_target}>
        COMMAND_EXPAND_LISTS)
endforeach()

if(WIN32)
    set(RELAYCORE_UPDATE_REPOSITORY "l1nuvv/RelayCore-releases" CACHE STRING "Public GitHub repository for game releases")
    if(NOT RELAYCORE_UPDATE_REPOSITORY MATCHES "^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$")
        message(FATAL_ERROR "RELAYCORE_UPDATE_REPOSITORY must be owner/repository")
    endif()
    configure_file(${PROJECT_SOURCE_DIR}/tools/updater/updates.json.in ${CMAKE_CURRENT_BINARY_DIR}/updates.json @ONLY)
    find_program(RELAYCORE_CSC NAMES csc.exe HINTS "$ENV{WINDIR}/Microsoft.NET/Framework64/v4.0.30319" REQUIRED)
    file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}/tools/updater/RelayCoreUpdater.cs" updater_source)
    file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/RelayCoreUpdater.exe" updater_output)
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/RelayCoreUpdater.exe
        COMMAND "${RELAYCORE_CSC}" /nologo /target:winexe /optimize+
            /reference:System.Net.Http.dll /reference:System.IO.Compression.dll
            /reference:System.IO.Compression.FileSystem.dll /reference:System.Web.Extensions.dll
            /reference:System.Windows.Forms.dll /reference:System.Drawing.dll
            "/out:${updater_output}"
            "${updater_source}"
        DEPENDS ${PROJECT_SOURCE_DIR}/tools/updater/RelayCoreUpdater.cs VERBATIM)
    add_custom_target(relaycore_updater DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/RelayCoreUpdater.exe)
    add_dependencies(relaycore_demo relaycore_updater)
endif()

add_custom_command(
        TARGET relaycore_demo
        POST_BUILD
        COMMAND
        ${CMAKE_COMMAND} -E copy -t "$<TARGET_FILE_DIR:relaycore_demo>" $<TARGET_RUNTIME_DLLS:relaycore_demo>
        COMMENT
        "Copying runtime DLLs"
        COMMAND_EXPAND_LISTS
)

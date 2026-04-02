add_library(relaycore_engine STATIC)
add_library(RelayCore::engine ALIAS relaycore_engine)

target_sources(
        relaycore_engine
        PRIVATE
        app/application.cpp
        app/engine_services.cpp
        app/fly_camera_controller.cpp
        app/frame_timer.cpp
        app/run_application.cpp
        audio/audio_system.cpp
        audio/miniaudio_impl.cpp
        core/engine_config.cpp
        core/log.cpp
        core/paths.cpp
        debug/imgui_overlay.cpp
        platform/sdl_system.cpp
        platform/window.cpp
        render/camera.cpp
        render/gl_debug.cpp
        render/gl_loader.cpp
        render/mesh.cpp
        render/renderer.cpp
        render/shader_program.cpp
        world/blockout_loader.cpp
        PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${PROJECT_SOURCE_DIR}/src
        FILES
        app/app_delegate.hpp
        app/application.hpp
        app/engine_services.hpp
        app/fly_camera_controller.hpp
        app/frame_timer.hpp
        app/run_application.hpp
        audio/audio_system.hpp
        core/engine_config.hpp
        core/log.hpp
        core/paths.hpp
        core/profiling.hpp
        core/sdl_error.hpp
        debug/imgui_overlay.hpp
        platform/sdl_system.hpp
        platform/window.hpp
        render/camera.hpp
        render/gl_debug.hpp
        render/gl_loader.hpp
        render/mesh.hpp
        render/renderer.hpp
        render/shader_program.hpp
        world/blockout_level.hpp
        world/blockout_loader.hpp
)

target_include_directories(
        relaycore_engine
        PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/extern/glad/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_definitions(relaycore_engine PRIVATE RELAYCORE_ASSET_DIR="${PROJECT_SOURCE_DIR}/assets")

if (NOT RELAYCORE_ENABLE_AUDIO)
    target_compile_definitions(relaycore_engine PRIVATE RELAYCORE_AUDIO_DISABLED)
endif ()

if (NOT RELAYCORE_ENABLE_CONFIG)
    target_compile_definitions(relaycore_engine PRIVATE RELAYCORE_CONFIG_DISABLED)
endif ()

if (NOT RELAYCORE_ENABLE_IMGUI)
    target_compile_definitions(relaycore_engine PRIVATE RELAYCORE_IMGUI_DISABLED)
endif ()

target_link_libraries(
        relaycore_engine
        PUBLIC
        RelayCore::project_options
        RelayCore::warnings
        RelayCore::toml_headers
        SDL3::SDL3
        spdlog::spdlog
        glm::glm
        RelayCore::glad
        PRIVATE
        RelayCore::miniaudio_headers
)

if (RELAYCORE_ENABLE_IMGUI)
    target_link_libraries(relaycore_engine PRIVATE ${RELAYCORE_IMGUI_TARGET})
    target_compile_definitions(relaycore_engine PRIVATE RELAYCORE_ENABLE_IMGUI=1)
endif ()

if (RELAYCORE_ENABLE_TRACY)
    target_link_libraries(relaycore_engine PRIVATE Tracy::TracyClient)
    target_compile_definitions(relaycore_engine PRIVATE TRACY_ENABLE)
endif ()

set_target_properties(relaycore_engine PROPERTIES FOLDER "engine")
relaycore_apply_standard_target_settings(relaycore_engine)

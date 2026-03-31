cmake_minimum_required(VERSION 4.0...4.3)

project(
    RelayCore
    VERSION 0.1.0
    DESCRIPTION "Retro FPS engine prototype"
    LANGUAGES C CXX
)

include(CheckIPOSupported)

if(PROJECT_IS_TOP_LEVEL)
    include(CTest)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MAP_IMPORTED_CONFIG_ASAN Debug)

if(CMAKE_CONFIGURATION_TYPES)
    list(APPEND CMAKE_CONFIGURATION_TYPES Asan)
    list(REMOVE_DUPLICATES CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "Supported build configurations" FORCE)
else()
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel Asan)
endif()

option(RELAYCORE_ENABLE_IPO "Enable IPO/LTO for optimized builds" ON)
option(RELAYCORE_ENABLE_ASAN "Enable AddressSanitizer flags for Asan builds" ON)
option(RELAYCORE_ENABLE_PCH "Enable precompiled headers for RelayCore targets" OFF)
option(RELAYCORE_ENABLE_TRACY "Enable Tracy instrumentation and linking" OFF)
option(RELAYCORE_ENABLE_MSVC_ANALYZE "Enable MSVC /analyze static analysis" OFF)
option(RELAYCORE_ENABLE_IMGUI "Enable Dear ImGui debug overlay" ON)
option(RELAYCORE_ENABLE_AUDIO "Enable miniaudio audio system" ON)
option(RELAYCORE_ENABLE_CONFIG "Enable TOML engine config loading" ON)

check_ipo_supported(
    RESULT RELAYCORE_IPO_SUPPORTED
    OUTPUT RELAYCORE_IPO_OUTPUT
    LANGUAGES C CXX
)

find_package(SDL3 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

if(RELAYCORE_ENABLE_TRACY)
    find_package(Tracy CONFIG REQUIRED)
endif()

if(RELAYCORE_ENABLE_IMGUI)
    find_package(imgui CONFIG REQUIRED)
endif()

find_path(RELAYCORE_TOMLPLUSPLUS_INCLUDE_DIR toml++/toml.hpp REQUIRED)
find_path(RELAYCORE_MINIAUDIO_INCLUDE_DIR miniaudio.h REQUIRED)
if(BUILD_TESTING)
    find_path(RELAYCORE_DOCTEST_INCLUDE_DIR doctest/doctest.h REQUIRED)
endif()

function(relaycore_find_first_target out_var)
    set(found_target "")
    foreach(candidate IN LISTS ARGN)
        if(TARGET "${candidate}")
            set(found_target "${candidate}")
            break()
        endif()
    endforeach()
    set(${out_var} "${found_target}" PARENT_SCOPE)
endfunction()

if(RELAYCORE_ENABLE_IMGUI)
    relaycore_find_first_target(
        RELAYCORE_IMGUI_TARGET
        Unofficial::DearImGui::imgui
        unofficial::imgui::imgui
        imgui::imgui
        imgui
    )

    if(RELAYCORE_IMGUI_TARGET STREQUAL "")
        message(FATAL_ERROR "Could not resolve the imported target for imgui. Check your vcpkg installation and imgui features.")
    endif()
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extern/glad/include/glad/gl.h")
    message(FATAL_ERROR "Vendored glad headers were not found in extern/glad/include")
endif()
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extern/glad/src/gl.c")
    message(FATAL_ERROR "Vendored glad source was not found in extern/glad/src/gl.c")
endif()

add_library(relaycore_project_options INTERFACE)
add_library(RelayCore::project_options ALIAS relaycore_project_options)

target_compile_features(relaycore_project_options INTERFACE c_std_11 cxx_std_20)

target_compile_definitions(relaycore_project_options INTERFACE
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:Asan>>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE>
    $<$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:Asan>>>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO>
)

add_library(relaycore_warnings INTERFACE)
add_library(RelayCore::warnings ALIAS relaycore_warnings)

if(MSVC)
    target_compile_options(relaycore_project_options INTERFACE
        /permissive-
        /EHsc
        /Zc:preprocessor
        /Zc:__cplusplus
        /utf-8
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:RelWithDebInfo>:/O2>
        $<$<CONFIG:Asan>:/Od>
        $<$<CONFIG:Asan>:/Zi>
    )

    target_link_options(relaycore_project_options INTERFACE
        $<$<CONFIG:Asan>:/DEBUG>
        $<$<CONFIG:Asan>:/INCREMENTAL:NO>
    )

    target_compile_definitions(relaycore_project_options INTERFACE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        $<$<CONFIG:Asan>:_DEBUG>
    )

    target_compile_options(relaycore_warnings INTERFACE
        /W4
        /external:anglebrackets
        /external:W0
    )
else()
    target_compile_options(relaycore_project_options INTERFACE
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:RelWithDebInfo>:-O3>
        $<$<CONFIG:Asan>:-O1>
        $<$<CONFIG:Asan>:-g>
        $<$<CONFIG:Asan>:-fsanitize=address>
    )

    target_link_options(relaycore_project_options INTERFACE
        $<$<CONFIG:Asan>:-fsanitize=address>
    )

    target_compile_options(relaycore_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wshadow
    )
endif()

add_library(relaycore_glad STATIC)
add_library(RelayCore::glad ALIAS relaycore_glad)

target_sources(relaycore_glad
    PRIVATE
        extern/glad/src/gl.c
)

target_include_directories(relaycore_glad
    SYSTEM PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/extern/glad/include>
)

target_link_libraries(relaycore_glad
    PUBLIC
        RelayCore::project_options
        RelayCore::warnings
)

set_target_properties(relaycore_glad PROPERTIES UNITY_BUILD OFF)

add_library(relaycore_toml_headers INTERFACE)
add_library(RelayCore::toml_headers ALIAS relaycore_toml_headers)
target_include_directories(relaycore_toml_headers SYSTEM INTERFACE ${RELAYCORE_TOMLPLUSPLUS_INCLUDE_DIR})

add_library(relaycore_miniaudio_headers INTERFACE)
add_library(RelayCore::miniaudio_headers ALIAS relaycore_miniaudio_headers)
target_include_directories(relaycore_miniaudio_headers SYSTEM INTERFACE ${RELAYCORE_MINIAUDIO_INCLUDE_DIR})

if(BUILD_TESTING)
    add_library(relaycore_doctest_headers INTERFACE)
    add_library(RelayCore::doctest_headers ALIAS relaycore_doctest_headers)
    target_include_directories(relaycore_doctest_headers SYSTEM INTERFACE ${RELAYCORE_DOCTEST_INCLUDE_DIR})
endif()

add_library(relaycore_engine STATIC)
add_library(RelayCore::engine ALIAS relaycore_engine)

target_sources(relaycore_engine
    PRIVATE
        src/app/application.cpp
        src/app/fly_camera_controller.cpp
        src/app/frame_timer.cpp
        src/audio/audio_system.cpp
        src/audio/miniaudio_impl.cpp
        src/core/engine_config.cpp
        src/core/log.cpp
        src/core/paths.cpp
        src/debug/imgui_overlay.cpp
        src/platform/sdl_system.cpp
        src/platform/window.cpp
        src/render/camera.cpp
        src/render/gl_debug.cpp
        src/render/gl_loader.cpp
        src/render/mesh.cpp
        src/render/renderer.cpp
        src/render/shader_program.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS src
        FILES
            src/app/app_delegate.hpp
            src/app/application.hpp
            src/app/fly_camera_controller.hpp
            src/app/frame_timer.hpp
            src/audio/audio_system.hpp
            src/core/engine_config.hpp
            src/core/log.hpp
            src/core/paths.hpp
            src/core/profiling.hpp
            src/core/sdl_error.hpp
            src/debug/imgui_overlay.hpp
            src/platform/sdl_system.hpp
            src/platform/window.hpp
            src/render/camera.hpp
            src/render/gl_debug.hpp
            src/render/gl_loader.hpp
            src/render/mesh.hpp
            src/render/renderer.hpp
            src/render/shader_program.hpp
)

target_include_directories(relaycore_engine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

target_compile_definitions(relaycore_engine
    PRIVATE
        RELAYCORE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

if(NOT RELAYCORE_ENABLE_AUDIO)
    target_compile_definitions(relaycore_engine PRIVATE RELAYCORE_AUDIO_DISABLED)
endif()
if(NOT RELAYCORE_ENABLE_CONFIG)
    target_compile_definitions(relaycore_engine PRIVATE RELAYCORE_CONFIG_DISABLED)
endif()
if(NOT RELAYCORE_ENABLE_IMGUI)
    target_compile_definitions(relaycore_engine PRIVATE RELAYCORE_IMGUI_DISABLED)
endif()

target_link_libraries(relaycore_engine
    PUBLIC
        RelayCore::project_options
        RelayCore::warnings
        RelayCore::toml_headers
        SDL3::SDL3
        spdlog::spdlog
        glm::glm
    PUBLIC
        RelayCore::glad
    PRIVATE
        RelayCore::miniaudio_headers
)

if(RELAYCORE_ENABLE_IMGUI)
    target_link_libraries(relaycore_engine PRIVATE ${RELAYCORE_IMGUI_TARGET})
    target_compile_definitions(relaycore_engine PRIVATE RELAYCORE_ENABLE_IMGUI=1)
endif()

if(RELAYCORE_ENABLE_TRACY)
    target_link_libraries(relaycore_engine PRIVATE Tracy::TracyClient)
    target_compile_definitions(relaycore_engine PRIVATE TRACY_ENABLE)
endif()

add_executable(RelayCore)
set_target_properties(relaycore_glad PROPERTIES FOLDER "third_party")
set_target_properties(relaycore_engine PROPERTIES FOLDER "engine")
set_target_properties(RelayCore PROPERTIES FOLDER "apps")
target_sources(RelayCore PRIVATE src/main.cpp)

target_link_libraries(RelayCore
        PRIVATE
        RelayCore::engine
        SDL3::SDL3
)

if(MSVC)
    foreach(relaycore_target IN ITEMS relaycore_glad relaycore_engine RelayCore)
        set_property(TARGET ${relaycore_target} PROPERTY MSVC_RUNTIME_LIBRARY
            "MultiThreaded$<$<OR:$<CONFIG:Debug>,$<CONFIG:Asan>>:Debug>DLL")
    endforeach()
endif()

if(RELAYCORE_ENABLE_ASAN)
    if(MSVC)
        foreach(relaycore_target IN ITEMS relaycore_glad relaycore_engine RelayCore)
            target_compile_options(${relaycore_target} PRIVATE
                $<$<CONFIG:Asan>:/fsanitize=address>
            )
            target_link_options(${relaycore_target} PRIVATE
                $<$<CONFIG:Asan>:/fsanitize=address>
            )
        endforeach()
    endif()
endif()

if(MSVC AND RELAYCORE_ENABLE_MSVC_ANALYZE)
    foreach(relaycore_target IN ITEMS relaycore_engine RelayCore)
        target_compile_options(${relaycore_target} PRIVATE /analyze)
    endforeach()
endif()

if(RELAYCORE_ENABLE_IPO AND RELAYCORE_IPO_SUPPORTED)
    set_property(TARGET relaycore_glad PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    set_property(TARGET relaycore_glad PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
    set_property(TARGET relaycore_engine PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    set_property(TARGET relaycore_engine PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
    set_property(TARGET RelayCore PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    set_property(TARGET RelayCore PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
elseif(RELAYCORE_ENABLE_IPO)
    message(STATUS "IPO/LTO not enabled: ${RELAYCORE_IPO_OUTPUT}")
endif()

if(RELAYCORE_ENABLE_PCH)
    set(RELAYCORE_PCH_HEADERS
        <array>
        <chrono>
        <cmath>
        <cstdint>
        <filesystem>
        <memory>
        <optional>
        <span>
        <stdexcept>
        <string>
        <string_view>
        <utility>
        <vector>
        <SDL3/SDL.h>
        <glm/glm.hpp>
        <spdlog/spdlog.h>
    )

    target_precompile_headers(relaycore_engine PRIVATE ${RELAYCORE_PCH_HEADERS})
    target_precompile_headers(RelayCore PRIVATE ${RELAYCORE_PCH_HEADERS})
endif()

if(BUILD_TESTING)
    add_executable(relaycore_tests)
    set_target_properties(relaycore_tests PROPERTIES FOLDER "tests")

    target_sources(relaycore_tests
        PRIVATE
            tests/camera_tests.cpp
    )

    target_link_libraries(relaycore_tests
        PRIVATE
            RelayCore::engine
            RelayCore::project_options
            RelayCore::warnings
            RelayCore::doctest_headers
    )

    if(RELAYCORE_ENABLE_PCH)
        target_precompile_headers(relaycore_tests PRIVATE ${RELAYCORE_PCH_HEADERS})
    endif()

    if(MSVC AND RELAYCORE_ENABLE_MSVC_ANALYZE)
        target_compile_options(relaycore_tests PRIVATE /analyze)
    endif()

    add_test(NAME relaycore.camera COMMAND relaycore_tests)
    set_tests_properties(relaycore.camera PROPERTIES LABELS "unit;math;camera")
endif()

add_custom_command(
    TARGET RelayCore POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:RelayCore>/assets"
    COMMENT "Copying runtime assets"
)

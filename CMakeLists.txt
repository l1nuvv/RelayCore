cmake_minimum_required(VERSION 4.3.1)

project(
    RelayCore
    VERSION 0.1.0
    DESCRIPTION "Retro FPS engine prototype"
    LANGUAGES C CXX
)

if (MSVC)
    cmake_policy(SET CMP0091 NEW)
    cmake_policy(SET CMP0141 NEW)

    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
            "$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>")

    add_compile_options(
            /permissive-
            /Zc:preprocessor
            /Zc:__cplusplus
            /utf-8
    )

    add_compile_options(
            $<$<CONFIG:RelWithDebInfo,Release>:/Gy>
            $<$<CONFIG:RelWithDebInfo,Release>:/Gw>
            $<$<CONFIG:RelWithDebInfo>:/Zo>
    )

    add_link_options(
            $<$<CONFIG:RelWithDebInfo,Release>:/OPT:REF>
            $<$<CONFIG:RelWithDebInfo,Release>:/OPT:ICF>
            $<$<CONFIG:RelWithDebInfo,Release>:/INCREMENTAL:NO>
    )
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(RelayCoreOptions)
include(RelayCoreDependencies)

relaycore_initialize_project()
relaycore_find_dependencies()
relaycore_create_dependency_targets()

find_program(CLANG_FORMAT_EXE NAMES clang-format clang-format.exe)

if (CLANG_FORMAT_EXE)
    add_custom_target(format
            COMMAND pwsh -NoProfile -ExecutionPolicy Bypass -Command
            "$files = git ls-files -- '*.c' '*.cpp' '*.h' '*.hpp' ':(exclude)extern/**'; if (-not $files) { exit 0 }; & '${CLANG_FORMAT_EXE}' -i $files"
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running clang-format on tracked source files")
endif()

add_subdirectory(src/relaycore)
add_subdirectory(src/relaygame)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

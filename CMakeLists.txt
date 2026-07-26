cmake_minimum_required(VERSION 4.0...4.3)

if (POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

if (POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
endif()

project(
    RelayCore
    VERSION 0.1.0
    DESCRIPTION "Retro indoor FPS engine prototype"
    LANGUAGES C CXX
)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (MSVC)
    if (CMAKE_GENERATOR MATCHES "Ninja" AND NOT DEFINED CMAKE_CL_SHOWINCLUDES_PREFIX)
        if (DEFINED ENV{VSLANG} AND "$ENV{VSLANG}" STREQUAL "1049")
            set(CMAKE_CL_SHOWINCLUDES_PREFIX "Примечание: включение файла:")
        else()
            set(CMAKE_CL_SHOWINCLUDES_PREFIX "Note: including file:")
        endif()
    endif()

    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:ProgramDatabase>")

    add_compile_options(
        $<$<COMPILE_LANGUAGE:C,CXX>:/utf-8>
        $<$<COMPILE_LANGUAGE:C,CXX>:/Zc:preprocessor>
        $<$<COMPILE_LANGUAGE:CXX>:/permissive->
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>
    )

    add_compile_options(
        $<$<OR:$<CONFIG:RelWithDebInfo>,$<CONFIG:Release>>:/Gy>
        $<$<OR:$<CONFIG:RelWithDebInfo>,$<CONFIG:Release>>:/Gw>
        $<$<CONFIG:RelWithDebInfo>:/Zo>
    )

    add_link_options(
        $<$<OR:$<CONFIG:RelWithDebInfo>,$<CONFIG:Release>>:/OPT:REF>
        $<$<OR:$<CONFIG:RelWithDebInfo>,$<CONFIG:Release>>:/OPT:ICF>
        $<$<OR:$<CONFIG:RelWithDebInfo>,$<CONFIG:Release>>:/INCREMENTAL:NO>
    )
endif()

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CTest)
include(RelayCoreOptions)
include(RelayCoreDependencies)

relaycore_initialize_project()
relaycore_find_dependencies()
relaycore_create_dependency_targets()

find_package(Git QUIET)
find_program(CLANG_FORMAT_EXE NAMES clang-format clang-format.exe)

if (CLANG_FORMAT_EXE AND Git_FOUND)
    add_custom_target(
        format
        COMMAND
            pwsh -NoProfile -ExecutionPolicy Bypass -Command
            "$files = & '${GIT_EXECUTABLE}' -C '${CMAKE_SOURCE_DIR}' ls-files -- '*.c' '*.cc' '*.cpp' '*.cxx' '*.h' '*.hh' '*.hpp' '*.hxx' ':(exclude)extern/**'; if (-not $files) { exit 0 }; & '${CLANG_FORMAT_EXE}' -i $files"
        WORKING_DIRECTORY
            ${CMAKE_SOURCE_DIR}
        COMMENT
            "Running clang-format on tracked source files"
        VERBATIM
    )

    add_custom_target(
        format-check
        COMMAND
            pwsh -NoProfile -ExecutionPolicy Bypass -Command
            "$files = & '${GIT_EXECUTABLE}' -C '${CMAKE_SOURCE_DIR}' ls-files -- '*.c' '*.cc' '*.cpp' '*.cxx' '*.h' '*.hh' '*.hpp' '*.hxx' ':(exclude)extern/**'; if (-not $files) { exit 0 }; & '${CLANG_FORMAT_EXE}' --dry-run --Werror $files"
        WORKING_DIRECTORY
            ${CMAKE_SOURCE_DIR}
        COMMENT
            "Checking code format with clang-format"
        VERBATIM
    )
endif()

add_subdirectory(src/relaycore)
add_subdirectory(src/relaygame)

if (BUILD_TESTING)
    add_subdirectory(tests)
endif()

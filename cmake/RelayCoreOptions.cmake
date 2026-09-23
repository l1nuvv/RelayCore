include_guard(GLOBAL)

include(CheckIPOSupported)

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

function(relaycore_find_msvc_asan_runtime out_var)
    set(runtime_candidates "")

    if(CMAKE_CXX_COMPILER)
        get_filename_component(cxx_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
        list(APPEND runtime_candidates "${cxx_compiler_dir}/clang_rt.asan_dynamic-x86_64.dll")
    endif()

    if(CMAKE_C_COMPILER)
        get_filename_component(c_compiler_dir "${CMAKE_C_COMPILER}" DIRECTORY)
        list(APPEND runtime_candidates "${c_compiler_dir}/clang_rt.asan_dynamic-x86_64.dll")
    endif()

    foreach(candidate IN LISTS runtime_candidates)
        if(EXISTS "${candidate}")
            set(${out_var} "${candidate}" PARENT_SCOPE)
            return()
        endif()
    endforeach()

    set(${out_var} "" PARENT_SCOPE)
endfunction()

macro(relaycore_initialize_project)
    if(PROJECT_IS_TOP_LEVEL)
        include(CTest)
        set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    endif()

    # RelayCore does not currently use C++20 modules. Disabling compiler-side
    # module scanning keeps the Ninja/MSVC build graph simpler and avoids the
    # extra scanned-object workflow for every translation unit.
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

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
    option(RELAYCORE_ENABLE_TRACY "Enable Tracy instrumentation and linking" OFF)
    option(RELAYCORE_ENABLE_MSVC_ANALYZE "Enable MSVC /analyze static analysis" OFF)

    check_ipo_supported(
        RESULT RELAYCORE_IPO_SUPPORTED
        OUTPUT RELAYCORE_IPO_OUTPUT
        LANGUAGES C CXX
    )

    add_library(relaycore_project_options INTERFACE)
    add_library(RelayCore::project_options ALIAS relaycore_project_options)
    target_compile_features(relaycore_project_options INTERFACE c_std_11 cxx_std_20)
    target_compile_definitions(
        relaycore_project_options
        INTERFACE
            $<$<CONFIG:Debug>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE>
            $<$<NOT:$<CONFIG:Debug>>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO>
    )

    add_library(relaycore_warnings INTERFACE)
    add_library(RelayCore::warnings ALIAS relaycore_warnings)

    if(MSVC)
        target_compile_options(
            relaycore_project_options
            INTERFACE
                /permissive-
                /Zc:preprocessor
                /Zc:__cplusplus
                /utf-8
                $<$<CONFIG:Asan>:/Zi>
        )
        target_link_options(
            relaycore_project_options
            INTERFACE
                $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>,$<CONFIG:Asan>>:/DEBUG>
        )
        target_compile_definitions(
            relaycore_project_options
            INTERFACE
                _CRT_SECURE_NO_WARNINGS
                NOMINMAX
                WIN32_LEAN_AND_MEAN
                $<$<CONFIG:Debug>:_DEBUG>
        )
        target_compile_options(relaycore_warnings INTERFACE /W4 /external:anglebrackets /external:W0)
    else()
        target_compile_options(
            relaycore_project_options
            INTERFACE
                $<$<CONFIG:Release>:-O3>
                $<$<CONFIG:RelWithDebInfo>:-O3>
                $<$<CONFIG:MinSizeRel>:-O1>
                $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>,$<CONFIG:Asan>>:-g>
        )
        target_compile_options(
            relaycore_warnings
            INTERFACE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wsign-conversion
                -Wshadow
        )
    endif()

endmacro()

function(relaycore_apply_standard_target_settings target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "relaycore_apply_standard_target_settings: target '${target_name}' does not exist")
    endif()

    if(MSVC)
        set_property(TARGET "${target_name}" PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<OR:$<CONFIG:Debug>,$<CONFIG:Asan>>:Debug>DLL")

        if(RELAYCORE_ENABLE_ASAN)
            target_compile_options("${target_name}" PRIVATE $<$<CONFIG:Asan>:/fsanitize=address>)
            target_link_options("${target_name}" PRIVATE $<$<CONFIG:Asan>:/fsanitize=address>)

            get_target_property(target_type "${target_name}" TYPE)
            if(target_type STREQUAL "EXECUTABLE")
                relaycore_find_msvc_asan_runtime(asan_runtime_dll)
                if(asan_runtime_dll)
                    add_custom_command(
                        TARGET "${target_name}"
                        POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${asan_runtime_dll}"
                        "$<TARGET_FILE_DIR:${target_name}>"
                        COMMENT "Copying MSVC ASan runtime"
                    )
                else()
                    message(WARNING "MSVC ASan runtime DLL was not found for target '${target_name}'")
                endif()
            endif()
        endif()

        if(RELAYCORE_ENABLE_MSVC_ANALYZE)
            target_compile_options("${target_name}" PRIVATE /analyze)
        endif()
    else()
        if(RELAYCORE_ENABLE_ASAN)
            target_compile_options("${target_name}" PRIVATE $<$<CONFIG:Asan>:-fsanitize=address>)
            target_link_options("${target_name}" PRIVATE $<$<CONFIG:Asan>:-fsanitize=address>)
        endif()
    endif()

    if(RELAYCORE_ENABLE_IPO AND RELAYCORE_IPO_SUPPORTED)
        set_property(TARGET "${target_name}" PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        set_property(TARGET "${target_name}" PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
    elseif(RELAYCORE_ENABLE_IPO)
        message(STATUS "IPO/LTO not enabled for ${target_name}: ${RELAYCORE_IPO_OUTPUT}")
    endif()

endfunction()

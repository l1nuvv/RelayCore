cmake_minimum_required(VERSION 4.0...4.3)

if (POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

if (POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
endif()

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" RELAYCORE_VERSION)
string(STRIP "${RELAYCORE_VERSION}" RELAYCORE_VERSION)
if(NOT RELAYCORE_VERSION MATCHES "^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$")
    message(FATAL_ERROR "VERSION must contain MAJOR.MINOR.PATCH")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")

project(
    RelayCore
    VERSION ${RELAYCORE_VERSION}
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

set(RELAYCORE_SPIRV_DIRECTORY "${PROJECT_BINARY_DIR}/generated/shaders")
add_custom_command(
    OUTPUT "${RELAYCORE_SPIRV_DIRECTORY}/toolchain_check.vert.spv"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RELAYCORE_SPIRV_DIRECTORY}"
    COMMAND "${SLANGC_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/toolchain_check.slang"
        -entry vertex_main -stage vertex -target spirv -profile spirv_1_5
        -o "${RELAYCORE_SPIRV_DIRECTORY}/toolchain_check.vert.spv"
    DEPENDS
        "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/toolchain_check.slang"
        "${SLANGC_EXECUTABLE}"
    COMMENT "Compiling Vulkan toolchain shader to SPIR-V"
    VERBATIM
)
set(RELAYCORE_MESH_SPIRV)
foreach(stage IN ITEMS vert frag)
    if(stage STREQUAL "vert")
        set(entry vertex_main)
        set(shader_stage vertex)
    else()
        set(entry fragment_main)
        set(shader_stage fragment)
    endif()
    set(output "${RELAYCORE_SPIRV_DIRECTORY}/mesh.${stage}.spv")
    add_custom_command(OUTPUT "${output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${RELAYCORE_SPIRV_DIRECTORY}"
        COMMAND "${SLANGC_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/mesh.slang"
            -entry ${entry} -stage ${shader_stage} -target spirv -profile spirv_1_5
            -matrix-layout-column-major -fvk-use-entrypoint-name -o "${output}"
        DEPENDS "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/mesh.slang"
                "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/scene_materials.slang" "${SLANGC_EXECUTABLE}"
        COMMENT "Compiling mesh.slang (${entry}) to SPIR-V" VERBATIM)
    list(APPEND RELAYCORE_MESH_SPIRV "${output}")
endforeach()
foreach(shadow IN ITEMS shadow point_shadow)
    if(shadow STREQUAL "point_shadow")
        set(point_shadow 1)
    else()
        set(point_shadow 0)
    endif()
    foreach(stage IN ITEMS vert frag)
        if(stage STREQUAL "vert")
            set(entry vertex_main)
            set(shader_stage vertex)
        else()
            set(entry fragment_main)
            set(shader_stage fragment)
        endif()
        set(output "${RELAYCORE_SPIRV_DIRECTORY}/${shadow}.${stage}.spv")
        add_custom_command(OUTPUT "${output}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${RELAYCORE_SPIRV_DIRECTORY}"
            COMMAND "${SLANGC_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/shadow.slang"
                -DPOINT_SHADOW=${point_shadow} -entry ${entry} -stage ${shader_stage} -target spirv -profile spirv_1_5
                -matrix-layout-column-major -fvk-use-entrypoint-name -o "${output}"
            DEPENDS "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/shadow.slang" "${SLANGC_EXECUTABLE}"
            COMMENT "Compiling shadow.slang (${shadow}, ${entry}) to SPIR-V" VERBATIM)
        list(APPEND RELAYCORE_MESH_SPIRV "${output}")
    endforeach()
endforeach()
add_custom_target(relaycore_vulkan_shaders ALL
    DEPENDS "${RELAYCORE_SPIRV_DIRECTORY}/toolchain_check.vert.spv" ${RELAYCORE_MESH_SPIRV})
set(runtime_shader_names)
foreach(shader IN LISTS RELAYCORE_MESH_SPIRV)
    get_filename_component(shader_name "${shader}" NAME)
    list(APPEND runtime_shader_names "${shader_name}")
endforeach()
list(JOIN runtime_shader_names "\n" runtime_shader_manifest)
file(GENERATE OUTPUT "${RELAYCORE_SPIRV_DIRECTORY}/runtime-shaders.txt" CONTENT "${runtime_shader_manifest}\n")
set_target_properties(relaycore_vulkan_shaders PROPERTIES FOLDER "render")

add_executable(relaycore_vulkan_toolchain_check EXCLUDE_FROM_ALL tools/vulkan_toolchain_check.cpp)
target_link_libraries(relaycore_vulkan_toolchain_check PRIVATE
    RelayCore::project_options RelayCore::warnings Vulkan::Vulkan
    vk-bootstrap::vk-bootstrap GPUOpen::VulkanMemoryAllocator)
target_compile_definitions(relaycore_vulkan_toolchain_check PRIVATE
    RELAYCORE_VULKAN_VALIDATION=$<AND:$<BOOL:${RELAYCORE_ENABLE_VULKAN_VALIDATION}>,$<OR:$<CONFIG:Debug>,$<CONFIG:Asan>>>)
set_target_properties(relaycore_vulkan_toolchain_check PROPERTIES FOLDER "tools")
relaycore_apply_standard_target_settings(relaycore_vulkan_toolchain_check)
add_custom_command(TARGET relaycore_vulkan_toolchain_check POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy -t "$<TARGET_FILE_DIR:relaycore_vulkan_toolchain_check>"
        $<TARGET_RUNTIME_DLLS:relaycore_vulkan_toolchain_check>
    COMMAND_EXPAND_LISTS
    VERBATIM)

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

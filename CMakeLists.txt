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

endif()

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CTest)
include(RelayCoreOptions)
include(RelayCoreDependencies)

relaycore_initialize_project()
if(MSVC)
    # Non-IPO iteration builds keep incremental linking; final/profile builds keep COMDAT optimization.
    add_link_options(
        $<$<OR:$<CONFIG:Release>,$<AND:$<CONFIG:RelWithDebInfo>,$<BOOL:${RELAYCORE_ENABLE_IPO}>>>:/OPT:REF>
        $<$<OR:$<CONFIG:Release>,$<AND:$<CONFIG:RelWithDebInfo>,$<BOOL:${RELAYCORE_ENABLE_IPO}>>>:/OPT:ICF>
        $<$<OR:$<CONFIG:Release>,$<AND:$<CONFIG:RelWithDebInfo>,$<BOOL:${RELAYCORE_ENABLE_IPO}>>>:/INCREMENTAL:NO>
        $<$<AND:$<CONFIG:RelWithDebInfo>,$<NOT:$<BOOL:${RELAYCORE_ENABLE_IPO}>>>:/INCREMENTAL>
    )
endif()
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
                "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/scene_postfx.slang"
        COMMENT "Compiling mesh.slang (${entry}) to SPIR-V" VERBATIM)
    list(APPEND RELAYCORE_MESH_SPIRV "${output}")
endforeach()
foreach(stage IN ITEMS vert frag comp bloom)
    if(stage STREQUAL "comp")
        set(entry compute_main)
        set(shader_stage compute)
    elseif(stage STREQUAL "bloom")
        set(entry bloom_fragment)
        set(shader_stage fragment)
    elseif(stage STREQUAL "vert")
        set(entry vertex_main)
        set(shader_stage vertex)
    else()
        set(entry fragment_main)
        set(shader_stage fragment)
    endif()
    set(output "${RELAYCORE_SPIRV_DIRECTORY}/tone_mapping.${stage}.spv")
    add_custom_command(OUTPUT "${output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${RELAYCORE_SPIRV_DIRECTORY}"
        COMMAND "${SLANGC_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/tone_mapping.slang"
            -entry ${entry} -stage ${shader_stage} -target spirv -profile spirv_1_5
            -matrix-layout-column-major -fvk-use-entrypoint-name -o "${output}"
        DEPENDS "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/tone_mapping.slang"
                "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/scene_postfx.slang" "${SLANGC_EXECUTABLE}"
        COMMENT "Compiling tone_mapping.slang (${entry}) to SPIR-V" VERBATIM)
    list(APPEND RELAYCORE_MESH_SPIRV "${output}")
endforeach()
foreach(entry IN ITEMS input_vertex input_fragment debug_vertex debug_fragment ao_vertex ao_fragment)
    if(entry MATCHES "vertex$")
        set(shader_stage vertex)
    else()
        set(shader_stage fragment)
    endif()
    if(entry MATCHES "^ao_")
        set(input_source mesh)
        set(input_defines -DSCREEN_EFFECTS=1)
    else()
        set(input_source pass_inputs)
        set(input_defines)
    endif()
    set(output "${RELAYCORE_SPIRV_DIRECTORY}/pass_inputs.${entry}.spv")
    add_custom_command(OUTPUT "${output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${RELAYCORE_SPIRV_DIRECTORY}"
        COMMAND "${SLANGC_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/${input_source}.slang" ${input_defines}
            -entry ${entry} -stage ${shader_stage} -target spirv -profile spirv_1_5
            -matrix-layout-column-major -fvk-use-entrypoint-name -o "${output}"
        DEPENDS "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/pass_inputs.slang"
                "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/mesh.slang"
                "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/scene_materials.slang"
                "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/scene_postfx.slang" "${SLANGC_EXECUTABLE}"
                "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/scene_effects.slang"
        COMMENT "Compiling pass_inputs.slang (${entry}) to SPIR-V" VERBATIM)
    list(APPEND RELAYCORE_MESH_SPIRV "${output}")
endforeach()
foreach(stage IN ITEMS vertex fragment)
    set(output "${RELAYCORE_SPIRV_DIRECTORY}/temporal.temporal_${stage}.spv")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${SLANGC_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/temporal.slang"
            -entry temporal_${stage} -stage ${stage} -target spirv -profile spirv_1_5
            -matrix-layout-column-major -fvk-use-entrypoint-name -o "${output}"
        DEPENDS "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/temporal.slang" "${SLANGC_EXECUTABLE}"
        COMMENT "Compiling temporal.slang (${stage}) to SPIR-V" VERBATIM)
    list(APPEND RELAYCORE_MESH_SPIRV "${output}")
endforeach()
foreach(entry IN ITEMS advanced_vertex gi_fragment lens_fragment)
    if(entry MATCHES "vertex$")
        set(shader_stage vertex)
        set(output "${RELAYCORE_SPIRV_DIRECTORY}/advanced.vert.spv")
    else()
        set(shader_stage fragment)
        string(REPLACE "_fragment" "" advanced_kind "${entry}")
        set(output "${RELAYCORE_SPIRV_DIRECTORY}/advanced_${advanced_kind}.frag.spv")
    endif()
    add_custom_command(OUTPUT "${output}"
        COMMAND "${SLANGC_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/advanced.slang"
            -entry ${entry} -stage ${shader_stage} -target spirv -profile spirv_1_5
            -matrix-layout-column-major -fvk-use-entrypoint-name -o "${output}"
        DEPENDS "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/advanced.slang"
                "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/mesh.slang"
                "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/scene_effects.slang"
                "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/scene_materials.slang"
                "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/scene_postfx.slang" "${SLANGC_EXECUTABLE}"
        COMMENT "Compiling advanced.slang (${entry}) to SPIR-V" VERBATIM)
    list(APPEND RELAYCORE_MESH_SPIRV "${output}")
endforeach()
foreach(effect IN ITEMS effects volume)
    set(effect_defines -DSCREEN_EFFECTS=1)
    if(effect STREQUAL "volume")
        list(APPEND effect_defines -DVOLUME_ONLY=1)
    endif()
    set(output "${RELAYCORE_SPIRV_DIRECTORY}/mesh_${effect}.frag.spv")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${SLANGC_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/mesh.slang"
            ${effect_defines} -entry fragment_main -stage fragment -target spirv -profile spirv_1_5
            -matrix-layout-column-major -fvk-use-entrypoint-name -o "${output}"
        DEPENDS "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/mesh.slang"
            "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/scene_effects.slang"
            "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/scene_materials.slang"
            "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/scene_postfx.slang" "${SLANGC_EXECUTABLE}"
        COMMENT "Compiling mesh screen effects to SPIR-V" VERBATIM)
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
foreach(stage IN ITEMS vert frag)
    if(stage STREQUAL "vert")
        set(entry vertex_main)
        set(shader_stage vertex)
    else()
        set(entry fragment_main)
        set(shader_stage fragment)
    endif()
    set(output "${RELAYCORE_SPIRV_DIRECTORY}/player_ui.${stage}.spv")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${SLANGC_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/player_ui.slang"
            -entry ${entry} -stage ${shader_stage} -target spirv -profile spirv_1_5
            -matrix-layout-column-major -fvk-use-entrypoint-name -o "${output}"
        DEPENDS "${PROJECT_SOURCE_DIR}/assets/shaders/vulkan/player_ui.slang" "${SLANGC_EXECUTABLE}"
        VERBATIM)
    list(APPEND RELAYCORE_MESH_SPIRV "${output}")
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
add_library(relaycore_model_importer STATIC tools/model_import.cpp)
target_link_libraries(relaycore_model_importer PRIVATE RelayCore::engine fastgltf::fastgltf windowscodecs ole32)
target_include_directories(relaycore_model_importer PUBLIC "${PROJECT_SOURCE_DIR}/tools")
target_compile_definitions(relaycore_model_importer PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
relaycore_apply_standard_target_settings(relaycore_model_importer)
set_target_properties(relaycore_model_importer PROPERTIES FOLDER "tools" UNITY_BUILD OFF)
add_executable(relaycore_model_import tools/model_import_main.cpp)
target_link_libraries(relaycore_model_import PRIVATE relaycore_model_importer RelayCore::engine)
relaycore_apply_standard_target_settings(relaycore_model_import)
set_target_properties(relaycore_model_import PROPERTIES FOLDER "tools")
add_custom_command(TARGET relaycore_model_import POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy -t "$<TARGET_FILE_DIR:relaycore_model_import>" $<TARGET_RUNTIME_DLLS:relaycore_model_import>
    COMMAND_EXPAND_LISTS VERBATIM)
include(cmake/RelayCoreModelAssets.cmake)
add_executable(relaycore_collision_compile tools/collision_compile.cpp)
target_link_libraries(relaycore_collision_compile PRIVATE RelayCore::engine RelayCore::toml_headers)
relaycore_apply_standard_target_settings(relaycore_collision_compile)
set_target_properties(relaycore_collision_compile PROPERTIES FOLDER "tools" UNITY_BUILD OFF)
add_custom_command(TARGET relaycore_collision_compile POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy -t "$<TARGET_FILE_DIR:relaycore_collision_compile>"
        $<TARGET_RUNTIME_DLLS:relaycore_collision_compile>
    COMMAND_EXPAND_LISTS VERBATIM)
include(cmake/RelayCoreCollisionAssets.cmake)
add_subdirectory(src/relaygame)

if (BUILD_TESTING)
    add_subdirectory(tests)
endif()

include_guard(GLOBAL)

macro(relaycore_find_dependencies)
    find_package(SDL3 CONFIG REQUIRED)
    find_package(spdlog CONFIG REQUIRED)
    find_package(glm CONFIG REQUIRED)

    if(RELAYCORE_ENABLE_TRACY)
        find_package(Tracy CONFIG REQUIRED)
    endif()

    find_package(imgui CONFIG REQUIRED)
    find_package(Vulkan REQUIRED)
    find_package(vk-bootstrap CONFIG REQUIRED)
    find_package(VulkanMemoryAllocator CONFIG REQUIRED)
    find_package(slang CONFIG REQUIRED)

    message(STATUS "Vulkan headers: ${Vulkan_VERSION}; loader: ${Vulkan_LIBRARY}")
    message(STATUS "Slang compiler: ${SLANGC_EXECUTABLE}")

    find_path(RELAYCORE_TOMLPLUSPLUS_INCLUDE_DIR toml++/toml.hpp REQUIRED)
    find_path(RELAYCORE_MINIAUDIO_INCLUDE_DIR miniaudio.h REQUIRED)

    if(BUILD_TESTING)
        find_path(RELAYCORE_DOCTEST_INCLUDE_DIR doctest/doctest.h REQUIRED)
    endif()

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
endmacro()

macro(relaycore_create_dependency_targets)
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/extern/glad/include/glad/gl.h")
        message(FATAL_ERROR "Vendored glad headers were not found in extern/glad/include")
    endif()

    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/extern/glad/src/gl.c")
        message(FATAL_ERROR "Vendored glad source was not found in extern/glad/src/gl.c")
    endif()

    add_library(relaycore_glad STATIC)
    add_library(RelayCore::glad ALIAS relaycore_glad)
    target_sources(relaycore_glad PRIVATE "${PROJECT_SOURCE_DIR}/extern/glad/src/gl.c")
    target_include_directories(relaycore_glad SYSTEM PUBLIC "${PROJECT_SOURCE_DIR}/extern/glad/include")
    target_link_libraries(relaycore_glad PUBLIC RelayCore::project_options RelayCore::warnings)
    set_target_properties(relaycore_glad PROPERTIES UNITY_BUILD OFF FOLDER "third_party")
    relaycore_apply_standard_target_settings(relaycore_glad)

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
endmacro()

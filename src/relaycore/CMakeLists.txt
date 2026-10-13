set(RELAYCORE_ENGINE_SOURCES
    app/application.cpp
    app/engine_services.cpp
    app/frame_timer.cpp
    app/run_application.cpp
    app/startup_options.cpp
    audio/audio_system.cpp
    audio/miniaudio_impl.cpp
    core/engine_config.cpp
    core/log.cpp
    core/paths.cpp
    debug/imgui_overlay.cpp
    debug/imgui_overlay_gameplay_panel.cpp
    debug/imgui_overlay_inspector_panel.cpp
    debug/imgui_overlay_iteration_panel.cpp
    debug/imgui_overlay_summary_panel.cpp
    debug/imgui_overlay_system_panel.cpp
    debug/imgui_overlay_widgets.cpp
    debug/inspector_source_reference.cpp
    gameplay/combat_runtime.cpp
    gameplay/interaction_descriptors.cpp
    gameplay/interaction_runtime_doors.cpp
    gameplay/interaction_runtime_factory.cpp
    gameplay/interaction_runtime_queries.cpp
    physics/collision.cpp
    physics/player_motion.cpp
    physics/world_query.cpp
    physics/physics_world.cpp
    physics/collision_asset.cpp
    world/world_collision.cpp
    platform/sdl_system.cpp
    platform/window.cpp
    render/blockout_mesh_builder.cpp
    render/blockout_texture_atlas.cpp
    render/authored_color.cpp
    render/camera.cpp
    render/player_ui.cpp
    render/vulkan_rml_renderer.cpp
    ${PROJECT_SOURCE_DIR}/extern/rmlui/RmlUi_Platform_SDL.cpp
    render/vulkan_device.cpp
    render/vulkan_swapchain.cpp
    render/vulkan_resources.cpp
    render/vulkan_uploads.cpp
    render/vulkan_scene_submissions.cpp
    render/vulkan_mesh_renderer.cpp
    render/vulkan_material_table.cpp
    render/vma_impl.cpp
    render/mesh.cpp
    render/meshlets.cpp
    render/static_model.cpp
    render/model_asset.cpp
    render/reflection_probe.cpp
    render/irradiance_probes.cpp
    render/render_authoring_config.cpp
    render/pipeline_authoring_overlay.cpp


    render/renderer.cpp
    render/shadow_math.cpp
    render/vulkan_gpu_profiler.cpp
    render/texture2d.cpp
    world/blockout_loader.cpp
    world/blockout_loader_parse.cpp
    world/blockout_loader_transform.cpp
    world/blockout_loader_pipeline.cpp
    world/blockout_loader_templates.cpp
    world/blockout_loader_validation.cpp
    world/surface_material_utils.cpp
)

set(RELAYCORE_ENGINE_HEADERS
    app/app_delegate.h
    app/application.h
    app/engine_services.h
    app/frame_timer.h
    app/run_application.h
    app/startup_options.h
    audio/audio_system.h
    core/engine_config.h
    core/log.h
    core/paths.h
    core/profiling.h
    core/sdl_error.h
    core/spatial_index_2d.h
    debug/imgui_overlay.h
    debug/imgui_overlay_gameplay_panel.h
    debug/imgui_overlay_inspector_panel.h
    debug/imgui_overlay_iteration_panel.h
    debug/imgui_overlay_summary_panel.h
    debug/imgui_overlay_system_panel.h
    debug/imgui_overlay_widgets.h
    debug/inspector_source_reference.h
    gameplay/combat_runtime.h
    gameplay/entity_spawn_parsing.h
    gameplay/interaction_descriptors.h
    gameplay/interaction_runtime.h
    physics/collision.h
    physics/physics_world.h
    physics/collision_asset.h
    physics/player_motion.h
    physics/world_query.h
    platform/sdl_system.h
    platform/window.h
    render/blockout_mesh_builder.h
    render/blockout_texture_atlas.h
    render/authored_color.h
    render/camera.h
    render/player_ui.h
    render/player_ui_data.h
    render/vulkan_device.h
    render/vulkan_swapchain.h
    render/vulkan_resources.h
    render/vulkan_uploads.h
    render/vulkan_scene_submissions.h
    render/vulkan_mesh_renderer.h
    render/mesh.h
    render/reflection_probe.h
    render/irradiance_probes.h
    render/static_model.h
    render/model_asset.h
    render/render_authoring_config.h
    render/pipeline_authoring_overlay.h


    render/render_diagnostics.h
    render/render_queue.h
    render/renderer.h
    render/render_resolution.h
    render/shadow_math.h
    render/vulkan_gpu_profiler.h
    render/texture2d.h
    world/blockout_level.h
    world/blockout_loader.h
    world/blockout_loader_entity_props.h
    world/blockout_loader_templates.h
    world/blockout_loader_validation.h
    world/surface_material_utils.h
    world/world_collision.h
)

add_library(relaycore_engine STATIC)
add_library(RelayCore::engine ALIAS relaycore_engine)

target_precompile_headers(relaycore_engine PRIVATE
    <algorithm> <array> <filesystem> <memory> <optional> <span>
    <string> <unordered_map> <vector> <glm/glm.hpp> <spdlog/spdlog.h>)
set_source_files_properties(audio/miniaudio_impl.cpp render/vma_impl.cpp PROPERTIES
    SKIP_PRECOMPILE_HEADERS ON)

set_source_files_properties(
    render/reflection_probe.cpp
    render/irradiance_probes.cpp
    render/player_ui.cpp
    ${PROJECT_SOURCE_DIR}/extern/rmlui/RmlUi_Platform_SDL.cpp
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
)

target_sources(
    relaycore_engine
    PRIVATE
        ${RELAYCORE_ENGINE_SOURCES}
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${PROJECT_SOURCE_DIR}/src
        FILES ${RELAYCORE_ENGINE_HEADERS}
)

target_compile_features(relaycore_engine PUBLIC cxx_std_20)

target_include_directories(
    relaycore_engine
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(
    relaycore_engine
    PUBLIC
        RelayCore::project_options
        RelayCore::warnings
        RelayCore::toml_headers
        SDL3::SDL3
        spdlog::spdlog
        glm::glm
    PRIVATE
        RelayCore::miniaudio_headers
        ${RELAYCORE_IMGUI_TARGET}
        RmlUi::RmlUi
        Vulkan::Vulkan
        vk-bootstrap::vk-bootstrap
        GPUOpen::VulkanMemoryAllocator
        Jolt::Jolt
)

target_compile_definitions(relaycore_engine PRIVATE
    RMLUI_SDL_VERSION_MAJOR=3
    RELAYCORE_VULKAN_VALIDATION=$<AND:$<BOOL:${RELAYCORE_ENABLE_VULKAN_VALIDATION}>,$<OR:$<CONFIG:Debug>,$<CONFIG:Asan>>>)
set_source_files_properties(render/vma_impl.cpp PROPERTIES
    SKIP_UNITY_BUILD_INCLUSION ON
    COMPILE_DEFINITIONS "VMA_STATIC_VULKAN_FUNCTIONS=0;VMA_DYNAMIC_VULKAN_FUNCTIONS=1")
set_source_files_properties(
    physics/world_query.cpp
    physics/collision_asset.cpp
    physics/physics_world.cpp
    world/world_collision.cpp
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)

if (RELAYCORE_ENABLE_TRACY)
    target_link_libraries(relaycore_engine PRIVATE Tracy::TracyClient)
    target_compile_definitions(relaycore_engine PRIVATE TRACY_ENABLE)
endif()


set_target_properties(relaycore_engine PROPERTIES FOLDER "engine")
relaycore_apply_standard_target_settings(relaycore_engine)

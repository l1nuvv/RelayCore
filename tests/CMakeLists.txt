add_executable(relaycore_tests)
if(WIN32)
    configure_file(${PROJECT_SOURCE_DIR}/content/graphics/branding/relaycore.rc.in generated/relaycore.rc @ONLY)
    set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/generated/relaycore.rc PROPERTIES
        OBJECT_DEPENDS "${PROJECT_SOURCE_DIR}/assets/branding/relaycore.ico")
    file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}/tools/updater/RelayCoreUpdater.cs" updater_source)
    file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/updater_tests.cs" updater_test_source)
    file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/relaycore_updater_tests.exe" updater_test_output)
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/relaycore_updater_tests.exe
        COMMAND "${RELAYCORE_CSC}" /nologo /target:exe /main:UpdaterTests /optimize+
            /reference:System.Net.Http.dll /reference:System.IO.Compression.dll
            /reference:System.IO.Compression.FileSystem.dll /reference:System.Web.Extensions.dll
            /reference:System.Windows.Forms.dll /reference:System.Drawing.dll
            "/out:${updater_test_output}"
            "${updater_source}"
            "${updater_test_source}"
        DEPENDS ${PROJECT_SOURCE_DIR}/tools/updater/RelayCoreUpdater.cs ${CMAKE_CURRENT_SOURCE_DIR}/updater_tests.cs
        VERBATIM)
    add_custom_target(relaycore_updater_tests DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/relaycore_updater_tests.exe)
    add_dependencies(relaycore_tests relaycore_updater_tests)
    add_test(NAME relaycore.updater COMMAND ${CMAKE_CURRENT_BINARY_DIR}/relaycore_updater_tests.exe)
    find_program(RELAYCORE_PWSH_EXECUTABLE NAMES pwsh REQUIRED)
    add_dependencies(relaycore_tests relaycore_level_check)
    foreach(script IN ITEMS pre_commit release_scripts release_workflow)
        add_test(NAME relaycore.tooling.${script}
            COMMAND "${RELAYCORE_PWSH_EXECUTABLE}" -NoProfile -File "${CMAKE_CURRENT_SOURCE_DIR}/${script}_tests.ps1")
    endforeach()
    if(NOT CLANG_FORMAT_EXE)
        message(FATAL_ERROR "clang-format is required for the pre-commit regression test")
    endif()
    get_filename_component(clang_format_directory "${CLANG_FORMAT_EXE}" DIRECTORY)
    set_tests_properties(relaycore.tooling.pre_commit PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${clang_format_directory}")
    add_test(NAME relaycore.tooling.level_check
        COMMAND "${RELAYCORE_PWSH_EXECUTABLE}" -NoProfile -File "${CMAKE_CURRENT_SOURCE_DIR}/level_check_tests.ps1"
            -Executable $<TARGET_FILE:relaycore_level_check>)
    add_test(NAME relaycore.tooling.model_assets
        COMMAND "${RELAYCORE_PWSH_EXECUTABLE}" -NoProfile -File "${CMAKE_CURRENT_SOURCE_DIR}/model_assets_tests.ps1"
            -Importer $<TARGET_FILE:relaycore_model_import> -CMake "${CMAKE_COMMAND}" -Ninja "${CMAKE_MAKE_PROGRAM}")
    add_test(NAME relaycore.tooling.collision_assets
        COMMAND "${RELAYCORE_PWSH_EXECUTABLE}" -NoProfile -File "${CMAKE_CURRENT_SOURCE_DIR}/collision_assets_tests.ps1"
            -Compiler $<TARGET_FILE:relaycore_collision_compile> -Importer $<TARGET_FILE:relaycore_model_import>
            -CMake "${CMAKE_COMMAND}" -Ninja "${CMAKE_MAKE_PROGRAM}")
    add_test(NAME relaycore.tooling.irradiance_assets
        COMMAND "${RELAYCORE_PWSH_EXECUTABLE}" -NoProfile -File "${CMAKE_CURRENT_SOURCE_DIR}/irradiance_assets_tests.ps1"
            -Baker $<TARGET_FILE:relaycore_irradiance_bake> -Authoring "${PROJECT_SOURCE_DIR}/assets/config/render_authoring.toml")
    set_tests_properties(relaycore.tooling.irradiance_assets PROPERTIES
        ENVIRONMENT "RELAYCORE_ASSET_ROOT=$<TARGET_FILE_DIR:relaycore_demo>/assets")
endif()
add_executable(relaycore_content_validation)
add_executable(relaycore_physics_tests content_validation_main.cpp physics_world_tests.cpp collision_asset_tests.cpp
    mesh_movement_tests.cpp native_collision_gameplay_tests.cpp player_motion_tests.cpp)
add_dependencies(relaycore_physics_tests relaycore_collision_fixture_assets)
target_compile_definitions(relaycore_physics_tests PRIVATE
    RELAYCORE_TEST_COLLISION_PRODUCT="${RELAYCORE_COLLISION_FIXTURE_ROOT}/collision/lab_bench.rccollision")
target_link_libraries(relaycore_physics_tests PRIVATE RelayGame::runtime RelayCore::doctest_headers)
set_target_properties(relaycore_physics_tests PROPERTIES FOLDER "tests" UNITY_BUILD OFF)
add_dependencies(relaycore_tests relaycore_physics_tests)
add_executable(relaycore_audio_lifetime audio_lifetime_tests.cpp)
target_link_libraries(relaycore_audio_lifetime PRIVATE
    RelayCore::engine RelayCore::project_options RelayCore::warnings
)
set_target_properties(relaycore_audio_lifetime PROPERTIES FOLDER "tests" UNITY_BUILD OFF)
add_executable(relaycore_game_lifecycle game_lifecycle_tests.cpp)
target_link_libraries(relaycore_game_lifecycle PRIVATE RelayGame::runtime RelayCore::project_options RelayCore::warnings)
add_executable(relaycore_player_ui_graphics player_ui_graphics.cpp)
target_link_libraries(relaycore_player_ui_graphics PRIVATE RelayGame::runtime RmlUi::RmlUi RelayCore::project_options RelayCore::warnings)
add_executable(relaycore_bench_relay_run bench_relay_run.cpp)
add_executable(relaycore_containment_loop_graphics containment_loop_graphics.cpp)
target_link_libraries(relaycore_containment_loop_graphics PRIVATE RelayGame::runtime)
target_link_libraries(relaycore_bench_relay_run PRIVATE RelayGame::runtime RelayCore::project_options RelayCore::warnings)
set_target_properties(relaycore_bench_relay_run PROPERTIES FOLDER "tests" UNITY_BUILD OFF)
add_executable(relaycore_episode_profile EXCLUDE_FROM_ALL episode_profile.cpp)
target_link_libraries(relaycore_episode_profile PRIVATE RelayGame::runtime RelayCore::project_options RelayCore::warnings)
set_target_properties(relaycore_episode_profile PROPERTIES FOLDER "tests" UNITY_BUILD OFF)
add_executable(relaycore_containment_profile EXCLUDE_FROM_ALL containment_profile.cpp)
target_link_libraries(relaycore_containment_profile PRIVATE RelayGame::runtime RelayCore::project_options RelayCore::warnings)
set_target_properties(relaycore_containment_profile PROPERTIES FOLDER "tests" UNITY_BUILD OFF)
add_executable(relaycore_window_lifetime_tests window_lifetime_tests.cpp)
add_executable(relaycore_vulkan_window_tests vulkan_window_tests.cpp)
add_executable(relaycore_vulkan_device_tests vulkan_device_tests.cpp)
add_executable(relaycore_vulkan_presentation_tests vulkan_presentation_tests.cpp)
add_executable(relaycore_vulkan_upload_tests vulkan_upload_tests.cpp)
add_executable(relaycore_vulkan_mesh_tests vulkan_mesh_tests.cpp)
target_link_libraries(relaycore_vulkan_mesh_tests PRIVATE
    RelayGame::runtime RelayCore::engine Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator ${RELAYCORE_IMGUI_TARGET})
target_compile_definitions(relaycore_vulkan_mesh_tests PRIVATE
    RELAYCORE_TEST_SPIRV_DIRECTORY="${RELAYCORE_SPIRV_DIRECTORY}"
    RELAYCORE_TEST_BASELINE_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/graphics-baselines")
add_dependencies(relaycore_vulkan_mesh_tests relaycore_vulkan_shaders)
set_target_properties(relaycore_vulkan_mesh_tests PROPERTIES FOLDER "tests" UNITY_BUILD OFF)
add_executable(relaycore_power_restoration_graphics
    power_restoration_graphics.cpp
)
target_link_libraries(relaycore_power_restoration_graphics PRIVATE
    RelayGame::runtime RelayCore::project_options RelayCore::warnings
)
set_target_properties(relaycore_power_restoration_graphics PROPERTIES FOLDER "tests" UNITY_BUILD OFF)
target_include_directories(relaycore_power_restoration_graphics PRIVATE ${PROJECT_BINARY_DIR}/src/relaygame/generated)
set_target_properties(relaycore_tests PROPERTIES FOLDER "tests" UNITY_BUILD OFF)
set_target_properties(relaycore_content_validation PROPERTIES FOLDER "tests" UNITY_BUILD OFF)
set_target_properties(relaycore_window_lifetime_tests PROPERTIES FOLDER "tests" UNITY_BUILD OFF)
set_target_properties(relaycore_vulkan_window_tests PROPERTIES FOLDER "tests" UNITY_BUILD OFF)
set_target_properties(relaycore_vulkan_device_tests PROPERTIES FOLDER "tests" UNITY_BUILD OFF)
set_target_properties(relaycore_vulkan_presentation_tests PROPERTIES FOLDER "tests" UNITY_BUILD OFF)
set_target_properties(relaycore_vulkan_upload_tests PROPERTIES FOLDER "tests" UNITY_BUILD OFF)

target_sources(
    relaycore_tests
    PRIVATE
        world_environment_tests.cpp
        static_model_tests.cpp
        model_asset_tests.cpp
        meshlet_tests.cpp
        irradiance_probe_tests.cpp
        reflection_probe_tests.cpp
        model_import_tests.cpp
        enemy_runtime_tests.cpp
        save_game_tests.cpp
        level_editor_tests.cpp
        camera_tests.cpp
        diagnostics_publication_policy_tests.cpp
        audio_system_tests.cpp
        authored_color_tests.cpp
        startup_options_tests.cpp
        blockout_loader_tests.cpp
        blockout_mesh_tests.cpp
        blockout_texture_atlas_tests.cpp
        fixed_simulation_loop_tests.cpp
        gameplay_input_ownership_tests.cpp
        inspector_source_reference_tests.cpp
        level_session_builder_tests.cpp
        local_light_order_tests.cpp
        log_tests.cpp
        mission_runtime_tests.cpp
        player_motion_tests.cpp
        world_query_tests.cpp
        player_experience_tests.cpp
        scientist_pose_tests.cpp
        render_authoring_config_tests.cpp
        pipeline_authoring_overlay_tests.cpp


        renderer_diagnostics_api_tests.cpp
        scoped_temp_directory_tests.cpp
        spatial_index_2d_tests.cpp
        runtime_model_tests.cpp
        combat_runtime_tests.cpp
        engine_config_tests.cpp
        player_controller_tests.cpp
        window_presentation_tests.cpp
        world_audio_zones_tests.cpp
        world_inspector_tests.cpp
        interaction_runtime_tests.cpp
)


target_sources(
    relaycore_content_validation
    PRIVATE
        content_validation_main.cpp
        shipped_content_validation_tests.cpp
        development_sandbox_tests.cpp
        native_lab_tests.cpp
        pressure_transfer_tests.cpp
        foundation_scenario_tests.cpp
        campaign_route_tests.cpp
        cooling_loop_tests.cpp
        containment_loop_tests.cpp
        bench_relay_tests.cpp
)

target_link_libraries(
    relaycore_tests
    PRIVATE
        RelayGame::runtime
        RelayCore::engine
        RelayCore::project_options
        RelayCore::warnings
        RelayCore::doctest_headers
)


target_link_libraries(
    relaycore_content_validation
    PRIVATE
        RelayGame::runtime
        RelayCore::engine
        RelayCore::project_options
        RelayCore::warnings
        RelayCore::doctest_headers
)

find_package(doctest CONFIG REQUIRED)
target_link_libraries(relaycore_tests PRIVATE relaycore_model_importer)
target_compile_definitions(relaycore_tests PRIVATE RELAYCORE_MODEL_SOURCE="${PROJECT_SOURCE_DIR}/content/models/service_cylinder.glb")
include(doctest)

target_link_libraries(relaycore_window_lifetime_tests PRIVATE RelayCore::engine)
target_link_libraries(relaycore_vulkan_window_tests PRIVATE RelayCore::engine)
target_link_libraries(relaycore_vulkan_device_tests PRIVATE
    RelayCore::engine Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator)
target_compile_definitions(relaycore_vulkan_device_tests PRIVATE
    RELAYCORE_VULKAN_VALIDATION=$<AND:$<BOOL:${RELAYCORE_ENABLE_VULKAN_VALIDATION}>,$<OR:$<CONFIG:Debug>,$<CONFIG:Asan>>>)

target_link_libraries(relaycore_vulkan_presentation_tests PRIVATE
    RelayCore::engine Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator)
target_link_libraries(relaycore_vulkan_upload_tests PRIVATE
    RelayCore::engine Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator)
if(RELAYCORE_ENABLE_TRACY)
    target_link_libraries(relaycore_vulkan_mesh_tests PRIVATE Tracy::TracyClient)
    target_compile_definitions(relaycore_vulkan_mesh_tests PRIVATE TRACY_ENABLE)
endif()
if(WIN32)
    target_link_libraries(relaycore_vulkan_presentation_tests PRIVATE gdi32 user32)
endif()


foreach(test_target IN ITEMS
    relaycore_tests
    relaycore_audio_lifetime
    relaycore_content_validation
    relaycore_physics_tests
    relaycore_window_lifetime_tests
    relaycore_vulkan_window_tests
    relaycore_vulkan_device_tests
    relaycore_vulkan_presentation_tests
    relaycore_vulkan_upload_tests
    relaycore_vulkan_mesh_tests
    relaycore_power_restoration_graphics
    relaycore_game_lifecycle
    relaycore_player_ui_graphics
    relaycore_episode_profile
    relaycore_containment_profile
    relaycore_bench_relay_run
    relaycore_containment_loop_graphics
)
    relaycore_apply_standard_target_settings(${test_target})
    if(WIN32)
        target_sources(${test_target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated/relaycore.rc)
    endif()

    add_custom_command(
        TARGET ${test_target}
        POST_BUILD
        COMMAND
        ${CMAKE_COMMAND} -E copy -t "$<TARGET_FILE_DIR:${test_target}>" $<TARGET_RUNTIME_DLLS:${test_target}>
        COMMENT
        "Copying runtime DLLs"
        COMMAND_EXPAND_LISTS
    )
endforeach()

add_test(NAME relaycore.audio_lifetime COMMAND relaycore_audio_lifetime)
set_tests_properties(relaycore.audio_lifetime PROPERTIES
    LABELS "integration;audio"
    RUN_SERIAL TRUE
    SKIP_RETURN_CODE 77
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_audio_lifetime>"
    ENVIRONMENT "RELAYCORE_ASSET_ROOT=$<TARGET_FILE_DIR:relaycore_demo>/assets"
)

foreach(runtime_graphics_target IN ITEMS relaycore_tests relaycore_content_validation relaycore_audio_lifetime
    relaycore_vulkan_mesh_tests relaycore_window_lifetime_tests relaycore_game_lifecycle
    relaycore_player_ui_graphics
    relaycore_power_restoration_graphics relaycore_episode_profile relaycore_containment_profile relaycore_bench_relay_run relaycore_containment_loop_graphics)
    add_dependencies(${runtime_graphics_target} relaycore_demo_assets)
endforeach()

add_test(NAME graphics.window_lifetime COMMAND relaycore_window_lifetime_tests)
add_test(NAME graphics.vulkan_window COMMAND relaycore_vulkan_window_tests)
add_test(NAME graphics.vulkan_device COMMAND relaycore_vulkan_device_tests)
add_test(NAME graphics.vulkan_presentation COMMAND relaycore_vulkan_presentation_tests)
add_test(NAME graphics.vulkan_upload COMMAND relaycore_vulkan_upload_tests)
add_test(NAME graphics.vulkan_mesh COMMAND relaycore_vulkan_mesh_tests)
add_test(NAME graphics.mesh_shaders COMMAND relaycore_vulkan_mesh_tests --mesh-shaders)
add_test(NAME graphics.mesh_shaders_fallback COMMAND relaycore_vulkan_mesh_tests --mesh-shaders)
add_test(NAME graphics.descriptor_indexing COMMAND relaycore_vulkan_mesh_tests --descriptor-indexing)
add_test(NAME graphics.descriptor_indexing_fallback COMMAND relaycore_vulkan_mesh_tests --descriptor-indexing)
add_test(NAME graphics.scene_submission COMMAND relaycore_vulkan_mesh_tests --scene-submission)
add_test(NAME graphics.scene_culling COMMAND relaycore_vulkan_mesh_tests --scene-culling)
add_test(NAME graphics.scene_culling_fallback COMMAND relaycore_vulkan_mesh_tests --scene-culling)
add_test(NAME graphics.local_light_selection COMMAND relaycore_vulkan_mesh_tests --light-selection)
add_test(NAME graphics.mesh_pipeline COMMAND relaycore_vulkan_mesh_tests --pipeline-only)
add_test(NAME graphics.depth_formats COMMAND relaycore_vulkan_mesh_tests --depth-formats)
add_test(NAME graphics.power_restoration_slice COMMAND relaycore_vulkan_mesh_tests --power-restoration-slice)
add_test(NAME graphics.game_hud COMMAND relaycore_vulkan_mesh_tests --ui-only)
add_test(NAME graphics.player_ui COMMAND relaycore_player_ui_graphics)
add_test(NAME graphics.quality_presets COMMAND relaycore_player_ui_graphics --quality-presets)
add_test(NAME graphics.quality_presets_fallback COMMAND relaycore_player_ui_graphics --quality-presets)
add_test(NAME graphics.containment_loop COMMAND relaycore_containment_loop_graphics)
add_test(NAME graphics.containment_loop_hdr COMMAND relaycore_containment_loop_graphics --hdr)
add_test(NAME graphics.containment_color COMMAND relaycore_containment_loop_graphics --color-audit)
set_tests_properties(graphics.containment_loop graphics.containment_loop_hdr graphics.containment_color PROPERTIES
    LABELS "integration;graphics"
    RUN_SERIAL TRUE
    SKIP_RETURN_CODE 77
    TIMEOUT 180
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_containment_loop_graphics>"
    ENVIRONMENT "RELAYCORE_ASSET_ROOT=$<TARGET_FILE_DIR:relaycore_demo>/assets"
    FAIL_REGULAR_EXPRESSION "Vulkan validation (error|warning);Vulkan .* shutdown failed")
set_tests_properties(graphics.player_ui graphics.quality_presets graphics.quality_presets_fallback PROPERTIES
    LABELS "integration;graphics"
    RUN_SERIAL TRUE
    SKIP_RETURN_CODE 77
    TIMEOUT 180
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_player_ui_graphics>"
    ENVIRONMENT "RELAYCORE_ASSET_ROOT=$<TARGET_FILE_DIR:relaycore_demo>/assets"
    FAIL_REGULAR_EXPRESSION "Vulkan validation (error|warning);Vulkan .* shutdown failed")
set_property(TEST graphics.quality_presets graphics.quality_presets_fallback APPEND PROPERTY ENVIRONMENT
    "RELAYCORE_VULKAN_SCENE_SUBMISSION=gpu_culled")
set_property(TEST graphics.quality_presets_fallback APPEND PROPERTY ENVIRONMENT
    "RELAYCORE_VULKAN_DISABLE_DESCRIPTOR_INDEXING=1;RELAYCORE_VULKAN_DISABLE_GPU_CULLING=1;RELAYCORE_VULKAN_DISABLE_COMPUTE_TONE_MAPPING=1")
add_test(NAME graphics.resource_preparation COMMAND relaycore_vulkan_mesh_tests --resource-preparation)
add_test(NAME graphics.material_descriptors COMMAND relaycore_vulkan_mesh_tests --material-descriptors)
add_test(NAME graphics.pistol_viewmodel COMMAND relaycore_vulkan_mesh_tests --pistol-viewmodel)
add_test(NAME graphics.shotgun_viewmodel COMMAND relaycore_vulkan_mesh_tests --shotgun-viewmodel)
add_test(NAME graphics.vulkan_color_baseline COMMAND relaycore_vulkan_mesh_tests --color-baseline)
add_test(NAME graphics.hdr_pipeline COMMAND relaycore_vulkan_mesh_tests --hdr)
add_test(NAME graphics.msaa COMMAND relaycore_vulkan_mesh_tests --msaa)
add_test(NAME graphics.visual_effects COMMAND relaycore_vulkan_mesh_tests --visual-effects)
add_test(NAME graphics.visual_effects_raster COMMAND relaycore_vulkan_mesh_tests --visual-effects)
add_test(NAME graphics.advanced_effects COMMAND relaycore_vulkan_mesh_tests --advanced-effects)
add_test(NAME graphics.advanced_effects_raster COMMAND relaycore_vulkan_mesh_tests --advanced-effects)
add_test(NAME graphics.temporal_scene COMMAND relaycore_vulkan_mesh_tests --temporal-scene)
add_test(NAME graphics.dynamic_resolution COMMAND relaycore_vulkan_mesh_tests --temporal-dynamic)
add_test(NAME graphics.temporal COMMAND relaycore_vulkan_mesh_tests --temporal)
add_test(NAME graphics.temporal_raster COMMAND relaycore_vulkan_mesh_tests --temporal)
add_test(NAME graphics.pass_inputs COMMAND relaycore_vulkan_mesh_tests --pass-inputs)
add_test(NAME graphics.irradiance_probes COMMAND relaycore_vulkan_mesh_tests --irradiance)
add_test(NAME graphics.material_maps_reflections COMMAND relaycore_vulkan_mesh_tests --metallic-roughness)
add_test(NAME graphics.emissive_maps COMMAND relaycore_vulkan_mesh_tests --emission)
add_test(NAME graphics.point_shadow_receiver COMMAND relaycore_vulkan_mesh_tests --point-receiver-only)
set_tests_properties(graphics.mesh_shaders graphics.mesh_shaders_fallback graphics.descriptor_indexing graphics.descriptor_indexing_fallback graphics.scene_culling_fallback graphics.scene_culling graphics.scene_submission graphics.local_light_selection graphics.advanced_effects graphics.advanced_effects_raster graphics.dynamic_resolution graphics.temporal_scene graphics.temporal graphics.temporal_raster graphics.visual_effects graphics.visual_effects_raster graphics.pass_inputs graphics.depth_formats graphics.msaa graphics.vulkan_mesh graphics.mesh_pipeline graphics.power_restoration_slice graphics.game_hud graphics.resource_preparation graphics.material_descriptors graphics.pistol_viewmodel graphics.shotgun_viewmodel graphics.vulkan_color_baseline graphics.hdr_pipeline graphics.irradiance_probes graphics.material_maps_reflections graphics.emissive_maps graphics.point_shadow_receiver PROPERTIES
    LABELS "integration;graphics"
    RUN_SERIAL TRUE
    SKIP_RETURN_CODE 77
    TIMEOUT 180
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_vulkan_mesh_tests>"
    ENVIRONMENT "RELAYCORE_ASSET_ROOT=$<TARGET_FILE_DIR:relaycore_demo>/assets"
    FAIL_REGULAR_EXPRESSION "Vulkan validation (error|warning);Vulkan .* shutdown failed")
set_property(TEST graphics.mesh_shaders_fallback APPEND PROPERTY ENVIRONMENT
    "RELAYCORE_VULKAN_DISABLE_MESH_SHADERS=1")
set_property(TEST graphics.descriptor_indexing_fallback APPEND PROPERTY ENVIRONMENT
    "RELAYCORE_VULKAN_DISABLE_DESCRIPTOR_INDEXING=1")
set_property(TEST graphics.scene_culling_fallback APPEND PROPERTY ENVIRONMENT
    "RELAYCORE_VULKAN_DISABLE_GPU_CULLING=1")
set_property(TEST graphics.advanced_effects_raster APPEND PROPERTY ENVIRONMENT
    "RELAYCORE_VULKAN_DISABLE_COMPUTE_TONE_MAPPING=1")
set_property(TEST graphics.temporal_raster APPEND PROPERTY ENVIRONMENT
    "RELAYCORE_VULKAN_DISABLE_COMPUTE_TONE_MAPPING=1")
set_property(TEST graphics.visual_effects_raster APPEND PROPERTY ENVIRONMENT
    "RELAYCORE_VULKAN_DISABLE_COMPUTE_TONE_MAPPING=1")
set_tests_properties(graphics.vulkan_upload PROPERTIES
    LABELS "integration;graphics"
    RUN_SERIAL TRUE
    SKIP_RETURN_CODE 77
    TIMEOUT 120
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_vulkan_upload_tests>"
    FAIL_REGULAR_EXPRESSION "Vulkan validation (error|warning);Vulkan .* shutdown failed"
)
add_test(NAME graphics.vulkan_recreation_failure COMMAND relaycore_vulkan_presentation_tests --allocation-failure)
add_test(NAME graphics.surface_loss COMMAND relaycore_vulkan_presentation_tests --surface-loss)
set_tests_properties(graphics.vulkan_presentation graphics.vulkan_recreation_failure graphics.surface_loss PROPERTIES
    LABELS "integration;graphics"
    RUN_SERIAL TRUE
    SKIP_RETURN_CODE 77
    TIMEOUT 120
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_vulkan_presentation_tests>"
    FAIL_REGULAR_EXPRESSION "Vulkan validation (error|warning);Vulkan .* shutdown failed"
)
# OBS retries failed vkCreateSwapchainKHR with an already retired oldSwapchain.
# Isolate native allocation failure injection; the normal presentation test retains installed hooks.
set_tests_properties(graphics.vulkan_recreation_failure PROPERTIES ENVIRONMENT "DISABLE_VULKAN_OBS_CAPTURE=1")
set_tests_properties(graphics.vulkan_device PROPERTIES
    LABELS "integration;graphics"
    RUN_SERIAL TRUE
    SKIP_RETURN_CODE 77
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_vulkan_device_tests>"
    FAIL_REGULAR_EXPRESSION "Vulkan validation (error|warning);Vulkan device shutdown failed"
)
if(RELAYCORE_ENABLE_VULKAN_VALIDATION AND CMAKE_BUILD_TYPE MATCHES "^(Debug|Asan)$")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/empty-vulkan-layers")
    add_test(NAME graphics.vulkan_missing_validation
        COMMAND relaycore_vulkan_device_tests --expect-missing-validation)
    set_tests_properties(graphics.vulkan_missing_validation PROPERTIES
        LABELS "integration;graphics"
        RUN_SERIAL TRUE
        SKIP_RETURN_CODE 77
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_vulkan_device_tests>"
        ENVIRONMENT "VK_LAYER_PATH=${CMAKE_CURRENT_BINARY_DIR}/empty-vulkan-layers"
    )
endif()
set_tests_properties(graphics.vulkan_window PROPERTIES
    LABELS "integration;graphics"
    RUN_SERIAL TRUE
    SKIP_RETURN_CODE 77
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_vulkan_window_tests>"
)
add_test(NAME graphics.game_lifecycle COMMAND relaycore_game_lifecycle)
add_test(NAME graphics.campaign_route COMMAND relaycore_game_lifecycle --campaign-route)
add_test(NAME graphics.bench_relay COMMAND relaycore_bench_relay_run)
add_test(NAME graphics.pressure_transfer COMMAND relaycore_bench_relay_run --pressure-transfer)
add_test(NAME graphics.cooling_loop COMMAND relaycore_bench_relay_run --cooling-loop)
set_tests_properties(graphics.cooling_loop PROPERTIES
    LABELS "integration;graphics" RUN_SERIAL TRUE SKIP_RETURN_CODE 77 TIMEOUT 180
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_bench_relay_run>"
    ENVIRONMENT "RELAYCORE_ASSET_ROOT=$<TARGET_FILE_DIR:relaycore_demo>/assets"
    FAIL_REGULAR_EXPRESSION "Vulkan validation (error|warning);Vulkan .* shutdown failed")
add_test(NAME graphics.pressure_transfer_visual COMMAND relaycore_vulkan_mesh_tests --pressure-transfer-slice)
set_tests_properties(graphics.pressure_transfer_visual PROPERTIES
    LABELS "integration;graphics" RUN_SERIAL TRUE SKIP_RETURN_CODE 77 TIMEOUT 180
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_vulkan_mesh_tests>"
    ENVIRONMENT "RELAYCORE_ASSET_ROOT=$<TARGET_FILE_DIR:relaycore_demo>/assets"
    FAIL_REGULAR_EXPRESSION "Vulkan validation (error|warning);Vulkan .* shutdown failed")
set_tests_properties(graphics.pressure_transfer PROPERTIES LABELS "integration;graphics" RUN_SERIAL TRUE SKIP_RETURN_CODE 77
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_bench_relay_run>"
    ENVIRONMENT "RELAYCORE_ASSET_ROOT=$<TARGET_FILE_DIR:relaycore_demo>/assets")
set_tests_properties(graphics.bench_relay PROPERTIES LABELS "integration;graphics" RUN_SERIAL TRUE SKIP_RETURN_CODE 77
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_bench_relay_run>"
    ENVIRONMENT "RELAYCORE_ASSET_ROOT=$<TARGET_FILE_DIR:relaycore_demo>/assets")
add_test(NAME graphics.hdr_settings COMMAND relaycore_game_lifecycle --hdr-settings)
if(WIN32)
    foreach(mode IN ITEMS ui no-ui failed-ui failed-no-ui)
        add_test(NAME graphics.update_readiness.${mode} COMMAND relaycore_game_lifecycle --update-readiness ${mode})
        set_tests_properties(graphics.update_readiness.${mode} PROPERTIES
            LABELS "integration;graphics"
            RUN_SERIAL TRUE
            SKIP_RETURN_CODE 77
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_game_lifecycle>"
            ENVIRONMENT "RELAYCORE_ASSET_ROOT=$<TARGET_FILE_DIR:relaycore_demo>/assets")
    endforeach()
endif()
set_tests_properties(graphics.game_lifecycle graphics.campaign_route graphics.hdr_settings PROPERTIES
    LABELS "integration;graphics"
    RUN_SERIAL TRUE
    SKIP_RETURN_CODE 77
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_game_lifecycle>"
    ENVIRONMENT "RELAYCORE_ASSET_ROOT=$<TARGET_FILE_DIR:relaycore_demo>/assets"
)
add_test(NAME graphics.power_restoration COMMAND relaycore_power_restoration_graphics)
add_test(NAME graphics.hdr_episode COMMAND relaycore_power_restoration_graphics --hdr)
add_test(NAME graphics.foundation_intake COMMAND relaycore_power_restoration_graphics --intake-views)
add_test(NAME graphics.foundation_intake_sdr COMMAND relaycore_power_restoration_graphics --intake-views-sdr)
add_test(NAME graphics.foundation_pump COMMAND relaycore_power_restoration_graphics --pump-views)
add_test(NAME graphics.foundation_pump_sdr COMMAND relaycore_power_restoration_graphics --pump-views-sdr)
add_test(NAME graphics.character_pickups COMMAND relaycore_power_restoration_graphics --character-views)
add_test(NAME graphics.character_pickups_sdr COMMAND relaycore_power_restoration_graphics --character-views-sdr)
add_test(NAME graphics.player_body COMMAND relaycore_power_restoration_graphics --body-views)
add_test(NAME graphics.player_body_sdr COMMAND relaycore_power_restoration_graphics --body-views-sdr)
add_test(NAME graphics.world_effects COMMAND relaycore_power_restoration_graphics --effect-views)
add_test(NAME graphics.world_effects_sdr COMMAND relaycore_power_restoration_graphics --effect-views-sdr)
set_tests_properties(graphics.world_effects graphics.world_effects_sdr PROPERTIES
    LABELS "integration;graphics" RUN_SERIAL TRUE SKIP_RETURN_CODE 77 TIMEOUT 180
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_power_restoration_graphics>"
    ENVIRONMENT "RELAYCORE_ASSET_ROOT=$<TARGET_FILE_DIR:relaycore_demo>/assets"
    FAIL_REGULAR_EXPRESSION "Vulkan validation (error|warning);Vulkan .* shutdown failed")
set_tests_properties(graphics.player_body graphics.player_body_sdr PROPERTIES
    LABELS "integration;graphics" RUN_SERIAL TRUE SKIP_RETURN_CODE 77 TIMEOUT 120
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_power_restoration_graphics>"
    ENVIRONMENT "RELAYCORE_ASSET_ROOT=$<TARGET_FILE_DIR:relaycore_demo>/assets"
    FAIL_REGULAR_EXPRESSION "Vulkan validation (error|warning);Vulkan .* shutdown failed")
set_tests_properties(graphics.character_pickups graphics.character_pickups_sdr PROPERTIES
    LABELS "integration;graphics" RUN_SERIAL TRUE SKIP_RETURN_CODE 77 TIMEOUT 300
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_power_restoration_graphics>"
    ENVIRONMENT "RELAYCORE_ASSET_ROOT=$<TARGET_FILE_DIR:relaycore_demo>/assets"
    FAIL_REGULAR_EXPRESSION "Vulkan validation (error|warning);Vulkan .* shutdown failed")
add_test(NAME graphics.b17 COMMAND relaycore_power_restoration_graphics --b17-views)
add_test(NAME graphics.b17_sdr COMMAND relaycore_power_restoration_graphics --b17-views-sdr)
add_test(NAME graphics.instruments COMMAND relaycore_power_restoration_graphics --instrument-views)
add_test(NAME graphics.light_fixtures COMMAND relaycore_power_restoration_graphics --fixture-views)
add_test(NAME graphics.light_fixtures_sdr COMMAND relaycore_power_restoration_graphics --fixture-views-sdr)
add_test(NAME graphics.room_lighting COMMAND relaycore_power_restoration_graphics --lighting-views)
add_test(NAME graphics.room_lighting_sdr COMMAND relaycore_power_restoration_graphics --lighting-views-sdr)
add_test(NAME graphics.contact_quality COMMAND relaycore_power_restoration_graphics --contact-views)
add_test(NAME graphics.contact_quality_sdr COMMAND relaycore_power_restoration_graphics --contact-views-sdr)
add_test(NAME graphics.route_atmosphere COMMAND relaycore_power_restoration_graphics --atmosphere-views)
add_test(NAME graphics.route_atmosphere_sdr COMMAND relaycore_power_restoration_graphics --atmosphere-views-sdr)
add_test(NAME graphics.instruments_sdr COMMAND relaycore_power_restoration_graphics --instrument-views-sdr)
add_test(NAME graphics.route_signage COMMAND relaycore_power_restoration_graphics --route-views)
add_test(NAME graphics.route_signage_sdr COMMAND relaycore_power_restoration_graphics --route-views-sdr)
add_test(NAME graphics.sector_signage COMMAND relaycore_power_restoration_graphics --signage-views)
add_test(NAME graphics.sector_signage_sdr COMMAND relaycore_power_restoration_graphics --signage-views-sdr)
add_test(NAME graphics.weapon_impacts COMMAND relaycore_power_restoration_graphics --impact-marks)
add_test(NAME graphics.weapon_impacts_hdr COMMAND relaycore_power_restoration_graphics --impact-marks-hdr)
set_tests_properties(graphics.weapon_impacts graphics.weapon_impacts_hdr PROPERTIES
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_power_restoration_graphics>"
    ENVIRONMENT "RELAYCORE_ASSET_ROOT=$<TARGET_FILE_DIR:relaycore_demo>/assets"
    RUN_SERIAL TRUE SKIP_RETURN_CODE 77
    FAIL_REGULAR_EXPRESSION "Vulkan validation (error|warning);Vulkan .* shutdown failed")
add_test(NAME graphics.special_materials COMMAND relaycore_power_restoration_graphics --surface-views)
add_test(NAME graphics.special_materials_sdr COMMAND relaycore_power_restoration_graphics --surface-views-sdr)
add_test(NAME graphics.interactive_hardware COMMAND relaycore_power_restoration_graphics --hardware-views)
add_test(NAME graphics.interactive_hardware_sdr COMMAND relaycore_power_restoration_graphics --hardware-views-sdr)
set_tests_properties(graphics.contact_quality graphics.contact_quality_sdr graphics.route_atmosphere graphics.route_atmosphere_sdr graphics.light_fixtures graphics.light_fixtures_sdr graphics.room_lighting graphics.room_lighting_sdr graphics.instruments graphics.instruments_sdr graphics.route_signage graphics.route_signage_sdr graphics.sector_signage graphics.sector_signage_sdr graphics.foundation_intake graphics.foundation_intake_sdr graphics.foundation_pump graphics.foundation_pump_sdr graphics.b17 graphics.b17_sdr graphics.special_materials graphics.special_materials_sdr graphics.interactive_hardware graphics.interactive_hardware_sdr PROPERTIES
    LABELS "integration;graphics"
    RUN_SERIAL TRUE
    SKIP_RETURN_CODE 77
    TIMEOUT 120
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_power_restoration_graphics>"
    ENVIRONMENT "RELAYCORE_ASSET_ROOT=$<TARGET_FILE_DIR:relaycore_demo>/assets"
    FAIL_REGULAR_EXPRESSION "Vulkan validation (error|warning);Vulkan .* shutdown failed")
set_tests_properties(graphics.power_restoration graphics.hdr_episode PROPERTIES
    LABELS "integration;graphics"
    RUN_SERIAL TRUE
    SKIP_RETURN_CODE 77
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_power_restoration_graphics>"
    ENVIRONMENT "RELAYCORE_ASSET_ROOT=$<TARGET_FILE_DIR:relaycore_demo>/assets"
)
set_tests_properties(
    graphics.window_lifetime
    PROPERTIES
        LABELS "integration;graphics"
        RUN_SERIAL TRUE
        SKIP_RETURN_CODE 77
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_window_lifetime_tests>"
        ENVIRONMENT "RELAYCORE_ASSET_ROOT=$<TARGET_FILE_DIR:relaycore_demo>/assets"
)


set_tests_properties(graphics.window_lifetime graphics.game_lifecycle graphics.campaign_route graphics.hdr_settings graphics.power_restoration graphics.hdr_episode graphics.bench_relay graphics.pressure_transfer PROPERTIES
    FAIL_REGULAR_EXPRESSION "Vulkan validation (error|warning);Vulkan .* shutdown failed")

doctest_discover_tests(
    relaycore_tests
    TEST_PREFIX "relaycore."
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_tests>"
    PROPERTIES ENVIRONMENT "RELAYCORE_ASSET_ROOT=$<TARGET_FILE_DIR:relaycore_demo>/assets"
)

doctest_discover_tests(
    relaycore_content_validation
    TEST_PREFIX "validation."
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_content_validation>"
    PROPERTIES ENVIRONMENT "RELAYCORE_ASSET_ROOT=$<TARGET_FILE_DIR:relaycore_demo>/assets"
)

doctest_discover_tests(
    relaycore_physics_tests
    TEST_PREFIX "relaycore.physics."
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:relaycore_physics_tests>"
    PROPERTIES ENVIRONMENT "RELAYCORE_ASSET_ROOT=${RELAYCORE_COLLISION_FIXTURE_ROOT}"
)

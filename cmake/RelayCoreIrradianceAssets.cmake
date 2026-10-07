set(probe_products)
set(probe_runtime_paths)
foreach(name IN ITEMS native_lab bench_relay occult_lab_b17)
    set(probe_product "${RELAYCORE_MODEL_ASSET_ROOT}/lighting/${name}.rcprobe")
    add_custom_command(
        OUTPUT "${probe_product}"
        COMMAND ${CMAKE_COMMAND}
            "-DRELAYCORE_PROBE_BAKER=$<TARGET_FILE:relaycore_irradiance_bake>"
            "-DRELAYCORE_PROBE_LEVEL=${PROJECT_SOURCE_DIR}/assets/levels/${name}.toml"
            "-DRELAYCORE_PROBE_AUTHORING=${PROJECT_SOURCE_DIR}/assets/config/render_authoring.toml"
            "-DRELAYCORE_PROBE_PRODUCT=${probe_product}"
            "-DRELAYCORE_PROBE_MODEL_ROOT=${RELAYCORE_MODEL_ASSET_ROOT}"
            -P "${PROJECT_SOURCE_DIR}/cmake/compile_irradiance_asset.cmake"
        DEPENDS relaycore_irradiance_bake ${model_products}
            "${PROJECT_SOURCE_DIR}/assets/levels/${name}.toml"
            "${PROJECT_SOURCE_DIR}/assets/config/render_authoring.toml"
            "${CMAKE_CURRENT_LIST_FILE}"
            "${PROJECT_SOURCE_DIR}/cmake/compile_irradiance_asset.cmake"
        COMMENT "Baking ${name} diffuse irradiance"
        VERBATIM)
    list(APPEND probe_products "${probe_product}")
    list(APPEND probe_runtime_paths "lighting/${name}.rcprobe")
endforeach()
add_custom_target(relaycore_irradiance_assets DEPENDS ${probe_products})
list(JOIN probe_runtime_paths "\n" probe_manifest)
file(CONFIGURE OUTPUT "${PROJECT_BINARY_DIR}/generated/runtime-assets.txt"
    CONTENT "${model_manifest}\n${collision_manifest}\n${probe_manifest}\n" @ONLY)

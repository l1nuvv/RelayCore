set(probe_product "${RELAYCORE_MODEL_ASSET_ROOT}/lighting/native_lab.rcprobe")
add_custom_command(
    OUTPUT "${probe_product}"
    COMMAND ${CMAKE_COMMAND}
        "-DRELAYCORE_PROBE_BAKER=$<TARGET_FILE:relaycore_irradiance_bake>"
        "-DRELAYCORE_PROBE_LEVEL=${PROJECT_SOURCE_DIR}/assets/levels/native_lab.toml"
        "-DRELAYCORE_PROBE_AUTHORING=${PROJECT_SOURCE_DIR}/assets/config/render_authoring.toml"
        "-DRELAYCORE_PROBE_PRODUCT=${probe_product}"
        "-DRELAYCORE_PROBE_MODEL_ROOT=${RELAYCORE_MODEL_ASSET_ROOT}"
        -P "${PROJECT_SOURCE_DIR}/cmake/compile_irradiance_asset.cmake"
    DEPENDS relaycore_irradiance_bake ${model_products}
        "${PROJECT_SOURCE_DIR}/assets/levels/native_lab.toml"
        "${PROJECT_SOURCE_DIR}/assets/config/render_authoring.toml"
        "${CMAKE_CURRENT_LIST_FILE}"
        "${PROJECT_SOURCE_DIR}/cmake/compile_irradiance_asset.cmake"
    COMMENT "Baking native lab diffuse irradiance"
    VERBATIM)
add_custom_target(relaycore_irradiance_assets DEPENDS "${probe_product}")
file(CONFIGURE OUTPUT "${PROJECT_BINARY_DIR}/generated/runtime-assets.txt"
    CONTENT "${model_manifest}\nlighting/native_lab.rcprobe\n" @ONLY)

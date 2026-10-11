set(reflection_product "${RELAYCORE_MODEL_ASSET_ROOT}/lighting/containment_loop.rcreflection")
add_custom_command(
    OUTPUT "${reflection_product}"
    COMMAND ${CMAKE_COMMAND}
        "-DRELAYCORE_PROBE_BAKER=$<TARGET_FILE:relaycore_reflection_bake>"
        "-DRELAYCORE_PROBE_LEVEL=${PROJECT_SOURCE_DIR}/assets/levels/containment_loop.toml"
        "-DRELAYCORE_PROBE_AUTHORING=${PROJECT_SOURCE_DIR}/assets/config/render_authoring.toml"
        "-DRELAYCORE_PROBE_PRODUCT=${reflection_product}"
        "-DRELAYCORE_PROBE_MODEL_ROOT=${RELAYCORE_MODEL_ASSET_ROOT}"
        -P "${PROJECT_SOURCE_DIR}/cmake/compile_irradiance_asset.cmake"
    DEPENDS relaycore_reflection_bake ${model_products}
        "${PROJECT_SOURCE_DIR}/assets/levels/containment_loop.toml"
        "${PROJECT_SOURCE_DIR}/assets/config/render_authoring.toml"
        "${CMAKE_CURRENT_LIST_FILE}"
        "${PROJECT_SOURCE_DIR}/cmake/compile_irradiance_asset.cmake"
    COMMENT "Baking containment hall reflection cube"
    VERBATIM)
add_custom_target(relaycore_reflection_assets DEPENDS "${reflection_product}")
file(CONFIGURE OUTPUT "${PROJECT_BINARY_DIR}/generated/runtime-assets.txt"
    CONTENT "${model_manifest}\n${collision_manifest}\n${probe_manifest}\nlighting/containment_loop.rcreflection\n" @ONLY)

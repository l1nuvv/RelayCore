# Each declaration explicitly pairs an authored collision source with a native model product.
set(RELAYCORE_COLLISION_SOURCES lab_bench power_pump power_filtration b17_observation_camera)
set(RELAYCORE_COLLISION_MODEL_lab_bench lab_bench)
set(RELAYCORE_COLLISION_MODEL_power_pump power_pump)
set(RELAYCORE_COLLISION_MODEL_power_filtration power_filtration)
set(RELAYCORE_COLLISION_MODEL_b17_observation_camera b17_observation_camera)
set(collision_runtime_paths)
set(collision_products)
foreach(name IN LISTS RELAYCORE_COLLISION_SOURCES)
    if(NOT name MATCHES "^[A-Za-z0-9_-]+$")
        message(FATAL_ERROR "Invalid collision name: ${name}")
    endif()
    set(model "${RELAYCORE_COLLISION_MODEL_${name}}")
    if(NOT "${model}.glb" IN_LIST RELAYCORE_MODEL_SOURCES)
        message(FATAL_ERROR "Collision ${name} references an undeclared model: ${model}")
    endif()
    set(runtime_path "collision/${name}.rccollision")
    set(product "${RELAYCORE_MODEL_ASSET_ROOT}/${runtime_path}")
    set(receipt "${PROJECT_BINARY_DIR}/generated/collision-receipts/${name}.json")
    set(model_product "${RELAYCORE_MODEL_ASSET_ROOT}/models/${model}.rcmodel")
    add_custom_command(OUTPUT "${product}" "${receipt}"
        COMMAND ${CMAKE_COMMAND}
            "-DSOURCE=${PROJECT_SOURCE_DIR}/content/collision/${name}.toml"
            "-DPRODUCT=${product}" "-DRECEIPT=${receipt}"
            "-DCOMPILER=$<TARGET_FILE:relaycore_collision_compile>"
            "-DMODEL=${model_product}" "-DMODEL_REFERENCE=models/${model}.rcmodel"
            "-DDECLARATION=${CMAKE_CURRENT_LIST_FILE}"
            -P "${CMAKE_CURRENT_LIST_DIR}/compile_collision_asset.cmake"
        DEPENDS relaycore_collision_compile "${model_product}"
            "${PROJECT_SOURCE_DIR}/content/collision/${name}.toml"
            "${CMAKE_CURRENT_LIST_FILE}" "${CMAKE_CURRENT_LIST_DIR}/compile_collision_asset.cmake"
        COMMENT "Compiling collision ${name}" VERBATIM)
    list(APPEND collision_runtime_paths "${runtime_path}")
    list(APPEND collision_products "${product}" "${receipt}")
endforeach()
add_custom_target(relaycore_collision_assets DEPENDS ${collision_products})
list(JOIN collision_runtime_paths "\n" collision_manifest)
file(CONFIGURE OUTPUT "${PROJECT_BINARY_DIR}/generated/runtime-collisions.txt"
    CONTENT "${collision_manifest}\n" @ONLY)

# Stage only the products needed by the isolated physics fixture, without a lighting bake.
set(RELAYCORE_COLLISION_FIXTURE_ROOT "${PROJECT_BINARY_DIR}/physics-fixture/assets")
file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/generated/collision-fixture-source")
file(CONFIGURE OUTPUT "${PROJECT_BINARY_DIR}/generated/runtime-collision-fixture.txt"
    CONTENT "${model_manifest}\n${collision_manifest}\n" @ONLY)
add_custom_target(relaycore_collision_fixture_assets
    COMMAND ${CMAKE_COMMAND}
        "-DRELAYCORE_ASSET_SOURCE=${PROJECT_BINARY_DIR}/generated/collision-fixture-source"
        "-DRELAYCORE_GENERATED_ASSETS=${RELAYCORE_MODEL_ASSET_ROOT}"
        "-DRELAYCORE_GENERATED_ASSET_MANIFEST=${PROJECT_BINARY_DIR}/generated/runtime-collision-fixture.txt"
        "-DRELAYCORE_ASSET_OUTPUT=${RELAYCORE_COLLISION_FIXTURE_ROOT}"
        "-DRELAYCORE_ASSET_MANIFEST=${PROJECT_BINARY_DIR}/physics-fixture/assets.manifest"
        "-DCOLLISION_PROJECT_ROOT=${PROJECT_SOURCE_DIR}"
        "-DCOLLISION_BUILD_ROOT=${PROJECT_BINARY_DIR}"
        "-DCOLLISION_COMPILER=$<TARGET_FILE:relaycore_collision_compile>"
        -P "${CMAKE_CURRENT_LIST_DIR}/sync_runtime_assets.cmake"
    VERBATIM)
add_dependencies(relaycore_collision_fixture_assets relaycore_model_assets relaycore_collision_assets)

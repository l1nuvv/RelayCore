set(RELAYCORE_MODEL_SOURCES
    service_cylinder.glb
    lab_bench.glb
    power_cabinet.glb
    crt_terminal.glb
    instrument_rack.glb
    ventilation_unit.glb
    resonance_apparatus.glb
    facility_sign.glb
    scientist_hazmat.glb
    concrete_arch.glb
    utility_pipe.glb
    lab_notes_board.glb
    safety_notice.glb
    lab_accessories.glb
    control_console.glb
    lab_microscope.glb
    lab_multimeter.glb
    surveillance_monitor.glb
    analog_navigation_instrument.glb
    fluorescent_fixture.glb
)
set(RELAYCORE_MODEL_ASSET_ROOT "${PROJECT_BINARY_DIR}/generated/model-assets")
set(RELAYCORE_MODEL_RECEIPT_ROOT "${PROJECT_BINARY_DIR}/generated/model-receipts")
set(model_products)
set(model_runtime_paths)
foreach(name IN ITEMS lab_bench power_cabinet crt_terminal instrument_rack ventilation_unit
        resonance_apparatus facility_sign scientist_hazmat concrete_arch utility_pipe
        lab_notes_board safety_notice lab_accessories control_console lab_microscope
        lab_multimeter surveillance_monitor analog_navigation_instrument fluorescent_fixture)
    set(RELAYCORE_MODEL_METALLIC_ROUGHNESS_${name} ON)
endforeach()
foreach(source IN LISTS RELAYCORE_MODEL_SOURCES)
    if(NOT source MATCHES "^[A-Za-z0-9_-]+\\.glb$")
        message(FATAL_ERROR "Model source must be a flat .glb filename using letters, digits, '_' or '-': ${source}")
    endif()
    get_filename_component(name "${source}" NAME_WE)
    set(runtime_path "models/${name}.rcmodel")
    set(product "${RELAYCORE_MODEL_ASSET_ROOT}/${runtime_path}")
    set(receipt "${RELAYCORE_MODEL_RECEIPT_ROOT}/${runtime_path}.json")
    set(metallic_roughness OFF)
    if(RELAYCORE_MODEL_METALLIC_ROUGHNESS_${name})
        set(metallic_roughness ON)
    endif()
    add_custom_command(
        OUTPUT "${product}" "${receipt}"
        COMMAND ${CMAKE_COMMAND}
            "-DRELAYCORE_MODEL_SOURCE=${PROJECT_SOURCE_DIR}/content/models/${source}"
            "-DRELAYCORE_MODEL_PRODUCT=${product}"
            "-DRELAYCORE_MODEL_RECEIPT=${receipt}"
            "-DRELAYCORE_MODEL_IMPORTER=$<TARGET_FILE:relaycore_model_import>"
            "-DRELAYCORE_MODEL_DECLARATION=${CMAKE_CURRENT_LIST_FILE}"
            "-DRELAYCORE_MODEL_METALLIC_ROUGHNESS=${metallic_roughness}"
            "-DRELAYCORE_PROJECT_ROOT=${PROJECT_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_LIST_DIR}/compile_model_asset.cmake"
        DEPENDS relaycore_model_import "${PROJECT_SOURCE_DIR}/content/models/${source}"
            "${CMAKE_CURRENT_LIST_FILE}" "${CMAKE_CURRENT_LIST_DIR}/compile_model_asset.cmake"
        COMMENT "Compiling model ${source}"
        VERBATIM)
    list(APPEND model_products "${product}" "${receipt}")
    list(APPEND model_runtime_paths "${runtime_path}")
endforeach()
add_custom_target(relaycore_model_assets DEPENDS ${model_products})
list(JOIN model_runtime_paths "\n" model_manifest)
file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/generated")
file(CONFIGURE OUTPUT "${PROJECT_BINARY_DIR}/generated/runtime-models.txt"
    CONTENT "${model_manifest}\n" @ONLY)

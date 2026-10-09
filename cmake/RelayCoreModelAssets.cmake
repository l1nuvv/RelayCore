set(RELAYCORE_MODEL_SOURCES
    shotgun_world.glb
    shotgun_view_body.glb
    shotgun_view_pump.glb
    pistol_frame.glb
    pistol_slide.glb
    b17_observation_camera.glb
    electrical_notes_board.glb
    pressure_notes_board.glb
    cooling_notes_board.glb
    pressure_route_signage.glb
    cooling_route_signage.glb
    power_route_signage.glb
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
    pressure_lab_notes_board.glb
    pressure_safety_notice.glb
    cooling_lab_notes_board.glb
    cooling_safety_notice.glb
    power_lab_notes_board.glb
    power_safety_notice.glb
    blood_pentagram.glb
    occult_research_board.glb
    lab_accessories.glb
    control_console.glb
    lab_microscope.glb
    lab_multimeter.glb
    surveillance_monitor.glb
    analog_navigation_instrument.glb
    fluorescent_fixture.glb
    pressure_manifold.glb
    transfer_cover_casing.glb
    pressure_control_casing.glb
    cooling_control_casing.glb
    transfer_room_lining.glb
    equipment_bay_lining.glb
    equipment_bay_furniture.glb
    service_hall_lining.glb
    isolation_gallery_lining.glb
    transfer_exit_lining.glb
    power_intake.glb
    power_service.glb
    power_switchgear.glb
    power_maintenance.glb
    power_filtration.glb
    power_pump.glb
    bench_workshop.glb
    cooling_exchanger.glb
    cooling_service.glb
    foundation_intake.glb
    service_key.glb
    foundation_pump_machine.glb
    foundation_pump_room.glb
    b17_services.glb
    b17_lining.glb
    door_lift.glb
    door_slide.glb
    panel_door.glb
    panel_isolator.glb
    panel_start.glb
    panel_instrument.glb
    guardian_body.glb
    guardian_arm.glb
    containment_grip.glb
    containment_ram.glb
    limiter_cassette.glb
    mobile_shooter.glb
    mobile_shooter_destroyed.glb
    containment_lining.glb
)
set(RELAYCORE_MODEL_ASSET_ROOT "${PROJECT_BINARY_DIR}/generated/model-assets")
set(RELAYCORE_MODEL_RECEIPT_ROOT "${PROJECT_BINARY_DIR}/generated/model-receipts")
set(model_products)
set(model_runtime_paths)
foreach(name IN ITEMS b17_observation_camera electrical_notes_board pressure_notes_board cooling_notes_board pressure_route_signage cooling_route_signage power_route_signage lab_bench power_cabinet crt_terminal instrument_rack ventilation_unit
        resonance_apparatus facility_sign scientist_hazmat concrete_arch utility_pipe
        blood_pentagram occult_research_board
        pressure_lab_notes_board pressure_safety_notice cooling_lab_notes_board cooling_safety_notice power_lab_notes_board power_safety_notice
        lab_notes_board safety_notice lab_accessories control_console lab_microscope
        lab_multimeter surveillance_monitor analog_navigation_instrument fluorescent_fixture
        pressure_manifold transfer_cover_casing pressure_control_casing cooling_control_casing transfer_room_lining
        equipment_bay_lining equipment_bay_furniture service_hall_lining isolation_gallery_lining
        transfer_exit_lining power_intake power_service power_switchgear power_maintenance
        power_filtration power_pump bench_workshop cooling_exchanger cooling_service
        foundation_intake service_key foundation_pump_machine foundation_pump_room b17_services b17_lining
        door_lift door_slide panel_door panel_isolator panel_start panel_instrument pistol_frame pistol_slide
        shotgun_view_body shotgun_view_pump shotgun_world
        guardian_body guardian_arm containment_grip containment_ram limiter_cassette
        mobile_shooter mobile_shooter_destroyed containment_lining)
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

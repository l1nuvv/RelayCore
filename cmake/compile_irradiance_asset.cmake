cmake_minimum_required(VERSION 4.0)
foreach(required IN ITEMS RELAYCORE_PROBE_BAKER RELAYCORE_PROBE_LEVEL RELAYCORE_PROBE_AUTHORING
        RELAYCORE_PROBE_PRODUCT RELAYCORE_PROBE_MODEL_ROOT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "RELAYCORE_ASSET_ROOT=${RELAYCORE_PROBE_MODEL_ROOT}"
        "${RELAYCORE_PROBE_BAKER}" "${RELAYCORE_PROBE_LEVEL}" "${RELAYCORE_PROBE_AUTHORING}"
        "${RELAYCORE_PROBE_PRODUCT}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Irradiance bake failed: ${RELAYCORE_PROBE_LEVEL}\n${output}${error}")
endif()
message(STATUS "${output}")

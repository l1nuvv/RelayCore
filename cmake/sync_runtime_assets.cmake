cmake_minimum_required(VERSION 4.0)

foreach(required IN ITEMS RELAYCORE_ASSET_SOURCE RELAYCORE_ASSET_OUTPUT RELAYCORE_ASSET_MANIFEST)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

if(NOT IS_DIRECTORY "${RELAYCORE_ASSET_SOURCE}")
    message(FATAL_ERROR "Asset source directory does not exist: ${RELAYCORE_ASSET_SOURCE}")
endif()

file(MAKE_DIRECTORY "${RELAYCORE_ASSET_OUTPUT}")
file(
    GLOB_RECURSE source_files
    LIST_DIRECTORIES false
    RELATIVE "${RELAYCORE_ASSET_SOURCE}"
    "${RELAYCORE_ASSET_SOURCE}/*"
)
list(SORT source_files)

if(EXISTS "${RELAYCORE_ASSET_MANIFEST}")
    file(STRINGS "${RELAYCORE_ASSET_MANIFEST}" previous_files)
    set(output_path "${RELAYCORE_ASSET_OUTPUT}")
    foreach(relative_path IN LISTS previous_files)
        if(relative_path IN_LIST source_files)
            continue()
        endif()
        set(stale_path "${RELAYCORE_ASSET_OUTPUT}/${relative_path}")
        cmake_path(IS_PREFIX output_path "${stale_path}" NORMALIZE within_output)
        if(IS_ABSOLUTE "${relative_path}" OR NOT within_output)
            message(FATAL_ERROR "Invalid staged asset path in manifest: ${relative_path}")
        endif()
        file(REMOVE "${stale_path}")
        if(EXISTS "${stale_path}")
            message(FATAL_ERROR "Could not remove stale staged asset: ${stale_path}")
        endif()
    endforeach()
endif()

foreach(relative_path IN LISTS source_files)
    set(destination "${RELAYCORE_ASSET_OUTPUT}/${relative_path}")
    get_filename_component(destination_directory "${destination}" DIRECTORY)
    file(MAKE_DIRECTORY "${destination_directory}")
    file(COPY_FILE "${RELAYCORE_ASSET_SOURCE}/${relative_path}" "${destination}" ONLY_IF_DIFFERENT)
endforeach()

list(JOIN source_files "\n" manifest_contents)
if(NOT manifest_contents STREQUAL "")
    string(APPEND manifest_contents "\n")
endif()
file(WRITE "${RELAYCORE_ASSET_MANIFEST}.tmp" "${manifest_contents}")
file(RENAME "${RELAYCORE_ASSET_MANIFEST}.tmp" "${RELAYCORE_ASSET_MANIFEST}")

# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
if(NOT DEFINED SOURCE_DIR OR SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "SOURCE_DIR must be provided to SyncLaunchScripts.cmake")
endif()

if(NOT DEFINED DEST_DIR OR DEST_DIR STREQUAL "")
    message(FATAL_ERROR "DEST_DIR must be provided to SyncLaunchScripts.cmake")
endif()

if(NOT DEFINED FILE_NAMES OR FILE_NAMES STREQUAL "")
    message(FATAL_ERROR "FILE_NAMES must be provided to SyncLaunchScripts.cmake")
endif()

file(MAKE_DIRECTORY "${DEST_DIR}")

set(launch_script_names ${FILE_NAMES})

foreach(launch_script_name IN LISTS launch_script_names)
    if(launch_script_name STREQUAL "")
        continue()
    endif()

    set(source_path "${SOURCE_DIR}/${launch_script_name}")
    if(NOT EXISTS "${source_path}")
        message(FATAL_ERROR "Expected launch script not found: ${source_path}")
    endif()

    file(COPY_FILE "${source_path}" "${DEST_DIR}/${launch_script_name}" ONLY_IF_DIFFERENT)

    get_filename_component(script_stem "${launch_script_name}" NAME_WE)
    set(legacy_script_path "${DEST_DIR}/${script_stem}.ps1")
    if(EXISTS "${legacy_script_path}")
        file(REMOVE "${legacy_script_path}")
    endif()
endforeach()

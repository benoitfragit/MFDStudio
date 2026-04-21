# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
if(NOT DEFINED SOURCE_DIR OR SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "SOURCE_DIR is required.")
endif()

if(NOT DEFINED DEST_DIR OR DEST_DIR STREQUAL "")
    message(FATAL_ERROR "DEST_DIR is required.")
endif()

if(NOT EXISTS "${SOURCE_DIR}")
    message(FATAL_ERROR "Asset source directory does not exist: ${SOURCE_DIR}")
endif()

file(MAKE_DIRECTORY "${DEST_DIR}")

file(GLOB_RECURSE source_files
    LIST_DIRECTORIES false
    RELATIVE "${SOURCE_DIR}"
    "${SOURCE_DIR}/*")

file(GLOB_RECURSE dest_files
    LIST_DIRECTORIES false
    RELATIVE "${DEST_DIR}"
    "${DEST_DIR}/*")

foreach(dest_file IN LISTS dest_files)
    if(EXISTS "${SOURCE_DIR}/${dest_file}")
        continue()
    endif()

    file(REMOVE "${DEST_DIR}/${dest_file}")
endforeach()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${SOURCE_DIR}" "${DEST_DIR}"
    RESULT_VARIABLE copy_result)

if(NOT copy_result EQUAL 0)
    message(FATAL_ERROR "Unable to copy asset tree from '${SOURCE_DIR}' to '${DEST_DIR}'")
endif()

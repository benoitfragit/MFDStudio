# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
if(DEFINED SOURCE_DIRS_ENCODED AND NOT SOURCE_DIRS_ENCODED STREQUAL "")
    string(REPLACE "#" ";" SOURCE_DIRS "${SOURCE_DIRS_ENCODED}")
endif()

if(NOT DEFINED SOURCE_DIRS OR SOURCE_DIRS STREQUAL "")
    message(FATAL_ERROR "SOURCE_DIRS is required.")
endif()

if(NOT DEFINED DEST_DIR OR DEST_DIR STREQUAL "")
    message(FATAL_ERROR "DEST_DIR is required.")
endif()

if(NOT DEFINED CLEAN_DEST)
    set(CLEAN_DEST ON)
endif()

if(CLEAN_DEST)
    file(REMOVE_RECURSE "${DEST_DIR}")
endif()

file(MAKE_DIRECTORY "${DEST_DIR}")

foreach(source_dir IN LISTS SOURCE_DIRS)
    if(NOT EXISTS "${source_dir}")
        message(FATAL_ERROR "Asset source directory does not exist: ${source_dir}")
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${source_dir}" "${DEST_DIR}"
        RESULT_VARIABLE copy_result)

    if(NOT copy_result EQUAL 0)
        message(FATAL_ERROR "Unable to copy asset tree from '${source_dir}' to '${DEST_DIR}'")
    endif()
endforeach()

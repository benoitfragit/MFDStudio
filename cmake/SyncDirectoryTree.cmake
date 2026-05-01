# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
if(NOT DEFINED SOURCE_DIR OR SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "SOURCE_DIR must be provided to SyncDirectoryTree.cmake")
endif()

if(NOT DEFINED DEST_DIR OR DEST_DIR STREQUAL "")
    message(FATAL_ERROR "DEST_DIR must be provided to SyncDirectoryTree.cmake")
endif()

if(NOT EXISTS "${SOURCE_DIR}")
    message(FATAL_ERROR "Source directory does not exist: ${SOURCE_DIR}")
endif()

file(REMOVE_RECURSE "${DEST_DIR}")
file(MAKE_DIRECTORY "${DEST_DIR}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${SOURCE_DIR}" "${DEST_DIR}"
    RESULT_VARIABLE copy_result)

if(NOT copy_result EQUAL 0)
    message(FATAL_ERROR "Unable to copy directory tree from '${SOURCE_DIR}' to '${DEST_DIR}'")
endif()

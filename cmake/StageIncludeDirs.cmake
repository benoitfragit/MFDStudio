# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
if(NOT DEFINED SOURCE_DIRS OR SOURCE_DIRS STREQUAL "")
    message(FATAL_ERROR "SOURCE_DIRS must be provided to StageIncludeDirs.cmake")
endif()

if(NOT DEFINED DEST_DIR OR DEST_DIR STREQUAL "")
    message(FATAL_ERROR "DEST_DIR must be provided to StageIncludeDirs.cmake")
endif()

file(MAKE_DIRECTORY "${DEST_DIR}")

string(REPLACE "|" ";" include_source_dirs "${SOURCE_DIRS}")

foreach(include_source_dir IN LISTS include_source_dirs)
    if(include_source_dir STREQUAL "")
        continue()
    endif()

    if(NOT EXISTS "${include_source_dir}")
        message(FATAL_ERROR "Include source directory does not exist: ${include_source_dir}")
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${include_source_dir}" "${DEST_DIR}"
        RESULT_VARIABLE copy_result)

    if(NOT copy_result EQUAL 0)
        message(FATAL_ERROR "Unable to copy include directory '${include_source_dir}' to '${DEST_DIR}'")
    endif()
endforeach()

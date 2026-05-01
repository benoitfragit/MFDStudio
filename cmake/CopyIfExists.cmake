# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
if(NOT DEFINED SOURCE OR SOURCE STREQUAL "")
    return()
endif()

if(NOT DEFINED DEST_FILE OR DEST_FILE STREQUAL "")
    message(FATAL_ERROR "DEST_FILE must be provided to CopyIfExists.cmake")
endif()

if(NOT EXISTS "${SOURCE}")
    return()
endif()

get_filename_component(dest_directory "${DEST_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${dest_directory}")
file(COPY_FILE "${SOURCE}" "${DEST_FILE}" ONLY_IF_DIFFERENT)

# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
if(NOT DEFINED DEST_DIR OR DEST_DIR STREQUAL "")
    message(FATAL_ERROR "DEST_DIR must be provided to CopyRuntimeDependencies.cmake")
endif()

file(MAKE_DIRECTORY "${DEST_DIR}")

if(NOT DEFINED RUNTIME_DLLS OR RUNTIME_DLLS STREQUAL "")
    return()
endif()

string(REPLACE "|" ";" runtime_dlls "${RUNTIME_DLLS}")
list(REMOVE_DUPLICATES runtime_dlls)

foreach(runtime_dll IN LISTS runtime_dlls)
    if(runtime_dll STREQUAL "")
        continue()
    endif()

    if(NOT EXISTS "${runtime_dll}")
        message(FATAL_ERROR "Expected runtime DLL not found: ${runtime_dll}")
    endif()

    get_filename_component(runtime_dll_name "${runtime_dll}" NAME)
    file(COPY_FILE "${runtime_dll}" "${DEST_DIR}/${runtime_dll_name}" ONLY_IF_DIFFERENT)
endforeach()

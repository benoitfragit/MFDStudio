# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio

function(mfd_public_library target)
    if(ARGC LESS 2)
        message(FATAL_ERROR
            "mfd_public_library(<target> <library> [<library>...]) requires at least one library.")
    endif()

    target_link_libraries(${target} PUBLIC ${ARGN})
endfunction()

function(mfd_private_library target)
    if(ARGC LESS 2)
        message(FATAL_ERROR
            "mfd_private_library(<target> <library> [<library>...]) requires at least one library.")
    endif()

    target_link_libraries(${target} PRIVATE ${ARGN})
endfunction()

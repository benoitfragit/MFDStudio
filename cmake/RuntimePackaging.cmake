# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
if(CMAKE_BINARY_DIR MATCHES [[(^|[/\\])vs2022-win32([/\\]|$)]])
    set(MFD_EXEC_PLATFORM_NAME "win32")
elseif(CMAKE_BINARY_DIR MATCHES [[(^|[/\\])vs2022-x64([/\\]|$)]])
    set(MFD_EXEC_PLATFORM_NAME "x64")
elseif(DEFINED CMAKE_GENERATOR_PLATFORM AND NOT CMAKE_GENERATOR_PLATFORM STREQUAL "")
    set(MFD_EXEC_PLATFORM_NAME "${CMAKE_GENERATOR_PLATFORM}")
elseif(DEFINED CMAKE_VS_PLATFORM_NAME AND NOT CMAKE_VS_PLATFORM_NAME STREQUAL "")
    set(MFD_EXEC_PLATFORM_NAME "${CMAKE_VS_PLATFORM_NAME}")
elseif(DEFINED MSVC_CXX_ARCHITECTURE_ID AND NOT MSVC_CXX_ARCHITECTURE_ID STREQUAL "")
    set(MFD_EXEC_PLATFORM_NAME "${MSVC_CXX_ARCHITECTURE_ID}")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(MFD_EXEC_PLATFORM_NAME "x64")
else()
    set(MFD_EXEC_PLATFORM_NAME "win32")
endif()

string(TOLOWER "${MFD_EXEC_PLATFORM_NAME}" MFD_EXEC_PLATFORM_DIR)

if(MSVC AND DEFINED MSVC_TOOLSET_VERSION AND NOT MSVC_TOOLSET_VERSION STREQUAL "")
    set(MFD_EXEC_COMPILER_DIR "v${MSVC_TOOLSET_VERSION}")
else()
    set(MFD_EXEC_COMPILER_DIR "v${PROJECT_VERSION}")
endif()

function(mfd_stage_runtime target_name)
    set(copy_assets OFF)
    set(copy_runtime_dlls OFF)

    foreach(arg IN LISTS ARGN)
        if(arg STREQUAL "WITH_ASSETS")
            set(copy_assets ON)
        elseif(arg STREQUAL "WITH_RUNTIME_DLLS")
            set(copy_runtime_dlls ON)
        endif()
    endforeach()

    set(stage_dir "${MFD_ROOT_DIR}/_Exec/${MFD_EXEC_COMPILER_DIR}/${MFD_EXEC_PLATFORM_DIR}/$<CONFIG>")

    set(runtime_dll_commands)
    if(copy_runtime_dlls AND WIN32)
        list(APPEND runtime_dll_commands
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:${target_name}> "${stage_dir}")
    endif()

    set(asset_commands)
    if(copy_assets)
        list(APPEND asset_commands
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${MFD_ROOT_DIR}/assets" "${stage_dir}/assets")
    endif()

    add_custom_command(
        TARGET ${target_name}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${stage_dir}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${target_name}>" "${stage_dir}"
        ${runtime_dll_commands}
        ${asset_commands}
        COMMAND_EXPAND_LISTS)
endfunction()

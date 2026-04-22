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
            COMMAND
                ${CMAKE_COMMAND}
                "-DSOURCE_DIR=${MFD_ROOT_DIR}/assets"
                "-DDEST_DIR=${stage_dir}/assets"
                -P
                "${MFD_ROOT_DIR}/cmake/SyncAssetTree.cmake")
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

function(mfd_add_runtime_layout_smoke_test target_name)
    if(NOT WIN32 OR NOT MFD_BUILD_TESTS)
        return()
    endif()

    set(options VERIFY_ASSETS)
    set(one_value_args NAME)
    set(multi_value_args EXTRA_RUNTIME_TARGETS EXTRA_FILES LABELS)
    cmake_parse_arguments(MFD_RUNTIME_SMOKE
        "${options}"
        "${one_value_args}"
        "${multi_value_args}"
        ${ARGN})

    if(NOT MFD_RUNTIME_SMOKE_NAME)
        set(MFD_RUNTIME_SMOKE_NAME "${target_name}_runtime_layout_smoke")
    endif()

    set(expected_runtime_dlls "$<JOIN:$<TARGET_RUNTIME_DLLS:${target_name}>,|>")
    foreach(runtime_target IN LISTS MFD_RUNTIME_SMOKE_EXTRA_RUNTIME_TARGETS)
        string(APPEND expected_runtime_dlls "|$<JOIN:$<TARGET_RUNTIME_DLLS:${runtime_target}>,|>")
    endforeach()

    add_test(
        NAME ${MFD_RUNTIME_SMOKE_NAME}
        COMMAND
            ${CMAKE_COMMAND}
            "-DBUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
            "-DBUILD_CONFIG=$<CONFIG>"
            "-DTARGET_NAME=${target_name}"
            "-DTARGET_DIRECTORY=$<TARGET_FILE_DIR:${target_name}>"
            "-DEXPECTED_RUNTIME_DLLS=${expected_runtime_dlls}"
            "-DEXPECTED_FILES=$<JOIN:${MFD_RUNTIME_SMOKE_EXTRA_FILES},|>"
            "$<$<BOOL:${MFD_RUNTIME_SMOKE_VERIFY_ASSETS}>:-DASSET_SOURCE_DIR=${MFD_ROOT_DIR}/assets>"
            "$<$<BOOL:${MFD_RUNTIME_SMOKE_VERIFY_ASSETS}>:-DASSET_TARGET_DIR=$<TARGET_FILE_DIR:${target_name}>/assets>"
            -P
            "${MFD_ROOT_DIR}/cmake/VerifyRuntimeLayout.cmake")

    set(test_labels "smoke;runtime")
    if(MFD_RUNTIME_SMOKE_LABELS)
        list(APPEND test_labels ${MFD_RUNTIME_SMOKE_LABELS})
    endif()

    set_tests_properties(${MFD_RUNTIME_SMOKE_NAME} PROPERTIES LABELS "${test_labels}")
endfunction()

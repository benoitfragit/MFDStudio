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

function(mfd_resolve_stage_dir out_var)
    set(options)
    set(one_value_args STAGE_SUBDIR)
    cmake_parse_arguments(MFD_STAGE_DIR "${options}" "${one_value_args}" "" ${ARGN})

    set(stage_dir "${MFD_ROOT_DIR}/_Exec/${MFD_EXEC_COMPILER_DIR}/${MFD_EXEC_PLATFORM_DIR}/$<CONFIG>")
    if(MFD_STAGE_DIR_STAGE_SUBDIR)
        string(APPEND stage_dir "/${MFD_STAGE_DIR_STAGE_SUBDIR}")
    endif()

    set(${out_var} "${stage_dir}" PARENT_SCOPE)
endfunction()

function(mfd_configure_test_runtime_output target_name)
    set(options)
    set(one_value_args BUILD_SUBDIR)
    cmake_parse_arguments(MFD_TEST_OUTPUT "${options}" "${one_value_args}" "" ${ARGN})

    if(NOT MFD_TEST_OUTPUT_BUILD_SUBDIR)
        set(MFD_TEST_OUTPUT_BUILD_SUBDIR "tests")
    endif()

    if(CMAKE_CONFIGURATION_TYPES)
        foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)
            string(TOUPPER "${config}" config_upper)
            set_target_properties(${target_name} PROPERTIES
                "RUNTIME_OUTPUT_DIRECTORY_${config_upper}" "${CMAKE_BINARY_DIR}/${MFD_TEST_OUTPUT_BUILD_SUBDIR}/${config}")
        endforeach()
    else()
        set_target_properties(${target_name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${MFD_TEST_OUTPUT_BUILD_SUBDIR}")
    endif()
endfunction()

function(mfd_stage_runtime target_name)
    set(options WITH_ASSETS WITH_BRANDING WITH_RUNTIME_DLLS)
    set(one_value_args STAGE_SUBDIR)
    cmake_parse_arguments(MFD_STAGE_RUNTIME "${options}" "${one_value_args}" "" ${ARGN})

    set(copy_assets ${MFD_STAGE_RUNTIME_WITH_ASSETS})
    set(copy_runtime_dlls ${MFD_STAGE_RUNTIME_WITH_RUNTIME_DLLS})
    set(copy_branding ${MFD_STAGE_RUNTIME_WITH_BRANDING})

    mfd_resolve_stage_dir(stage_dir STAGE_SUBDIR "${MFD_STAGE_RUNTIME_STAGE_SUBDIR}")
    set(runtime_dlls_arg "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:${target_name}>,$<SEMICOLON>>")

    set(runtime_dll_commands)
    if(copy_runtime_dlls AND WIN32)
        list(APPEND runtime_dll_commands
            COMMAND
                ${CMAKE_COMMAND}
                "-DDEST_DIR=${stage_dir}"
                "${runtime_dlls_arg}"
                -P
                "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake")
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

    set(branding_commands)
    if(copy_branding)
        list(APPEND branding_commands
            COMMAND
                ${CMAKE_COMMAND}
                "-DSOURCE_DIR=${MFD_ROOT_DIR}/branding"
                "-DDEST_DIR=${stage_dir}/branding"
                -P
                "${MFD_ROOT_DIR}/cmake/SyncDirectoryTree.cmake")
    endif()

    add_custom_command(
        TARGET ${target_name}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${stage_dir}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${target_name}>" "${stage_dir}"
        ${runtime_dll_commands}
        ${asset_commands}
        ${branding_commands}
        VERBATIM)
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

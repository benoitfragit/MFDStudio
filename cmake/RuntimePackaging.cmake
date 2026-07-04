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

function(_mfd_runtime_stage_register
         target_name
         stage_subdir
         copy_assets
         copy_branding
         copy_runtime_dlls
         root_asset_dirs_encoded
         script_source_dir)
    set(script_names ${ARGN})
    string(JOIN "#" script_names_encoded ${script_names})
    set(registration
        "${target_name}|${stage_subdir}|${copy_assets}|${copy_branding}|${copy_runtime_dlls}|${root_asset_dirs_encoded}|${script_source_dir}|${script_names_encoded}")
    set_property(GLOBAL APPEND PROPERTY MFD_RUNTIME_STAGE_REGISTRATIONS "${registration}")
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
    set(one_value_args STAGE_SUBDIR SCRIPT_SOURCE_DIR)
    set(multi_value_args SCRIPT_NAMES ROOT_ASSET_DIRS)
    cmake_parse_arguments(MFD_STAGE_RUNTIME "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    set(copy_assets ${MFD_STAGE_RUNTIME_WITH_ASSETS})
    set(copy_runtime_dlls ${MFD_STAGE_RUNTIME_WITH_RUNTIME_DLLS})
    set(copy_branding ${MFD_STAGE_RUNTIME_WITH_BRANDING})

    if(MFD_STAGE_RUNTIME_SCRIPT_NAMES
       AND (NOT MFD_STAGE_RUNTIME_SCRIPT_SOURCE_DIR OR NOT EXISTS "${MFD_STAGE_RUNTIME_SCRIPT_SOURCE_DIR}"))
        message(FATAL_ERROR "mfd_stage_runtime requires SCRIPT_SOURCE_DIR when SCRIPT_NAMES are provided.")
    endif()

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

    set(root_asset_names)
    foreach(root_asset_dir IN LISTS MFD_STAGE_RUNTIME_ROOT_ASSET_DIRS)
        if(NOT EXISTS "${root_asset_dir}")
            message(FATAL_ERROR "Root asset directory does not exist: ${root_asset_dir}")
        endif()

        get_filename_component(root_asset_name "${root_asset_dir}" NAME)
        string(MAKE_C_IDENTIFIER "${root_asset_name}" root_asset_key)
        list(FIND root_asset_names "${root_asset_key}" root_asset_name_index)
        if(root_asset_name_index EQUAL -1)
            list(APPEND root_asset_names "${root_asset_key}")
            set(root_asset_destination_name_${root_asset_key} "${root_asset_name}")
            set(root_asset_sources_${root_asset_key})
        endif()

        list(APPEND root_asset_sources_${root_asset_key} "${root_asset_dir}")
    endforeach()

    set(root_asset_commands)
    foreach(root_asset_key IN LISTS root_asset_names)
        string(JOIN "#" root_asset_source_list_encoded ${root_asset_sources_${root_asset_key}})
        set(root_asset_name "${root_asset_destination_name_${root_asset_key}}")
        list(APPEND root_asset_commands
            COMMAND
                ${CMAKE_COMMAND}
                "-DSOURCE_DIRS_ENCODED=${root_asset_source_list_encoded}"
                "-DDEST_DIR=${stage_dir}/${root_asset_name}"
                "-DCLEAN_DEST=OFF"
                -P
                "${MFD_ROOT_DIR}/cmake/SyncMergedAssetTrees.cmake")
    endforeach()

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

    set(runtime_script_commands)
    if(MFD_STAGE_RUNTIME_SCRIPT_NAMES)
        string(JOIN "#" runtime_script_list_encoded ${MFD_STAGE_RUNTIME_SCRIPT_NAMES})
        list(APPEND runtime_script_commands
            COMMAND
                ${CMAKE_COMMAND}
                "-DSOURCE_DIR=${MFD_STAGE_RUNTIME_SCRIPT_SOURCE_DIR}"
                "-DDEST_DIR=${stage_dir}"
                "-DFILE_NAMES_ENCODED=${runtime_script_list_encoded}"
                -P
                "${MFD_ROOT_DIR}/cmake/SyncLaunchScripts.cmake")
    endif()

    add_custom_command(
        TARGET ${target_name}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${stage_dir}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${target_name}>" "${stage_dir}"
        ${runtime_dll_commands}
        ${asset_commands}
        ${root_asset_commands}
        ${branding_commands}
        ${runtime_script_commands}
        VERBATIM)

    string(JOIN "#" root_asset_dirs_encoded ${MFD_STAGE_RUNTIME_ROOT_ASSET_DIRS})
    _mfd_runtime_stage_register(
        "${target_name}"
        "${MFD_STAGE_RUNTIME_STAGE_SUBDIR}"
        "${copy_assets}"
        "${copy_branding}"
        "${copy_runtime_dlls}"
        "${root_asset_dirs_encoded}"
        "${MFD_STAGE_RUNTIME_SCRIPT_SOURCE_DIR}"
        ${MFD_STAGE_RUNTIME_SCRIPT_NAMES})
endfunction()

function(mfd_finalize_runtime_stage_targets)
    get_property(runtime_stage_registrations GLOBAL PROPERTY MFD_RUNTIME_STAGE_REGISTRATIONS)
    if(NOT runtime_stage_registrations)
        return()
    endif()

    set(runtime_stage_keys)
    foreach(registration IN LISTS runtime_stage_registrations)
        string(REPLACE "|" ";" registration_fields "${registration}")
        list(GET registration_fields 1 stage_subdir)
        if(stage_subdir STREQUAL "")
            set(stage_key "__root__")
        else()
            set(stage_key "${stage_subdir}")
        endif()

        list(FIND runtime_stage_keys "${stage_key}" stage_key_index)
        if(stage_key_index EQUAL -1)
            list(APPEND runtime_stage_keys "${stage_key}")
        endif()
    endforeach()

    add_custom_target(stage_exec ALL)

    foreach(stage_key IN LISTS runtime_stage_keys)
        if(stage_key STREQUAL "__root__")
            set(stage_subdir "")
            set(stage_target_name "stage_exec_root")
        else()
            set(stage_subdir "${stage_key}")
            string(MAKE_C_IDENTIFIER "stage_exec_${stage_subdir}" stage_target_name)
        endif()

        mfd_resolve_stage_dir(stage_dir STAGE_SUBDIR "${stage_subdir}")

        set(stage_commands
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${stage_dir}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${stage_dir}")
        set(stage_dependencies)
        set(stage_root_asset_dirs)

        foreach(registration IN LISTS runtime_stage_registrations)
            string(REPLACE "|" ";" registration_fields "${registration}")
            list(GET registration_fields 0 target_name)
            list(GET registration_fields 1 registration_stage_subdir)
            list(GET registration_fields 2 copy_assets)
            list(GET registration_fields 3 copy_branding)
            list(GET registration_fields 4 copy_runtime_dlls)
            list(GET registration_fields 5 root_asset_dirs_encoded)
            list(GET registration_fields 6 script_source_dir)
            list(GET registration_fields 7 script_names_encoded)

            if(stage_subdir STREQUAL "")
                if(NOT registration_stage_subdir STREQUAL "")
                    continue()
                endif()
            elseif(NOT registration_stage_subdir STREQUAL "${stage_subdir}")
                continue()
            endif()

            list(APPEND stage_dependencies "${target_name}")
            list(APPEND stage_commands
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:${target_name}>"
                    "${stage_dir}/$<TARGET_FILE_NAME:${target_name}>")

            if(copy_runtime_dlls AND WIN32)
                list(APPEND stage_commands
                    COMMAND
                        ${CMAKE_COMMAND}
                        "-DDEST_DIR=${stage_dir}"
                        "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:${target_name}>,$<SEMICOLON>>"
                        -P
                        "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake")
            endif()

            if(copy_assets)
                list(APPEND stage_commands
                    COMMAND
                        ${CMAKE_COMMAND}
                        "-DSOURCE_DIR=${MFD_ROOT_DIR}/assets"
                        "-DDEST_DIR=${stage_dir}/assets"
                        -P
                        "${MFD_ROOT_DIR}/cmake/SyncAssetTree.cmake")
            endif()

            if(NOT root_asset_dirs_encoded STREQUAL "")
                string(REPLACE "#" ";" root_asset_dirs "${root_asset_dirs_encoded}")
                list(APPEND stage_root_asset_dirs ${root_asset_dirs})
            endif()

            if(copy_branding)
                list(APPEND stage_commands
                    COMMAND
                        ${CMAKE_COMMAND}
                        "-DSOURCE_DIR=${MFD_ROOT_DIR}/branding"
                        "-DDEST_DIR=${stage_dir}/branding"
                        -P
                        "${MFD_ROOT_DIR}/cmake/SyncDirectoryTree.cmake")
            endif()

            if(NOT script_names_encoded STREQUAL "")
                string(REPLACE "#" ";" script_names "${script_names_encoded}")
                string(JOIN "#" script_name_list_encoded ${script_names})
                list(APPEND stage_commands
                    COMMAND
                        ${CMAKE_COMMAND}
                        "-DSOURCE_DIR=${script_source_dir}"
                        "-DDEST_DIR=${stage_dir}"
                        "-DFILE_NAMES_ENCODED=${script_name_list_encoded}"
                        -P
                "${MFD_ROOT_DIR}/cmake/SyncLaunchScripts.cmake")
            endif()
        endforeach()

        set(stage_root_asset_names)
        foreach(root_asset_dir IN LISTS stage_root_asset_dirs)
            get_filename_component(root_asset_name "${root_asset_dir}" NAME)
            string(MAKE_C_IDENTIFIER "${root_asset_name}" root_asset_key)
            list(FIND stage_root_asset_names "${root_asset_key}" root_asset_name_index)
            if(root_asset_name_index EQUAL -1)
                list(APPEND stage_root_asset_names "${root_asset_key}")
                set(stage_root_asset_destination_name_${root_asset_key} "${root_asset_name}")
                set(stage_root_asset_sources_${root_asset_key})
            endif()

            list(APPEND stage_root_asset_sources_${root_asset_key} "${root_asset_dir}")
        endforeach()

        foreach(root_asset_key IN LISTS stage_root_asset_names)
            string(JOIN "#" root_asset_source_list_encoded ${stage_root_asset_sources_${root_asset_key}})
            set(root_asset_name "${stage_root_asset_destination_name_${root_asset_key}}")
            list(APPEND stage_commands
                COMMAND
                    ${CMAKE_COMMAND}
                    "-DSOURCE_DIRS_ENCODED=${root_asset_source_list_encoded}"
                    "-DDEST_DIR=${stage_dir}/${root_asset_name}"
                    -P
                    "${MFD_ROOT_DIR}/cmake/SyncMergedAssetTrees.cmake")
        endforeach()

        add_custom_target(${stage_target_name}
            ${stage_commands}
            DEPENDS ${stage_dependencies}
            VERBATIM)
        add_dependencies(stage_exec ${stage_target_name})
    endforeach()
endfunction()

function(mfd_add_runtime_layout_smoke_test target_name)
    if(NOT WIN32 OR NOT MFD_BUILD_TESTS)
        return()
    endif()

    set(options VERIFY_ASSETS ALLOW_EXTRA_ASSETS)
    set(one_value_args NAME ASSET_SOURCE_DIR)
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

    set(asset_source_dir "${MFD_ROOT_DIR}/assets")
    if(MFD_RUNTIME_SMOKE_ASSET_SOURCE_DIR)
        set(asset_source_dir "${MFD_RUNTIME_SMOKE_ASSET_SOURCE_DIR}")
    endif()

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
            "$<$<BOOL:${MFD_RUNTIME_SMOKE_VERIFY_ASSETS}>:-DASSET_SOURCE_DIR=${asset_source_dir}>"
            "$<$<BOOL:${MFD_RUNTIME_SMOKE_VERIFY_ASSETS}>:-DASSET_TARGET_DIR=$<TARGET_FILE_DIR:${target_name}>/assets>"
            "$<$<BOOL:${MFD_RUNTIME_SMOKE_ALLOW_EXTRA_ASSETS}>:-DALLOW_EXTRA_ASSETS=ON>"
            -P
            "${MFD_ROOT_DIR}/cmake/VerifyRuntimeLayout.cmake")

    set(test_labels "smoke;runtime")
    if(MFD_RUNTIME_SMOKE_LABELS)
        list(APPEND test_labels ${MFD_RUNTIME_SMOKE_LABELS})
    endif()

    set_tests_properties(${MFD_RUNTIME_SMOKE_NAME} PROPERTIES LABELS "${test_labels}")
endfunction()

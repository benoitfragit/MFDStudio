# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
include(CMakePackageConfigHelpers)

set(MFD_DELIVERIES_ROOT "${MFD_ROOT_DIR}/_Deliveries")
set(MFD_DELIVERIES_DEV_ROOT "${MFD_DELIVERIES_ROOT}/packages_windows")
set(MFD_DELIVERIES_RUNTIME_ROOT "${MFD_DELIVERIES_ROOT}/packages_bin_windows")

function(mfd_delivery_resolve_dev_package_root out_var package_name)
    set(${out_var} "${MFD_DELIVERIES_DEV_ROOT}/${package_name}/build/native" PARENT_SCOPE)
endfunction()

function(mfd_delivery_resolve_runtime_package_root out_var package_name)
    set(${out_var} "${MFD_DELIVERIES_RUNTIME_ROOT}/${package_name}" PARENT_SCOPE)
endfunction()

function(_mfd_delivery_make_stage_target_name out_var prefix package_name)
    string(MAKE_C_IDENTIFIER "${prefix}_${package_name}" sanitized_target_name)
    set(${out_var} "${sanitized_target_name}" PARENT_SCOPE)
endfunction()

function(_mfd_delivery_register_stage_target target_name)
    set_property(GLOBAL APPEND PROPERTY MFD_DELIVERY_STAGE_TARGETS "${target_name}")
endfunction()

function(mfd_register_dev_package)
    set(options HEADER_ONLY)
    set(one_value_args NAME TARGET IMPORTED_TARGET_NAME CONFIG_TEMPLATE)
    set(multi_value_args
        ADDITIONAL_LIBRARY_TARGETS
        PUBLIC_INCLUDE_DIRS
        PUBLIC_HEADER_FILES
        DOCUMENTATION_FILES
        EXAMPLE_DIRECTORIES
        PACKAGE_CMAKE_FILES
        TOOL_DIRECTORIES)
    cmake_parse_arguments(MFD_DEV "${options}" "${one_value_args}" "${multi_value_args}" "" ${ARGN})

    if(NOT MFD_DEV_NAME)
        message(FATAL_ERROR "mfd_register_dev_package requires NAME.")
    endif()

    if(NOT MFD_DEV_IMPORTED_TARGET_NAME)
        message(FATAL_ERROR "mfd_register_dev_package requires IMPORTED_TARGET_NAME.")
    endif()

    if(NOT MFD_DEV_CONFIG_TEMPLATE)
        message(FATAL_ERROR "mfd_register_dev_package requires CONFIG_TEMPLATE.")
    endif()

    if(NOT EXISTS "${MFD_DEV_CONFIG_TEMPLATE}")
        message(FATAL_ERROR "Dev package config template not found: ${MFD_DEV_CONFIG_TEMPLATE}")
    endif()

    if(NOT MFD_DEV_HEADER_ONLY AND NOT MFD_DEV_TARGET)
        message(FATAL_ERROR "mfd_register_dev_package requires TARGET for non header-only packages.")
    endif()

    foreach(include_dir IN LISTS MFD_DEV_PUBLIC_INCLUDE_DIRS)
        if(NOT EXISTS "${include_dir}")
            message(FATAL_ERROR "Dev package include directory does not exist: ${include_dir}")
        endif()
    endforeach()

    foreach(public_header_file IN LISTS MFD_DEV_PUBLIC_HEADER_FILES)
        if(NOT EXISTS "${public_header_file}")
            message(FATAL_ERROR "Dev package public header file does not exist: ${public_header_file}")
        endif()
    endforeach()

    foreach(documentation_file IN LISTS MFD_DEV_DOCUMENTATION_FILES)
        if(NOT EXISTS "${documentation_file}")
            message(FATAL_ERROR "Dev package documentation file does not exist: ${documentation_file}")
        endif()
    endforeach()

    foreach(example_directory IN LISTS MFD_DEV_EXAMPLE_DIRECTORIES)
        if(NOT EXISTS "${example_directory}")
            message(FATAL_ERROR "Dev package example directory does not exist: ${example_directory}")
        endif()
    endforeach()

    foreach(package_cmake_file IN LISTS MFD_DEV_PACKAGE_CMAKE_FILES)
        if(NOT EXISTS "${package_cmake_file}")
            message(FATAL_ERROR "Dev package CMake helper file does not exist: ${package_cmake_file}")
        endif()
    endforeach()

    foreach(tool_directory IN LISTS MFD_DEV_TOOL_DIRECTORIES)
        if(NOT EXISTS "${tool_directory}")
            message(FATAL_ERROR "Dev package tool directory does not exist: ${tool_directory}")
        endif()
    endforeach()

    mfd_delivery_resolve_dev_package_root(package_root "${MFD_DEV_NAME}")

    set(include_root "${package_root}/include")
    set(cmake_root "${package_root}/cmake")
    set(share_root "${package_root}/share/${MFD_DEV_NAME}")
    set(examples_root "${package_root}/examples")
    set(lib_root "${package_root}/libs/${MFD_EXEC_COMPILER_DIR}/${MFD_EXEC_PLATFORM_DIR}/$<CONFIG>")

    set(MFD_PACKAGE_IMPORTED_TARGET_NAME "${MFD_DEV_IMPORTED_TARGET_NAME}")
    set(MFD_PACKAGE_TOOLSET "${MFD_EXEC_COMPILER_DIR}")

    configure_file(
        "${MFD_DEV_CONFIG_TEMPLATE}"
        "${CMAKE_CURRENT_BINARY_DIR}/${MFD_DEV_NAME}Config.cmake"
        @ONLY)

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/${MFD_DEV_NAME}ConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMajorVersion)

    set(mfd_dev_manifest_targets)
    if(MFD_DEV_TARGET)
        list(APPEND mfd_dev_manifest_targets "${MFD_DEV_TARGET}")
    endif()

    if(MFD_DEV_ADDITIONAL_LIBRARY_TARGETS)
        list(APPEND mfd_dev_manifest_targets ${MFD_DEV_ADDITIONAL_LIBRARY_TARGETS})
    endif()

    if(mfd_dev_manifest_targets)
        list(JOIN mfd_dev_manifest_targets "|" manifest_targets_arg)
    else()
        set(manifest_targets_arg "header-only")
    endif()

    set(stage_commands
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${include_root}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${cmake_root}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${share_root}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${examples_root}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${package_root}/tools"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${include_root}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${cmake_root}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${share_root}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/${MFD_DEV_NAME}Config.cmake"
            "${cmake_root}/${MFD_DEV_NAME}Config.cmake"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/${MFD_DEV_NAME}ConfigVersion.cmake"
            "${cmake_root}/${MFD_DEV_NAME}ConfigVersion.cmake"
        COMMAND ${CMAKE_COMMAND}
            "-DOUTPUT_FILE=${share_root}/package_manifest.json"
            "-DPACKAGE_NAME=${MFD_DEV_NAME}"
            "-DPACKAGE_KIND=development"
            "-DPROJECT_VERSION=${PROJECT_VERSION}"
            "-DTOOLSET=${MFD_EXEC_COMPILER_DIR}"
            "-DTARGETS=${manifest_targets_arg}"
            -P
            "${MFD_ROOT_DIR}/cmake/WritePackageManifest.cmake")

    if(MFD_DEV_PUBLIC_HEADER_FILES)
        foreach(public_header_file IN LISTS MFD_DEV_PUBLIC_HEADER_FILES)
            get_filename_component(public_header_file_abs "${public_header_file}" ABSOLUTE)
            file(TO_CMAKE_PATH "${public_header_file_abs}" public_header_file_abs)

            set(public_header_relative_path "")
            foreach(include_dir IN LISTS MFD_DEV_PUBLIC_INCLUDE_DIRS)
                get_filename_component(include_dir_abs "${include_dir}" ABSOLUTE)
                file(TO_CMAKE_PATH "${include_dir_abs}" include_dir_abs)
                string(LENGTH "${include_dir_abs}" include_dir_length)
                string(SUBSTRING "${public_header_file_abs}" 0 ${include_dir_length} include_prefix)

                if(NOT include_prefix STREQUAL include_dir_abs)
                    continue()
                endif()

                string(SUBSTRING "${public_header_file_abs}" ${include_dir_length} -1 public_header_relative_path)
                string(REGEX REPLACE "^/" "" public_header_relative_path "${public_header_relative_path}")
                break()
            endforeach()

            if(public_header_relative_path STREQUAL "")
                message(FATAL_ERROR
                    "Unable to resolve one include-root-relative path for '${public_header_file_abs}'. "
                    "Add the matching root to PUBLIC_INCLUDE_DIRS.")
            endif()

            get_filename_component(public_header_destination_directory
                "${include_root}/${public_header_relative_path}" DIRECTORY)
            list(APPEND stage_commands
                COMMAND ${CMAKE_COMMAND} -E make_directory "${public_header_destination_directory}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${public_header_file_abs}"
                    "${include_root}/${public_header_relative_path}")
        endforeach()
    else()
        list(JOIN MFD_DEV_PUBLIC_INCLUDE_DIRS "|" include_dirs_arg)
        list(APPEND stage_commands
            COMMAND ${CMAKE_COMMAND}
                "-DSOURCE_DIRS=${include_dirs_arg}"
                "-DDEST_DIR=${include_root}"
                -P
                "${MFD_ROOT_DIR}/cmake/StageIncludeDirs.cmake")
    endif()

    foreach(documentation_file IN LISTS MFD_DEV_DOCUMENTATION_FILES)
        get_filename_component(documentation_file_name "${documentation_file}" NAME)
        list(APPEND stage_commands
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${documentation_file}"
                "${share_root}/${documentation_file_name}")
    endforeach()

    foreach(example_directory IN LISTS MFD_DEV_EXAMPLE_DIRECTORIES)
        get_filename_component(example_directory_name "${example_directory}" NAME)
        list(APPEND stage_commands
            COMMAND ${CMAKE_COMMAND}
                "-DSOURCE_DIR=${example_directory}"
                "-DDEST_DIR=${examples_root}/${example_directory_name}"
                -P
                "${MFD_ROOT_DIR}/cmake/SyncDirectoryTree.cmake")
    endforeach()

    foreach(package_cmake_file IN LISTS MFD_DEV_PACKAGE_CMAKE_FILES)
        get_filename_component(package_cmake_file_name "${package_cmake_file}" NAME)
        list(APPEND stage_commands
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${package_cmake_file}"
                "${cmake_root}/${package_cmake_file_name}")
    endforeach()

    foreach(tool_directory IN LISTS MFD_DEV_TOOL_DIRECTORIES)
        get_filename_component(tool_directory_name "${tool_directory}" NAME)
        list(APPEND stage_commands
            COMMAND ${CMAKE_COMMAND}
                "-DSOURCE_DIR=${tool_directory}"
                "-DDEST_DIR=${package_root}/tools/${tool_directory_name}"
                -P
                "${MFD_ROOT_DIR}/cmake/SyncDirectoryTree.cmake")
    endforeach()

    set(stage_dependencies)
    if(NOT MFD_DEV_HEADER_ONLY)
        set(mfd_dev_library_targets "${MFD_DEV_TARGET}")
        if(MFD_DEV_ADDITIONAL_LIBRARY_TARGETS)
            list(APPEND mfd_dev_library_targets ${MFD_DEV_ADDITIONAL_LIBRARY_TARGETS})
        endif()

        list(APPEND stage_dependencies ${mfd_dev_library_targets})
        list(APPEND stage_commands
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${lib_root}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${lib_root}")

        foreach(mfd_dev_library_target IN LISTS mfd_dev_library_targets)
            get_target_property(mfd_dev_target_type "${mfd_dev_library_target}" TYPE)
            list(APPEND stage_commands
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_LINKER_FILE:${mfd_dev_library_target}>"
                    "${lib_root}/$<TARGET_LINKER_FILE_NAME:${mfd_dev_library_target}>")

            if(NOT mfd_dev_target_type STREQUAL "STATIC_LIBRARY")
                list(APPEND stage_commands
                    COMMAND ${CMAKE_COMMAND}
                        "-DSOURCE=$<TARGET_PDB_FILE:${mfd_dev_library_target}>"
                        "-DDEST_FILE=${lib_root}/$<TARGET_PDB_FILE_NAME:${mfd_dev_library_target}>"
                        -P
                        "${MFD_ROOT_DIR}/cmake/CopyIfExists.cmake")
            endif()
        endforeach()
    endif()

    _mfd_delivery_make_stage_target_name(stage_target_name "stage_dev_package" "${MFD_DEV_NAME}")
    if(stage_dependencies)
        add_custom_target(${stage_target_name}
            ${stage_commands}
            DEPENDS ${stage_dependencies}
            VERBATIM)
    else()
        add_custom_target(${stage_target_name}
            ${stage_commands}
            VERBATIM)
    endif()

    _mfd_delivery_register_stage_target(${stage_target_name})
endfunction()

function(mfd_register_runtime_package)
    set(options WITH_ASSETS WITH_BRANDING)
    set(one_value_args NAME SCRIPT_SOURCE_DIR)
    set(multi_value_args TARGETS SCRIPT_NAMES EXTRA_FILES EXTRA_DIRECTORIES)
    cmake_parse_arguments(MFD_RUNTIME "${options}" "${one_value_args}" "${multi_value_args}" "" ${ARGN})

    if(NOT MFD_RUNTIME_NAME)
        message(FATAL_ERROR "mfd_register_runtime_package requires NAME.")
    endif()

    if(NOT MFD_RUNTIME_TARGETS)
        message(FATAL_ERROR "mfd_register_runtime_package requires at least one TARGETS entry.")
    endif()

    foreach(extra_file IN LISTS MFD_RUNTIME_EXTRA_FILES)
        if(NOT EXISTS "${extra_file}")
            message(FATAL_ERROR "Runtime package extra file does not exist: ${extra_file}")
        endif()
    endforeach()

    foreach(extra_directory IN LISTS MFD_RUNTIME_EXTRA_DIRECTORIES)
        if(NOT EXISTS "${extra_directory}")
            message(FATAL_ERROR "Runtime package extra directory does not exist: ${extra_directory}")
        endif()
    endforeach()

    if(MFD_RUNTIME_WITH_BRANDING AND NOT EXISTS "${MFD_ROOT_DIR}/branding")
        message(FATAL_ERROR "Runtime package requested WITH_BRANDING but '${MFD_ROOT_DIR}/branding' does not exist.")
    endif()

    if(MFD_RUNTIME_SCRIPT_NAMES AND (NOT MFD_RUNTIME_SCRIPT_SOURCE_DIR OR NOT EXISTS "${MFD_RUNTIME_SCRIPT_SOURCE_DIR}"))
        message(FATAL_ERROR "mfd_register_runtime_package requires SCRIPT_SOURCE_DIR when SCRIPT_NAMES are provided.")
    endif()

    mfd_delivery_resolve_runtime_package_root(package_root "${MFD_RUNTIME_NAME}")

    set(exec_root "${package_root}/_Exec")
    set(conf_root "${exec_root}/_Conf")
    set(config_root "${exec_root}/${MFD_EXEC_COMPILER_DIR}/${MFD_EXEC_PLATFORM_DIR}/$<CONFIG>")

    list(JOIN MFD_RUNTIME_TARGETS "|" manifest_targets_arg)

    set(stage_commands
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${config_root}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${conf_root}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${config_root}"
        COMMAND ${CMAKE_COMMAND}
            "-DOUTPUT_FILE=${conf_root}/package_manifest.json"
            "-DPACKAGE_NAME=${MFD_RUNTIME_NAME}"
            "-DPACKAGE_KIND=runtime"
            "-DPROJECT_VERSION=${PROJECT_VERSION}"
            "-DTOOLSET=${MFD_EXEC_COMPILER_DIR}"
            "-DTARGETS=${manifest_targets_arg}"
            -P
            "${MFD_ROOT_DIR}/cmake/WritePackageManifest.cmake")

    foreach(runtime_target IN LISTS MFD_RUNTIME_TARGETS)
        list(APPEND stage_commands
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:${runtime_target}>"
                "${config_root}/$<TARGET_FILE_NAME:${runtime_target}>"
            COMMAND ${CMAKE_COMMAND}
                "-DSOURCE=$<TARGET_PDB_FILE:${runtime_target}>"
                "-DDEST_FILE=${config_root}/$<TARGET_PDB_FILE_NAME:${runtime_target}>"
                -P
                "${MFD_ROOT_DIR}/cmake/CopyIfExists.cmake")

        if(WIN32)
            list(APPEND stage_commands
                COMMAND ${CMAKE_COMMAND}
                    "-DDEST_DIR=${config_root}"
                    "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:${runtime_target}>,$<SEMICOLON>>"
                    -P
                    "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake")
        endif()
    endforeach()

    if(MFD_RUNTIME_WITH_ASSETS)
        list(APPEND stage_commands
            COMMAND ${CMAKE_COMMAND}
                "-DSOURCE_DIR=${MFD_ROOT_DIR}/assets"
                "-DDEST_DIR=${config_root}/assets"
                -P
                "${MFD_ROOT_DIR}/cmake/SyncDirectoryTree.cmake")
    endif()

    if(MFD_RUNTIME_WITH_BRANDING)
        list(APPEND stage_commands
            COMMAND ${CMAKE_COMMAND}
                "-DSOURCE_DIR=${MFD_ROOT_DIR}/branding"
                "-DDEST_DIR=${config_root}/branding"
                -P
                "${MFD_ROOT_DIR}/cmake/SyncDirectoryTree.cmake")
    endif()

    if(MFD_RUNTIME_SCRIPT_NAMES)
        string(JOIN ";" runtime_script_list ${MFD_RUNTIME_SCRIPT_NAMES})
        list(APPEND stage_commands
            COMMAND ${CMAKE_COMMAND}
                "-DSOURCE_DIR=${MFD_RUNTIME_SCRIPT_SOURCE_DIR}"
                "-DDEST_DIR=${config_root}"
                "-DFILE_NAMES=${runtime_script_list}"
                -P
                "${MFD_ROOT_DIR}/cmake/SyncLaunchScripts.cmake")
    endif()

    foreach(extra_file IN LISTS MFD_RUNTIME_EXTRA_FILES)
        get_filename_component(extra_file_name "${extra_file}" NAME)
        list(APPEND stage_commands
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${extra_file}"
                "${config_root}/${extra_file_name}")
    endforeach()

    foreach(extra_directory IN LISTS MFD_RUNTIME_EXTRA_DIRECTORIES)
        get_filename_component(extra_directory_name "${extra_directory}" NAME)
        list(APPEND stage_commands
            COMMAND ${CMAKE_COMMAND}
                "-DSOURCE_DIR=${extra_directory}"
                "-DDEST_DIR=${config_root}/${extra_directory_name}"
                -P
                "${MFD_ROOT_DIR}/cmake/SyncDirectoryTree.cmake")
    endforeach()

    _mfd_delivery_make_stage_target_name(stage_target_name "stage_runtime_package" "${MFD_RUNTIME_NAME}")
    add_custom_target(${stage_target_name}
        ${stage_commands}
        DEPENDS ${MFD_RUNTIME_TARGETS}
        VERBATIM)

    _mfd_delivery_register_stage_target(${stage_target_name})
endfunction()

function(mfd_finalize_delivery_targets)
    get_property(stage_targets GLOBAL PROPERTY MFD_DELIVERY_STAGE_TARGETS)

    if(NOT stage_targets)
        return()
    endif()

    add_custom_target(stage_deliveries)
    add_dependencies(stage_deliveries ${stage_targets})
endfunction()

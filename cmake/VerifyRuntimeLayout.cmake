# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
if(NOT DEFINED BUILD_DIRECTORY OR BUILD_DIRECTORY STREQUAL "")
    message(FATAL_ERROR "BUILD_DIRECTORY is required.")
endif()

if(NOT DEFINED TARGET_NAME OR TARGET_NAME STREQUAL "")
    message(FATAL_ERROR "TARGET_NAME is required.")
endif()

if(NOT DEFINED TARGET_DIRECTORY OR TARGET_DIRECTORY STREQUAL "")
    message(FATAL_ERROR "TARGET_DIRECTORY is required.")
endif()

set(build_command "${CMAKE_COMMAND}" --build "${BUILD_DIRECTORY}" --target "${TARGET_NAME}")
if(DEFINED BUILD_CONFIG AND NOT BUILD_CONFIG STREQUAL "")
    list(APPEND build_command --config "${BUILD_CONFIG}")
endif()

execute_process(
    COMMAND ${build_command}
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_stdout
    ERROR_VARIABLE build_stderr)

if(NOT build_result EQUAL 0)
    message(FATAL_ERROR
        "Failed to build target '${TARGET_NAME}'.\n"
        "stdout:\n${build_stdout}\n"
        "stderr:\n${build_stderr}")
endif()

if(NOT EXISTS "${TARGET_DIRECTORY}")
    message(FATAL_ERROR "Target directory does not exist: ${TARGET_DIRECTORY}")
endif()

if(NOT DEFINED EXPECTED_RUNTIME_DLLS)
    set(EXPECTED_RUNTIME_DLLS "")
endif()

string(REPLACE "|" ";" expected_runtime_dlls "${EXPECTED_RUNTIME_DLLS}")
set(missing_runtime_dlls)

foreach(runtime_dll IN LISTS expected_runtime_dlls)
    if(runtime_dll STREQUAL "")
        continue()
    endif()

    get_filename_component(runtime_dll_name "${runtime_dll}" NAME)
    if(runtime_dll_name STREQUAL "")
        continue()
    endif()

    if(NOT EXISTS "${TARGET_DIRECTORY}/${runtime_dll_name}")
        list(APPEND missing_runtime_dlls "${runtime_dll_name}")
    endif()
endforeach()

list(REMOVE_DUPLICATES missing_runtime_dlls)

if(missing_runtime_dlls)
    string(JOIN "\n  - " missing_runtime_dlls_text ${missing_runtime_dlls})
    message(FATAL_ERROR
        "Missing runtime DLLs in ${TARGET_DIRECTORY}:\n"
        "  - ${missing_runtime_dlls_text}")
endif()

if(NOT DEFINED EXPECTED_FILES)
    set(EXPECTED_FILES "")
endif()

string(REPLACE "|" ";" expected_files "${EXPECTED_FILES}")
set(missing_support_files)

foreach(expected_file IN LISTS expected_files)
    if(expected_file STREQUAL "")
        continue()
    endif()

    if(NOT EXISTS "${TARGET_DIRECTORY}/${expected_file}")
        list(APPEND missing_support_files "${expected_file}")
    endif()
endforeach()

list(REMOVE_DUPLICATES missing_support_files)

if(missing_support_files)
    string(JOIN "\n  - " missing_support_files_text ${missing_support_files})
    message(FATAL_ERROR
        "Missing support files in ${TARGET_DIRECTORY}:\n"
        "  - ${missing_support_files_text}")
endif()

if(DEFINED ASSET_SOURCE_DIR OR DEFINED ASSET_TARGET_DIR)
    if(NOT DEFINED ASSET_SOURCE_DIR OR ASSET_SOURCE_DIR STREQUAL "")
        message(FATAL_ERROR "ASSET_SOURCE_DIR is required when verifying staged assets.")
    endif()

    if(NOT DEFINED ASSET_TARGET_DIR OR ASSET_TARGET_DIR STREQUAL "")
        message(FATAL_ERROR "ASSET_TARGET_DIR is required when verifying staged assets.")
    endif()

    if(NOT EXISTS "${ASSET_SOURCE_DIR}")
        message(FATAL_ERROR "Asset source directory does not exist: ${ASSET_SOURCE_DIR}")
    endif()

    if(NOT EXISTS "${ASSET_TARGET_DIR}")
        message(FATAL_ERROR "Asset target directory does not exist: ${ASSET_TARGET_DIR}")
    endif()

    file(GLOB_RECURSE asset_source_files
        LIST_DIRECTORIES false
        RELATIVE "${ASSET_SOURCE_DIR}"
        "${ASSET_SOURCE_DIR}/*")

    file(GLOB_RECURSE asset_target_files
        LIST_DIRECTORIES false
        RELATIVE "${ASSET_TARGET_DIR}"
        "${ASSET_TARGET_DIR}/*")

    file(GLOB_RECURSE asset_source_entries
        LIST_DIRECTORIES true
        RELATIVE "${ASSET_SOURCE_DIR}"
        "${ASSET_SOURCE_DIR}/*")

    file(GLOB_RECURSE asset_target_entries
        LIST_DIRECTORIES true
        RELATIVE "${ASSET_TARGET_DIR}"
        "${ASSET_TARGET_DIR}/*")

    set(missing_asset_files)
    foreach(asset_file IN LISTS asset_source_files)
        if(NOT EXISTS "${ASSET_TARGET_DIR}/${asset_file}")
            list(APPEND missing_asset_files "${asset_file}")
        endif()
    endforeach()

    set(stale_asset_files)
    if(NOT ALLOW_EXTRA_ASSETS)
        foreach(asset_file IN LISTS asset_target_files)
            if(NOT EXISTS "${ASSET_SOURCE_DIR}/${asset_file}")
                list(APPEND stale_asset_files "${asset_file}")
            endif()
        endforeach()
    endif()

    set(asset_source_dirs)
    foreach(asset_entry IN LISTS asset_source_entries)
        if(IS_DIRECTORY "${ASSET_SOURCE_DIR}/${asset_entry}")
            list(APPEND asset_source_dirs "${asset_entry}")
        endif()
    endforeach()

    set(asset_target_dirs)
    foreach(asset_entry IN LISTS asset_target_entries)
        if(IS_DIRECTORY "${ASSET_TARGET_DIR}/${asset_entry}")
            list(APPEND asset_target_dirs "${asset_entry}")
        endif()
    endforeach()

    set(missing_asset_dirs)
    foreach(asset_dir IN LISTS asset_source_dirs)
        if(NOT IS_DIRECTORY "${ASSET_TARGET_DIR}/${asset_dir}")
            list(APPEND missing_asset_dirs "${asset_dir}")
        endif()
    endforeach()

    set(stale_asset_dirs)
    if(NOT ALLOW_EXTRA_ASSETS)
        foreach(asset_dir IN LISTS asset_target_dirs)
            if(NOT IS_DIRECTORY "${ASSET_SOURCE_DIR}/${asset_dir}")
                list(APPEND stale_asset_dirs "${asset_dir}")
            endif()
        endforeach()
    endif()

    if(missing_asset_files OR stale_asset_files OR missing_asset_dirs OR stale_asset_dirs)
        list(REMOVE_DUPLICATES missing_asset_files)
        list(REMOVE_DUPLICATES stale_asset_files)
        list(REMOVE_DUPLICATES missing_asset_dirs)
        list(REMOVE_DUPLICATES stale_asset_dirs)

        set(asset_error_message "Staged assets do not match the source asset tree.")
        if(missing_asset_files)
            string(JOIN "\n  - " missing_asset_files_text ${missing_asset_files})
            string(APPEND asset_error_message "\nMissing staged files:\n  - ${missing_asset_files_text}")
        endif()

        if(stale_asset_files)
            string(JOIN "\n  - " stale_asset_files_text ${stale_asset_files})
            string(APPEND asset_error_message "\nUnexpected staged files:\n  - ${stale_asset_files_text}")
        endif()

        if(missing_asset_dirs)
            string(JOIN "\n  - " missing_asset_dirs_text ${missing_asset_dirs})
            string(APPEND asset_error_message "\nMissing staged directories:\n  - ${missing_asset_dirs_text}")
        endif()

        if(stale_asset_dirs)
            string(JOIN "\n  - " stale_asset_dirs_text ${stale_asset_dirs})
            string(APPEND asset_error_message "\nUnexpected staged directories:\n  - ${stale_asset_dirs_text}")
        endif()

        message(FATAL_ERROR "${asset_error_message}")
    endif()
endif()

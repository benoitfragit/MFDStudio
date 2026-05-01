# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
include_guard(GLOBAL)

set(_CLIENT_API_GENERATOR_MODULE_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(_client_api_generator_resolve_script_root out_var)
    if(DEFINED MFD_ROOT_DIR AND EXISTS "${MFD_ROOT_DIR}/mfd_client_api/generator/scripts/generate_ui.py")
        set(${out_var} "${MFD_ROOT_DIR}/mfd_client_api/generator" PARENT_SCOPE)
        return()
    endif()

    get_filename_component(current_cmake_dir "${_CLIENT_API_GENERATOR_MODULE_DIR}" ABSOLUTE)
    if(EXISTS "${current_cmake_dir}/../tools/generator/scripts/generate_ui.py")
        get_filename_component(package_generator_root "${current_cmake_dir}/../tools/generator" ABSOLUTE)
        set(${out_var} "${package_generator_root}" PARENT_SCOPE)
        return()
    endif()

    message(FATAL_ERROR
        "Unable to resolve the client API generator root from '${_CLIENT_API_GENERATOR_MODULE_DIR}'.")
endfunction()

function(_client_api_generator_resolve_input_path out_var raw_path)
    if(IS_ABSOLUTE "${raw_path}")
        set(${out_var} "${raw_path}" PARENT_SCOPE)
        return()
    endif()

    get_filename_component(source_relative_path "${raw_path}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(EXISTS "${source_relative_path}")
        set(${out_var} "${source_relative_path}" PARENT_SCOPE)
        return()
    endif()

    if(DEFINED MFD_ROOT_DIR)
        get_filename_component(root_relative_path "${MFD_ROOT_DIR}/${raw_path}" ABSOLUTE)
        if(EXISTS "${root_relative_path}")
            set(${out_var} "${root_relative_path}" PARENT_SCOPE)
            return()
        endif()
    endif()

    set(${out_var} "${source_relative_path}" PARENT_SCOPE)
endfunction()

function(_client_api_generator_resolve_output_path out_var raw_path)
    if(IS_ABSOLUTE "${raw_path}")
        set(${out_var} "${raw_path}" PARENT_SCOPE)
        return()
    endif()

    get_filename_component(resolved_path "${raw_path}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    set(${out_var} "${resolved_path}" PARENT_SCOPE)
endfunction()

function(client_api_generate_ui)
    set(options)
    set(oneValueArgs
        WINDOW_JSON
        OUTPUT_HEADER
        OUTPUT_MAP
        OUTPUT_SOURCE
        NAMESPACE
        UI_CLASS_NAME
        PAGE_CLASS_SUFFIX
        UI_CLASS_SUFFIX
        HEADER_INCLUDE)
    set(multiValueArgs CONFIGURE_DEPENDS)
    cmake_parse_arguments(CAG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT CAG_WINDOW_JSON)
        message(FATAL_ERROR "client_api_generate_ui requires WINDOW_JSON")
    endif()

    if(NOT CAG_OUTPUT_HEADER)
        message(FATAL_ERROR "client_api_generate_ui requires OUTPUT_HEADER")
    endif()

    if(NOT CAG_OUTPUT_SOURCE)
        message(FATAL_ERROR "client_api_generate_ui requires OUTPUT_SOURCE")
    endif()

    if(NOT CAG_NAMESPACE)
        set(CAG_NAMESPACE "mockup_ui")
    endif()

    if(NOT CAG_PAGE_CLASS_SUFFIX)
        set(CAG_PAGE_CLASS_SUFFIX "MockupPage")
    endif()

    if(NOT CAG_UI_CLASS_SUFFIX)
        set(CAG_UI_CLASS_SUFFIX "MockupUi")
    endif()

    if(NOT CAG_HEADER_INCLUDE)
        get_filename_component(CAG_HEADER_INCLUDE "${CAG_OUTPUT_HEADER}" NAME)
    endif()

    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    _client_api_generator_resolve_script_root(generator_root)
    _client_api_generator_resolve_input_path(window_json_path "${CAG_WINDOW_JSON}")
    _client_api_generator_resolve_output_path(output_header_path "${CAG_OUTPUT_HEADER}")
    _client_api_generator_resolve_output_path(output_source_path "${CAG_OUTPUT_SOURCE}")

    set(generator_script "${generator_root}/scripts/generate_ui.py")
    get_filename_component(output_header_dir "${output_header_path}" DIRECTORY)
    get_filename_component(output_source_dir "${output_source_path}" DIRECTORY)
    file(MAKE_DIRECTORY "${output_header_dir}")
    file(MAKE_DIRECTORY "${output_source_dir}")

    execute_process(
        COMMAND
            "${Python3_EXECUTABLE}"
            "${generator_script}"
            --window-json "${window_json_path}"
            --output-header "${output_header_path}"
            --output-source "${output_source_path}"
            --print-inputs
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        RESULT_VARIABLE input_scan_result
        OUTPUT_VARIABLE input_scan_stdout
        ERROR_VARIABLE input_scan_stderr)

    if(NOT input_scan_result EQUAL 0)
        message(FATAL_ERROR
            "client_api_generate_ui input scan failed for ${window_json_path}\n"
            "stdout:\n${input_scan_stdout}\n"
            "stderr:\n${input_scan_stderr}")
    endif()

    string(REPLACE "\r\n" "\n" input_scan_stdout "${input_scan_stdout}")
    string(REPLACE "\n" ";" generator_input_paths "${input_scan_stdout}")

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${generator_script}")
    foreach(input_path IN LISTS generator_input_paths)
        if(NOT input_path STREQUAL "")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${input_path}")
        endif()
    endforeach()

    foreach(input_path IN LISTS CAG_CONFIGURE_DEPENDS)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${input_path}")
    endforeach()

    set(generator_command
        "${Python3_EXECUTABLE}"
        "${generator_script}"
        --window-json "${window_json_path}"
        --output-header "${output_header_path}"
        --output-source "${output_source_path}"
        # CMake regenerates tracked mockup helpers in place during configure.
        --force-overwrite
        --namespace "${CAG_NAMESPACE}"
        --page-class-suffix "${CAG_PAGE_CLASS_SUFFIX}"
        --ui-class-suffix "${CAG_UI_CLASS_SUFFIX}"
        --header-include "${CAG_HEADER_INCLUDE}")

    if(CAG_UI_CLASS_NAME)
        list(APPEND generator_command --ui-class-name "${CAG_UI_CLASS_NAME}")
    endif()

    if(CAG_OUTPUT_MAP)
        _client_api_generator_resolve_output_path(output_map_path "${CAG_OUTPUT_MAP}")
        get_filename_component(output_map_dir "${output_map_path}" DIRECTORY)
        file(MAKE_DIRECTORY "${output_map_dir}")
        list(APPEND generator_command --output-map "${output_map_path}")
        set_source_files_properties("${output_map_path}" PROPERTIES GENERATED TRUE)
    endif()

    execute_process(
        COMMAND ${generator_command}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        RESULT_VARIABLE generation_result
        OUTPUT_VARIABLE generation_stdout
        ERROR_VARIABLE generation_stderr)

    if(NOT generation_result EQUAL 0)
        message(FATAL_ERROR
            "client_api_generate_ui failed for ${window_json_path}\n"
            "stdout:\n${generation_stdout}\n"
            "stderr:\n${generation_stderr}")
    endif()

    set_source_files_properties(
        "${output_header_path}"
        "${output_source_path}"
        PROPERTIES GENERATED TRUE)
endfunction()

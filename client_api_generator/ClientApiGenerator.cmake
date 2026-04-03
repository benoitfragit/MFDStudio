# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
include_guard(GLOBAL)

function(client_api_generate_ui)
    set(options)
    set(oneValueArgs
        WINDOW_JSON
        OUTPUT_HEADER
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

    set(generator_script "${MFD_ROOT_DIR}/client_api_generator/scripts/generate_ui.py")
    get_filename_component(output_header_dir "${CAG_OUTPUT_HEADER}" DIRECTORY)
    get_filename_component(output_source_dir "${CAG_OUTPUT_SOURCE}" DIRECTORY)
    file(MAKE_DIRECTORY "${output_header_dir}")
    file(MAKE_DIRECTORY "${output_source_dir}")

    execute_process(
        COMMAND
            "${Python3_EXECUTABLE}"
            "${generator_script}"
            --window-json "${CAG_WINDOW_JSON}"
            --output-header "${CAG_OUTPUT_HEADER}"
            --output-source "${CAG_OUTPUT_SOURCE}"
            --print-inputs
        WORKING_DIRECTORY "${MFD_ROOT_DIR}"
        RESULT_VARIABLE input_scan_result
        OUTPUT_VARIABLE input_scan_stdout
        ERROR_VARIABLE input_scan_stderr)

    if(NOT input_scan_result EQUAL 0)
        message(FATAL_ERROR
            "client_api_generate_ui input scan failed for ${CAG_WINDOW_JSON}\n"
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
        --window-json "${CAG_WINDOW_JSON}"
        --output-header "${CAG_OUTPUT_HEADER}"
        --output-source "${CAG_OUTPUT_SOURCE}"
        --namespace "${CAG_NAMESPACE}"
        --page-class-suffix "${CAG_PAGE_CLASS_SUFFIX}"
        --ui-class-suffix "${CAG_UI_CLASS_SUFFIX}"
        --header-include "${CAG_HEADER_INCLUDE}")

    if(CAG_UI_CLASS_NAME)
        list(APPEND generator_command --ui-class-name "${CAG_UI_CLASS_NAME}")
    endif()

    execute_process(
        COMMAND ${generator_command}
        WORKING_DIRECTORY "${MFD_ROOT_DIR}"
        RESULT_VARIABLE generation_result
        OUTPUT_VARIABLE generation_stdout
        ERROR_VARIABLE generation_stderr)

    if(NOT generation_result EQUAL 0)
        message(FATAL_ERROR
            "client_api_generate_ui failed for ${CAG_WINDOW_JSON}\n"
            "stdout:\n${generation_stdout}\n"
            "stderr:\n${generation_stderr}")
    endif()

    set_source_files_properties(
        "${CAG_OUTPUT_HEADER}"
        "${CAG_OUTPUT_SOURCE}"
        PROPERTIES GENERATED TRUE)
endfunction()

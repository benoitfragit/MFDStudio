# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
foreach(required_var
        BUILD_DIRECTORY
        BUILD_CONFIG
        FIXTURE_SOURCE_DIR
        FIXTURE_BUILD_DIR
        MFD_PACKAGE_CMAKE_DIR
        MFD_SOURCE_DIR
        PARENT_CMAKE_GENERATOR)
    if(NOT DEFINED ${required_var} OR "${${required_var}}" STREQUAL "")
        message(FATAL_ERROR "RunPackageConsumerBuild.cmake requires ${required_var}.")
    endif()
endforeach()

execute_process(
    COMMAND
        ${CMAKE_COMMAND}
        --build
        "${BUILD_DIRECTORY}"
        --config
        "${BUILD_CONFIG}"
        --target
        stage_deliveries
    RESULT_VARIABLE stage_result
    OUTPUT_VARIABLE stage_stdout
    ERROR_VARIABLE stage_stderr)

if(NOT stage_result EQUAL 0)
    message(FATAL_ERROR
        "Failed to build stage_deliveries before package-consumer smoke test.\n"
        "stdout:\n${stage_stdout}\n"
        "stderr:\n${stage_stderr}")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${FIXTURE_BUILD_DIR}"
    RESULT_VARIABLE clean_result
    OUTPUT_VARIABLE clean_stdout
    ERROR_VARIABLE clean_stderr)

if(NOT clean_result EQUAL 0)
    message(FATAL_ERROR
        "Failed to clean fixture build directory '${FIXTURE_BUILD_DIR}'.\n"
        "stdout:\n${clean_stdout}\n"
        "stderr:\n${clean_stderr}")
endif()

set(configure_command
    ${CMAKE_COMMAND}
    -S "${FIXTURE_SOURCE_DIR}"
    -B "${FIXTURE_BUILD_DIR}"
    -G "${PARENT_CMAKE_GENERATOR}")

if(DEFINED PARENT_CMAKE_GENERATOR_PLATFORM AND NOT "${PARENT_CMAKE_GENERATOR_PLATFORM}" STREQUAL "")
    list(APPEND configure_command -A "${PARENT_CMAKE_GENERATOR_PLATFORM}")
endif()

if(DEFINED PARENT_CMAKE_GENERATOR_TOOLSET AND NOT "${PARENT_CMAKE_GENERATOR_TOOLSET}" STREQUAL "")
    list(APPEND configure_command -T "${PARENT_CMAKE_GENERATOR_TOOLSET}")
endif()

list(APPEND configure_command
    "-DMFD_PACKAGE_CMAKE_DIR=${MFD_PACKAGE_CMAKE_DIR}"
    "-DMFD_SOURCE_DIR=${MFD_SOURCE_DIR}")

execute_process(
    COMMAND ${configure_command}
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_stdout
    ERROR_VARIABLE configure_stderr)

if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR
        "Failed to configure consumer fixture '${FIXTURE_SOURCE_DIR}'.\n"
        "stdout:\n${configure_stdout}\n"
        "stderr:\n${configure_stderr}")
endif()

execute_process(
    COMMAND
        ${CMAKE_COMMAND}
        --build
        "${FIXTURE_BUILD_DIR}"
        --config
        "${BUILD_CONFIG}"
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_stdout
    ERROR_VARIABLE build_stderr)

if(NOT build_result EQUAL 0)
    message(FATAL_ERROR
        "Failed to build consumer fixture '${FIXTURE_SOURCE_DIR}'.\n"
        "stdout:\n${build_stdout}\n"
        "stderr:\n${build_stderr}")
endif()

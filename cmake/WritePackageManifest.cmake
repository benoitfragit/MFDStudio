# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
if(NOT DEFINED OUTPUT_FILE OR OUTPUT_FILE STREQUAL "")
    message(FATAL_ERROR "OUTPUT_FILE must be provided to WritePackageManifest.cmake")
endif()

if(NOT DEFINED PACKAGE_NAME OR PACKAGE_NAME STREQUAL "")
    message(FATAL_ERROR "PACKAGE_NAME must be provided to WritePackageManifest.cmake")
endif()

if(NOT DEFINED PACKAGE_KIND OR PACKAGE_KIND STREQUAL "")
    message(FATAL_ERROR "PACKAGE_KIND must be provided to WritePackageManifest.cmake")
endif()

if(NOT DEFINED PROJECT_VERSION OR PROJECT_VERSION STREQUAL "")
    message(FATAL_ERROR "PROJECT_VERSION must be provided to WritePackageManifest.cmake")
endif()

if(NOT DEFINED TOOLSET OR TOOLSET STREQUAL "")
    message(FATAL_ERROR "TOOLSET must be provided to WritePackageManifest.cmake")
endif()

set(targets_json "")
if(DEFINED TARGETS AND NOT TARGETS STREQUAL "")
    string(REPLACE "|" ";" package_targets "${TARGETS}")
    foreach(package_target IN LISTS package_targets)
        if(package_target STREQUAL "")
            continue()
        endif()

        if(targets_json STREQUAL "")
            set(targets_json "    \"${package_target}\"")
        else()
            string(APPEND targets_json ",\n    \"${package_target}\"")
        endif()
    endforeach()
endif()

if(targets_json STREQUAL "")
    set(targets_json "    \"none\"")
endif()

string(TIMESTAMP generated_utc "%Y-%m-%dT%H:%M:%SZ" UTC)

get_filename_component(output_directory "${OUTPUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${output_directory}")

file(WRITE "${OUTPUT_FILE}" "{\n")
file(APPEND "${OUTPUT_FILE}" "  \"packageName\": \"${PACKAGE_NAME}\",\n")
file(APPEND "${OUTPUT_FILE}" "  \"packageKind\": \"${PACKAGE_KIND}\",\n")
file(APPEND "${OUTPUT_FILE}" "  \"projectVersion\": \"${PROJECT_VERSION}\",\n")
file(APPEND "${OUTPUT_FILE}" "  \"toolset\": \"${TOOLSET}\",\n")
file(APPEND "${OUTPUT_FILE}" "  \"generatedUtc\": \"${generated_utc}\",\n")
file(APPEND "${OUTPUT_FILE}" "  \"targets\": [\n${targets_json}\n  ]\n")
file(APPEND "${OUTPUT_FILE}" "}\n")

# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio

function(mfd_configure_fuzzer_target target_name)
    mfd_configure_test_runtime_output(${target_name} BUILD_SUBDIR fuzz)
    target_compile_features(${target_name} PRIVATE cxx_std_17)
    mfd_enable_target_warnings(${target_name})
    mfd_enable_fuzzing(${target_name})

    if(WIN32)
        add_custom_command(
            TARGET ${target_name}
            POST_BUILD
            COMMAND
                ${CMAKE_COMMAND}
                "-DDEST_DIR=$<TARGET_FILE_DIR:${target_name}>"
                "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:${target_name}>,$<SEMICOLON>>"
                -P
                "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
            VERBATIM)
    endif()
endfunction()

add_executable(mfd_api_json_fuzzer
    ${CMAKE_CURRENT_SOURCE_DIR}/ApiJsonLoaderFuzzer.cpp)

target_link_libraries(mfd_api_json_fuzzer
    PRIVATE
        mfd::io_json
        mfd::model
        mfd::runtime
        mfd::transport)

mfd_configure_fuzzer_target(mfd_api_json_fuzzer)

add_executable(mfd_api_transport_fuzzer
    ${CMAKE_CURRENT_SOURCE_DIR}/ApiTransportFuzzer.cpp)

target_link_libraries(mfd_api_transport_fuzzer
    PRIVATE
        mfd::model
        mfd::transport)

mfd_configure_fuzzer_target(mfd_api_transport_fuzzer)

add_executable(mfd_editor_serializer_fuzzer
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorSerializerFuzzer.cpp
    ${MFD_ROOT_DIR}/mfd_editor/include/EditorDocumentSerializer.h
    ${MFD_ROOT_DIR}/mfd_editor/src/EditorDocumentSerializer.cpp)

target_include_directories(mfd_editor_serializer_fuzzer
    PRIVATE
        ${MFD_ROOT_DIR}/mfd_editor/include
        ${MFD_ROOT_DIR}/mfd_editor/src
        ${MFD_ROOT_DIR}/mfd_common_api/include
        ${MFD_ROOT_DIR}/mfd_api/include)

target_link_libraries(mfd_editor_serializer_fuzzer
    PRIVATE
        mfd::io_json
        mfd::model
        mfd::runtime
        mfd::transport)

mfd_private_library(mfd_editor_serializer_fuzzer nlohmann_json::nlohmann_json)

mfd_configure_fuzzer_target(mfd_editor_serializer_fuzzer)

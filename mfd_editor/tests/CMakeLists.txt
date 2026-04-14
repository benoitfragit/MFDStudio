# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
include(GoogleTest)

set(MFD_EDITOR_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorDocumentSerializerTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorTutorialDataTests.cpp)

add_executable(mfd_editor_tests ${MFD_EDITOR_TEST_SOURCES})

target_link_libraries(mfd_editor_tests
    PRIVATE
        GTest::gtest_main
        nlohmann_json::nlohmann_json
        imgui)

target_include_directories(mfd_editor_tests
    PRIVATE
        ${MFD_ROOT_DIR}/mfd_editor/include
        ${MFD_ROOT_DIR}/mfd_api/include)

target_sources(mfd_editor_tests
    PRIVATE
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorTutorialData.cpp
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorDocumentSerializer.cpp
        ${MFD_ROOT_DIR}/mfd_api/src/model/PageName.cpp
        ${MFD_ROOT_DIR}/mfd_api/src/model/Reticle.cpp
        ${MFD_ROOT_DIR}/mfd_api/src/model/Types.cpp)

target_compile_features(mfd_editor_tests PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(mfd_editor_tests
        PRIVATE
            /W4
            /permissive-
            /Zc:__cplusplus
            /EHsc
            /wd4251
            /wd4275)
else()
    target_compile_options(mfd_editor_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

gtest_discover_tests(mfd_editor_tests DISCOVERY_TIMEOUT 60)

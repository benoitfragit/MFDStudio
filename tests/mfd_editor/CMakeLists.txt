# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
include(GoogleTest)

set(MFD_EDITOR_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorAssetReferenceIndexServiceTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorAssetPathServiceTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorDocumentSerializerTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorDesignExportServiceTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorFullscreenPreviewControllerTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorLaunchOptionsTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorWorkspaceLayoutTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorLayerFocusControllerTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorPageManagementServiceTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorPageImportServiceTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorPageRenameServiceTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorReticleExtractionServiceTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorReticleUsageHighlightServiceTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorReticleRenameServiceTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorTutorialDataTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorUiThemeTests.cpp)

add_executable(mfd_editor_tests ${MFD_EDITOR_TEST_SOURCES})

mfd_configure_test_runtime_output(mfd_editor_tests)

target_link_libraries(mfd_editor_tests
    PRIVATE
        mfd::io_json
        mfd::model
        mfd::render_raylib)

mfd_private_library(mfd_editor_tests GTest::gtest_main)
mfd_private_library(mfd_editor_tests nlohmann_json::nlohmann_json)
mfd_private_library(mfd_editor_tests imgui)

target_include_directories(mfd_editor_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${MFD_ROOT_DIR}/mfd_editor/include
        ${MFD_ROOT_DIR}/mfd_common_api/include
        ${MFD_ROOT_DIR}/mfd_api/include
        ${MFD_ROOT_DIR}/mfd_api/src/render)

target_sources(mfd_editor_tests
    PRIVATE
        ${MFD_ROOT_DIR}/mfd_editor/include/EditorAssetReferenceIndexService.h
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorAssetReferenceIndexService.cpp
        ${MFD_ROOT_DIR}/mfd_editor/include/EditorAssetPathService.h
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorAssetPathService.cpp
        ${MFD_ROOT_DIR}/mfd_editor/include/EditorLaunchOptions.h
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorLaunchOptions.cpp
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorTutorialData.cpp
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorDocumentSerializer.cpp
        ${MFD_ROOT_DIR}/mfd_editor/include/EditorDesignExportService.h
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorDesignExportService.cpp
        ${MFD_ROOT_DIR}/mfd_editor/include/EditorFullscreenPreviewController.h
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorFullscreenPreviewController.cpp
        ${MFD_ROOT_DIR}/mfd_editor/include/EditorWorkspaceLayout.h
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorWorkspaceLayout.cpp
        ${MFD_ROOT_DIR}/mfd_editor/include/EditorLayerFocusController.h
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorLayerFocusController.cpp
        ${MFD_ROOT_DIR}/mfd_editor/include/EditorPageManagementService.h
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorPageManagementService.cpp
        ${MFD_ROOT_DIR}/mfd_editor/include/EditorPageImportService.h
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorPageImportService.cpp
        ${MFD_ROOT_DIR}/mfd_editor/include/EditorPageRenameService.h
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorPageRenameService.cpp
        ${MFD_ROOT_DIR}/mfd_editor/include/EditorReticleExtractionService.h
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorReticleExtractionService.cpp
        ${MFD_ROOT_DIR}/mfd_editor/include/EditorReticleRenameService.h
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorReticleRenameService.cpp
        ${MFD_ROOT_DIR}/mfd_editor/include/EditorReticleUsageHighlightService.h
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorReticleUsageHighlightService.cpp
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorUiTheme.cpp)

target_compile_features(mfd_editor_tests PRIVATE cxx_std_17)

mfd_enable_target_warnings(mfd_editor_tests)

set_target_properties(mfd_editor_tests PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_editor_tests>")

if(WIN32)
    add_custom_command(
        TARGET mfd_editor_tests
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND}
            "-DDEST_DIR=$<TARGET_FILE_DIR:mfd_editor_tests>"
            "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:mfd_editor_tests>,$<SEMICOLON>>"
            -P
            "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
        VERBATIM)
endif()

mfd_stage_runtime(mfd_editor_tests WITH_RUNTIME_DLLS STAGE_SUBDIR tests)

gtest_discover_tests(mfd_editor_tests
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_editor_tests>"
    DISCOVERY_TIMEOUT 60)

add_custom_target(mfd_editor_tests_run
    COMMAND $<TARGET_FILE:mfd_editor_tests>
    DEPENDS mfd_editor_tests
    USES_TERMINAL)

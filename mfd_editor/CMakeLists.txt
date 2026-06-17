# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
set(MFD_EDITOR_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorAssetReferenceIndexService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorAssetPathService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorApplication.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/application/EditorViewportGrid.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/application/EditorViewportSnap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/application/EditorApplicationShell.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/application/EditorApplicationInspectors.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorApplicationWorkflow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorDocumentSerializer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorDesignExportService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorFileDialogs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorFullscreenPreviewController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorLaunchOptions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorResponsiveLayout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorWorkspaceLayout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorLayerFocusController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorPagePreviewDrawOrderController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorPageManagementService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorPageImportService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorPageRenameService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorPrimitiveClipboardService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorReticleExtractionService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorReticleRenameService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorReticleUsageHighlightService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorTutorialController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorTutorialData.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorUiStatePersistence.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorUiTheme.cpp)

set(MFD_EDITOR_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorAssetReferenceIndexService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorAssetPathService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorApplication.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorAssetWatcher.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorAutosaveScheduler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorDocumentSerializer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorDesignExportService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorFileDialogs.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorFullscreenPreviewController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorLaunchOptions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorResponsiveLayout.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorWorkspaceLayout.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorLayerFocusController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorPagePreviewHit.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorProblemNavigation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/internal/application/EditorPagePreviewDrawOrderController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorPageManagementService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorPageImportService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorPageRenameService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorPrimitiveClipboardService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorReticleExtractionService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorPagePreviewViewOptions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorReticleUsageHighlightService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorReticleRenameService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorSnapping.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorTutorialController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorTutorialData.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorUiStatePersistence.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorUiTheme.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorUndoHistory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/internal/application/EditorApplicationAuthoringSupport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/internal/application/EditorApplicationInternal.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/internal/application/EditorApplicationState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/internal/application/EditorViewportGrid.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/internal/application/EditorViewportSnap.h)

set(MFD_EDITOR_WINDOWS_RESOURCES)
if(WIN32)
    list(APPEND MFD_EDITOR_WINDOWS_RESOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/mfd_editor.rc)
endif()

add_executable(mfd_editor
    ${MFD_EDITOR_SOURCES}
    ${MFD_EDITOR_HEADERS}
    ${MFD_EDITOR_WINDOWS_RESOURCES})

target_include_directories(mfd_editor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${MFD_ROOT_DIR}/mfd_api/src/render)

target_link_libraries(mfd_editor
    PRIVATE
        mfd::api
        mfd::render_raylib)

mfd_private_library(mfd_editor imgui)
mfd_private_library(mfd_editor nlohmann_json::nlohmann_json)
mfd_private_library(mfd_editor raylib)
mfd_private_library(mfd_editor rlimgui)

mfd_enable_target_warnings(mfd_editor)

if(WIN32)
    target_link_libraries(mfd_editor PRIVATE Comdlg32 Ole32 Shell32)
endif()

set_target_properties(mfd_editor PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_editor>")

add_custom_command(
    TARGET mfd_editor
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND}
        "-DSOURCE_DIR=${MFD_ROOT_DIR}/branding"
        "-DDEST_DIR=$<TARGET_FILE_DIR:mfd_editor>/branding"
        -P
        "${MFD_ROOT_DIR}/cmake/SyncDirectoryTree.cmake"
    COMMAND
        ${CMAKE_COMMAND}
        "-DSOURCE_DIR=${MFD_ROOT_DIR}/assets"
        "-DDEST_DIR=$<TARGET_FILE_DIR:mfd_editor>/assets"
        -P
        "${MFD_ROOT_DIR}/cmake/SyncAssetTree.cmake"
    COMMAND
        ${CMAKE_COMMAND}
        "-DDEST_DIR=$<TARGET_FILE_DIR:mfd_editor>"
        "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:mfd_editor>,$<SEMICOLON>>"
        -P
        "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
    VERBATIM)

mfd_stage_runtime(mfd_editor WITH_ASSETS WITH_BRANDING WITH_RUNTIME_DLLS)

install(TARGETS mfd_editor
    RUNTIME DESTINATION .
    COMPONENT ${MFD_COMPONENT_EDITOR_RUNTIME})

if(MFD_API_BUILD_SHARED)
    install(FILES
        "$<TARGET_FILE:mfd_api>"
        DESTINATION .
        COMPONENT ${MFD_COMPONENT_EDITOR_RUNTIME})
endif()

install(DIRECTORY
    "${MFD_ROOT_DIR}/branding/"
    DESTINATION branding
    COMPONENT ${MFD_COMPONENT_EDITOR_RUNTIME})

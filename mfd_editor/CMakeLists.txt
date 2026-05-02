# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
if(MFD_BUILD_DEMO)
    set(MFD_EDITOR_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AssetReferenceIndexService.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorApplication.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorAutomationPluginLoader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorDocumentSerializer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorFileDialogs.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/PageManagementService.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/PageImportService.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/PageRenameService.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorTutorialController.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorTutorialData.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorUiTheme.cpp)

    set(MFD_EDITOR_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/include/AssetReferenceIndexService.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorApplication.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorAutomationBridge.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorAutomationDocumentUtils.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorAutomationPluginLoader.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorAutomationServices.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorAutomationTypes.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorDocumentSerializer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorFileDialogs.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/PageManagementService.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/PageImportService.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/PageRenameService.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/PagePreviewViewOptions.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorTutorialController.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorTutorialData.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorUiTheme.h)

    set(MFD_EDITOR_WINDOWS_RESOURCES)
    if(WIN32)
        list(APPEND MFD_EDITOR_WINDOWS_RESOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/resources/mfd_editor.rc)
    endif()

    add_executable(mfd_editor
        ${MFD_EDITOR_SOURCES}
        ${MFD_EDITOR_HEADERS}
        ${MFD_EDITOR_WINDOWS_RESOURCES})

    target_sources(mfd_editor
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorAutomationBridge.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorAutomationCore.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorAutomationDocumentUtils.cpp)

    target_include_directories(mfd_editor
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${MFD_ROOT_DIR}/mfd_api/src/render)

    target_link_libraries(mfd_editor
        PRIVATE
            mfd::io_json
            mfd::render_raylib
            mfd::editor_plugin_api)

    mfd_private_library(mfd_editor imgui)
    mfd_private_library(mfd_editor nlohmann_json::nlohmann_json)
    mfd_private_library(mfd_editor raylib)
    mfd_private_library(mfd_editor rlimgui)

    mfd_enable_target_warnings(mfd_editor)

    if(WIN32)
        target_link_libraries(mfd_editor PRIVATE Comdlg32)
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

    mfd_register_runtime_package(
        NAME MFDStudioEditor.Install
        TARGETS mfd_editor
        WITH_BRANDING)
endif()

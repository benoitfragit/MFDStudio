# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
set(MFD_EDITOR_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorApplication.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorDocumentSerializer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorFileDialogs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorTutorialController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorTutorialData.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorUiTheme.cpp)

set(MFD_EDITOR_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorApplication.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorDocumentSerializer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorFileDialogs.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorTutorialController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorTutorialData.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EditorUiTheme.h)

add_executable(mfd_editor ${MFD_EDITOR_SOURCES} ${MFD_EDITOR_HEADERS})

target_include_directories(mfd_editor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(mfd_editor
    PRIVATE
        mfd::api
        imgui
        rlimgui)

if (WIN32)
    target_link_libraries(mfd_editor PRIVATE Comdlg32)
endif()

set_target_properties(mfd_editor PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_editor>")

add_custom_command(
    TARGET mfd_editor
    POST_BUILD
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

mfd_stage_runtime(mfd_editor WITH_ASSETS WITH_RUNTIME_DLLS)

# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio

set(MFD_EDITOR_AUTOMATION_SHARED_HEADERS
    ${MFD_ROOT_DIR}/mfd_editor/include/EditorAutomationBridge.h
    ${MFD_ROOT_DIR}/mfd_editor/include/EditorAutomationDocumentUtils.h
    ${MFD_ROOT_DIR}/mfd_editor/include/EditorAutomationServices.h
    ${MFD_ROOT_DIR}/mfd_editor/include/EditorAutomationTypes.h)

set(MFD_EDITOR_AUTOMATION_SHARED_SOURCES
    ${MFD_ROOT_DIR}/mfd_editor/src/EditorAutomationCore.cpp
    ${MFD_ROOT_DIR}/mfd_editor/src/EditorAutomationDocumentUtils.cpp)

set(MFD_EDITOR_AUTOMATION_EDITOR_BRIDGE_SOURCE
    ${MFD_ROOT_DIR}/mfd_editor/src/EditorAutomationBridge.cpp)

function(mfd_editor_enable_automation_for_target target)
    set(options SKIP_EDITOR_APPLICATION_BRIDGE)
    cmake_parse_arguments(ARG "${options}" "" "" ${ARGN})

    target_sources(${target}
        PRIVATE
            ${MFD_EDITOR_AUTOMATION_SHARED_HEADERS}
            ${MFD_EDITOR_AUTOMATION_SHARED_SOURCES})

    if(NOT ARG_SKIP_EDITOR_APPLICATION_BRIDGE)
        target_sources(${target} PRIVATE ${MFD_EDITOR_AUTOMATION_EDITOR_BRIDGE_SOURCE})
    endif()
endfunction()

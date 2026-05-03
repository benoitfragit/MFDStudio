/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorFullscreenPreviewController.h"

/**
 * @file
 * @brief Implementation of the editor-only fullscreen preview controller.
 */

namespace editor
{
namespace
{
FullscreenPreviewLayoutState MakeFullscreenState(const FullscreenPreviewLayoutState& currentState)
{
    FullscreenPreviewLayoutState fullscreenState = currentState;
    fullscreenState.sidebarVisible = false;
    fullscreenState.inspectorVisible = false;
    fullscreenState.pageContextVisible = false;
    fullscreenState.layerInspectorVisible = false;
    fullscreenState.minimapVisible = false;
    fullscreenState.problemsVisible = false;
    return fullscreenState;
}
} // namespace

FullscreenPreviewTransition FullscreenPreviewController::Enter(const FullscreenPreviewLayoutState& currentState)
{
    if (active_)
    {
        return {false, MakeFullscreenState(previousState_)};
    }

    active_ = true;
    previousState_ = currentState;
    return {true, MakeFullscreenState(currentState)};
}

FullscreenPreviewTransition FullscreenPreviewController::Exit()
{
    if (!active_)
    {
        return {};
    }

    active_ = false;
    return {true, previousState_};
}

FullscreenPreviewTransition FullscreenPreviewController::Toggle(const FullscreenPreviewLayoutState& currentState)
{
    return active_ ? Exit() : Enter(currentState);
}

bool FullscreenPreviewController::IsActive() const noexcept
{
    return active_;
}
} // namespace editor

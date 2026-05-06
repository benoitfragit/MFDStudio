/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Tutorial metadata and helper rendering used by the editor coaching panel.
 */

#include "mfd/core/ArrayView.h"

namespace editor::tutorial
{
/**
 * @brief Stable identifiers for the integrated editor tutorial steps.
 *
 * @note The order of the enumerators matches the order of the immutable step
 * table returned by `Steps()`.
 */
enum class TutorialStepId : int
{
    CreateWindow = 0,
    CreateRadarTrackReticle,
    CreateCircleReticle,
    CreateStrobeCursorReticle,
    CreatePage1,
    CreateRadarTrackLayerOnPage1,
    AllowPage1DynamicReticleTemplate,
    AssignPage1StrobeTemplate,
    AddCircleReticleToPage1,
    ClipCircleOutside,
    AddAndHideEditorLayer,
    CreatePage2,
    CreateProgressBarReticle,
    ExposeProgressBarFillPrimitive,
    AddProgressBarToPage2,
    ShowPageContext,
    ShowLayerInspector,
    ShowMinimap,
    ShowReticleUsageHighlights,
    ShowProblemsPanel,
    ToggleFullscreenPreview,
    SaveTutorialAssets,
    InspectPageImportWorkflow,
    InspectPageRenameWorkflow,
    InspectReticleRenameWorkflow,
    InspectDesignExportWorkflow,
    OpenTutorialFollowUpGuide,
    ReviewDocumentationPath,
    Count
};

enum class TutorialStepKind
{
    UiAction,
    ReferenceDocument
};

/**
 * @brief Immutable description of one guided tutorial step.
 */
struct TutorialStepDefinition
{
    TutorialStepKind kind;
    const char* title;
    const char* instruction;
    const char* targetId;
    const char* filePath;
    const char* explanation;
    const char* advanceLabel;
};

/**
 * @brief Returns the static set of tutorial steps displayed by the editor.
 */
mfd::ArrayView<const TutorialStepDefinition> Steps() noexcept;

/**
 * @brief Returns the number of tutorial steps.
 */
int StepCount() noexcept;

/**
 * @brief Returns `true` when the provided step is a UI-driven action.
 */
bool IsUiStep(const TutorialStepDefinition& step) noexcept;

/**
 * @brief Returns `true` when the provided step opens one reference document.
 */
bool IsReferenceDocumentStep(const TutorialStepDefinition& step) noexcept;
} // namespace editor::tutorial

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
    AllowPage1DynamicReticleTemplate,
    AllowPage1SteeringCueDynamicReticleTemplate,
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
    ReviewPage1StrobeJson,
    ReviewGeneratedUiHeader,
    ReviewGeneratedTransportMap,
    ReviewGeneratedUiIntegration,
    ReviewPageSwitchingInClient,
    ReviewDynamicReticleCreation,
    ReviewDynamicReticleUpdates,
    ReviewDynamicDeclutter,
    ReviewDynamicReticleRemoval,
    ReviewStaticReticleCommands,
    ReviewStrobeFeedbackHandling,
    ReviewRgba32FramebufferCapture,
    ReviewTutorialClientBuildGate,
    ReviewTutorialTargetRegistration,
    ReviewDocumentationPath,
    Count
};

enum class TutorialStepKind
{
    UiAction,
    FileReview
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
    const char* beforeText;
    const char* afterText;
    const char* explanation;
    const char* advanceLabel;
    int beforeFirstLine;
    int afterFirstLine;
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
 * @brief Returns `true` when the provided step is a file-review step.
 */
bool IsFileReviewStep(const TutorialStepDefinition& step) noexcept;
} // namespace editor::tutorial

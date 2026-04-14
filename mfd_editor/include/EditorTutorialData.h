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
 * @brief Immutable description of one guided tutorial step.
 */
struct TutorialStepDefinition
{
    const char* title;
    const char* instruction;
    const char* targetId;
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
 * @brief Draws file-edit hints associated with one tutorial step.
 * @param stepIndex Zero-based tutorial step index.
 */
void DrawTutorialFileHints(int stepIndex);
} // namespace editor::tutorial

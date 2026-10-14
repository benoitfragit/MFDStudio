/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

#include "mfd/model/Types.h"

namespace editor::ui
{
/** @brief UI intent only; the caller owns domain mutation and undo snapshots. */
struct ColorPickerInteraction
{
    bool changed = false;
    bool beganEdit = false;
    bool popupOpened = false;
    bool popupVisible = false;
    mfd::ColorPreset selected = mfd::ColorPreset::Custom;
};

/**
 * @brief Draws a current-color button opening RGBA editing with eight preset shortcuts.
 * @param label Unique control label within the current ImGui ID scope.
 * @param color Four normalized RGBA channels, updated immediately when the user changes the effective color.
 * @return Selection intent, including the first change of each gesture for undo capture.
 * @note Opening or closing the popup alone never modifies color.
 */
ColorPickerInteraction DrawColorPicker(const char* label, float* color);
} // namespace editor::ui

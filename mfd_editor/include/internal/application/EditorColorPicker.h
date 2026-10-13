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
    bool customOpened = false;
    bool customVisible = false;
    mfd::ColorPreset selected = mfd::ColorPreset::Custom;
};

/**
 * @brief Draws eight preset swatches and a transactional Custom RGBA popup.
 * @param label Unique control label within the current ImGui ID scope.
 * @param color Four normalized RGBA channels, modified only on a committed edit.
 * @return Selection intent; displaying or cancelling the popup never modifies color.
 */
ColorPickerInteraction DrawColorPicker(const char* label, float* color);
} // namespace editor::ui

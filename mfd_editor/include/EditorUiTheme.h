/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Shared ImGui styling helpers used by the MFD editor shell.
 */

#include <optional>
#include <string>

#include "mfd/model/Types.h"

namespace editor::ui
{
/**
 * @brief Width in pixels of draggable pane splitters used by the editor layout.
 */
inline constexpr float kPaneSplitterWidth = 8.0f;

/**
 * @brief Applies the editor color palette and rounded-corner theme.
 */
void ApplyEditorTheme();

/**
 * @brief Draws an accent-colored button.
 * @param label Button label.
 * @return `true` when the button is pressed on this frame.
 */
bool AccentButton(const char* label);

/**
 * @brief Draws a draggable vertical splitter handle.
 * @param id Stable ImGui id.
 * @param height Splitter height in pixels.
 * @return `true` while the splitter is pressed or active.
 */
bool DrawVerticalSplitter(const char* id, float height);

/**
 * @brief Shows a delayed tooltip for the last item when hovered.
 * @param text Tooltip text.
 */
void ShowItemTooltip(const char* text);

/**
 * @brief Formats the compact viewport overlay text shown next to the help button.
 * @param zoom Current editor-only zoom factor for the viewport.
 * @param mouseLogical Optional mouse coordinates in logical space.
 * @return Text containing the zoom percentage and, when available, the logical mouse coordinates.
 */
std::string FormatViewportToolbarInfoLabel(float zoom, const std::optional<mfd::Vec2>& mouseLogical);
} // namespace editor::ui

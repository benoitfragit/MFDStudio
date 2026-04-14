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
} // namespace editor::ui

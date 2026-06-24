/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Vector glyph icons and flat icon buttons shared by the editor UI.
 *
 * The icons are drawn directly with ImDrawList primitives so the editor keeps a
 * uniform white-silhouette action language (rename, delete, copy, paste, ...)
 * without bundling an icon font. Only thin, presentation-only helpers are
 * exposed here: every business mutation stays in the caller's named functions.
 */

#include <imgui.h>

namespace editor::ui
{
/**
 * @brief Identifies one vector glyph drawn by the editor icon helpers.
 */
enum class EditorIcon
{
    Rename,           ///< Pencil silhouette used for rename actions.
    Delete,           ///< Trash silhouette used for destructive delete actions.
    RemoveFromWindow, ///< Eject silhouette used to detach an asset without deleting it.
    Copy,             ///< Overlapping-sheets silhouette used for copy.
    Paste,            ///< Clipboard silhouette used for paste.
    Cut,              ///< Scissors silhouette used for cut.
    Duplicate,        ///< Copy silhouette with a plus marker used for duplicate.
    Add,              ///< Plus silhouette used for create/add actions.
    Search,           ///< Magnifier silhouette used to annotate the filter field and file pickers.
    Overflow,         ///< Three-dot silhouette used for an overflow menu trigger.
    Tutorial,         ///< Graduation-cap silhouette used for the guided tutorial entry point.
    Recent,           ///< Clock silhouette used to reopen recently authored windows.
    Help,             ///< Question-mark silhouette used to open a view's controls summary.
    Recenter          ///< Crosshair silhouette used to recenter a preview camera.
};

/**
 * @brief Draws a flat, transparent-background icon button with a white glyph.
 * @param strId Stable unique ImGui id for the button (e.g. "##delete_page_2").
 * @param icon Glyph to render.
 * @param tooltip Optional delayed hover tooltip; skipped when null or empty.
 * @param enabled When `false` the button is dimmed and never reports a press.
 * @param sizeOverride Square side in pixels; `0` (default) uses the current frame
 *        height so the button lines up with adjacent standard widgets. Pass the
 *        value returned by BeginRowIconStrip to fit a compact tree row.
 * @return `true` when the button is clicked this frame while enabled.
 * @note The button hovers with the theme accent color over a rounded highlight.
 */
bool IconButton(const char* strId, EditorIcon icon, const char* tooltip, bool enabled = true, float sizeOverride = 0.0f);

/**
 * @brief Draws a non-interactive icon glyph inline, advancing the cursor like text.
 * @param icon Glyph to render, vertically centered on the current text line.
 * @note Used to prefix the sidebar filter with a magnifier affordance.
 */
void IconText(EditorIcon icon);

/**
 * @brief Positions the cursor for a right-aligned, vertically centered row icon strip.
 * @param rowMin Top-left screen corner of the tree row (from `ImGui::GetItemRectMin`).
 * @param rowMax Bottom-right screen corner of the tree row (from `ImGui::GetItemRectMax`).
 * @param iconCount Number of icon buttons the strip will contain.
 * @return Square icon side in pixels to pass to each `IconButton` `sizeOverride`.
 * @note The icons are sized to the row height so they never exceed the line and shift
 *       neighboring rows, and are centered vertically on the row text. Place the buttons
 *       immediately after this call, separated by `ImGui::SameLine()`.
 */
float BeginRowIconStrip(const ImVec2& rowMin, const ImVec2& rowMax, int iconCount);
} // namespace editor::ui

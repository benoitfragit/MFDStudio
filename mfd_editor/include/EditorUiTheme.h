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

#include <map>
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
 * @brief Resolves the editor font size for the current display scale.
 * @param displayScale Vertical display scale reported by raylib.
 * @return Finite font size in pixels, clamped to the supported UI range.
 */
[[nodiscard]] float EditorFontPixelSize(float displayScale) noexcept;

/**
 * @brief Adds the editor UI font to the active ImGui atlas.
 * @param displayScale Vertical display scale reported by raylib.
 * @return `true` when the Windows Segoe UI font was loaded; `false` when the
 *         readable ImGui fallback was installed instead.
 * @pre An ImGui context exists and the rlImGui custom initialization phase is active.
 * @note This font belongs to the editor shell only and never changes authored MFD asset fonts.
 */
bool ConfigureEditorFont(float displayScale);

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
 * @brief Draws the title and optional description of an inspector.
 * @param title Short inspector title.
 * @param description Optional explanatory caption shown below the title.
 */
void DrawInspectorHeading(const char* title, const char* description = nullptr);

/**
 * @brief Resolves the label column width for an inspector field row.
 * @param availableWidth Width available to the row in pixels.
 * @return A finite label width clamped for narrow and wide inspector panes.
 */
[[nodiscard]] float InspectorFieldLabelWidth(float availableWidth) noexcept;

/**
 * @brief Starts one aligned label/value row in an inspector form.
 * @param label Human-readable field label and stable ImGui id scope.
 * @pre Must be paired with exactly one call to `EndInspectorField` after drawing the value widget.
 * @note Draw the value widget with a hidden label such as `"##value"` so labels stay in one column.
 */
void BeginInspectorField(const char* label);

/**
 * @brief Ends the ImGui id scope opened by `BeginInspectorField`.
 * @pre `BeginInspectorField` was called earlier in the current window and row.
 */
void EndInspectorField();

/**
 * @brief Draws a draggable vertical splitter handle.
 * @param id Stable ImGui id.
 * @param height Splitter height in pixels.
 * @return `true` while the splitter is pressed or active.
 */
bool DrawVerticalSplitter(const char* id, float height);

/**
 * @brief Draws a draggable horizontal splitter handle stacked between two sections.
 * @param id Stable ImGui id.
 * @param width Splitter width in pixels.
 * @param thickness Splitter height in pixels reserved for the grab handle.
 * @return `true` while the splitter is pressed or active.
 */
bool DrawHorizontalSplitter(const char* id, float width, float thickness);

/**
 * @brief Shows a delayed tooltip for the last item when hovered.
 * @param text Tooltip text.
 */
void ShowItemTooltip(const char* text);

/**
 * @brief Draws a collapsible inspector section header used to tame dense inspector panels.
 * @param strId Stable unique ImGui id for the section, kept distinct from the visible label so two
 *              sections sharing a caption across inspectors never collide.
 * @param label Human-readable section title.
 * @param defaultOpen Whether the section starts expanded the first time it is shown in a session.
 * @param forceOpen When `true` the section is forced open for this frame; used while the guided
 *                  tutorial runs so every highlighted target stays visible.
 * @return `true` when the section body must be drawn this frame.
 */
bool BeginInspectorSection(const char* strId, const char* label, bool defaultOpen, bool forceOpen);

/**
 * @brief Draws a restrained collapsible section header for a sidebar navigation group.
 * @param label Visible caption, optionally followed by an ImGui `###stable_id` suffix.
 * @param forceOpen Whether the section must remain open for tutorial target visibility.
 * @return `true` when the section body must be drawn this frame.
 */
bool BeginNavigationSection(const char* label, bool forceOpen);

/**
 * @brief Injects the map used to persist collapsible inspector-section open states across sessions.
 * @param store Map keyed by section id, owned by the caller, or `nullptr` to detach (e.g. on shutdown).
 * @note When attached, `BeginInspectorSection` seeds each section from the stored value on first use
 *       and mirrors live changes back into the map so the owner can serialize it. The store is left
 *       untouched while a section is force-opened so the user's real preference survives the tutorial.
 */
void SetInspectorSectionStateStore(std::map<std::string, bool>* store) noexcept;

/**
 * @brief Draws dimmed helper text that wraps onto several lines at the panel edge.
 * @param text Caption to display; skipped when null or empty.
 * @note Used for the long explanatory captions in inspectors and side panels so they
 *       stay fully readable on narrow panes instead of being clipped at the border.
 */
void DisabledTextWrapped(const char* text);

/**
 * @brief Draws a compact "(?)" marker that reveals detailed guidance in a hover tooltip.
 * @param text Tooltip text. The marker is skipped when the text is null or empty.
 * @note Drawn on the same line as the previous item so it can annotate a section header or field.
 */
void InspectorHelpMarker(const char* text);

/**
 * @brief Formats the compact viewport overlay text shown next to the help button.
 * @param zoom Current editor-only zoom factor for the viewport.
 * @param mouseLogical Optional mouse coordinates in logical space.
 * @return Text containing the zoom percentage and, when available, the logical mouse coordinates.
 */
std::string FormatViewportToolbarInfoLabel(float zoom, const std::optional<mfd::Vec2>& mouseLogical);
} // namespace editor::ui

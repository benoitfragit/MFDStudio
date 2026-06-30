/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Lightweight persistence for editor workspace layout preferences (panel widths and
 *        collapsible inspector-section open states) stored next to the authored assets.
 */

#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace editor
{
/**
 * @brief Session-spanning editor UI preferences serialized to a small JSON file.
 *
 * @details Widths are optional so a missing or partial file keeps the in-code defaults. Section
 * open states are keyed by the stable inspector-section id passed to
 * `editor::ui::BeginInspectorSection`.
 */
struct EditorUiPersistentState
{
    /** @brief Persisted width of the left navigation sidebar. */
    std::optional<float> sidebarWidth {};
    /** @brief Persisted visibility preference of the left navigation sidebar. */
    std::optional<bool> sidebarVisible {};
    /** @brief Persisted width of the right inspector panel. */
    std::optional<float> inspectorWidth {};
    /** @brief Persisted visibility preference of the right inspector panel. */
    std::optional<bool> inspectorVisible {};
    /** @brief Persisted state of the shared visible editor grid. */
    std::optional<bool> showGrid {};
    /** @brief Persisted state of the editor-only page border outlining the window bounds. */
    std::optional<bool> showPageBorder {};
    /** @brief Persisted state of the shared grid snapping mode. */
    std::optional<bool> snapToGrid {};
    /** @brief Persisted shared logical step used by the visible grid and snapping. */
    std::optional<float> gridStepLogical {};
    /** @brief Persisted fraction (0..1) of the sidebar body given to the Pages section. */
    std::optional<float> sidebarPagesSplitFraction {};
    /** @brief Persisted open-state map for inspector sections keyed by their stable id. */
    std::map<std::string, bool> sectionOpen {};
};

/**
 * @brief Loads persisted editor UI preferences.
 * @param file JSON file to read.
 * @return Parsed preferences, or an empty value (all defaults) when the file is missing or invalid.
 */
[[nodiscard]] EditorUiPersistentState LoadEditorUiState(const std::filesystem::path& file);

/**
 * @brief Saves editor UI preferences, best-effort.
 * @param file JSON file to write.
 * @param state Preferences to persist.
 * @note Failures are swallowed so a read-only asset tree never breaks the editor shutdown path.
 */
void SaveEditorUiState(const std::filesystem::path& file, const EditorUiPersistentState& state);
} // namespace editor

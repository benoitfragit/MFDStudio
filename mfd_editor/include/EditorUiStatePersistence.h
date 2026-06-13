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
    std::optional<float> sidebarWidth {};
    std::optional<float> inspectorWidth {};
    std::optional<float> libraryStudioPageWidth {};
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

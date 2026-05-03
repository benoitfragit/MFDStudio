/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Native file-dialog helpers used by the editor shell.
 */

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace editor
{
/**
 * @brief Opens a native file-explorer dialog to pick one existing window JSON asset.
 * @param initialFolder Folder opened first by the dialog.
 * @param error Optional error message populated when the dialog fails.
 * @return Selected window JSON path, or `std::nullopt` when the user cancels.
 */
std::optional<std::filesystem::path> OpenWindowAssetFileDialog(const std::filesystem::path& initialFolder,
                                                               std::string* error);
/**
 * @brief Opens a native file-explorer dialog to pick one existing page JSON asset.
 * @param initialFolder Folder opened first by the dialog.
 * @param error Optional error message populated when the dialog fails.
 * @return Selected page JSON path, or `std::nullopt` when the user cancels.
 */
std::optional<std::filesystem::path> OpenPageAssetFileDialog(const std::filesystem::path& initialFolder,
                                                             std::string* error);
/**
 * @brief Opens a native folder picker used by export workflows.
 * @param initialFolder Folder opened first by the dialog.
 * @param title Window title shown by the native picker.
 * @param error Optional error message populated when the dialog fails.
 * @return Selected folder path, or `std::nullopt` when the user cancels.
 */
std::optional<std::filesystem::path> OpenFolderDialog(const std::filesystem::path& initialFolder,
                                                      std::string_view title,
                                                      std::string* error);
/**
 * @brief Opens one folder in the native file explorer.
 * @param folder Folder to reveal.
 * @param error Optional error message populated when the request fails.
 * @return `true` when the folder-open request was submitted successfully.
 */
bool OpenFolderInFileExplorer(const std::filesystem::path& folder, std::string* error);
} // namespace editor

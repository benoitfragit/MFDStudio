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
} // namespace editor

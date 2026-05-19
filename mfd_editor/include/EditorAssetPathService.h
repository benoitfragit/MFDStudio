/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Editor asset path defaults and safety helpers.
 */

#include <filesystem>
#include <string_view>

#include "EditorDocumentSerializer.h"

namespace editor
{
/**
 * @brief Resolves default editor asset paths from either the repository root or
 * a user-provided asset directory.
 *
 * @details The editor keeps this policy out of the UI shell so command-line
 * defaults, creation popups, import planning and destructive-action guards all
 * use the same source of truth.
 */
class EditorAssetPathService
{
public:
    /**
     * @brief Builds the path service.
     * @param assetDirectory Optional authored `assets` root used as the editor default.
     * @note When `assetDirectory` is empty, the service discovers the repository
     * root from the current working directory and falls back to `./assets`.
     */
    explicit EditorAssetPathService(std::filesystem::path assetDirectory = {});

    /**
     * @brief Returns the configured authored asset root, when one was provided.
     * @return Absolute normalized asset directory or an empty path.
     */
    [[nodiscard]] const std::filesystem::path& ConfiguredAssetDirectory() const noexcept;

    /**
     * @brief Returns whether command-line configuration provided an asset root.
     * @return `true` when this service uses an explicit asset directory.
     */
    [[nodiscard]] bool HasConfiguredAssetDirectory() const noexcept;

    /**
     * @brief Resolves a default path under the editor asset root.
     * @param relativeAssetPath Relative path such as `assets/windows/demo.json`
     * or `assets/reticles`.
     * @return Absolute normalized path.
     * @note When an explicit asset directory exists, a leading `assets` segment
     * is stripped so `--asset-directory X` maps `assets/windows` to `X/windows`.
     */
    [[nodiscard]] std::filesystem::path DefaultAssetPath(std::string_view relativeAssetPath) const;

    /**
     * @brief Builds a sibling asset file path from an existing anchor file.
     * @param anchorFile Window or page file used as sibling anchor.
     * @param siblingFolder Target sibling folder, for example `pages`.
     * @param fileName File name to append. May be empty for folder paths.
     * @return Normalized path beside the anchor or inside the default asset root.
     */
    [[nodiscard]] std::filesystem::path DefaultSiblingAssetFile(const std::filesystem::path& anchorFile,
                                                               std::string_view siblingFolder,
                                                               std::string_view fileName) const;

    /**
     * @brief Resolves the asset root that should be scanned for one authored file.
     * @param path Authored asset path used as the scan anchor.
     * @return The nearest ancestor named `assets`, the configured asset root, or
     * the discovered repository asset root.
     */
    [[nodiscard]] std::filesystem::path ResolveAssetRootForPath(const std::filesystem::path& path) const;

    /**
     * @brief Computes the default target folder for importing one page asset.
     * @param windowFile Current root window JSON file.
     * @param files Current editor file layout.
     * @return Folder where imported page JSON should be staged by default.
     */
    [[nodiscard]] std::filesystem::path CurrentPageImportTargetFolder(const std::filesystem::path& windowFile,
                                                                      const EditorFileLayout& files) const;

    /**
     * @brief Returns a usable folder from a configured file or folder path.
     * @param configuredPath Path read from a UI field.
     * @return Parent folder for file-like paths, or the path itself for folders.
     */
    [[nodiscard]] static std::filesystem::path ConfiguredPathFolder(const std::filesystem::path& configuredPath);

    /**
     * @brief Returns a JSON file name derived from a path or a fallback.
     * @param candidate Candidate path whose filename should be reused.
     * @param fallbackFileName Fallback JSON file name when the candidate has none.
     * @return Filename with a `.json` extension.
     */
    [[nodiscard]] static std::filesystem::path JsonFileNameOrFallback(const std::filesystem::path& candidate,
                                                                      std::string_view fallbackFileName);

    /**
     * @brief Detects paths inside the staged runtime `_Exec` tree.
     * @param path Path to inspect.
     * @return `true` when one path segment is `_Exec`, case-insensitively.
     */
    [[nodiscard]] static bool IsExecStagingPath(const std::filesystem::path& path);

private:
    std::filesystem::path configuredAssetDirectory_ {};
};
} // namespace editor

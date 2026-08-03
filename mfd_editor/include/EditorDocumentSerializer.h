/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Serialization helpers mapping the in-memory editor document to authored JSON files.
 */

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "mfd/io/JsonLoader.h"
#include "mfd/model/PageName.h"

namespace editor
{
/**
 * @brief Tracks the authored files created, updated or removed by the editor.
 */
struct EditorFileLayout
{
    std::vector<std::filesystem::path> pageFiles;
    std::vector<std::filesystem::path> removedPageFiles;
    std::unordered_map<std::string, std::filesystem::path, mfd::TransparentStringHash, mfd::TransparentStringEqual>
        templateFiles;
    std::vector<std::filesystem::path> removedTemplateFiles;
};

/** @brief Returns the default page JSON path for a new page authored from a window file. */
std::filesystem::path DefaultPageFilePath(const std::filesystem::path& windowFile, std::string_view pageName);
/** @brief Returns the default template JSON path for a reticle template id. */
std::filesystem::path DefaultTemplateFilePath(const std::filesystem::path& libraryFolder, std::string_view templateId);

/** @brief Discovers template JSON files from a library folder and updates the editor file layout. */
bool DiscoverReticleTemplateFiles(const std::filesystem::path& libraryFolder,
                                  EditorFileLayout& layout,
                                  std::string* error);

/** @brief Serializes one reticle template to its normalized JSON representation. */
std::string SerializeReticleTemplateToJsonString(const mfd::ReticleGroup& reticle,
                                                 const std::filesystem::path& baseFolder = {});
/** @brief Serializes one page reticle (with library overrides) to JSON. */
std::string SerializePageReticleToJsonString(const mfd::ReticleGroup& reticle,
                                             const mfd::ReticleLibrary& library,
                                             const std::filesystem::path& baseFolder = {});
/** @brief Serializes one full page asset to its normalized JSON representation. */
std::string SerializePageToJsonString(const mfd::PageDefinition& page,
                                      const mfd::ReticleLibrary& library,
                                      const EditorFileLayout& layout,
                                      std::size_t pageIndex);
/** @brief Serializes one full window asset to its normalized JSON representation. */
std::string SerializeWindowToJsonString(const mfd::WindowAssetDefinition& window,
                                        const mfd::MfdDocument& document,
                                        const EditorFileLayout& layout);

/**
 * @brief Saves all currently loaded editor assets to disk as one transaction.
 * @param loaded Loaded document whose authored assets must be saved.
 * @param layout Target and obsolete authored file paths.
 * @param error Optional destination for a human-readable failure description.
 * @return `true` when every staged file was verified and promoted.
 * @note Unsafe identifiers, paths outside the authored asset root and target collisions are rejected
 * before writing. Existing files are restored if staging or promotion fails.
 */
bool SaveEditorDocument(const mfd::LoadedWindowConfiguration& loaded,
                        const EditorFileLayout& layout,
                        std::string* error);

/**
 * @brief Serializes the whole editor document into one self-contained crash-recovery bundle.
 * @param loaded Loaded window configuration to snapshot.
 * @param layout Authored file layout providing the target paths.
 * @return A JSON string carrying every authored file path and its serialized content.
 */
std::string SerializeRecoveryBundleToJsonString(const mfd::LoadedWindowConfiguration& loaded,
                                                const EditorFileLayout& layout);

/**
 * @brief Restores a recovery bundle by writing its files back to disk, transactionally.
 * @param bundleJson Bundle produced by @ref SerializeRecoveryBundleToJsonString.
 * @param assetRoot Authored asset root; every bundle path must resolve inside it.
 * @param windowFile Receives the window file path to reload after restoration.
 * @param error Optional destination for a human-readable failure description.
 * @return `true` when every file was written and the window path was recovered.
 * @pre `assetRoot` is the directory that legitimately contains the authored files.
 * @note Paths outside `assetRoot` are rejected before any write. All entries are validated and staged
 * into temporary files first, then renamed into place, so a mid-restore failure leaves the existing
 * authored files untouched.
 */
bool RestoreRecoveryBundle(const std::string& bundleJson,
                           const std::filesystem::path& assetRoot,
                           std::filesystem::path& windowFile,
                           std::string* error);
} // namespace editor

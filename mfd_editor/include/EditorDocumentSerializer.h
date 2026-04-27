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

/** @brief Saves all currently loaded editor assets to disk. */
bool SaveEditorDocument(const mfd::LoadedWindowConfiguration& loaded,
                        const EditorFileLayout& layout,
                        std::string* error);
} // namespace editor

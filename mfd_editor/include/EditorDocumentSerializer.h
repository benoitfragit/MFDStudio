/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "mfd/io/JsonLoader.h"
#include "mfd/model/PageName.h"

namespace editor
{
struct EditorFileLayout
{
    std::vector<std::filesystem::path> pageFiles;
    std::vector<std::filesystem::path> removedPageFiles;
    std::unordered_map<std::string, std::filesystem::path, mfd::TransparentStringHash, mfd::TransparentStringEqual>
        templateFiles;
    std::vector<std::filesystem::path> removedTemplateFiles;
};

std::filesystem::path DefaultPageFilePath(const std::filesystem::path& windowFile, std::string_view pageName);
std::filesystem::path DefaultTemplateFilePath(const std::filesystem::path& libraryFolder, std::string_view templateId);

bool DiscoverReticleTemplateFiles(const std::filesystem::path& libraryFolder,
                                  EditorFileLayout& layout,
                                  std::string* error);

std::string SerializeReticleTemplateToJsonString(const mfd::ReticleGroup& reticle);
std::string SerializePageReticleToJsonString(const mfd::ReticleGroup& reticle,
                                             const mfd::ReticleLibrary& library);

bool SaveEditorDocument(const mfd::LoadedWindowConfiguration& loaded,
                        const EditorFileLayout& layout,
                        std::string* error);
} // namespace editor

/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorAssetReferenceIndexService.h"

/**
 * @file
 * @brief Implementation of the editor asset reference scanner.
 */

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <utility>

#include <nlohmann/json.hpp>

#include "mfd/model/PageName.h"

namespace editor
{
namespace
{
using json = nlohmann::json;

enum class ParsedAssetKind
{
    Unknown,
    Window,
    Page,
    TemplateAsset
};

struct ParsedAssetFile
{
    std::filesystem::path file;
    ParsedAssetKind kind = ParsedAssetKind::Unknown;
    json document;
    std::string pageName;
    std::string templateId;
};

std::string Lowercase(std::string_view value)
{
    std::string lowered;
    lowered.reserve(value.size());

    for (const char character : value)
    {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
    }

    return lowered;
}

std::filesystem::path NormalizePath(std::filesystem::path path)
{
    if (path.empty())
    {
        return {};
    }

    std::error_code error;
    if (!path.is_absolute())
    {
        path = std::filesystem::absolute(path, error);
        if (error)
        {
            error.clear();
        }
    }

    return path.lexically_normal();
}

std::filesystem::path ResolvePath(const std::filesystem::path& baseFolder, const std::filesystem::path& candidate)
{
    if (candidate.empty())
    {
        return {};
    }

    if (candidate.is_absolute())
    {
        return NormalizePath(candidate);
    }

    return NormalizePath(baseFolder / candidate);
}

bool PathExists(const std::filesystem::path& path) noexcept
{
    if (path.empty())
    {
        return false;
    }

    std::error_code error;
    return std::filesystem::exists(path, error);
}

bool HasJsonExtension(const std::filesystem::path& path)
{
    return Lowercase(path.extension().string()) == ".json";
}

const json* FindField(const json& node, std::initializer_list<const char*> fieldNames)
{
    if (!node.is_object())
    {
        return nullptr;
    }

    for (const char* fieldName : fieldNames)
    {
        const auto iterator = node.find(fieldName);
        if (iterator != node.end())
        {
            return &(*iterator);
        }
    }

    return nullptr;
}

std::optional<std::string> GetStringField(const json& node, std::initializer_list<const char*> fieldNames)
{
    const json* field = FindField(node, fieldNames);
    if (field == nullptr || !field->is_string())
    {
        return std::nullopt;
    }

    return field->get<std::string>();
}

ParsedAssetKind ClassifyDocument(const json& document)
{
    if (!document.is_object())
    {
        return ParsedAssetKind::Unknown;
    }

    if (const json* pages = FindField(document, {"pageFiles", "pages", "pageJsons"});
        pages != nullptr && pages->is_array())
    {
        return ParsedAssetKind::Window;
    }

    if (document.contains("elements") && document.at("elements").is_array())
    {
        return ParsedAssetKind::TemplateAsset;
    }

    const json* pageNode = &document;
    if (document.contains("page") && document.at("page").is_object())
    {
        pageNode = &document.at("page");
    }

    if (FindField(*pageNode, {"name", "id", "staticReticles", "strobe", "blinkTypes", "defaultBlinkType", "backgroundColor", "bg"}) !=
        nullptr)
    {
        return ParsedAssetKind::Page;
    }

    return ParsedAssetKind::Unknown;
}

void AddScanError(AssetReferenceIndex& index, const std::filesystem::path& file, std::string message)
{
    index.errors.push_back(AssetScanError {NormalizePath(file), std::move(message)});
}

void AddMissingAsset(AssetReferenceIndex& index,
                     const MissingAssetKind kind,
                     const std::filesystem::path& ownerFile,
                     std::filesystem::path referencedPath,
                     std::string pageName = {},
                     std::string reticleId = {},
                     std::string templateId = {},
                     std::string primitiveId = {},
                     std::string detail = {})
{
    index.missingAssets.push_back(MissingAssetReference {
        kind,
        NormalizePath(ownerFile),
        NormalizePath(std::move(referencedPath)),
        std::move(pageName),
        std::move(reticleId),
        std::move(templateId),
        std::move(primitiveId),
        std::move(detail)});
}

void AddReference(AssetReferenceIndex& index,
                  const ReticleReferenceKind kind,
                  const std::filesystem::path& ownerFile,
                  const std::filesystem::path& targetFile,
                  std::string pageName = {},
                  std::string reticleId = {},
                  std::string templateId = {},
                  std::string primitiveId = {})
{
    index.reticles.push_back(ReticleReference {
        kind,
        NormalizePath(ownerFile),
        NormalizePath(targetFile),
        std::move(pageName),
        std::move(reticleId),
        std::move(templateId),
        std::move(primitiveId)});
}

void ScanImageElements(const json& elements,
                       const std::filesystem::path& ownerFile,
                       const std::string& pageName,
                       const std::string& reticleId,
                       const std::string& templateId,
                       AssetReferenceIndex& index)
{
    if (!elements.is_array())
    {
        AddScanError(index, ownerFile, "Reticle elements must be a JSON array");
        return;
    }

    for (const auto& element : elements)
    {
        if (!element.is_object())
        {
            continue;
        }

        const std::string primitiveType = Lowercase(element.value("type", ""));
        if (primitiveType != "image")
        {
            continue;
        }

        const std::string primitiveId = element.value("id", "");
        const json* fileField = FindField(element, {"file", "image", "source", "path"});
        if (fileField == nullptr || !fileField->is_string())
        {
            AddScanError(index,
                         ownerFile,
                         "Image primitive '" + primitiveId + "' must declare one string file path");
            continue;
        }

        const std::filesystem::path imageFile = ResolvePath(ownerFile.parent_path(), fileField->get<std::string>());
        AddReference(index,
                     ReticleReferenceKind::ReticleImageAsset,
                     ownerFile,
                     imageFile,
                     pageName,
                     reticleId,
                     templateId,
                     primitiveId);

        if (!PathExists(imageFile))
        {
            AddMissingAsset(index,
                            MissingAssetKind::ImageFile,
                            ownerFile,
                            imageFile,
                            pageName,
                            reticleId,
                            templateId,
                            primitiveId,
                            "Image asset does not exist");
        }
    }
}

void ScanReticleNode(const json& reticleNode,
                     const std::filesystem::path& ownerFile,
                     const std::string& pageName,
                     const ReticleReferenceKind templateReferenceKind,
                     const std::unordered_map<std::string, std::filesystem::path>& templateFilesById,
                     AssetReferenceIndex& index)
{
    if (!reticleNode.is_object())
    {
        return;
    }

    const std::string reticleId = reticleNode.value("id", "");
    if (const json* templateField = FindField(reticleNode, {"template"});
        templateField != nullptr && templateField->is_string())
    {
        const std::string templateId = templateField->get<std::string>();
        const auto iterator = templateFilesById.find(templateId);
        AddReference(index,
                     templateReferenceKind,
                     ownerFile,
                     iterator != templateFilesById.end() ? iterator->second : std::filesystem::path {},
                     pageName,
                     reticleId.empty() ? templateId : reticleId,
                     templateId);

        if (iterator == templateFilesById.end())
        {
            AddMissingAsset(index,
                            MissingAssetKind::ReticleTemplate,
                            ownerFile,
                            {},
                            pageName,
                            reticleId.empty() ? templateId : reticleId,
                            templateId,
                            {},
                            "Reticle template id was not found under the scanned assets root");
        }
    }

    if (const json* elements = FindField(reticleNode, {"elements"}); elements != nullptr)
    {
        ScanImageElements(*elements, ownerFile, pageName, reticleId, {}, index);
    }
}

std::vector<std::filesystem::path> ParseWindowPageFiles(const json& document,
                                                        const std::filesystem::path& ownerFile,
                                                        AssetReferenceIndex& index)
{
    std::vector<std::filesystem::path> pageFiles;
    const json* pages = FindField(document, {"pageFiles", "pages", "pageJsons"});
    if (pages == nullptr)
    {
        return pageFiles;
    }

    if (!pages->is_array())
    {
        AddScanError(index, ownerFile, "Window pages field must be a JSON array");
        return pageFiles;
    }

    pageFiles.reserve(pages->size());
    for (const auto& entry : *pages)
    {
        if (entry.is_string())
        {
            pageFiles.push_back(ResolvePath(ownerFile.parent_path(), entry.get<std::string>()));
            continue;
        }

        if (entry.is_object())
        {
            const json* fileField = FindField(entry, {"file", "path", "json"});
            if (fileField != nullptr && fileField->is_string())
            {
                pageFiles.push_back(ResolvePath(ownerFile.parent_path(), fileField->get<std::string>()));
                continue;
            }
        }

        AddScanError(index,
                     ownerFile,
                     "Each page entry must be a string path or an object containing file/path/json");
    }

    return pageFiles;
}

void ProcessWindowAssetReference(const json& document,
                                 const std::filesystem::path& ownerFile,
                                 const std::initializer_list<const char*> fieldNames,
                                 const ReticleReferenceKind referenceKind,
                                 const MissingAssetKind missingKind,
                                 AssetReferenceIndex& index)
{
    const json* field = FindField(document, fieldNames);
    if (field == nullptr)
    {
        return;
    }

    if (!field->is_string())
    {
        AddScanError(index, ownerFile, "Window asset path fields must be strings");
        return;
    }

    const std::filesystem::path targetFile = ResolvePath(ownerFile.parent_path(), field->get<std::string>());
    AddReference(index, referenceKind, ownerFile, targetFile);
    if (!PathExists(targetFile))
    {
        AddMissingAsset(index, missingKind, ownerFile, targetFile, {}, {}, {}, {}, "Window asset does not exist");
    }
}

void ProcessWindowDocument(const ParsedAssetFile& asset,
                           const std::unordered_map<std::string, std::string>& pageNamesByPath,
                           AssetReferenceIndex& index)
{
    const std::optional<std::string> defaultPageName = GetStringField(asset.document, {"defaultPage"});
    const std::vector<std::filesystem::path> pageFiles = ParseWindowPageFiles(asset.document, asset.file, index);

    for (const auto& pageFile : pageFiles)
    {
        std::string pageName;
        const auto pageNameIterator = pageNamesByPath.find(pageFile.generic_string());
        if (pageNameIterator != pageNamesByPath.end())
        {
            pageName = pageNameIterator->second;
        }

        index.pages.push_back(PageReference {
            NormalizePath(asset.file),
            NormalizePath(pageFile),
            pageName,
            defaultPageName.has_value() && !pageName.empty() && mfd::PageNamesEqual(pageName, *defaultPageName)});

        if (!PathExists(pageFile))
        {
            AddMissingAsset(index,
                            MissingAssetKind::PageFile,
                            asset.file,
                            pageFile,
                            pageName,
                            {},
                            {},
                            {},
                            "Window references a missing page JSON file");
        }
    }

    ProcessWindowAssetReference(
        asset.document,
        asset.file,
        {"fontFile", "font", "fontPath"},
        ReticleReferenceKind::WindowFontAsset,
        MissingAssetKind::FontFile,
        index);
    ProcessWindowAssetReference(
        asset.document,
        asset.file,
        {"iconFile", "icon", "windowIcon", "windowIconFile", "iconPath"},
        ReticleReferenceKind::WindowIconAsset,
        MissingAssetKind::IconFile,
        index);
}

void ProcessPageDocument(const ParsedAssetFile& asset,
                         const std::unordered_map<std::string, std::filesystem::path>& templateFilesById,
                         AssetReferenceIndex& index)
{
    const json& pageNode =
        asset.document.contains("page") && asset.document.at("page").is_object() ? asset.document.at("page")
                                                                                  : asset.document;

    if (const json* staticReticles = FindField(pageNode, {"staticReticles"}); staticReticles != nullptr)
    {
        if (!staticReticles->is_array())
        {
            AddScanError(index, asset.file, "Page staticReticles field must be a JSON array");
        }
        else
        {
            for (const auto& reticleNode : *staticReticles)
            {
                ScanReticleNode(reticleNode,
                                asset.file,
                                asset.pageName,
                                ReticleReferenceKind::PageReticleTemplate,
                                templateFilesById,
                                index);
            }
        }
    }

    if (const json* strobe = FindField(pageNode, {"strobe"}); strobe != nullptr)
    {
        if (!strobe->is_object())
        {
            AddScanError(index, asset.file, "Page strobe field must be a JSON object");
        }
        else
        {
            ScanReticleNode(*strobe,
                            asset.file,
                            asset.pageName,
                            ReticleReferenceKind::PageStrobeTemplate,
                            templateFilesById,
                            index);
        }
    }

    const json* editorNode = FindField(pageNode, {"_editor"});
    if (editorNode != nullptr && editorNode->is_object())
    {
        const json* dynamicTemplates = FindField(*editorNode, {"dynamicReticleTemplates"});
        if (dynamicTemplates != nullptr)
        {
            if (!dynamicTemplates->is_array())
            {
                AddScanError(index, asset.file, "Page editor dynamicReticleTemplates field must be a JSON array");
            }
            else
            {
                for (const auto& entry : *dynamicTemplates)
                {
                    if (!entry.is_string())
                    {
                        AddScanError(index, asset.file, "Page editor dynamic reticle template ids must be strings");
                        continue;
                    }

                    const std::string templateId = entry.get<std::string>();
                    const auto iterator = templateFilesById.find(templateId);
                    AddReference(index,
                                 ReticleReferenceKind::PageDynamicTemplate,
                                 asset.file,
                                 iterator != templateFilesById.end() ? iterator->second : std::filesystem::path {},
                                 asset.pageName,
                                 {},
                                 templateId);

                    if (iterator == templateFilesById.end())
                    {
                        AddMissingAsset(index,
                                        MissingAssetKind::ReticleTemplate,
                                        asset.file,
                                        {},
                                        asset.pageName,
                                        {},
                                        templateId,
                                        {},
                                        "Editor dynamic reticle template id was not found under the scanned assets root");
                    }
                }
            }
        }
    }
}

void ProcessTemplateDocument(const ParsedAssetFile& asset, AssetReferenceIndex& index)
{
    const json* elements = FindField(asset.document, {"elements"});
    if (elements == nullptr)
    {
        return;
    }

    ScanImageElements(*elements, asset.file, {}, asset.templateId, asset.templateId, index);
}

void SortIndex(AssetReferenceIndex& index)
{
    const auto pathKey = [](const std::filesystem::path& path)
    {
        return path.generic_string();
    };

    std::sort(index.pages.begin(),
              index.pages.end(),
              [&pathKey](const PageReference& lhs, const PageReference& rhs)
              {
                  return std::make_tuple(pathKey(lhs.windowFile), pathKey(lhs.pageFile), lhs.pageName, lhs.defaultPage) <
                         std::make_tuple(pathKey(rhs.windowFile), pathKey(rhs.pageFile), rhs.pageName, rhs.defaultPage);
              });

    std::sort(index.reticles.begin(),
              index.reticles.end(),
              [&pathKey](const ReticleReference& lhs, const ReticleReference& rhs)
              {
                  return std::make_tuple(static_cast<int>(lhs.kind),
                                         pathKey(lhs.ownerFile),
                                         pathKey(lhs.targetFile),
                                         lhs.pageName,
                                         lhs.reticleId,
                                         lhs.templateId,
                                         lhs.primitiveId) <
                         std::make_tuple(static_cast<int>(rhs.kind),
                                         pathKey(rhs.ownerFile),
                                         pathKey(rhs.targetFile),
                                         rhs.pageName,
                                         rhs.reticleId,
                                         rhs.templateId,
                                         rhs.primitiveId);
              });

    std::sort(index.missingAssets.begin(),
              index.missingAssets.end(),
              [&pathKey](const MissingAssetReference& lhs, const MissingAssetReference& rhs)
              {
                  return std::make_tuple(static_cast<int>(lhs.kind),
                                         pathKey(lhs.ownerFile),
                                         pathKey(lhs.referencedPath),
                                         lhs.pageName,
                                         lhs.reticleId,
                                         lhs.templateId,
                                         lhs.primitiveId,
                                         lhs.detail) <
                         std::make_tuple(static_cast<int>(rhs.kind),
                                         pathKey(rhs.ownerFile),
                                         pathKey(rhs.referencedPath),
                                         rhs.pageName,
                                         rhs.reticleId,
                                         rhs.templateId,
                                         rhs.primitiveId,
                                         rhs.detail);
              });

    std::sort(index.errors.begin(),
              index.errors.end(),
              [&pathKey](const AssetScanError& lhs, const AssetScanError& rhs)
              {
                  return std::make_tuple(pathKey(lhs.file), lhs.message) <
                         std::make_tuple(pathKey(rhs.file), rhs.message);
              });
}
} // namespace

AssetReferenceIndex AssetReferenceIndexService::BuildIndex(const AssetReferenceIndexRequest& request) const
{
    AssetReferenceIndex index;

    const std::filesystem::path assetRoot = NormalizePath(request.assetsRoot.empty() ? std::filesystem::current_path()
                                                                                     : request.assetsRoot);

    std::error_code rootError;
    if (!std::filesystem::exists(assetRoot, rootError) || !std::filesystem::is_directory(assetRoot, rootError))
    {
        AddScanError(index, assetRoot, "Asset root does not exist or is not a directory");
        return index;
    }

    std::vector<ParsedAssetFile> parsedAssets;
    std::unordered_map<std::string, std::filesystem::path> templateFilesById;

    std::error_code iterationError;
    std::filesystem::recursive_directory_iterator iterator(
        assetRoot,
        std::filesystem::directory_options::skip_permission_denied,
        iterationError);
    const std::filesystem::recursive_directory_iterator end;

    if (iterationError)
    {
        AddScanError(index, assetRoot, "Unable to enumerate asset root: " + iterationError.message());
        return index;
    }

    while (iterator != end)
    {
        const std::filesystem::directory_entry entry = *iterator;
        const std::filesystem::path entryPath = NormalizePath(entry.path());

        std::error_code statusError;
        if (entry.is_directory(statusError))
        {
            iterator.increment(iterationError);
            if (iterationError)
            {
                AddScanError(index, assetRoot, "Directory scan failed: " + iterationError.message());
                iterationError.clear();
            }
            continue;
        }

        if (statusError)
        {
            AddScanError(index, entryPath, "Unable to query filesystem entry: " + statusError.message());
            iterator.increment(iterationError);
            if (iterationError)
            {
                AddScanError(index, assetRoot, "Directory scan failed: " + iterationError.message());
                iterationError.clear();
            }
            continue;
        }

        if (!entry.is_regular_file(statusError) || statusError || !HasJsonExtension(entryPath))
        {
            iterator.increment(iterationError);
            if (iterationError)
            {
                AddScanError(index, assetRoot, "Directory scan failed: " + iterationError.message());
                iterationError.clear();
            }
            continue;
        }

        try
        {
            std::ifstream input(entryPath);
            if (!input.is_open())
            {
                AddScanError(index, entryPath, "Unable to open JSON file");
            }
            else
            {
                json document;
                input >> document;

                ParsedAssetFile parsedAsset;
                parsedAsset.file = entryPath;
                parsedAsset.kind = ClassifyDocument(document);
                parsedAsset.document = std::move(document);
                if (parsedAsset.kind == ParsedAssetKind::Page)
                {
                    const json& pageNode =
                        parsedAsset.document.contains("page") && parsedAsset.document.at("page").is_object()
                            ? parsedAsset.document.at("page")
                            : parsedAsset.document;
                    parsedAsset.pageName = pageNode.value("name", pageNode.value("id", ""));
                }
                else if (parsedAsset.kind == ParsedAssetKind::TemplateAsset)
                {
                    parsedAsset.templateId = parsedAsset.document.value("id", "");
                    if (parsedAsset.templateId.empty())
                    {
                        AddScanError(index, entryPath, "Reticle template files must declare one non-empty id");
                    }
                    else if (const auto [iteratorTemplate, inserted] =
                                 templateFilesById.emplace(parsedAsset.templateId, parsedAsset.file);
                             !inserted)
                    {
                        AddScanError(index,
                                     entryPath,
                                     "Duplicate reticle template id '" + parsedAsset.templateId + "'");
                        AddScanError(index,
                                     iteratorTemplate->second,
                                     "Duplicate reticle template id '" + parsedAsset.templateId + "'");
                    }
                }

                parsedAssets.push_back(std::move(parsedAsset));
            }
        }
        catch (const nlohmann::json::parse_error& exception)
        {
            AddScanError(index, entryPath, exception.what());
        }
        catch (const nlohmann::json::exception& exception)
        {
            AddScanError(index, entryPath, exception.what());
        }
        catch (const std::exception& exception)
        {
            AddScanError(index, entryPath, exception.what());
        }

        iterator.increment(iterationError);
        if (iterationError)
        {
            AddScanError(index, assetRoot, "Directory scan failed: " + iterationError.message());
            iterationError.clear();
        }
    }

    std::unordered_map<std::string, std::string> pageNamesByPath;
    for (const ParsedAssetFile& asset : parsedAssets)
    {
        if (asset.kind == ParsedAssetKind::Page)
        {
            pageNamesByPath.emplace(asset.file.generic_string(), asset.pageName);
        }
    }

    for (const ParsedAssetFile& asset : parsedAssets)
    {
        switch (asset.kind)
        {
        case ParsedAssetKind::Window:
            ProcessWindowDocument(asset, pageNamesByPath, index);
            break;
        case ParsedAssetKind::Page:
            ProcessPageDocument(asset, templateFilesById, index);
            break;
        case ParsedAssetKind::TemplateAsset:
            ProcessTemplateDocument(asset, index);
            break;
        case ParsedAssetKind::Unknown:
        default:
            break;
        }
    }

    SortIndex(index);
    return index;
}
} // namespace editor

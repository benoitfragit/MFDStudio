/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorPageImportService.h"

/**
 * @file
 * @brief Implementation of the editor page-import planning service.
 */

#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

#include <nlohmann/json.hpp>

namespace editor
{
namespace
{
using json = nlohmann::json;

struct PageSourceMetadata
{
    std::string pageName;
    std::vector<std::string> templateIds;
    bool usesPageWrapper = false;
};

class ScopedImportWorkspace
{
public:
    ScopedImportWorkspace()
    {
        const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
        root_ = std::filesystem::temp_directory_path() /
                std::filesystem::path("mfd_page_import_" + std::to_string(ticks));
        std::filesystem::create_directories(root_);
    }

    ~ScopedImportWorkspace()
    {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
    }

    [[nodiscard]] const std::filesystem::path& Root() const noexcept
    {
        return root_;
    }

private:
    std::filesystem::path root_;
};

std::string Lowercase(const std::string_view value)
{
    std::string lowered;
    lowered.reserve(value.size());

    for (const char character : value)
    {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
    }

    return lowered;
}

std::string PathKey(const std::filesystem::path& path)
{
    return Lowercase(path.generic_string());
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

bool PathExists(const std::filesystem::path& path)
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

bool PathContainsSegment(const std::filesystem::path& path, const std::string_view segment)
{
    const std::string loweredSegment = Lowercase(segment);
    for (const auto& component : path)
    {
        if (Lowercase(component.string()) == loweredSegment)
        {
            return true;
        }
    }

    return false;
}

std::optional<std::filesystem::path> FindAncestorNamed(std::filesystem::path path, const std::string_view folderName)
{
    path = NormalizePath(std::move(path));
    const std::string loweredFolderName = Lowercase(folderName);

    while (!path.empty())
    {
        if (Lowercase(path.filename().string()) == loweredFolderName)
        {
            return path;
        }

        if (!path.has_parent_path() || path == path.parent_path())
        {
            break;
        }

        path = path.parent_path();
    }

    return std::nullopt;
}

std::filesystem::path InferAssetsRootFromPageFile(const std::filesystem::path& sourcePageFile)
{
    const std::filesystem::path normalizedPageFile = NormalizePath(sourcePageFile);
    const std::filesystem::path pageFolder = normalizedPageFile.parent_path();

    if (const auto assetsRoot = FindAncestorNamed(pageFolder, "assets"); assetsRoot.has_value())
    {
        return *assetsRoot;
    }

    if (Lowercase(pageFolder.filename().string()) == "pages" && pageFolder.has_parent_path())
    {
        return pageFolder.parent_path();
    }

    std::error_code error;
    if (std::filesystem::exists(pageFolder / "reticles", error))
    {
        return pageFolder;
    }

    error.clear();
    if (pageFolder.has_parent_path() && std::filesystem::exists(pageFolder.parent_path() / "reticles", error))
    {
        return pageFolder.parent_path();
    }

    return pageFolder;
}

json LoadJsonFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open())
    {
        throw std::runtime_error("Unable to open JSON file: " + path.string());
    }

    json document;
    input >> document;
    return document;
}

std::string CanonicalizeJson(const json& document)
{
    return document.dump();
}

std::string CanonicalizeJsonString(const std::string_view jsonText)
{
    return CanonicalizeJson(json::parse(jsonText.begin(), jsonText.end()));
}

std::string LoadCanonicalJsonFile(const std::filesystem::path& path)
{
    return CanonicalizeJson(LoadJsonFile(path));
}

const json& ExtractPageNode(const json& document)
{
    if (document.is_object() && document.contains("page") && document.at("page").is_object())
    {
        return document.at("page");
    }

    return document;
}

PageSourceMetadata ParsePageSourceMetadata(const std::filesystem::path& sourcePageFile)
{
    const json document = LoadJsonFile(sourcePageFile);
    const json& pageNode = ExtractPageNode(document);
    if (!pageNode.is_object())
    {
        throw std::runtime_error("Page JSON must contain one object root or one {\"page\": {...}} wrapper.");
    }

    const json* nameField = pageNode.contains("name") ? &pageNode.at("name") : nullptr;
    if (nameField == nullptr && pageNode.contains("id"))
    {
        nameField = &pageNode.at("id");
    }
    if (nameField == nullptr || !nameField->is_string() || nameField->get<std::string>().empty())
    {
        throw std::runtime_error("Imported page JSON must declare one non-empty page name.");
    }

    PageSourceMetadata metadata;
    metadata.pageName = nameField->get<std::string>();
    metadata.usesPageWrapper = &pageNode != &document;

    std::unordered_set<std::string, mfd::TransparentStringHash, mfd::TransparentStringEqual> seenTemplateIds;

    if (pageNode.contains("staticReticles"))
    {
        if (!pageNode.at("staticReticles").is_array())
        {
            throw std::runtime_error("Page staticReticles field must be a JSON array.");
        }

        for (const auto& reticleNode : pageNode.at("staticReticles"))
        {
            if (!reticleNode.is_object() || !reticleNode.contains("template") || !reticleNode.at("template").is_string())
            {
                continue;
            }

            const std::string templateId = reticleNode.at("template").get<std::string>();
            if (seenTemplateIds.insert(templateId).second)
            {
                metadata.templateIds.push_back(templateId);
            }
        }
    }

    if (pageNode.contains("strobe"))
    {
        if (!pageNode.at("strobe").is_object())
        {
            throw std::runtime_error("Page strobe field must be a JSON object.");
        }

        const auto& strobeNode = pageNode.at("strobe");
        if (strobeNode.contains("template") && strobeNode.at("template").is_string())
        {
            const std::string templateId = strobeNode.at("template").get<std::string>();
            if (seenTemplateIds.insert(templateId).second)
            {
                metadata.templateIds.push_back(templateId);
            }
        }
    }

    return metadata;
}

std::string NormalizeIdentifier(const std::string_view value)
{
    return mfd::NormalizePageName(value);
}

bool HasPageName(const mfd::LoadedWindowConfiguration& loaded, const std::string_view pageName)
{
    const std::string normalizedCandidate = NormalizeIdentifier(pageName);
    return std::any_of(loaded.document.pages.begin(),
                       loaded.document.pages.end(),
                       [&normalizedCandidate](const mfd::PageDefinition& page)
                       {
                           return page.normalizedName == normalizedCandidate;
                       });
}

bool HasTemplateId(const mfd::LoadedWindowConfiguration& loaded, const std::string_view templateId)
{
    const std::string normalizedCandidate = NormalizeIdentifier(templateId);
    return std::any_of(loaded.document.reticleLibrary.begin(),
                       loaded.document.reticleLibrary.end(),
                       [&normalizedCandidate](const auto& entry)
                       {
                           return NormalizeIdentifier(entry.first) == normalizedCandidate;
                       });
}

std::string SuggestUniqueImportedId(
    const std::string_view baseId,
    const std::function<bool(std::string_view)>& isTaken)
{
    if (!isTaken(baseId))
    {
        return std::string(baseId);
    }

    const std::string imported = std::string(baseId) + "_Imported";
    if (!isTaken(imported))
    {
        return imported;
    }

    for (int suffix = 2; suffix < 10000; ++suffix)
    {
        const std::string candidate = std::string(baseId) + "_Imported_" + std::to_string(suffix);
        if (!isTaken(candidate))
        {
            return candidate;
        }
    }

    throw std::runtime_error("Unable to find a unique imported identifier.");
}

std::filesystem::path SuggestUniqueJsonPath(
    const std::filesystem::path& preferredPath,
    const std::function<bool(const std::filesystem::path&)>& isTaken)
{
    if (!isTaken(preferredPath))
    {
        return preferredPath;
    }

    const std::string stem = preferredPath.stem().string();
    const std::filesystem::path extension = preferredPath.extension().empty() ? std::filesystem::path(".json")
                                                                              : preferredPath.extension();
    for (int suffix = 2; suffix < 10000; ++suffix)
    {
        const std::filesystem::path candidate =
            preferredPath.parent_path() / std::filesystem::path(stem + "_" + std::to_string(suffix) + extension.string());
        if (!isTaken(candidate))
        {
            return candidate.lexically_normal();
        }
    }

    throw std::runtime_error("Unable to find a unique imported JSON target path.");
}

bool PathInList(const std::vector<std::filesystem::path>& paths, const std::filesystem::path& candidate)
{
    const std::string candidateKey = PathKey(candidate);
    return std::any_of(paths.begin(),
                       paths.end(),
                       [&candidateKey](const std::filesystem::path& path)
                       {
                           return PathKey(path) == candidateKey;
                       });
}

bool PathInTemplateMap(const EditorFileLayout& files, const std::filesystem::path& candidate)
{
    const std::string candidateKey = PathKey(candidate);
    return std::any_of(files.templateFiles.begin(),
                       files.templateFiles.end(),
                       [&candidateKey](const auto& entry)
                       {
                           return PathKey(entry.second) == candidateKey;
                       });
}

std::filesystem::path ResolveCurrentTemplateFile(const EditorFileLayout& files,
                                                 const std::filesystem::path& targetReticleLibraryFolder,
                                                 const std::string_view templateId)
{
    if (const auto iterator = files.templateFiles.find(std::string(templateId)); iterator != files.templateFiles.end())
    {
        return NormalizePath(iterator->second);
    }

    return NormalizePath(DefaultTemplateFilePath(targetReticleLibraryFolder, templateId));
}

bool CurrentTemplateMatchesSource(const mfd::LoadedWindowConfiguration& loaded,
                                  const EditorFileLayout& files,
                                  const std::filesystem::path& targetReticleLibraryFolder,
                                  const std::string_view templateId,
                                  const std::filesystem::path& sourceFile)
{
    const auto iterator = loaded.document.reticleLibrary.find(std::string(templateId));
    if (iterator == loaded.document.reticleLibrary.end())
    {
        return false;
    }

    const std::filesystem::path currentTemplateFile =
        ResolveCurrentTemplateFile(files, targetReticleLibraryFolder, templateId);
    const std::string sourceCanonical = LoadCanonicalJsonFile(sourceFile);
    if (PathExists(currentTemplateFile) && LoadCanonicalJsonFile(currentTemplateFile) == sourceCanonical)
    {
        return true;
    }

    const std::string currentCanonical = CanonicalizeJsonString(
        SerializeReticleTemplateToJsonString(iterator->second, currentTemplateFile.parent_path()));
    return currentCanonical == sourceCanonical;
}

bool DiskJsonMatchesSource(const std::filesystem::path& targetFile, const std::filesystem::path& sourceFile)
{
    if (!PathExists(targetFile))
    {
        return false;
    }

    return LoadCanonicalJsonFile(targetFile) == LoadCanonicalJsonFile(sourceFile);
}

void RenameImportedTemplate(mfd::ReticleGroup& reticle, const std::string& targetTemplateId)
{
    reticle.id = targetTemplateId;
    reticle.sourceTemplateId = targetTemplateId;
}

void ApplyTemplateIdRemap(
    mfd::PageDefinition& page,
    const std::unordered_map<std::string, std::string, mfd::TransparentStringHash, mfd::TransparentStringEqual>& remap)
{
    const auto remapOne = [&remap](mfd::ReticleGroup& reticle)
    {
        if (reticle.sourceTemplateId.empty())
        {
            return;
        }

        const auto iterator = remap.find(reticle.sourceTemplateId);
        if (iterator != remap.end())
        {
            reticle.sourceTemplateId = iterator->second;
        }
    };

    for (auto& reticle : page.staticReticles)
    {
        remapOne(reticle);
    }

    if (page.strobe.has_value())
    {
        remapOne(page.strobe->reticle);
    }
}

bool HasAnyDefaultPage(const std::vector<mfd::PageDefinition>& pages)
{
    return std::any_of(pages.begin(),
                       pages.end(),
                       [](const mfd::PageDefinition& page)
                       {
                           return page.defaultPage;
                       });
}

void RemovePathIfPresent(std::vector<std::filesystem::path>& paths, const std::filesystem::path& candidate)
{
    const std::string candidateKey = PathKey(candidate);
    paths.erase(std::remove_if(paths.begin(),
                               paths.end(),
                               [&candidateKey](const std::filesystem::path& path)
                               {
                                   return PathKey(path) == candidateKey;
                               }),
                paths.end());
}

mfd::LoadedWindowConfiguration LoadSourceConfigurationForImport(const PageImportPlan& plan)
{
    ScopedImportWorkspace workspace;
    const std::filesystem::path windowFile = workspace.Root() / "page_import_window.json";
    const std::filesystem::path emptyReticleFolder = workspace.Root() / "reticles";

    std::filesystem::path sourceReticleLibraryFolder = plan.sourceReticleLibraryFolder;
    if (sourceReticleLibraryFolder.empty())
    {
        std::filesystem::create_directories(emptyReticleFolder);
        sourceReticleLibraryFolder = emptyReticleFolder;
    }

    json windowDocument;
    windowDocument["title"] = "Page import staging";
    windowDocument["reticleLibraryFolder"] = NormalizePath(sourceReticleLibraryFolder).generic_string();
    windowDocument["pages"] = json::array({NormalizePath(plan.sourcePageFile).generic_string()});
    windowDocument["defaultPage"] = plan.sourcePageName;

    std::ofstream output(windowFile, std::ios::binary);
    if (!output.is_open())
    {
        throw std::runtime_error("Unable to create the temporary page-import wrapper file.");
    }
    output << windowDocument.dump(2);
    output.close();

    mfd::JsonLoader loader;
    return loader.LoadWindowConfiguration(windowFile);
}
} // namespace

PageImportPlan PageImportService::BuildPlan(const mfd::LoadedWindowConfiguration& loaded,
                                            const EditorFileLayout& files,
                                            const PageImportRequest& request) const
{
    PageImportPlan plan;
    plan.sourcePageFile = NormalizePath(request.sourcePageFile);
    plan.targetPageFolder = NormalizePath(request.targetPageFolder);
    plan.targetReticleLibraryFolder = NormalizePath(request.targetReticleLibraryFolder);

    try
    {
        if (plan.sourcePageFile.empty())
        {
            plan.error = "Choose one page JSON file to import.";
            return plan;
        }

        if (!HasJsonExtension(plan.sourcePageFile))
        {
            plan.error = "The selected import source must be one .json page file.";
            return plan;
        }

        if (!PathExists(plan.sourcePageFile))
        {
            plan.error = "The selected page JSON file does not exist.";
            return plan;
        }

        if (plan.targetPageFolder.empty())
        {
            plan.error = "The current window does not expose one valid target page folder.";
            return plan;
        }

        if (plan.targetReticleLibraryFolder.empty())
        {
            plan.error = "The current window does not expose one valid reticle-library folder.";
            return plan;
        }

        const PageSourceMetadata sourceMetadata = ParsePageSourceMetadata(plan.sourcePageFile);
        plan.sourcePageName = sourceMetadata.pageName;
        plan.targetPageName = sourceMetadata.pageName;
        plan.sourceUsesPageWrapper = sourceMetadata.usesPageWrapper;
        plan.sourceAssetsRoot = InferAssetsRootFromPageFile(plan.sourcePageFile);

        const bool includeExecAssets = PathContainsSegment(plan.sourceAssetsRoot, "_Exec") ||
                                       PathContainsSegment(plan.sourcePageFile, "_Exec");
        const AssetReferenceIndex index =
            referenceIndexService_.BuildIndex(AssetReferenceIndexRequest {plan.sourceAssetsRoot, includeExecAssets});

        const std::string sourcePageKey = PathKey(plan.sourcePageFile);
        for (const AssetScanError& scanError : index.errors)
        {
            if (PathKey(scanError.file) == sourcePageKey)
            {
                plan.error = "Unable to inspect the imported page JSON: " + scanError.message;
                return plan;
            }
        }

        std::vector<ReticleImportPlan> reticlePlans;
        reticlePlans.reserve(sourceMetadata.templateIds.size());

        std::unordered_set<std::string> reservedTemplateIds;
        reservedTemplateIds.reserve(loaded.document.reticleLibrary.size() + sourceMetadata.templateIds.size());
        for (const auto& entry : loaded.document.reticleLibrary)
        {
            reservedTemplateIds.insert(NormalizeIdentifier(entry.first));
        }

        std::unordered_set<std::string> sourceReticleFolders;

        const auto isTemplateIdTaken = [&reservedTemplateIds](const std::string_view templateId)
        {
            return reservedTemplateIds.find(NormalizeIdentifier(templateId)) != reservedTemplateIds.end();
        };

        const auto isTemplatePathTaken = [&files](const std::filesystem::path& candidate)
        {
            return PathExists(candidate) || PathInTemplateMap(files, candidate);
        };

        for (const std::string& sourceTemplateId : sourceMetadata.templateIds)
        {
            const auto missingIterator =
                std::find_if(index.missingAssets.begin(),
                             index.missingAssets.end(),
                             [&sourcePageKey, &sourceTemplateId](const MissingAssetReference& missingAsset)
                             {
                                 return PathKey(missingAsset.ownerFile) == sourcePageKey &&
                                        missingAsset.kind == MissingAssetKind::ReticleTemplate &&
                                        mfd::PageNamesEqual(missingAsset.templateId, sourceTemplateId);
                             });
            if (missingIterator != index.missingAssets.end())
            {
                plan.error = "Imported page is missing reticle template '" + sourceTemplateId + "'.";
                return plan;
            }

            const auto referenceIterator =
                std::find_if(index.reticles.begin(),
                             index.reticles.end(),
                             [&sourcePageKey, &sourceTemplateId](const ReticleReference& reference)
                             {
                                 return PathKey(reference.ownerFile) == sourcePageKey &&
                                        (reference.kind == ReticleReferenceKind::PageReticleTemplate ||
                                         reference.kind == ReticleReferenceKind::PageStrobeTemplate) &&
                                        mfd::PageNamesEqual(reference.templateId, sourceTemplateId);
                             });
            if (referenceIterator == index.reticles.end() || referenceIterator->targetFile.empty())
            {
                plan.error = "Unable to resolve reticle template '" + sourceTemplateId + "' for the imported page.";
                return plan;
            }

            const std::filesystem::path sourceTemplateFile = NormalizePath(referenceIterator->targetFile);
            if (!PathExists(sourceTemplateFile))
            {
                plan.error = "Imported page references a reticle template that does not exist on disk: '" +
                             sourceTemplateId + "'.";
                return plan;
            }

            sourceReticleFolders.insert(PathKey(sourceTemplateFile.parent_path()));

            ReticleImportPlan reticlePlan;
            reticlePlan.sourceTemplateId = sourceTemplateId;
            reticlePlan.targetTemplateId = sourceTemplateId;
            reticlePlan.sourceFile = sourceTemplateFile;
            reticlePlan.targetFile =
                ResolveCurrentTemplateFile(files, plan.targetReticleLibraryFolder, reticlePlan.targetTemplateId);
            reticlePlan.disposition = ImportDisposition::CopyNew;

            if (HasTemplateId(loaded, sourceTemplateId))
            {
                if (CurrentTemplateMatchesSource(
                        loaded, files, plan.targetReticleLibraryFolder, sourceTemplateId, sourceTemplateFile))
                {
                    reticlePlan.disposition = ImportDisposition::KeepExisting;
                }
                else
                {
                    reticlePlan.targetTemplateId = SuggestUniqueImportedId(sourceTemplateId, isTemplateIdTaken);
                    reticlePlan.targetFile =
                        NormalizePath(DefaultTemplateFilePath(plan.targetReticleLibraryFolder, reticlePlan.targetTemplateId));
                    reticlePlan.disposition = ImportDisposition::RenameCopy;
                    reticlePlan.targetFile = SuggestUniqueJsonPath(
                        reticlePlan.targetFile,
                        [&isTemplatePathTaken](const std::filesystem::path& candidate)
                        {
                            return isTemplatePathTaken(candidate);
                        });
                }
            }
            else if (PathExists(reticlePlan.targetFile) &&
                     DiskJsonMatchesSource(reticlePlan.targetFile, sourceTemplateFile))
            {
                reticlePlan.disposition = ImportDisposition::KeepExisting;
            }
            else if (PathExists(reticlePlan.targetFile) || PathInTemplateMap(files, reticlePlan.targetFile))
            {
                reticlePlan.targetTemplateId = SuggestUniqueImportedId(sourceTemplateId, isTemplateIdTaken);
                reticlePlan.targetFile =
                    NormalizePath(DefaultTemplateFilePath(plan.targetReticleLibraryFolder, reticlePlan.targetTemplateId));
                reticlePlan.disposition = ImportDisposition::RenameCopy;
                reticlePlan.targetFile = SuggestUniqueJsonPath(
                    reticlePlan.targetFile,
                    [&isTemplatePathTaken](const std::filesystem::path& candidate)
                    {
                        return isTemplatePathTaken(candidate);
                    });
            }

            reservedTemplateIds.insert(NormalizeIdentifier(reticlePlan.targetTemplateId));
            reticlePlans.push_back(std::move(reticlePlan));
        }

        if (sourceReticleFolders.size() > 1U)
        {
            plan.error = "Imported page references reticle templates coming from multiple source folders.";
            return plan;
        }

        if (!sourceReticleFolders.empty())
        {
            plan.sourceReticleLibraryFolder = reticlePlans.front().sourceFile.parent_path();
        }

        if (HasPageName(loaded, plan.targetPageName))
        {
            plan.targetPageName = SuggestUniqueImportedId(
                plan.sourcePageName,
                [&loaded](const std::string_view candidate)
                {
                    return HasPageName(loaded, candidate);
                });
        }

        const bool pageRequiresRewrite =
            plan.sourceUsesPageWrapper || !mfd::PageNamesEqual(plan.sourcePageName, plan.targetPageName) ||
            std::any_of(reticlePlans.begin(),
                        reticlePlans.end(),
                        [](const ReticleImportPlan& reticlePlan)
                        {
                            return !mfd::PageNamesEqual(reticlePlan.sourceTemplateId, reticlePlan.targetTemplateId);
                        });

        plan.targetPageFile =
            NormalizePath(plan.targetPageFolder / std::filesystem::path(mfd::NormalizePageName(plan.targetPageName) + ".json"));

        const auto isPagePathTaken = [&files](const std::filesystem::path& candidate)
        {
            return PathInList(files.pageFiles, candidate) || PathExists(candidate);
        };

        if (pageRequiresRewrite)
        {
            const std::filesystem::path preferredPageFile = plan.targetPageFile;
            plan.targetPageFile = SuggestUniqueJsonPath(preferredPageFile, isPagePathTaken);
            plan.pageDisposition =
                PathKey(plan.targetPageFile) == PathKey(preferredPageFile) &&
                        mfd::PageNamesEqual(plan.targetPageName, plan.sourcePageName)
                    ? ImportDisposition::CopyNew
                    : ImportDisposition::RenameCopy;
        }
        else if (PathInList(files.pageFiles, plan.targetPageFile))
        {
            plan.targetPageFile = SuggestUniqueJsonPath(plan.targetPageFile, isPagePathTaken);
            plan.pageDisposition = ImportDisposition::RenameCopy;
        }
        else if (PathExists(plan.targetPageFile) &&
                 DiskJsonMatchesSource(plan.targetPageFile, plan.sourcePageFile))
        {
            plan.pageDisposition = ImportDisposition::KeepExisting;
        }
        else if (PathExists(plan.targetPageFile))
        {
            plan.targetPageFile = SuggestUniqueJsonPath(plan.targetPageFile, isPagePathTaken);
            plan.pageDisposition = ImportDisposition::RenameCopy;
        }
        else
        {
            plan.pageDisposition = ImportDisposition::CopyNew;
        }

        plan.reticles = std::move(reticlePlans);
        plan.canExecute = true;
        return plan;
    }
    catch (const nlohmann::json::parse_error& exception)
    {
        plan.error = "Unable to parse the imported page JSON: " + std::string(exception.what());
        return plan;
    }
    catch (const nlohmann::json::exception& exception)
    {
        plan.error = "Invalid imported page JSON: " + std::string(exception.what());
        return plan;
    }
    catch (const std::exception& exception)
    {
        plan.error = exception.what();
        return plan;
    }
}

bool PageImportService::Execute(const PageImportPlan& plan,
                                mfd::LoadedWindowConfiguration& loaded,
                                EditorFileLayout& files,
                                PageImportResult* result,
                                std::string* error) const
{
    if (!plan.canExecute)
    {
        if (error != nullptr)
        {
            *error = plan.error.empty() ? "The page import plan cannot execute." : plan.error;
        }
        return false;
    }

    try
    {
        mfd::LoadedWindowConfiguration importedConfiguration = LoadSourceConfigurationForImport(plan);
        if (importedConfiguration.document.pages.size() != 1U)
        {
            throw std::runtime_error("The temporary import wrapper did not resolve exactly one page.");
        }

        mfd::PageDefinition importedPage = importedConfiguration.document.pages.front();
        std::unordered_map<std::string, std::string, mfd::TransparentStringHash, mfd::TransparentStringEqual> templateIdRemap;
        templateIdRemap.reserve(plan.reticles.size());

        for (const ReticleImportPlan& reticlePlan : plan.reticles)
        {
            templateIdRemap[reticlePlan.sourceTemplateId] = reticlePlan.targetTemplateId;
        }

        ApplyTemplateIdRemap(importedPage, templateIdRemap);
        importedPage.name = plan.targetPageName;
        importedPage.normalizedName = mfd::NormalizePageName(plan.targetPageName);
        importedPage.defaultPage = !HasAnyDefaultPage(loaded.document.pages);

        std::vector<std::string> importedTemplateIds;
        std::vector<std::filesystem::path> stagedFiles;

        for (const ReticleImportPlan& reticlePlan : plan.reticles)
        {
            if (reticlePlan.disposition == ImportDisposition::KeepExisting)
            {
                RemovePathIfPresent(files.removedTemplateFiles, reticlePlan.targetFile);
                continue;
            }

            const auto sourceTemplateIterator =
                importedConfiguration.document.reticleLibrary.find(reticlePlan.sourceTemplateId);
            if (sourceTemplateIterator == importedConfiguration.document.reticleLibrary.end())
            {
                throw std::runtime_error("Imported source reticle '" + reticlePlan.sourceTemplateId +
                                         "' is no longer available.");
            }

            mfd::ReticleGroup importedTemplate = sourceTemplateIterator->second;
            RenameImportedTemplate(importedTemplate, reticlePlan.targetTemplateId);

            loaded.document.reticleLibrary[reticlePlan.targetTemplateId] = std::move(importedTemplate);
            files.templateFiles[reticlePlan.targetTemplateId] = NormalizePath(reticlePlan.targetFile);
            RemovePathIfPresent(files.removedTemplateFiles, reticlePlan.targetFile);

            importedTemplateIds.push_back(reticlePlan.targetTemplateId);
            stagedFiles.push_back(NormalizePath(reticlePlan.targetFile));
        }

        files.pageFiles.push_back(NormalizePath(plan.targetPageFile));
        RemovePathIfPresent(files.removedPageFiles, plan.targetPageFile);
        loaded.document.pages.push_back(std::move(importedPage));
        loaded.window.pageFiles = files.pageFiles;

        const int importedPageIndex = static_cast<int>(loaded.document.pages.size()) - 1;
        stagedFiles.push_back(NormalizePath(plan.targetPageFile));

        if (result != nullptr)
        {
            result->importedPageIndex = importedPageIndex;
            result->pageName = loaded.document.pages.back().name;
            result->pageFile = NormalizePath(plan.targetPageFile);
            result->importedTemplateIds = std::move(importedTemplateIds);
            result->stagedFiles = std::move(stagedFiles);
        }

        return true;
    }
    catch (const std::exception& exception)
    {
        if (error != nullptr)
        {
            *error = exception.what();
        }
        return false;
    }
}
} // namespace editor

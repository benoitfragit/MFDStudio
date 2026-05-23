/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorReticleRenameService.h"

/**
 * @file
 * @brief Implementation of the editor global reticle-template rename planning service.
 */

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "mfd/model/PageName.h"

namespace editor
{
namespace
{
using json = nlohmann::json;

struct PendingWrite
{
    std::filesystem::path file {};
    std::string content {};
    bool pageFile = false;
};

struct PageUsageAccumulator
{
    std::filesystem::path pageFile {};
    std::string pageName {};
    std::vector<RenameReticleReference> oldReferences {};
    std::vector<RenameReticleCollision> newReferences {};
};

std::string TrimAsciiWhitespace(const std::string_view value)
{
    std::size_t first = 0U;
    while (first < value.size() &&
           std::isspace(static_cast<unsigned char>(value[first])) != 0)
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(value[last - 1U])) != 0)
    {
        --last;
    }

    return std::string(value.substr(first, last - first));
}

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

std::string NormalizeIdentifier(const std::string_view value)
{
    return mfd::NormalizePageName(value);
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

bool PathsEqual(const std::filesystem::path& lhs, const std::filesystem::path& rhs)
{
    return PathKey(NormalizePath(lhs)) == PathKey(NormalizePath(rhs));
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

bool IsPathInsideRoot(const std::filesystem::path& path, const std::filesystem::path& root)
{
    if (path.empty() || root.empty())
    {
        return false;
    }

    const std::filesystem::path normalizedPath = NormalizePath(path);
    const std::filesystem::path normalizedRoot = NormalizePath(root);
    auto pathIt = normalizedPath.begin();
    const auto pathEnd = normalizedPath.end();

    for (auto rootIt = normalizedRoot.begin(); rootIt != normalizedRoot.end(); ++rootIt, ++pathIt)
    {
        if (pathIt == pathEnd)
        {
            return false;
        }

        if (Lowercase(pathIt->string()) != Lowercase(rootIt->string()))
        {
            return false;
        }
    }

    return true;
}

bool HasPathComponent(const std::filesystem::path& path, const std::string_view component)
{
    const std::string loweredComponent = Lowercase(component);
    for (const auto& part : path)
    {
        if (Lowercase(part.string()) == loweredComponent)
        {
            return true;
        }
    }

    return false;
}

bool HasJsonExtension(const std::filesystem::path& path)
{
    return Lowercase(path.extension().string()) == ".json";
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

void WriteTextFile(const std::filesystem::path& path, const std::string_view content)
{
    std::filesystem::create_directories(path.parent_path());

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open())
    {
        throw std::runtime_error("Unable to open file for writing: " + path.string());
    }

    output << content;
    if (!output.good())
    {
        throw std::runtime_error("Unable to write file: " + path.string());
    }
}

void DeleteFile(const std::filesystem::path& path)
{
    if (path.empty())
    {
        return;
    }

    std::error_code error;
    if (!std::filesystem::exists(path, error))
    {
        return;
    }

    if (!std::filesystem::remove(path, error) && error)
    {
        throw std::runtime_error("Unable to delete file: " + path.string() + " (" + error.message() + ")");
    }
}

bool IsTemplateReferenceKind(const ReticleReferenceKind kind)
{
    return kind == ReticleReferenceKind::PageReticleTemplate ||
           kind == ReticleReferenceKind::PageStrobeTemplate ||
           kind == ReticleReferenceKind::PageDynamicTemplate;
}

void AppendUniquePath(std::vector<std::filesystem::path>& paths, const std::filesystem::path& candidate)
{
    const std::filesystem::path normalizedCandidate = NormalizePath(candidate);
    if (normalizedCandidate.empty())
    {
        return;
    }

    const auto iterator = std::find_if(paths.begin(),
                                       paths.end(),
                                       [&normalizedCandidate](const std::filesystem::path& existing)
                                       {
                                           return PathsEqual(existing, normalizedCandidate);
                                       });
    if (iterator == paths.end())
    {
        paths.push_back(normalizedCandidate);
    }
}

void RemovePath(std::vector<std::filesystem::path>& paths, const std::filesystem::path& candidate)
{
    const std::filesystem::path normalizedCandidate = NormalizePath(candidate);
    paths.erase(std::remove_if(paths.begin(),
                               paths.end(),
                               [&normalizedCandidate](const std::filesystem::path& existing)
                               {
                                   return PathsEqual(existing, normalizedCandidate);
                               }),
                paths.end());
}

void SortPaths(std::vector<std::filesystem::path>& paths)
{
    std::sort(paths.begin(),
              paths.end(),
              [](const std::filesystem::path& lhs, const std::filesystem::path& rhs)
              {
                  return PathKey(lhs) < PathKey(rhs);
              });
}

void SortReferences(std::vector<RenameReticleReference>& references)
{
    std::sort(references.begin(),
              references.end(),
              [](const RenameReticleReference& lhs, const RenameReticleReference& rhs)
              {
                  return std::make_tuple(PathKey(lhs.pageFile),
                                         lhs.pageName,
                                         static_cast<int>(lhs.kind),
                                         lhs.ownerReticleId) <
                         std::make_tuple(PathKey(rhs.pageFile),
                                         rhs.pageName,
                                         static_cast<int>(rhs.kind),
                                         rhs.ownerReticleId);
              });
}

void SortCollisions(std::vector<RenameReticleCollision>& collisions)
{
    std::sort(collisions.begin(),
              collisions.end(),
              [](const RenameReticleCollision& lhs, const RenameReticleCollision& rhs)
              {
                  return std::make_tuple(PathKey(lhs.pageFile),
                                         lhs.pageName,
                                         static_cast<int>(lhs.kind),
                                         lhs.conflictingReticleId,
                                         lhs.conflictingTemplateId) <
                         std::make_tuple(PathKey(rhs.pageFile),
                                         rhs.pageName,
                                         static_cast<int>(rhs.kind),
                                         rhs.conflictingReticleId,
                                         rhs.conflictingTemplateId);
              });
}

const mfd::ReticleGroup* FindTemplateById(const mfd::LoadedWindowConfiguration& loaded,
                                          const std::string_view templateId,
                                          std::string* matchedKey)
{
    const std::string normalizedTemplateId = NormalizeIdentifier(templateId);
    for (const auto& entry : loaded.document.reticleLibrary)
    {
        if (NormalizeIdentifier(entry.first) != normalizedTemplateId)
        {
            continue;
        }

        if (matchedKey != nullptr)
        {
            *matchedKey = entry.first;
        }
        return &entry.second;
    }

    return nullptr;
}

std::optional<std::filesystem::path> FindTrackedTemplateFile(const EditorFileLayout& files,
                                                             const std::string_view templateId,
                                                             std::string* matchedKey)
{
    const std::string normalizedTemplateId = NormalizeIdentifier(templateId);
    for (const auto& entry : files.templateFiles)
    {
        if (NormalizeIdentifier(entry.first) != normalizedTemplateId)
        {
            continue;
        }

        if (matchedKey != nullptr)
        {
            *matchedKey = entry.first;
        }
        return NormalizePath(entry.second);
    }

    return std::nullopt;
}

std::string PageNameOrFallback(const std::string_view pageName, const std::filesystem::path& pageFile)
{
    if (!pageName.empty())
    {
        return std::string(pageName);
    }

    return pageFile.stem().string();
}

std::string OwnerReticleIdOrFallback(const std::string_view ownerReticleId, const std::string_view templateId)
{
    if (!ownerReticleId.empty())
    {
        return std::string(ownerReticleId);
    }

    return std::string(templateId);
}

void AddReferenceAccumulator(PageUsageAccumulator& usage,
                             const ReticleReferenceKind kind,
                             const std::filesystem::path& pageFile,
                             const std::string_view pageName,
                             const std::string_view ownerReticleId,
                             const std::string_view templateId)
{
    usage.pageFile = NormalizePath(pageFile);
    usage.pageName = PageNameOrFallback(pageName, usage.pageFile);
    usage.oldReferences.push_back(RenameReticleReference {
        kind,
        usage.pageFile,
        usage.pageName,
        OwnerReticleIdOrFallback(ownerReticleId, templateId)});
}

void AddCollisionAccumulator(PageUsageAccumulator& usage,
                             const ReticleReferenceKind kind,
                             const std::filesystem::path& pageFile,
                             const std::string_view pageName,
                             const std::string_view ownerReticleId,
                             const std::string_view templateId)
{
    usage.pageFile = NormalizePath(pageFile);
    usage.pageName = PageNameOrFallback(pageName, usage.pageFile);
    usage.newReferences.push_back(RenameReticleCollision {
        usage.pageFile,
        usage.pageName,
        kind,
        OwnerReticleIdOrFallback(ownerReticleId, templateId),
        std::string(templateId)});
}

std::string PageUsageKey(const std::filesystem::path& pageFile)
{
    return PathKey(NormalizePath(pageFile));
}

void CollectCurrentPageUsage(const mfd::PageDefinition& page,
                             const std::filesystem::path& pageFile,
                             const std::string_view oldTemplateId,
                             const std::string_view newTemplateId,
                             PageUsageAccumulator& usage)
{
    const std::string normalizedOldTemplateId = NormalizeIdentifier(oldTemplateId);
    const std::string normalizedNewTemplateId = NormalizeIdentifier(newTemplateId);

    for (const mfd::ReticleGroup& reticle : page.staticReticles)
    {
        if (reticle.sourceTemplateId.empty())
        {
            continue;
        }

        const std::string normalizedSourceTemplateId = NormalizeIdentifier(reticle.sourceTemplateId);
        if (normalizedSourceTemplateId == normalizedOldTemplateId)
        {
            AddReferenceAccumulator(
                usage,
                ReticleReferenceKind::PageReticleTemplate,
                pageFile,
                page.name,
                reticle.id,
                reticle.sourceTemplateId);
        }
        else if (normalizedSourceTemplateId == normalizedNewTemplateId)
        {
            AddCollisionAccumulator(
                usage,
                ReticleReferenceKind::PageReticleTemplate,
                pageFile,
                page.name,
                reticle.id,
                reticle.sourceTemplateId);
        }
    }

    for (const auto& strobe : page.strobes)
    {
        if (strobe.reticle.sourceTemplateId.empty())
        {
            continue;
        }

        const std::string normalizedStrobeTemplateId = NormalizeIdentifier(strobe.reticle.sourceTemplateId);
        if (normalizedStrobeTemplateId == normalizedOldTemplateId)
        {
            AddReferenceAccumulator(usage,
                                    ReticleReferenceKind::PageStrobeTemplate,
                                    pageFile,
                                    page.name,
                                    strobe.reticle.id,
                                    strobe.reticle.sourceTemplateId);
        }
        else if (normalizedStrobeTemplateId == normalizedNewTemplateId)
        {
            AddCollisionAccumulator(usage,
                                    ReticleReferenceKind::PageStrobeTemplate,
                                    pageFile,
                                    page.name,
                                    strobe.reticle.id,
                                    strobe.reticle.sourceTemplateId);
        }
    }

    for (const auto& binding : page.dynamicReticleBindings)
    {
        const std::string normalizedDynamicTemplateId = NormalizeIdentifier(binding.templateId);
        if (normalizedDynamicTemplateId == normalizedOldTemplateId)
        {
            AddReferenceAccumulator(usage,
                                    ReticleReferenceKind::PageDynamicTemplate,
                                    pageFile,
                                    page.name,
                                    {},
                                    binding.templateId);
        }
        else if (normalizedDynamicTemplateId == normalizedNewTemplateId)
        {
            AddCollisionAccumulator(usage,
                                    ReticleReferenceKind::PageDynamicTemplate,
                                    pageFile,
                                    page.name,
                                    {},
                                    binding.templateId);
        }
    }
}

std::size_t RenamePageTemplateReferences(mfd::PageDefinition& page,
                                         const std::string_view oldTemplateId,
                                         const std::string_view newTemplateId)
{
    const std::string normalizedOldTemplateId = NormalizeIdentifier(oldTemplateId);
    std::size_t updatedReferenceCount = 0U;

    for (mfd::ReticleGroup& reticle : page.staticReticles)
    {
        if (NormalizeIdentifier(reticle.sourceTemplateId) != normalizedOldTemplateId)
        {
            continue;
        }

        reticle.sourceTemplateId = std::string(newTemplateId);
        ++updatedReferenceCount;
    }

    for (auto& strobe : page.strobes)
    {
        if (NormalizeIdentifier(strobe.reticle.sourceTemplateId) != normalizedOldTemplateId)
        {
            continue;
        }

        strobe.reticle.sourceTemplateId = std::string(newTemplateId);
        ++updatedReferenceCount;
    }

    for (auto& binding : page.dynamicReticleBindings)
    {
        if (NormalizeIdentifier(binding.templateId) != normalizedOldTemplateId)
        {
            continue;
        }

        binding.templateId = std::string(newTemplateId);
        ++updatedReferenceCount;
    }

    return updatedReferenceCount;
}

json& ExtractMutablePageNode(json& document)
{
    if (document.is_object() &&
        document.contains("page") &&
        document.at("page").is_object())
    {
        return document.at("page");
    }

    return document;
}

void RewriteTemplateReferenceField(json& node,
                                   const std::string_view normalizedOldTemplateId,
                                   const std::string_view newTemplateId,
                                   std::size_t& updatedReferenceCount)
{
    if (!node.is_object() || !node.contains("template"))
    {
        return;
    }

    if (!node.at("template").is_string())
    {
        throw std::runtime_error("Page reticle template fields must be strings.");
    }

    if (NormalizeIdentifier(node.at("template").get<std::string>()) != normalizedOldTemplateId)
    {
        return;
    }

    node["template"] = std::string(newTemplateId);
    ++updatedReferenceCount;
}

std::size_t RewritePageTemplateReferences(json& document,
                                          const std::string_view oldTemplateId,
                                          const std::string_view newTemplateId)
{
    json& pageNode = ExtractMutablePageNode(document);
    if (!pageNode.is_object())
    {
        throw std::runtime_error("Page JSON must contain one object root or one {\"page\": {...}} wrapper.");
    }

    const std::string normalizedOldTemplateId = NormalizeIdentifier(oldTemplateId);
    std::size_t updatedReferenceCount = 0U;

    if (pageNode.contains("staticReticles"))
    {
        if (!pageNode.at("staticReticles").is_array())
        {
            throw std::runtime_error("Page staticReticles field must be a JSON array.");
        }

        for (auto& reticleNode : pageNode.at("staticReticles"))
        {
            RewriteTemplateReferenceField(reticleNode, normalizedOldTemplateId, newTemplateId, updatedReferenceCount);
        }
    }

    if (pageNode.contains("strobe"))
    {
        if (!pageNode.at("strobe").is_object())
        {
            throw std::runtime_error("Page strobe field must be a JSON object.");
        }

        RewriteTemplateReferenceField(pageNode.at("strobe"), normalizedOldTemplateId, newTemplateId, updatedReferenceCount);
    }

    if (pageNode.contains("_editor"))
    {
        if (!pageNode.at("_editor").is_object())
        {
            throw std::runtime_error("Page _editor field must be a JSON object.");
        }
    }

    if (pageNode.contains("dynamicReticleBindings"))
    {
        if (!pageNode.at("dynamicReticleBindings").is_array())
        {
            throw std::runtime_error("Page dynamicReticleBindings field must be a JSON array.");
        }

        for (auto& bindingNode : pageNode.at("dynamicReticleBindings"))
        {
            if (!bindingNode.is_object())
            {
                throw std::runtime_error("Page dynamicReticleBindings entries must be JSON objects.");
            }

            if (!bindingNode.contains("templateId") || !bindingNode.at("templateId").is_string())
            {
                throw std::runtime_error("Page dynamicReticleBindings.templateId fields must be strings.");
            }

            if (NormalizeIdentifier(bindingNode.at("templateId").get<std::string>()) == normalizedOldTemplateId)
            {
                bindingNode["templateId"] = std::string(newTemplateId);
                ++updatedReferenceCount;
            }
        }
    }

    return updatedReferenceCount;
}

std::filesystem::path PreferredRenamedTemplateFile(const mfd::LoadedWindowConfiguration& loaded,
                                                   const std::filesystem::path& currentTemplateFile,
                                                   const std::string_view newTemplateId)
{
    const std::filesystem::path libraryFolder =
        loaded.window.reticleLibraryFolder.empty() ? currentTemplateFile.parent_path() : loaded.window.reticleLibraryFolder;
    return NormalizePath(DefaultTemplateFilePath(libraryFolder, newTemplateId));
}

std::optional<std::filesystem::path> FindTemplateIdCollision(const std::filesystem::path& assetsRoot,
                                                             const std::string_view newTemplateId,
                                                             const std::filesystem::path& ignoredFile)
{
    const std::string normalizedNewTemplateId = NormalizeIdentifier(newTemplateId);

    std::error_code iterationError;
    std::filesystem::recursive_directory_iterator iterator(
        assetsRoot,
        std::filesystem::directory_options::skip_permission_denied,
        iterationError);
    const std::filesystem::recursive_directory_iterator end;
    if (iterationError)
    {
        throw std::runtime_error("Unable to enumerate asset root: " + iterationError.message());
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
                throw std::runtime_error("Directory scan failed: " + iterationError.message());
            }
            continue;
        }

        if (statusError)
        {
            throw std::runtime_error("Unable to query filesystem entry: " + entryPath.string() + " (" + statusError.message() + ")");
        }

        if (!entry.is_regular_file(statusError) || statusError || !HasJsonExtension(entryPath) ||
            PathsEqual(entryPath, ignoredFile))
        {
            iterator.increment(iterationError);
            if (iterationError)
            {
                throw std::runtime_error("Directory scan failed: " + iterationError.message());
            }
            continue;
        }

        try
        {
            const json document = LoadJsonFile(entryPath);
            if (document.is_object() &&
                document.contains("elements") &&
                document.at("elements").is_array() &&
                document.contains("id") &&
                document.at("id").is_string() &&
                NormalizeIdentifier(document.at("id").get<std::string>()) == normalizedNewTemplateId)
            {
                return entryPath;
            }
        }
        catch (const std::exception&)
        {
        }

        iterator.increment(iterationError);
        if (iterationError)
        {
            throw std::runtime_error("Directory scan failed: " + iterationError.message());
        }
    }

    return std::nullopt;
}

} // namespace

RenameReticlePlan ReticleRenameService::BuildPlan(const mfd::LoadedWindowConfiguration& loaded,
                                                  const EditorFileLayout& files,
                                                  const RenameReticleRequest& request) const
{
    RenameReticlePlan plan;
    plan.assetsRoot = NormalizePath(request.assetsRoot);
    plan.renameTemplateFile = request.renameTemplateFile;

    try
    {
        const std::string requestedOldTemplateId = TrimAsciiWhitespace(request.oldTemplateId);
        const std::string requestedNewTemplateId = TrimAsciiWhitespace(request.newTemplateId);
        if (NormalizeIdentifier(requestedOldTemplateId).empty())
        {
            throw std::runtime_error("No reticle template is selected.");
        }
        if (NormalizeIdentifier(requestedNewTemplateId).empty())
        {
            throw std::runtime_error("The new reticle template id cannot be empty.");
        }
        if (NormalizeIdentifier(requestedOldTemplateId) == NormalizeIdentifier(requestedNewTemplateId))
        {
            throw std::runtime_error("Choose a different reticle template id before applying the global rename.");
        }

        std::string currentTemplateKey;
        const mfd::ReticleGroup* currentTemplate = FindTemplateById(loaded, requestedOldTemplateId, &currentTemplateKey);
        if (currentTemplate == nullptr)
        {
            throw std::runtime_error("The selected reticle template does not exist in the current editor document.");
        }

        plan.oldReticleName = currentTemplate->id.empty() ? currentTemplateKey : currentTemplate->id;
        plan.newReticleName = requestedNewTemplateId;

        const std::optional<std::filesystem::path> trackedTemplateFile =
            FindTrackedTemplateFile(files, currentTemplateKey, nullptr);
        if (!trackedTemplateFile.has_value())
        {
            throw std::runtime_error("The selected reticle template does not expose a tracked source JSON file.");
        }

        plan.currentTemplateFile = NormalizePath(*trackedTemplateFile);
        plan.targetTemplateFile = request.renameTemplateFile
                                      ? PreferredRenamedTemplateFile(loaded, plan.currentTemplateFile, plan.newReticleName)
                                      : plan.currentTemplateFile;

        if (plan.assetsRoot.empty() || !PathExists(plan.assetsRoot))
        {
            throw std::runtime_error("The scanned assets root does not exist.");
        }
        if (plan.currentTemplateFile.empty() || !PathExists(plan.currentTemplateFile))
        {
            throw std::runtime_error("The selected reticle template JSON file could not be found on disk.");
        }
        if (!IsPathInsideRoot(plan.currentTemplateFile, plan.assetsRoot))
        {
            throw std::runtime_error("Refusing to rename one reticle template JSON outside the protected scanned assets root.");
        }
        if (!IsPathInsideRoot(plan.targetTemplateFile, plan.assetsRoot))
        {
            throw std::runtime_error("Refusing to move the reticle template JSON outside the protected scanned assets root.");
        }
        if (request.renameTemplateFile &&
            !PathsEqual(plan.currentTemplateFile, plan.targetTemplateFile) &&
            PathExists(plan.targetTemplateFile))
        {
            throw std::runtime_error(
                "Refusing to rename the reticle template file because the target path already exists: " +
                plan.targetTemplateFile.string());
        }

        const AssetReferenceIndex index = referenceIndexService_.BuildIndex(AssetReferenceIndexRequest {plan.assetsRoot});

        std::unordered_set<std::string> currentPageFileKeys;
        currentPageFileKeys.reserve(files.pageFiles.size());
        for (const std::filesystem::path& pageFile : files.pageFiles)
        {
            if (!pageFile.empty())
            {
                currentPageFileKeys.insert(PageUsageKey(pageFile));
            }
        }

        for (const AssetScanError& errorEntry : index.errors)
        {
            const std::string errorFileKey = PageUsageKey(errorEntry.file);
            if (currentPageFileKeys.find(errorFileKey) != currentPageFileKeys.end() ||
                PathsEqual(errorEntry.file, plan.currentTemplateFile))
            {
                continue;
            }

            if (HasPathComponent(errorEntry.file, "windows"))
            {
                continue;
            }

            throw std::runtime_error(
                "Safe global reticle rename requires valid page and reticle JSON files under the scanned assets root. "
                "Scan error: " +
                errorEntry.file.string() + " (" + errorEntry.message + ")");
        }

        if (const std::optional<std::filesystem::path> existingTemplateFile =
                FindTemplateIdCollision(plan.assetsRoot, plan.newReticleName, plan.currentTemplateFile);
            existingTemplateFile.has_value())
        {
            throw std::runtime_error(
                "Refusing the rename because another reticle template already uses id '" + plan.newReticleName +
                "': " + existingTemplateFile->string());
        }

        std::unordered_map<std::string, PageUsageAccumulator> usageByPage;
        usageByPage.reserve(index.reticles.size() + loaded.document.pages.size());

        for (int pageIndex = 0; pageIndex < static_cast<int>(loaded.document.pages.size()); ++pageIndex)
        {
            const mfd::PageDefinition& page = loaded.document.pages[static_cast<std::size_t>(pageIndex)];
            const std::filesystem::path pageFile =
                pageIndex < static_cast<int>(files.pageFiles.size()) ? NormalizePath(files.pageFiles[static_cast<std::size_t>(pageIndex)])
                                                                     : std::filesystem::path {};
            PageUsageAccumulator usage;
            CollectCurrentPageUsage(page, pageFile, plan.oldReticleName, plan.newReticleName, usage);
            if (usage.oldReferences.empty() && usage.newReferences.empty())
            {
                continue;
            }

            if (pageFile.empty())
            {
                throw std::runtime_error(
                    "Save the current window before renaming this reticle globally because one current page does not expose a tracked source JSON file.");
            }
            if (!IsPathInsideRoot(pageFile, plan.assetsRoot))
            {
                throw std::runtime_error(
                    "Refusing to rename the reticle globally because current page '" + page.name +
                    "' sits outside the protected scanned assets root.");
            }
            if (!PathExists(pageFile))
            {
                throw std::runtime_error(
                    "Save the current window before renaming this reticle globally because page JSON '" +
                    pageFile.string() + "' does not exist on disk yet.");
            }

            usage.pageFile = pageFile;
            usage.pageName = PageNameOrFallback(page.name, pageFile);
            usageByPage[PageUsageKey(pageFile)] = std::move(usage);
        }

        const std::string normalizedOldTemplateId = NormalizeIdentifier(plan.oldReticleName);
        const std::string normalizedNewTemplateId = NormalizeIdentifier(plan.newReticleName);

        for (const ReticleReference& reference : index.reticles)
        {
            if (!IsTemplateReferenceKind(reference.kind))
            {
                continue;
            }

            const std::filesystem::path pageFile = NormalizePath(reference.ownerFile);
            const std::string pageKey = PageUsageKey(pageFile);
            if (currentPageFileKeys.find(pageKey) != currentPageFileKeys.end())
            {
                continue;
            }

            const std::string normalizedTemplateId = NormalizeIdentifier(reference.templateId);
            if (normalizedTemplateId != normalizedOldTemplateId &&
                normalizedTemplateId != normalizedNewTemplateId)
            {
                continue;
            }

            auto& usage = usageByPage[pageKey];
            if (normalizedTemplateId == normalizedOldTemplateId)
            {
                AddReferenceAccumulator(
                    usage,
                    reference.kind,
                    pageFile,
                    reference.pageName,
                    reference.reticleId,
                    reference.templateId);
            }
            else
            {
                AddCollisionAccumulator(
                    usage,
                    reference.kind,
                    pageFile,
                    reference.pageName,
                    reference.reticleId,
                    reference.templateId);
            }
        }

        AppendUniquePath(plan.filesToModify, plan.targetTemplateFile);

        for (const auto& entry : usageByPage)
        {
            const PageUsageAccumulator& usage = entry.second;
            if (usage.oldReferences.empty())
            {
                continue;
            }

            plan.references.insert(plan.references.end(), usage.oldReferences.begin(), usage.oldReferences.end());
            plan.collisions.insert(plan.collisions.end(), usage.newReferences.begin(), usage.newReferences.end());
            AppendUniquePath(plan.filesToModify, usage.pageFile);
        }

        if (request.renameTemplateFile && !PathsEqual(plan.currentTemplateFile, plan.targetTemplateFile))
        {
            AppendUniquePath(plan.filesToDelete, plan.currentTemplateFile);
        }

        plan.warnings.push_back(
            "This rename may change generated template transport ids, mappingHash values, and generated client API names.");
        if (request.renameTemplateFile)
        {
            plan.warnings.push_back(
                "When the template JSON file moves, relative image paths are rewritten from the new file location.");
        }

        SortPaths(plan.filesToModify);
        SortPaths(plan.filesToDelete);
        SortReferences(plan.references);
        SortCollisions(plan.collisions);

        if (!plan.collisions.empty())
        {
            throw std::runtime_error(
                "Refusing the rename because at least one page already references target template id '" +
                plan.newReticleName + "'.");
        }

        plan.canExecute = true;
    }
    catch (const std::exception& exception)
    {
        plan.canExecute = false;
        plan.error = exception.what();
    }

    return plan;
}

bool ReticleRenameService::Execute(const RenameReticlePlan& plan,
                                   mfd::LoadedWindowConfiguration& loaded,
                                   EditorFileLayout& files,
                                   RenameReticleResult* result,
                                   std::string* error) const
{
    try
    {
        if (!plan.canExecute)
        {
            throw std::runtime_error(plan.error.empty() ? "The reticle rename plan cannot execute." : plan.error);
        }

        std::string currentTemplateKey;
        const mfd::ReticleGroup* currentTemplate = FindTemplateById(loaded, plan.oldReticleName, &currentTemplateKey);
        if (currentTemplate == nullptr)
        {
            throw std::runtime_error("The targeted reticle template no longer exists in the current editor document.");
        }

        std::string trackedTemplateKey;
        const std::optional<std::filesystem::path> trackedTemplateFile =
            FindTrackedTemplateFile(files, plan.oldReticleName, &trackedTemplateKey);
        if (!trackedTemplateFile.has_value() || !PathsEqual(*trackedTemplateFile, plan.currentTemplateFile))
        {
            throw std::runtime_error("The targeted reticle template JSON file no longer matches the tracked editor layout.");
        }

        if (!PathExists(plan.currentTemplateFile))
        {
            throw std::runtime_error("The targeted reticle template JSON file no longer exists on disk.");
        }

        mfd::ReticleGroup renamedTemplate = *currentTemplate;
        renamedTemplate.id = plan.newReticleName;
        renamedTemplate.sourceTemplateId.clear();

        mfd::ReticleLibrary serializedLibrary = loaded.document.reticleLibrary;
        serializedLibrary.erase(currentTemplateKey);
        serializedLibrary.erase(plan.newReticleName);
        serializedLibrary.emplace(plan.newReticleName, renamedTemplate);

        std::unordered_map<std::string, int> currentPageIndexByFile;
        currentPageIndexByFile.reserve(files.pageFiles.size());
        for (int pageIndex = 0; pageIndex < static_cast<int>(files.pageFiles.size()); ++pageIndex)
        {
            currentPageIndexByFile.emplace(PageUsageKey(files.pageFiles[static_cast<std::size_t>(pageIndex)]), pageIndex);
        }

        std::vector<PendingWrite> writes;
        writes.reserve(plan.filesToModify.size());
        std::unordered_set<std::string> updatedPageKeys;
        updatedPageKeys.reserve(plan.references.size());

        writes.push_back(PendingWrite {
            plan.targetTemplateFile,
            SerializeReticleTemplateToJsonString(renamedTemplate, plan.targetTemplateFile.parent_path()),
            false});

        for (const std::filesystem::path& file : plan.filesToModify)
        {
            const std::filesystem::path normalizedFile = NormalizePath(file);
            if (PathsEqual(normalizedFile, plan.targetTemplateFile))
            {
                continue;
            }

            const auto currentPageIterator = currentPageIndexByFile.find(PageUsageKey(normalizedFile));
            if (currentPageIterator != currentPageIndexByFile.end())
            {
                const int currentPageIndex = currentPageIterator->second;
                if (currentPageIndex < 0 || currentPageIndex >= static_cast<int>(loaded.document.pages.size()))
                {
                    throw std::runtime_error("The targeted current page no longer exists in the editor document.");
                }

                mfd::PageDefinition pageCopy = loaded.document.pages[static_cast<std::size_t>(currentPageIndex)];
                if (RenamePageTemplateReferences(pageCopy, plan.oldReticleName, plan.newReticleName) == 0U)
                {
                    continue;
                }

                writes.push_back(PendingWrite {
                    normalizedFile,
                    SerializePageToJsonString(pageCopy, serializedLibrary, files, static_cast<std::size_t>(currentPageIndex)),
                    true});
                updatedPageKeys.insert(PageUsageKey(normalizedFile));
                continue;
            }

            json pageDocument = LoadJsonFile(normalizedFile);
            if (RewritePageTemplateReferences(pageDocument, plan.oldReticleName, plan.newReticleName) == 0U)
            {
                continue;
            }

            writes.push_back(PendingWrite {
                normalizedFile,
                pageDocument.dump(2) + '\n',
                true});
            updatedPageKeys.insert(PageUsageKey(normalizedFile));
        }

        for (const PendingWrite& write : writes)
        {
            WriteTextFile(write.file, write.content);
        }

        for (const std::filesystem::path& fileToDelete : plan.filesToDelete)
        {
            DeleteFile(fileToDelete);
        }

        mfd::ReticleLibrary updatedLibrary = loaded.document.reticleLibrary;
        updatedLibrary.erase(currentTemplateKey);
        updatedLibrary.erase(plan.newReticleName);
        updatedLibrary.emplace(plan.newReticleName, renamedTemplate);
        loaded.document.reticleLibrary = std::move(updatedLibrary);

        for (mfd::PageDefinition& page : loaded.document.pages)
        {
            RenamePageTemplateReferences(page, plan.oldReticleName, plan.newReticleName);
        }

        files.templateFiles.erase(trackedTemplateKey);
        files.templateFiles.erase(plan.newReticleName);
        files.templateFiles.emplace(plan.newReticleName, plan.targetTemplateFile);
        RemovePath(files.removedTemplateFiles, plan.currentTemplateFile);
        RemovePath(files.removedTemplateFiles, plan.targetTemplateFile);

        if (result != nullptr)
        {
            result->modifiedFiles.clear();
            result->modifiedFiles.reserve(writes.size());
            result->deletedFiles = plan.filesToDelete;
            result->updatedPageCount = updatedPageKeys.size();
            result->renamedTemplateFile = !plan.filesToDelete.empty();
            result->updatedCurrentDocument = true;

            for (const PendingWrite& write : writes)
            {
                result->modifiedFiles.push_back(write.file);
            }
        }

        if (error != nullptr)
        {
            error->clear();
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

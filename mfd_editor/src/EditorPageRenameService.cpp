/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorPageRenameService.h"

/**
 * @file
 * @brief Implementation of the editor global page-rename planning service.
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
    std::filesystem::path file;
    std::string content;
    bool windowFile = false;
};

std::string TrimAsciiWhitespace(std::string_view value)
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

void WriteJsonFile(const std::filesystem::path& path, const std::string_view content)
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
        throw std::runtime_error("Unable to write JSON file: " + path.string());
    }
}

const json& ExtractPageNode(const json& document)
{
    if (document.is_object() &&
        document.contains("page") &&
        document.at("page").is_object())
    {
        return document.at("page");
    }

    return document;
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

std::string ExtractPageName(const json& document)
{
    const json& pageNode = ExtractPageNode(document);
    if (!pageNode.is_object())
    {
        throw std::runtime_error("Page JSON must contain one object root or one {\"page\": {...}} wrapper.");
    }

    if (pageNode.contains("name"))
    {
        if (!pageNode.at("name").is_string())
        {
            throw std::runtime_error("Page name field must be a string.");
        }

        return pageNode.at("name").get<std::string>();
    }

    if (pageNode.contains("id"))
    {
        if (!pageNode.at("id").is_string())
        {
            throw std::runtime_error("Page id field must be a string.");
        }

        return pageNode.at("id").get<std::string>();
    }

    throw std::runtime_error("Page JSON must define a name or id.");
}

void RewritePageName(json& document, const std::string& newPageName)
{
    json& pageNode = ExtractMutablePageNode(document);
    if (!pageNode.is_object())
    {
        throw std::runtime_error("Page JSON must contain one object root or one {\"page\": {...}} wrapper.");
    }

    bool updatedAnyField = false;
    if (pageNode.contains("name"))
    {
        if (!pageNode.at("name").is_string())
        {
            throw std::runtime_error("Page name field must be a string.");
        }

        pageNode["name"] = newPageName;
        updatedAnyField = true;
    }

    if (pageNode.contains("id"))
    {
        if (!pageNode.at("id").is_string())
        {
            throw std::runtime_error("Page id field must be a string.");
        }

        pageNode["id"] = newPageName;
        updatedAnyField = true;
    }

    if (!updatedAnyField)
    {
        throw std::runtime_error("Page JSON must define a name or id.");
    }
}

std::optional<std::string> ParseDefaultPageName(const json& document)
{
    if (!document.is_object() || !document.contains("defaultPage"))
    {
        return std::nullopt;
    }

    const json& defaultPage = document.at("defaultPage");
    if (defaultPage.is_null())
    {
        return std::nullopt;
    }

    if (!defaultPage.is_string())
    {
        throw std::runtime_error("Window defaultPage must be a string when present.");
    }

    return defaultPage.get<std::string>();
}

bool WindowDocumentUpdatesDefaultPage(const json& document, const std::string_view pageName)
{
    const std::optional<std::string> defaultPageName = ParseDefaultPageName(document);
    return defaultPageName.has_value() && mfd::PageNamesEqual(*defaultPageName, pageName);
}

std::vector<std::filesystem::path> ParseWindowPageFiles(const json& document, const std::filesystem::path& windowFile)
{
    if (!document.is_object())
    {
        throw std::runtime_error("Window JSON root must be one object.");
    }

    if (!document.contains("pages"))
    {
        throw std::runtime_error("Window JSON must define one pages array.");
    }

    const json& pages = document.at("pages");
    if (!pages.is_array())
    {
        throw std::runtime_error("Window pages field must be a JSON array.");
    }

    std::vector<std::filesystem::path> pageFiles;
    pageFiles.reserve(pages.size());

    for (const auto& entry : pages)
    {
        if (entry.is_string())
        {
            pageFiles.push_back(NormalizePath(windowFile.parent_path() / entry.get<std::string>()));
            continue;
        }

        if (entry.is_object())
        {
            for (const char* fieldName : {"file", "path", "json"})
            {
                if (!entry.contains(fieldName))
                {
                    continue;
                }

                if (!entry.at(fieldName).is_string())
                {
                    throw std::runtime_error("Window page entries must expose one string file/path/json value.");
                }

                pageFiles.push_back(NormalizePath(windowFile.parent_path() / entry.at(fieldName).get<std::string>()));
                goto page_entry_done;
            }
        }

        throw std::runtime_error("Each window page entry must be a string path or an object containing file/path/json.");

page_entry_done:
        continue;
    }

    return pageFiles;
}

bool RewriteWindowDefaultPage(json& document, const std::string& oldPageName, const std::string& newPageName)
{
    std::optional<std::string> defaultPageName = ParseDefaultPageName(document);
    if (!defaultPageName.has_value() || !mfd::PageNamesEqual(*defaultPageName, oldPageName))
    {
        return false;
    }

    document["defaultPage"] = newPageName;
    return true;
}

int FindTrackedPageIndex(const EditorFileLayout& files, const RenamePagePlan& plan)
{
    if (plan.pageIndex >= 0 &&
        plan.pageIndex < static_cast<int>(files.pageFiles.size()) &&
        NormalizePath(files.pageFiles[static_cast<std::size_t>(plan.pageIndex)]) == plan.pageFile)
    {
        return plan.pageIndex;
    }

    for (int index = 0; index < static_cast<int>(files.pageFiles.size()); ++index)
    {
        if (NormalizePath(files.pageFiles[static_cast<std::size_t>(index)]) == plan.pageFile)
        {
            return index;
        }
    }

    return -1;
}

void AppendUniquePath(std::vector<std::filesystem::path>& paths, const std::filesystem::path& candidate)
{
    const std::filesystem::path normalizedCandidate = NormalizePath(candidate);
    if (normalizedCandidate.empty())
    {
        return;
    }

    if (std::find(paths.begin(), paths.end(), normalizedCandidate) == paths.end())
    {
        paths.push_back(normalizedCandidate);
    }
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

void SortReferences(std::vector<RenamePageReference>& references)
{
    std::sort(references.begin(),
              references.end(),
              [](const RenamePageReference& lhs, const RenamePageReference& rhs)
              {
                  return std::make_tuple(PathKey(lhs.windowFile), PathKey(lhs.pageFile), lhs.updatesDefaultPage) <
                         std::make_tuple(PathKey(rhs.windowFile), PathKey(rhs.pageFile), rhs.updatesDefaultPage);
              });
}

void SortCollisions(std::vector<RenamePageCollision>& collisions)
{
    std::sort(collisions.begin(),
              collisions.end(),
              [](const RenamePageCollision& lhs, const RenamePageCollision& rhs)
              {
                  return std::make_tuple(PathKey(lhs.windowFile), PathKey(lhs.conflictingPageFile), lhs.conflictingPageName) <
                         std::make_tuple(PathKey(rhs.windowFile), PathKey(rhs.conflictingPageFile), rhs.conflictingPageName);
              });
}
} // namespace

RenamePagePlan PageRenameService::BuildPlan(const mfd::LoadedWindowConfiguration& loaded,
                                            const EditorFileLayout& files,
                                            const RenamePageRequest& request) const
{
    RenamePagePlan plan;
    plan.pageIndex = request.pageIndex;
    plan.assetsRoot = NormalizePath(request.assetsRoot);

    try
    {
        if (request.pageIndex < 0 || request.pageIndex >= static_cast<int>(loaded.document.pages.size()))
        {
            throw std::runtime_error("No valid page is selected.");
        }

        const mfd::PageDefinition& page = loaded.document.pages[static_cast<std::size_t>(request.pageIndex)];
        plan.oldPageName = page.name;
        plan.newPageName = TrimAsciiWhitespace(request.newPageName);

        if (mfd::NormalizePageName(plan.newPageName).empty())
        {
            throw std::runtime_error("The new page name cannot be empty.");
        }
        if (mfd::PageNamesEqual(plan.oldPageName, plan.newPageName))
        {
            throw std::runtime_error("Choose a different page name before applying the global rename.");
        }
        if (request.pageIndex >= static_cast<int>(files.pageFiles.size()))
        {
            throw std::runtime_error("The selected page does not expose a tracked source JSON file.");
        }

        plan.pageFile = NormalizePath(files.pageFiles[static_cast<std::size_t>(request.pageIndex)]);
        if (plan.pageFile.empty() || !PathExists(plan.pageFile))
        {
            throw std::runtime_error("The selected page JSON file could not be found on disk.");
        }
        if (plan.assetsRoot.empty() || !PathExists(plan.assetsRoot))
        {
            throw std::runtime_error("The scanned assets root does not exist.");
        }
        if (!IsPathInsideRoot(plan.pageFile, plan.assetsRoot))
        {
            throw std::runtime_error("Refusing to rename one page JSON outside the protected scanned assets root.");
        }

        const json currentPageDocument = LoadJsonFile(plan.pageFile);
        const std::string savedPageName = ExtractPageName(currentPageDocument);
        if (!mfd::PageNamesEqual(savedPageName, plan.oldPageName))
        {
            throw std::runtime_error(
                "Save or reload the current window before renaming this page globally because the in-memory page name differs from disk.");
        }

        const AssetReferenceIndex index = referenceIndexService_.BuildIndex(AssetReferenceIndexRequest {plan.assetsRoot});
        const std::filesystem::path currentWindowFile = NormalizePath(loaded.window.sourceFile);
        const bool currentWindowInsideAssetsRoot = IsPathInsideRoot(currentWindowFile, plan.assetsRoot);

        for (const AssetScanError& errorEntry : index.errors)
        {
            if (!HasPathComponent(errorEntry.file, "windows"))
            {
                continue;
            }
            if (currentWindowInsideAssetsRoot && NormalizePath(errorEntry.file) == currentWindowFile)
            {
                continue;
            }

            throw std::runtime_error(
                "Safe global rename requires valid window JSON files under the scanned assets root. "
                "Scan error: " +
                errorEntry.file.string() + " (" + errorEntry.message + ")");
        }

        std::unordered_map<std::string, std::vector<RenamePageReference>> referencesByWindow;
        for (const PageReference& reference : index.pages)
        {
            if (NormalizePath(reference.pageFile) != plan.pageFile)
            {
                continue;
            }

            const std::filesystem::path windowFile = NormalizePath(reference.windowFile);
            if (currentWindowInsideAssetsRoot && windowFile == currentWindowFile)
            {
                continue;
            }

            referencesByWindow[PathKey(windowFile)].push_back(RenamePageReference {windowFile, plan.pageFile, false});
        }

        if (currentWindowInsideAssetsRoot)
        {
            bool currentWindowReferencesPage = false;
            for (const std::filesystem::path& pageFile : files.pageFiles)
            {
                if (NormalizePath(pageFile) == plan.pageFile)
                {
                    currentWindowReferencesPage = true;
                    break;
                }
            }

            if (currentWindowReferencesPage)
            {
                std::vector<RenamePageReference> currentWindowReferences;
                for (const std::filesystem::path& pageFile : files.pageFiles)
                {
                    if (NormalizePath(pageFile) == plan.pageFile)
                    {
                        currentWindowReferences.push_back(RenamePageReference {currentWindowFile, plan.pageFile, false});
                    }
                }

                referencesByWindow[PathKey(currentWindowFile)] = std::move(currentWindowReferences);
            }
        }

        if (referencesByWindow.empty())
        {
            throw std::runtime_error(
                "No window under the scanned assets root references page '" + plan.oldPageName + "'.");
        }

        AppendUniquePath(plan.filesToModify, plan.pageFile);

        for (auto& entry : referencesByWindow)
        {
            auto& references = entry.second;
            if (references.empty())
            {
                continue;
            }

            const std::filesystem::path windowFile = NormalizePath(references.front().windowFile);
            const bool currentWindowReference = currentWindowInsideAssetsRoot && windowFile == currentWindowFile;

            const json windowDocument = LoadJsonFile(windowFile);
            const bool updatesDefaultPage = WindowDocumentUpdatesDefaultPage(windowDocument, plan.oldPageName);
            if (updatesDefaultPage)
            {
                AppendUniquePath(plan.filesToModify, windowFile);
            }

            for (RenamePageReference& reference : references)
            {
                reference.updatesDefaultPage = updatesDefaultPage;
                plan.references.push_back(reference);
            }

            if (currentWindowReference)
            {
                for (int indexPage = 0; indexPage < static_cast<int>(loaded.document.pages.size()); ++indexPage)
                {
                    if (indexPage == request.pageIndex)
                    {
                        continue;
                    }

                    const mfd::PageDefinition& currentPage = loaded.document.pages[static_cast<std::size_t>(indexPage)];
                    if (!mfd::PageNamesEqual(currentPage.name, plan.newPageName))
                    {
                        continue;
                    }

                    const std::filesystem::path conflictingPageFile =
                        indexPage < static_cast<int>(files.pageFiles.size())
                            ? NormalizePath(files.pageFiles[static_cast<std::size_t>(indexPage)])
                            : std::filesystem::path {};
                    plan.collisions.push_back(
                        RenamePageCollision {windowFile, conflictingPageFile, currentPage.name});
                }

                continue;
            }

            for (const std::filesystem::path& candidatePageFile : ParseWindowPageFiles(windowDocument, windowFile))
            {
                const std::filesystem::path normalizedCandidatePageFile = NormalizePath(candidatePageFile);
                if (normalizedCandidatePageFile == plan.pageFile)
                {
                    continue;
                }

                const std::string candidatePageName = ExtractPageName(LoadJsonFile(normalizedCandidatePageFile));
                if (!mfd::PageNamesEqual(candidatePageName, plan.newPageName))
                {
                    continue;
                }

                plan.collisions.push_back(
                    RenamePageCollision {windowFile, normalizedCandidatePageFile, candidatePageName});
            }
        }

        plan.warnings.push_back("This rename may change generated mappingHash values and generated client API page names.");

        SortPaths(plan.filesToModify);
        SortReferences(plan.references);
        SortCollisions(plan.collisions);

        if (!plan.collisions.empty())
        {
            throw std::runtime_error(
                "Refusing the rename because one referenced window already contains page name '" + plan.newPageName + "'.");
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

bool PageRenameService::Execute(const RenamePagePlan& plan,
                                mfd::LoadedWindowConfiguration& loaded,
                                EditorFileLayout& files,
                                RenamePageResult* result,
                                std::string* error) const
{
    try
    {
        if (!plan.canExecute)
        {
            throw std::runtime_error(plan.error.empty() ? "The page rename plan cannot execute." : plan.error);
        }

        const int trackedPageIndex = FindTrackedPageIndex(files, plan);
        if (trackedPageIndex < 0 || trackedPageIndex >= static_cast<int>(loaded.document.pages.size()))
        {
            throw std::runtime_error("The targeted page no longer exists in the current editor document.");
        }

        if (!PathExists(plan.pageFile))
        {
            throw std::runtime_error("The targeted page JSON file no longer exists on disk.");
        }

        std::vector<PendingWrite> writes;
        {
            json pageDocument = LoadJsonFile(plan.pageFile);
            RewritePageName(pageDocument, plan.newPageName);
            writes.push_back(PendingWrite {plan.pageFile, pageDocument.dump(2) + '\n', false});
        }

        for (const std::filesystem::path& file : plan.filesToModify)
        {
            const std::filesystem::path normalizedFile = NormalizePath(file);
            if (normalizedFile == plan.pageFile)
            {
                continue;
            }

            json windowDocument = LoadJsonFile(normalizedFile);
            if (!RewriteWindowDefaultPage(windowDocument, plan.oldPageName, plan.newPageName))
            {
                continue;
            }

            writes.push_back(PendingWrite {normalizedFile, windowDocument.dump(2) + '\n', true});
        }

        for (const PendingWrite& write : writes)
        {
            WriteJsonFile(write.file, write.content);
        }

        mfd::PageDefinition& renamedPage = loaded.document.pages[static_cast<std::size_t>(trackedPageIndex)];
        renamedPage.name = plan.newPageName;
        renamedPage.normalizedName = mfd::NormalizePageName(plan.newPageName);

        if (result != nullptr)
        {
            result->modifiedFiles.clear();
            result->modifiedFiles.reserve(writes.size());
            result->updatedWindowCount = 0U;

            for (const PendingWrite& write : writes)
            {
                result->modifiedFiles.push_back(write.file);
                if (write.windowFile)
                {
                    ++result->updatedWindowCount;
                }
            }

            result->updatedCurrentDocument = true;
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

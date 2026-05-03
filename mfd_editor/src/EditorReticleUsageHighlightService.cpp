/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorReticleUsageHighlightService.h"

/**
 * @file
 * @brief Implementation of the editor reticle-template usage highlight service.
 */

#include <algorithm>
#include <cctype>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

#include "mfd/model/PageName.h"

namespace editor
{
namespace
{
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

std::string PathKey(const std::filesystem::path& path)
{
    return Lowercase(NormalizePath(path).generic_string());
}

std::string PageNameOrFallback(const std::string_view pageName, const std::filesystem::path& pageFile)
{
    if (!pageName.empty())
    {
        return std::string(pageName);
    }

    return pageFile.empty() ? std::string {"page"} : pageFile.stem().string();
}

void SortHighlightPages(std::vector<ReticleUsageHighlightPage>& pages)
{
    std::sort(pages.begin(),
              pages.end(),
              [](const ReticleUsageHighlightPage& lhs, const ReticleUsageHighlightPage& rhs)
              {
                  const bool lhsCurrent = lhs.currentPageIndex >= 0;
                  const bool rhsCurrent = rhs.currentPageIndex >= 0;
                  return std::make_tuple(!lhsCurrent,
                                         lhs.currentPageIndex,
                                         Lowercase(lhs.pageName),
                                         PathKey(lhs.pageFile)) <
                         std::make_tuple(!rhsCurrent,
                                         rhs.currentPageIndex,
                                         Lowercase(rhs.pageName),
                                         PathKey(rhs.pageFile));
              });

    for (ReticleUsageHighlightPage& page : pages)
    {
        std::sort(page.matchingReticleIndices.begin(), page.matchingReticleIndices.end());
        page.matchingReticleIndices.erase(std::unique(page.matchingReticleIndices.begin(), page.matchingReticleIndices.end()),
                                          page.matchingReticleIndices.end());
    }
}
} // namespace

ReticleUsageHighlightResult ReticleUsageHighlightService::BuildHighlight(const mfd::LoadedWindowConfiguration& loaded,
                                                                         const EditorFileLayout& files,
                                                                         const ReticleUsageHighlightRequest& request) const
{
    ReticleUsageHighlightResult result;
    result.templateId = request.templateId;
    result.assetsRoot = NormalizePath(request.assetsRoot);

    const std::string normalizedTemplateId = NormalizeIdentifier(request.templateId);
    if (normalizedTemplateId.empty())
    {
        return result;
    }

    std::unordered_map<std::string, ReticleUsageHighlightPage> pagesByKey;
    std::unordered_set<std::string> currentPageFileKeys;
    currentPageFileKeys.reserve(files.pageFiles.size());

    for (int pageIndex = 0; pageIndex < static_cast<int>(loaded.document.pages.size()); ++pageIndex)
    {
        const mfd::PageDefinition& page = loaded.document.pages[static_cast<std::size_t>(pageIndex)];
        const std::filesystem::path pageFile =
            pageIndex < static_cast<int>(files.pageFiles.size()) ? NormalizePath(files.pageFiles[static_cast<std::size_t>(pageIndex)])
                                                                 : std::filesystem::path {};
        const std::string pageKey =
            pageFile.empty() ? "current:" + std::to_string(pageIndex) : PathKey(pageFile);
        currentPageFileKeys.insert(pageKey);

        ReticleUsageHighlightPage highlightPage;
        highlightPage.pageFile = pageFile;
        highlightPage.pageName = PageNameOrFallback(page.name, pageFile);
        highlightPage.currentPageIndex = pageIndex;

        for (int reticleIndex = 0; reticleIndex < static_cast<int>(page.staticReticles.size()); ++reticleIndex)
        {
            const mfd::ReticleGroup& reticle = page.staticReticles[static_cast<std::size_t>(reticleIndex)];
            if (NormalizeIdentifier(reticle.sourceTemplateId) != normalizedTemplateId)
            {
                continue;
            }

            highlightPage.matchingReticleIndices.push_back(reticleIndex);
            ++result.totalReferenceCount;
        }

        if (page.strobe.has_value() &&
            NormalizeIdentifier(page.strobe->reticle.sourceTemplateId) == normalizedTemplateId)
        {
            highlightPage.matchingStrobe = true;
            ++result.totalReferenceCount;
        }

        if (!highlightPage.matchingReticleIndices.empty() || highlightPage.matchingStrobe)
        {
            pagesByKey.emplace(pageKey, std::move(highlightPage));
        }
    }

    const bool canScanAssetsRoot = !result.assetsRoot.empty() && std::filesystem::exists(result.assetsRoot);
    if (canScanAssetsRoot)
    {
        const AssetReferenceIndex index = referenceIndexService_.BuildIndex(AssetReferenceIndexRequest {result.assetsRoot});

        for (const ReticleReference& reference : index.reticles)
        {
            if ((reference.kind != ReticleReferenceKind::PageReticleTemplate &&
                 reference.kind != ReticleReferenceKind::PageStrobeTemplate) ||
                NormalizeIdentifier(reference.templateId) != normalizedTemplateId)
            {
                continue;
            }

            const std::filesystem::path pageFile = NormalizePath(reference.ownerFile);
            const std::string pageKey = PathKey(pageFile);
            if (currentPageFileKeys.find(pageKey) != currentPageFileKeys.end())
            {
                continue;
            }

            ReticleUsageHighlightPage& highlightPage = pagesByKey[pageKey];
            highlightPage.pageFile = pageFile;
            highlightPage.pageName = PageNameOrFallback(reference.pageName, pageFile);

            if (reference.kind == ReticleReferenceKind::PageStrobeTemplate)
            {
                highlightPage.matchingStrobe = true;
            }

            ++result.totalReferenceCount;
        }
    }

    result.pages.reserve(pagesByKey.size());
    for (auto& entry : pagesByKey)
    {
        result.pages.push_back(std::move(entry.second));
    }

    SortHighlightPages(result.pages);
    result.hasUsage = !result.pages.empty();
    return result;
}
} // namespace editor

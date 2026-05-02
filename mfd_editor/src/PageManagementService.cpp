/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "PageManagementService.h"

/**
 * @file
 * @brief Implementation of the editor page-management planning service.
 */

#include <algorithm>
#include <cctype>
#include <string_view>

namespace editor
{
namespace
{
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

std::filesystem::path NormalizePath(const std::filesystem::path& path)
{
    if (path.empty())
    {
        return {};
    }

    std::error_code error;
    std::filesystem::path absolutePath = path;
    if (!absolutePath.is_absolute())
    {
        absolutePath = std::filesystem::absolute(absolutePath, error);
        if (error)
        {
            return path.lexically_normal();
        }
    }

    return absolutePath.lexically_normal();
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

void ResetDefaultFlags(std::vector<mfd::PageDefinition>& pages)
{
    for (mfd::PageDefinition& page : pages)
    {
        page.defaultPage = false;
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

int AdjustIndexAfterRemoval(const int removedIndex, const int originalIndex) noexcept
{
    if (originalIndex < 0)
    {
        return -1;
    }

    return originalIndex > removedIndex ? originalIndex - 1 : originalIndex;
}

int ComputeNextSelectedPageIndex(const int pageCount, const int pageIndex, const int replacementPageIndex) noexcept
{
    const int remainingPageCount = pageCount - 1;
    if (remainingPageCount <= 0)
    {
        return -1;
    }

    if (replacementPageIndex >= 0)
    {
        return std::clamp(AdjustIndexAfterRemoval(pageIndex, replacementPageIndex), 0, remainingPageCount - 1);
    }

    return std::clamp(pageIndex, 0, remainingPageCount - 1);
}

std::filesystem::path PageFileAt(const EditorFileLayout& files, const int pageIndex)
{
    if (pageIndex < 0 || pageIndex >= static_cast<int>(files.pageFiles.size()))
    {
        return {};
    }

    return NormalizePath(files.pageFiles[static_cast<std::size_t>(pageIndex)]);
}

template <typename PlanType>
PlanType BuildBasePlan(const mfd::LoadedWindowConfiguration& loaded,
                       const EditorFileLayout& files,
                       const int pageIndex,
                       const std::optional<int>& replacementPageIndex)
{
    PlanType plan;
    plan.pageIndex = pageIndex;

    if (pageIndex < 0 || pageIndex >= static_cast<int>(loaded.document.pages.size()))
    {
        plan.error = "No valid page is selected.";
        return plan;
    }

    if (loaded.document.pages.size() <= 1U)
    {
        plan.error = "The window must keep at least one page.";
        return plan;
    }

    const mfd::PageDefinition& page = loaded.document.pages[static_cast<std::size_t>(pageIndex)];
    plan.pageName = page.name;
    plan.pageFile = PageFileAt(files, pageIndex);

    if (page.defaultPage)
    {
        if (!replacementPageIndex.has_value())
        {
            plan.error = "Choose a replacement default page before removing the current default page.";
            return plan;
        }

        const int candidateReplacementIndex = *replacementPageIndex;
        if (candidateReplacementIndex < 0 ||
            candidateReplacementIndex >= static_cast<int>(loaded.document.pages.size()) ||
            candidateReplacementIndex == pageIndex)
        {
            plan.error = "Select a different valid replacement page.";
            return plan;
        }

        plan.replacementPageIndex = candidateReplacementIndex;
        plan.replacementPageName =
            loaded.document.pages[static_cast<std::size_t>(candidateReplacementIndex)].name;
    }

    plan.nextSelectedPageIndex =
        ComputeNextSelectedPageIndex(static_cast<int>(loaded.document.pages.size()), pageIndex, plan.replacementPageIndex);
    plan.canExecute = true;
    return plan;
}

void MarkPageFileAsRemoved(const std::filesystem::path& pageFile, EditorFileLayout& files)
{
    if (pageFile.empty())
    {
        return;
    }

    const std::filesystem::path normalizedPageFile = NormalizePath(pageFile);
    if (std::find(files.removedPageFiles.begin(), files.removedPageFiles.end(), normalizedPageFile) ==
        files.removedPageFiles.end())
    {
        files.removedPageFiles.push_back(normalizedPageFile);
    }
}

template <typename PlanType>
bool ExecutePlan(const PlanType& plan,
                 const bool markAssetDeleted,
                 mfd::LoadedWindowConfiguration& loaded,
                 EditorFileLayout& files,
                 std::string* error)
{
    if (!plan.canExecute)
    {
        if (error != nullptr)
        {
            *error = plan.error.empty() ? "The page management plan cannot execute." : plan.error;
        }
        return false;
    }

    if (plan.pageIndex < 0 || plan.pageIndex >= static_cast<int>(loaded.document.pages.size()))
    {
        if (error != nullptr)
        {
            *error = "The targeted page no longer exists.";
        }
        return false;
    }

    const bool removedPageWasDefault = loaded.document.pages[static_cast<std::size_t>(plan.pageIndex)].defaultPage;
    const std::filesystem::path pageFile = PageFileAt(files, plan.pageIndex);

    if (markAssetDeleted && pageFile.empty())
    {
        if (error != nullptr)
        {
            *error = "The page file is unknown and cannot be deleted safely.";
        }
        return false;
    }

    loaded.document.pages.erase(loaded.document.pages.begin() + plan.pageIndex);
    if (plan.pageIndex >= 0 && plan.pageIndex < static_cast<int>(files.pageFiles.size()))
    {
        files.pageFiles.erase(files.pageFiles.begin() + plan.pageIndex);
    }

    if (markAssetDeleted)
    {
        MarkPageFileAsRemoved(pageFile, files);
    }

    if (removedPageWasDefault)
    {
        ResetDefaultFlags(loaded.document.pages);
        const int adjustedReplacementIndex = AdjustIndexAfterRemoval(plan.pageIndex, plan.replacementPageIndex);
        if (adjustedReplacementIndex < 0 ||
            adjustedReplacementIndex >= static_cast<int>(loaded.document.pages.size()))
        {
            if (error != nullptr)
            {
                *error = "The replacement default page is no longer valid.";
            }
            return false;
        }

        loaded.document.pages[static_cast<std::size_t>(adjustedReplacementIndex)].defaultPage = true;
    }
    else if (!loaded.document.pages.empty() && !HasAnyDefaultPage(loaded.document.pages))
    {
        loaded.document.pages.front().defaultPage = true;
    }

    loaded.window.pageFiles = files.pageFiles;
    return true;
}
} // namespace

PageRemovePlan PageManagementService::BuildRemovePlan(const mfd::LoadedWindowConfiguration& loaded,
                                                      const EditorFileLayout& files,
                                                      const PageRemoveRequest& request) const
{
    return BuildBasePlan<PageRemovePlan>(loaded, files, request.pageIndex, request.replacementPageIndex);
}

PageDeletePlan PageManagementService::BuildDeletePlan(const mfd::LoadedWindowConfiguration& loaded,
                                                      const EditorFileLayout& files,
                                                      const PageDeleteRequest& request) const
{
    PageDeletePlan plan = BuildBasePlan<PageDeletePlan>(loaded, files, request.pageIndex, request.replacementPageIndex);
    plan.assetsRoot = NormalizePath(request.assetsRoot);

    if (!plan.canExecute)
    {
        return plan;
    }

    if (plan.pageFile.empty())
    {
        plan.canExecute = false;
        plan.error = "The selected page does not expose a tracked source JSON file.";
        return plan;
    }

    if (!plan.assetsRoot.empty())
    {
        plan.outsideAssetsRoot = !IsPathInsideRoot(plan.pageFile, plan.assetsRoot);
        plan.requiresAdvancedConfirmation = plan.outsideAssetsRoot && !request.allowOutsideAssetsRoot;
        if (plan.requiresAdvancedConfirmation)
        {
            plan.canExecute = false;
            plan.error = "Refusing to delete a page file outside the source assets root without advanced confirmation.";
        }
    }

    return plan;
}

bool PageManagementService::Execute(const PageRemovePlan& plan,
                                    mfd::LoadedWindowConfiguration& loaded,
                                    EditorFileLayout& files,
                                    std::string* error) const
{
    return ExecutePlan(plan, false, loaded, files, error);
}

bool PageManagementService::Execute(const PageDeletePlan& plan,
                                    mfd::LoadedWindowConfiguration& loaded,
                                    EditorFileLayout& files,
                                    std::string* error) const
{
    return ExecutePlan(plan, true, loaded, files, error);
}
} // namespace editor

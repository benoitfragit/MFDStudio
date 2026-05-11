/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Page removal and delete planning helpers used by the editor shell.
 */

#include <filesystem>
#include <optional>
#include <string>

#include "EditorDocumentSerializer.h"
#include "mfd/io/JsonLoader.h"

namespace editor
{
/**
 * @brief Input used to plan one page removal that only detaches the page from the current window.
 */
struct PageRemoveRequest
{
    /** @brief Zero-based index of the page to remove from the current window. */
    int pageIndex = -1;
    /** @brief Replacement default-page index when the removed page currently owns the default flag. */
    std::optional<int> replacementPageIndex {};
};

/**
 * @brief Input used to plan one page-asset deletion from the current window.
 */
struct PageDeleteRequest
{
    /** @brief Zero-based index of the page to delete from the current window. */
    int pageIndex = -1;
    /** @brief Replacement default-page index when the deleted page currently owns the default flag. */
    std::optional<int> replacementPageIndex {};
    /** @brief Source asset root used to refuse destructive deletes outside the repo asset tree. */
    std::filesystem::path assetsRoot {};
    /** @brief Allows one delete outside `assetsRoot` after an advanced user confirmation. */
    bool allowOutsideAssetsRoot = false;
};

/**
 * @brief Pure plan describing how one page would be removed from the current window.
 */
struct PageRemovePlan
{
    /** @brief Indicates whether the plan is valid and executable. */
    bool canExecute = false;
    /** @brief Zero-based page index targeted by the plan. */
    int pageIndex = -1;
    /** @brief Replacement default-page index in the pre-execution document, or `-1` when unused. */
    int replacementPageIndex = -1;
    /** @brief Suggested page selection index after the mutation has been applied. */
    int nextSelectedPageIndex = -1;
    /** @brief Human-readable page name targeted by the plan. */
    std::string pageName;
    /** @brief Human-readable replacement default page name when relevant. */
    std::string replacementPageName;
    /** @brief Authored page JSON file currently backing the page, when known. */
    std::filesystem::path pageFile;
    /** @brief Validation error explaining why the plan cannot execute. */
    std::string error;
};

/**
 * @brief Pure plan describing how one page asset would be removed and marked for deletion.
 */
struct PageDeletePlan
{
    /** @brief Indicates whether the plan is valid and executable. */
    bool canExecute = false;
    /** @brief Zero-based page index targeted by the plan. */
    int pageIndex = -1;
    /** @brief Replacement default-page index in the pre-execution document, or `-1` when unused. */
    int replacementPageIndex = -1;
    /** @brief Suggested page selection index after the mutation has been applied. */
    int nextSelectedPageIndex = -1;
    /** @brief Human-readable page name targeted by the plan. */
    std::string pageName;
    /** @brief Human-readable replacement default page name when relevant. */
    std::string replacementPageName;
    /** @brief Authored page JSON file currently backing the page. */
    std::filesystem::path pageFile;
    /** @brief Normalized source assets root used by the safety check. */
    std::filesystem::path assetsRoot;
    /** @brief Indicates that the page file currently sits outside the protected source assets root. */
    bool outsideAssetsRoot = false;
    /** @brief Indicates that an explicit advanced confirmation is still required. */
    bool requiresAdvancedConfirmation = false;
    /** @brief Validation error explaining why the plan cannot execute. */
    std::string error;
};

/**
 * @brief Stateless service planning and applying page-window mutations without mixing logic into ImGui.
 */
class PageManagementService
{
public:
    /**
     * @brief Builds one read-only plan for removing a page from the current window.
     * @param loaded Current authored window and document state.
     * @param files Current authored file layout tracked by the editor.
     * @param request Requested page-removal inputs.
     * @return Pure plan describing whether the mutation can execute.
     */
    [[nodiscard]] PageRemovePlan BuildRemovePlan(const mfd::LoadedWindowConfiguration& loaded,
                                                 const EditorFileLayout& files,
                                                 const PageRemoveRequest& request) const;

    /**
     * @brief Builds one read-only plan for deleting a page asset from the current window.
     * @param loaded Current authored window and document state.
     * @param files Current authored file layout tracked by the editor.
     * @param request Requested page-delete inputs.
     * @return Pure plan describing whether the mutation can execute.
     */
    [[nodiscard]] PageDeletePlan BuildDeletePlan(const mfd::LoadedWindowConfiguration& loaded,
                                                 const EditorFileLayout& files,
                                                 const PageDeleteRequest& request) const;

    /**
     * @brief Applies one valid page-removal plan to the live editor model.
     * @param plan Plan produced by `BuildRemovePlan()`.
     * @param loaded Mutable authored window/document state.
     * @param files Mutable file layout mirrored by the editor.
     * @param error Optional output populated when execution fails.
     * @return `true` when the plan was applied.
     */
    bool Execute(const PageRemovePlan& plan,
                 mfd::LoadedWindowConfiguration& loaded,
                 EditorFileLayout& files,
                 std::string* error) const;

    /**
     * @brief Applies one valid page-delete plan to the live editor model.
     * @param plan Plan produced by `BuildDeletePlan()`.
     * @param loaded Mutable authored window/document state.
     * @param files Mutable file layout mirrored by the editor.
     * @param error Optional output populated when execution fails.
     * @return `true` when the plan was applied.
     */
    bool Execute(const PageDeletePlan& plan,
                 mfd::LoadedWindowConfiguration& loaded,
                 EditorFileLayout& files,
                 std::string* error) const;
};
} // namespace editor

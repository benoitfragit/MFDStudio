/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Global page-rename planning helpers used by the editor shell to update shared assets safely.
 */

#include <filesystem>
#include <string>
#include <vector>

#include "EditorAssetReferenceIndexService.h"
#include "EditorDocumentSerializer.h"
#include "mfd/io/JsonLoader.h"

namespace editor
{
/**
 * @brief Input used to plan one global page rename across the scanned asset tree.
 */
struct RenamePageRequest
{
    /** @brief Zero-based index of the page selected in the current editor document. */
    int pageIndex = -1;
    /** @brief New authored page name that should replace the existing one. */
    std::string newPageName {};
    /** @brief Asset root scanned for window references. */
    std::filesystem::path assetsRoot {};
};

/**
 * @brief One exact `window -> page` reference affected by the rename plan.
 */
struct RenamePageReference
{
    /** @brief Window JSON file that references the target page asset. */
    std::filesystem::path windowFile;
    /** @brief Target page JSON file reused by that window. */
    std::filesystem::path pageFile;
    /** @brief Indicates whether the window `defaultPage` field must be rewritten. */
    bool updatesDefaultPage = false;
};

/**
 * @brief One blocking page-name collision discovered while validating the rename.
 */
struct RenamePageCollision
{
    /** @brief Window JSON file that already exposes the requested page name. */
    std::filesystem::path windowFile;
    /** @brief Conflicting page JSON file already loaded by that window. */
    std::filesystem::path conflictingPageFile;
    /** @brief Conflicting authored page name already present in that window. */
    std::string conflictingPageName;
};

/**
 * @brief Pure plan describing whether a global page rename can execute safely.
 */
struct RenamePagePlan
{
    /** @brief Indicates whether the plan is valid and executable. */
    bool canExecute = false;
    /** @brief Zero-based index of the targeted page in the current editor document. */
    int pageIndex = -1;
    /** @brief Current authored page name before the rename. */
    std::string oldPageName {};
    /** @brief Requested authored page name after the rename. */
    std::string newPageName {};
    /** @brief Target page JSON file that will be rewritten. */
    std::filesystem::path pageFile {};
    /** @brief Asset root scanned for dependent windows. */
    std::filesystem::path assetsRoot {};
    /** @brief Exact window references discovered for the target page. */
    std::vector<RenamePageReference> references {};
    /** @brief Blocking collisions discovered while evaluating the rename. */
    std::vector<RenamePageCollision> collisions {};
    /** @brief Concrete JSON files that would be rewritten by `Execute()`. */
    std::vector<std::filesystem::path> filesToModify {};
    /** @brief Non-blocking warnings shown to the user before execution. */
    std::vector<std::string> warnings {};
    /** @brief Validation error explaining why the plan cannot execute. */
    std::string error {};
};

/**
 * @brief Mutable execution result returned once one valid global page rename has been applied.
 */
struct RenamePageResult
{
    /** @brief JSON files rewritten on disk by the rename. */
    std::vector<std::filesystem::path> modifiedFiles {};
    /** @brief Number of distinct window JSON files rewritten. */
    std::size_t updatedWindowCount = 0U;
    /** @brief Indicates whether the current in-memory editor document was resynchronized. */
    bool updatedCurrentDocument = false;
};

/**
 * @brief Stateless service planning and applying global page renames without embedding filesystem logic in ImGui.
 */
class PageRenameService
{
public:
    /**
     * @brief Builds one read-only plan for renaming a shared page asset across the scanned window tree.
     * @param loaded Current authored window and document state.
     * @param files Current authored file layout tracked by the editor.
     * @param request Requested rename inputs.
     * @return Pure plan describing whether the rename can execute safely.
     */
    [[nodiscard]] RenamePagePlan BuildPlan(const mfd::LoadedWindowConfiguration& loaded,
                                           const EditorFileLayout& files,
                                           const RenamePageRequest& request) const;

    /**
     * @brief Applies one valid global page-rename plan to disk and resynchronizes the current editor document.
     * @param plan Plan produced by `BuildPlan()`.
     * @param loaded Mutable authored window/document state.
     * @param files Mutable file layout mirrored by the editor.
     * @param result Optional output populated with modified files and synchronization details.
     * @param error Optional output populated when execution fails.
     * @return `true` when the rename was applied.
     */
    bool Execute(const RenamePagePlan& plan,
                 mfd::LoadedWindowConfiguration& loaded,
                 EditorFileLayout& files,
                 RenamePageResult* result,
                 std::string* error) const;

private:
    AssetReferenceIndexService referenceIndexService_ {};
};
} // namespace editor

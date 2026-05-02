/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Page-import planning helpers used by the editor shell to stage external page assets safely.
 */

#include <filesystem>
#include <string>
#include <vector>

#include "AssetReferenceIndexService.h"
#include "EditorDocumentSerializer.h"
#include "mfd/io/JsonLoader.h"

namespace editor
{
/**
 * @brief File-disposition chosen by the import planner for one staged asset.
 */
enum class ImportDisposition
{
    CopyNew,
    KeepExisting,
    RenameCopy
};

/**
 * @brief One reticle-template dependency imported together with one page.
 */
struct ReticleImportPlan
{
    /** @brief Source template id referenced by the imported page. */
    std::string sourceTemplateId;
    /** @brief Target template id used after the import plan has resolved collisions. */
    std::string targetTemplateId;
    /** @brief Source template JSON file. */
    std::filesystem::path sourceFile;
    /** @brief Target template JSON file staged in the current reticle library. */
    std::filesystem::path targetFile;
    /** @brief File-disposition chosen for this dependency. */
    ImportDisposition disposition = ImportDisposition::CopyNew;
};

/**
 * @brief Input used to plan one page import into the current editor document.
 */
struct PageImportRequest
{
    /** @brief External page JSON file selected by the user. */
    std::filesystem::path sourcePageFile;
    /** @brief Folder where the imported page JSON should be staged. */
    std::filesystem::path targetPageFolder;
    /** @brief Folder where imported reticle templates should be staged. */
    std::filesystem::path targetReticleLibraryFolder;
};

/**
 * @brief Pure plan describing how one external page and its dependencies would be staged.
 */
struct PageImportPlan
{
    /** @brief Indicates whether the plan is valid and executable. */
    bool canExecute = false;
    /** @brief Source page JSON selected for the import. */
    std::filesystem::path sourcePageFile;
    /** @brief Inferred source assets root used to scan dependencies. */
    std::filesystem::path sourceAssetsRoot;
    /** @brief Source reticle-library folder inferred from the referenced templates. */
    std::filesystem::path sourceReticleLibraryFolder;
    /** @brief Page folder used by the current document for imported pages. */
    std::filesystem::path targetPageFolder;
    /** @brief Reticle-library folder used by the current document. */
    std::filesystem::path targetReticleLibraryFolder;
    /** @brief Source page name extracted from the selected JSON file. */
    std::string sourcePageName;
    /** @brief Target page name chosen after resolving name collisions. */
    std::string targetPageName;
    /** @brief Target page JSON file staged in the current document. */
    std::filesystem::path targetPageFile;
    /** @brief File-disposition chosen for the page JSON itself. */
    ImportDisposition pageDisposition = ImportDisposition::CopyNew;
    /** @brief Indicates that the source file used the `{"page": {...}}` wrapper syntax. */
    bool sourceUsesPageWrapper = false;
    /** @brief Reticle-template dependencies imported or reused by this plan. */
    std::vector<ReticleImportPlan> reticles;
    /** @brief Validation error explaining why the plan cannot execute. */
    std::string error;
};

/**
 * @brief Mutable execution result returned once one valid import plan has been applied.
 */
struct PageImportResult
{
    /** @brief Zero-based index of the imported page in the current document. */
    int importedPageIndex = -1;
    /** @brief Imported page name after collision resolution. */
    std::string pageName;
    /** @brief Imported page JSON file staged in the current document. */
    std::filesystem::path pageFile;
    /** @brief Template ids newly staged in the current reticle library. */
    std::vector<std::string> importedTemplateIds;
    /** @brief Files that now belong to the staged document after the import. */
    std::vector<std::filesystem::path> stagedFiles;
};

/**
 * @brief Stateless service planning and applying page imports without embedding filesystem logic in ImGui.
 */
class PageImportService
{
public:
    /**
     * @brief Builds one read-only plan for importing a page JSON and its reticle dependencies.
     * @param loaded Current authored window and document state.
     * @param files Current authored file layout tracked by the editor.
     * @param request Requested import inputs.
     * @return Pure plan describing whether the import can execute.
     */
    [[nodiscard]] PageImportPlan BuildPlan(const mfd::LoadedWindowConfiguration& loaded,
                                           const EditorFileLayout& files,
                                           const PageImportRequest& request) const;

    /**
     * @brief Applies one valid page-import plan to the live editor model.
     * @param plan Plan produced by `BuildPlan()`.
     * @param loaded Mutable authored window/document state.
     * @param files Mutable file layout mirrored by the editor.
     * @param result Optional output populated with the imported page index and staged files.
     * @param error Optional output populated when execution fails.
     * @return `true` when the plan was applied.
     */
    bool Execute(const PageImportPlan& plan,
                 mfd::LoadedWindowConfiguration& loaded,
                 EditorFileLayout& files,
                 PageImportResult* result,
                 std::string* error) const;

private:
    AssetReferenceIndexService referenceIndexService_ {};
};
} // namespace editor

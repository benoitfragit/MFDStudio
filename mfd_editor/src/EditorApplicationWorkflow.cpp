/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorApplication.h"

/**
 * @file
 * @brief Authoring workflows, popups, and file-dialog orchestration extracted from `EditorApplication`.
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

#include "internal/application/EditorApplicationInternal.h"
#include "EditorFileDialogs.h"
#include "EditorIcons.h"
#include "EditorTutorialController.h"
#include "EditorTutorialData.h"
#include "EditorUiTheme.h"

namespace
{
using editor::detail::BootstrapEditorLayersForPage;
using editor::detail::ClampFeedbackFastIntervalSeconds;
using editor::detail::ClampFeedbackHeartbeatIntervalSeconds;
using editor::detail::CopyTextBuffer;
using editor::detail::DefaultPageIndex;
using editor::detail::FindPageIndexByName;
using editor::detail::FindPageReticleIndexById;
using editor::detail::kPrimitiveTypes;
using editor::detail::kTutorialAircraftLabelPrimitiveId;
using editor::detail::kTutorialAircraftTemplateId;
using editor::detail::kTutorialPage1OwnshipAnchor;
using editor::detail::kTutorialSteeringCueTemplateId;
using editor::detail::kTutorialStrobeCursorTemplateId;
using editor::detail::MakeTutorialSteeringCueReticle;
using editor::detail::ReticleLibraryIdExistsNormalized;
using editor::detail::SuggestReplacementPageIndex;
using editor::detail::ToColorRgba;
using editor::ui::AccentButton;
using editor::ui::EditorIcon;
using editor::ui::IconButton;
using editor::ui::ShowItemTooltip;

const char* ReticleReferenceKindLabel(const editor::ReticleReferenceKind kind) noexcept
{
    switch (kind)
    {
    case editor::ReticleReferenceKind::PageStrobeTemplate:
        return "Page strobe";
    case editor::ReticleReferenceKind::PageDynamicTemplate:
        return "Page dynamic template";
    case editor::ReticleReferenceKind::PageReticleTemplate:
    default:
        return "Page reticle";
    }
}

const char* ImportDispositionLabel(const editor::ImportDisposition disposition) noexcept
{
    switch (disposition)
    {
    case editor::ImportDisposition::CopyNew:
        return "copy";
    case editor::ImportDisposition::KeepExisting:
        return "keep existing";
    case editor::ImportDisposition::RenameCopy:
        return "rename copy";
    }

    return "copy";
}

ImVec4 ImportDispositionColor(const editor::ImportDisposition disposition) noexcept
{
    switch (disposition)
    {
    case editor::ImportDisposition::CopyNew:
        return ImVec4(0.33f, 0.86f, 0.78f, 1.0f);
    case editor::ImportDisposition::KeepExisting:
        return ImVec4(0.66f, 0.78f, 0.95f, 1.0f);
    case editor::ImportDisposition::RenameCopy:
        return ImVec4(0.95f, 0.72f, 0.38f, 1.0f);
    }

    return ImVec4(0.33f, 0.86f, 0.78f, 1.0f);
}
}

void EditorApplication::OpenPageManagementPopup(const PageManagementAction action, const int pageIndex)
{
    if (documentState_.loaded.document.pages.empty() ||
        pageIndex < 0 ||
        pageIndex >= static_cast<int>(documentState_.loaded.document.pages.size()))
    {
        RebuildStatus("No page selected.", true);
        return;
    }

    workflowState_.pageManagementPopup.action = action;
    workflowState_.pageManagementPopup.openRequested = true;
    workflowState_.pageManagementPopup.pageIndex = pageIndex;
    workflowState_.pageManagementPopup.replacementPageIndex = SuggestReplacementPageIndex(documentState_.loaded.document.pages, pageIndex);
    workflowState_.pageManagementPopup.allowOutsideAssetsRoot = false;
    workflowState_.pageManagementPopup.confirmDelete = false;
}

void EditorApplication::OpenPageImportPopup(std::filesystem::path sourcePageFile)
{
    if (!HasOpenWindow())
    {
        RebuildStatus("Open or create one window before importing a page asset.", true);
        return;
    }

    workflowState_.pageImportPopup.sourcePageFile = std::move(sourcePageFile);
    workflowState_.pageImportPopup.openRequested = true;
}

void EditorApplication::OpenPageRenamePopup(const int pageIndex)
{
    if (documentState_.loaded.document.pages.empty() ||
        pageIndex < 0 ||
        pageIndex >= static_cast<int>(documentState_.loaded.document.pages.size()))
    {
        RebuildStatus("No page selected.", true);
        return;
    }

    workflowState_.pageRenamePopup.pageIndex = pageIndex;
    workflowState_.pageRenamePopup.openRequested = true;
    CopyTextBuffer(workflowState_.pageRenamePopup.newName, documentState_.loaded.document.pages[static_cast<std::size_t>(pageIndex)].name);
}

editor::PageImportRequest EditorApplication::BuildPageImportRequest(const std::filesystem::path& sourcePageFile) const
{
    return editor::PageImportRequest {
        sourcePageFile,
        documentState_.assetPaths.CurrentPageImportTargetFolder(documentState_.windowFile, documentState_.files),
        documentState_.loaded.window.reticleLibraryFolder};
}

editor::RenamePageRequest EditorApplication::BuildPageRenameRequest(const int pageIndex,
                                                                    const std::string_view newPageName) const
{
    const std::filesystem::path pageFile =
        pageIndex >= 0 && pageIndex < static_cast<int>(documentState_.files.pageFiles.size())
            ? documentState_.files.pageFiles[static_cast<std::size_t>(pageIndex)]
            : documentState_.loaded.window.sourceFile;
    return editor::RenamePageRequest {
        pageIndex,
        std::string(newPageName),
        documentState_.assetPaths.ResolveAssetRootForPath(pageFile)};
}

void EditorApplication::OpenReticleRenamePopup(std::string templateId)
{
    if (templateId.empty())
    {
        RebuildStatus("No library reticle selected.", true);
        return;
    }

    const auto iterator = documentState_.loaded.document.reticleLibrary.find(templateId);
    if (iterator == documentState_.loaded.document.reticleLibrary.end())
    {
        RebuildStatus("The selected library reticle is no longer available.", true);
        return;
    }

    workflowState_.reticleRenamePopup.currentTemplateId = std::move(templateId);
    workflowState_.reticleRenamePopup.openRequested = true;
    workflowState_.reticleRenamePopup.renameTemplateFile = true;
    CopyTextBuffer(workflowState_.reticleRenamePopup.newName, iterator->second.id.empty() ? iterator->first : iterator->second.id);
}

void EditorApplication::OpenReticleExtractionPopup()
{
    const std::vector<int> selectedIndices = SelectedPageReticleIndices();
    if (selectedIndices.empty())
    {
        RebuildStatus("Select one or more page reticles before extracting them as a reusable reticle.", true);
        return;
    }

    const mfd::PageDefinition* page = ActivePage();
    if (page == nullptr)
    {
        RebuildStatus("Select one page before extracting a reusable reticle.", true);
        return;
    }

    std::string suggestedTemplateId = "extracted_reticle";
    const int firstReticleIndex = selectedIndices.front();
    if (firstReticleIndex >= 0 && firstReticleIndex < static_cast<int>(page->staticReticles.size()))
    {
        const mfd::ReticleGroup& firstReticle = page->staticReticles[static_cast<std::size_t>(firstReticleIndex)];
        if (!firstReticle.sourceTemplateId.empty())
        {
            suggestedTemplateId = firstReticle.sourceTemplateId + "_extract";
        }
        else if (!firstReticle.id.empty())
        {
            suggestedTemplateId = firstReticle.id + "_extract";
        }
    }

    CopyTextBuffer(workflowState_.reticleExtractionPopup.templateId, suggestedTemplateId);
    if (documentState_.loaded.window.reticleLibraryFolder.empty())
    {
        CopyTextBuffer(workflowState_.reticleExtractionPopup.templateFile, "");
    }
    else
    {
        CopyTextBuffer(workflowState_.reticleExtractionPopup.templateFile,
                       editor::DefaultTemplateFilePath(documentState_.loaded.window.reticleLibraryFolder, suggestedTemplateId).string());
    }

    workflowState_.reticleExtractionPopup.openRequested = true;
}

editor::RenameReticleRequest EditorApplication::BuildReticleRenameRequest(const std::string_view oldTemplateId,
                                                                          const std::string_view newTemplateId,
                                                                          const bool renameTemplateFile) const
{
    std::filesystem::path templateFile = documentState_.loaded.window.reticleLibraryFolder;
    if (const auto iterator = documentState_.files.templateFiles.find(std::string(oldTemplateId)); iterator != documentState_.files.templateFiles.end())
    {
        templateFile = iterator->second;
    }

    return editor::RenameReticleRequest {
        std::string(oldTemplateId),
        std::string(newTemplateId),
        documentState_.assetPaths.ResolveAssetRootForPath(templateFile),
        renameTemplateFile};
}

editor::ReticleExtractionRequest EditorApplication::BuildReticleExtractionRequest() const
{
    std::filesystem::path requestedTemplateFile;
    if (workflowState_.reticleExtractionPopup.templateFile.front() != '\0')
    {
        requestedTemplateFile = std::filesystem::path(workflowState_.reticleExtractionPopup.templateFile.data()).lexically_normal();
    }

    return editor::ReticleExtractionRequest {
        documentState_.selection.pageIndex,
        SelectedPageReticleIndices(),
        workflowState_.reticleExtractionPopup.templateId.data(),
        requestedTemplateFile};
}

editor::FullscreenPreviewLayoutState EditorApplication::CaptureFullscreenPreviewLayoutState() const
{
    editor::FullscreenPreviewLayoutState state;
    state.sidebarVisible = layoutState_.sidebarVisible;
    state.inspectorVisible = layoutState_.inspectorVisible;
    state.layerInspectorVisible = layoutState_.pagePreviewViewOptions.showLayerInspector;
    state.minimapVisible = layoutState_.pagePreviewViewOptions.showMinimap;
    state.problemsVisible = layoutState_.pagePreviewViewOptions.showProblemsPanel;
    state.sidebarWidth = layoutState_.sidebarWidth;
    state.inspectorWidth = layoutState_.inspectorWidth;
    return state;
}

void EditorApplication::ApplyFullscreenPreviewLayoutState(const editor::FullscreenPreviewLayoutState& state)
{
    layoutState_.sidebarVisible = state.sidebarVisible;
    layoutState_.inspectorVisible = state.inspectorVisible;
    layoutState_.pagePreviewViewOptions.showLayerInspector = state.layerInspectorVisible;
    layoutState_.pagePreviewViewOptions.showMinimap = state.minimapVisible;
    layoutState_.pagePreviewViewOptions.showProblemsPanel = state.problemsVisible;
    layoutState_.sidebarWidth = state.sidebarWidth > 0.0f ? state.sidebarWidth : layoutState_.sidebarWidth;
    layoutState_.inspectorWidth = state.inspectorWidth > 0.0f ? state.inspectorWidth : layoutState_.inspectorWidth;
}

void EditorApplication::ToggleFullscreenPagePreview()
{
    if (!CanToggleFullscreenPagePreview())
    {
        return;
    }

    const editor::FullscreenPreviewTransition transition =
        services_.fullscreenPreview.Toggle(CaptureFullscreenPreviewLayoutState());
    if (!transition.changed)
    {
        return;
    }

    ApplyFullscreenPreviewLayoutState(transition.state);
    RebuildStatus(services_.fullscreenPreview.IsActive() ? "Fullscreen preview enabled." : "Fullscreen preview disabled.",
                  false);
}

void EditorApplication::OpenDesignExportPopup()
{
    const std::filesystem::path defaultFolder =
        documentState_.windowFile.empty() ? (documentState_.assetPaths.DefaultAssetPath("assets").parent_path() / "MFDStudioDesignExport")
                            : (documentState_.windowFile.parent_path() / std::filesystem::path("MFDStudioDesignExport"));
    CopyTextBuffer(workflowState_.designExportPopup.outputFolder, defaultFolder.lexically_normal().string());
    workflowState_.designExportPopup.exportCompleted = false;
    workflowState_.designExportPopup.exportedFolder.clear();
    workflowState_.designExportPopup.warnings.clear();
    workflowState_.designExportPopup.openRequested = true;
}

editor::DesignExportRequest EditorApplication::BuildDesignExportRequest() const
{
    editor::DesignExportRequest request;
    request.outputFolder = std::filesystem::path(workflowState_.designExportPopup.outputFolder.data()).lexically_normal();
    request.windowFile = documentState_.windowFile;
    request.loaded = HasOpenWindow() ? &documentState_.loaded : nullptr;
    request.files = HasOpenWindow() ? &documentState_.files : nullptr;
    request.exportMarkdownIcd = workflowState_.designExportPopup.exportMarkdownIcd;
    request.exportExplodedViews = workflowState_.designExportPopup.exportExplodedViews;
    request.includeCanvasCoordinates = workflowState_.designExportPopup.includeCanvasCoordinates;
    request.includeCppSnippets = workflowState_.designExportPopup.includeCppSnippets;
    request.includeStrobe = workflowState_.designExportPopup.includeStrobe;
    request.includeBlink = workflowState_.designExportPopup.includeBlink;
    request.includePrimitiveIds = workflowState_.designExportPopup.includePrimitiveIds;
    request.includeMappingHash = workflowState_.designExportPopup.includeMappingHash;
    return request;
}

bool EditorApplication::ExecuteDesignExportPlan(const editor::DesignExportPlan& plan)
{
    if (!plan.canExecute)
    {
        RebuildStatus(plan.error.empty() ? "The design export cannot execute." : plan.error, true);
        return false;
    }

    const editor::DesignExportResult result = services_.designExport.Execute(plan);
    workflowState_.designExportPopup.warnings = result.warnings;
    workflowState_.designExportPopup.exportCompleted = true;
    workflowState_.designExportPopup.exportedFolder = result.outputFolder;

    const bool hasErrors = !result.warnings.empty() &&
                           !std::filesystem::exists(plan.readmeFile) &&
                           !std::filesystem::exists(plan.windowIcdFile);
    if (hasErrors)
    {
        RebuildStatus("Design export failed. Review the popup warnings.", true);
        return false;
    }

    RebuildStatus("Design export created in '" + result.outputFolder.string() + "'.", false);
    return true;
}

bool EditorApplication::ExecutePageRemovePlan(const editor::PageRemovePlan& plan)
{
    if (!plan.canExecute)
    {
        RebuildStatus(plan.error.empty() ? "The page cannot be removed from the current window." : plan.error, true);
        return false;
    }

    UndoSnapshot undoSnapshot {documentState_.loaded, documentState_.files, documentState_.selection, layoutState_.pagePreviewView, layoutState_.libraryPreviewView};
    std::string error;
    if (!services_.pageManagement.Execute(plan, documentState_.loaded, documentState_.files, &error))
    {
        RebuildStatus("Remove page failed: " + error, true);
        return false;
    }
    PushUndoSnapshot(std::move(undoSnapshot));

    if (documentState_.loaded.document.pages.empty())
    {
        documentState_.selection = {};
    }
    else
    {
        documentState_.selection.pageIndex = plan.nextSelectedPageIndex;
        documentState_.selection.pageReticleIndex = -1;
        documentState_.selection.pageReticleIndices.clear();
        SelectPage(plan.nextSelectedPageIndex);
    }

    if (plan.replacementPageName.empty())
    {
        RebuildStatus("Page '" + plan.pageName + "' removed from the current window.", false);
    }
    else
    {
        RebuildStatus("Page '" + plan.pageName + "' removed. Default page switched to '" +
                          plan.replacementPageName + "'.",
                      false);
    }

    InvalidateReticleUsageHighlightCache();
    return true;
}

bool EditorApplication::ExecutePageImportPlan(const editor::PageImportPlan& plan)
{
    if (!plan.canExecute)
    {
        RebuildStatus(plan.error.empty() ? "The selected page cannot be imported." : plan.error, true);
        return false;
    }

    UndoSnapshot undoSnapshot {documentState_.loaded, documentState_.files, documentState_.selection, layoutState_.pagePreviewView, layoutState_.libraryPreviewView};
    editor::PageImportResult result;
    std::string error;
    if (!services_.pageImport.Execute(plan, documentState_.loaded, documentState_.files, &result, &error))
    {
        RebuildStatus(error.empty() ? "Importing the selected page failed." : error, true);
        return false;
    }
    PushUndoSnapshot(std::move(undoSnapshot));

    SelectPage(result.importedPageIndex);
    std::string status = "Page '" + result.pageName + "' imported";
    if (!result.importedTemplateIds.empty())
    {
        status += " with " + std::to_string(result.importedTemplateIds.size()) + " reticle template";
        if (result.importedTemplateIds.size() != 1U)
        {
            status += "s";
        }
    }
    status += ". Use File > Save to persist the staged JSON assets.";
    RebuildStatus(status, false);
    InvalidateReticleUsageHighlightCache();
    return true;
}

bool EditorApplication::ExecutePageRenamePlan(const editor::RenamePagePlan& plan)
{
    if (!plan.canExecute)
    {
        RebuildStatus(plan.error.empty() ? "The selected page cannot be renamed globally." : plan.error, true);
        return false;
    }

    editor::RenamePageResult result;
    std::string error;
    if (!services_.pageRename.Execute(plan, documentState_.loaded, documentState_.files, &result, &error))
    {
        RebuildStatus(error.empty() ? "Renaming the selected page globally failed." : error, true);
        return false;
    }

    std::string status = "Page '" + plan.oldPageName + "' renamed to '" + plan.newPageName + "'";
    if (!plan.references.empty())
    {
        status += " across " + std::to_string(plan.references.size()) + " window reference";
        if (plan.references.size() != 1U)
        {
            status += "s";
        }
    }
    if (result.updatedWindowCount > 0U)
    {
        status += " with " + std::to_string(result.updatedWindowCount) + " window JSON update";
        if (result.updatedWindowCount != 1U)
        {
            status += "s";
        }
    }
    status += ". Regenerate the generated client API if this page is exposed there.";
    RebuildStatus(status, false);
    InvalidateReticleUsageHighlightCache();
    return true;
}

bool EditorApplication::ExecuteReticleRenamePlan(const editor::RenameReticlePlan& plan)
{
    if (!plan.canExecute)
    {
        RebuildStatus(plan.error.empty() ? "The selected reticle template cannot be renamed globally." : plan.error, true);
        return false;
    }

    editor::RenameReticleResult result;
    std::string error;
    if (!services_.reticleRename.Execute(plan, documentState_.loaded, documentState_.files, &result, &error))
    {
        RebuildStatus(error.empty() ? "Renaming the selected reticle template globally failed." : error, true);
        return false;
    }

    if (mfd::PageNamesEqual(documentState_.selection.libraryReticleId, plan.oldReticleName))
    {
        documentState_.selection.libraryReticleId = plan.newReticleName;
    }
    if (mfd::PageNamesEqual(documentState_.selection.libraryBrowserReticleId, plan.oldReticleName))
    {
        documentState_.selection.libraryBrowserReticleId = plan.newReticleName;
    }

    std::string status = "Reticle template '" + plan.oldReticleName + "' renamed to '" + plan.newReticleName + "'";
    if (!plan.references.empty())
    {
        status += " across " + std::to_string(plan.references.size()) + " page reference";
        if (plan.references.size() != 1U)
        {
            status += "s";
        }
    }
    if (result.updatedPageCount > 0U)
    {
        status += " with " + std::to_string(result.updatedPageCount) + " page JSON update";
        if (result.updatedPageCount != 1U)
        {
            status += "s";
        }
    }
    if (result.renamedTemplateFile)
    {
        status += " and one template file move";
    }
    status += ". Regenerate the generated client API if this template is exposed there.";
    RebuildStatus(status, false);
    InvalidateReticleUsageHighlightCache();
    return true;
}

bool EditorApplication::ExecuteReticleExtractionPlan(const editor::ReticleExtractionPlan& plan)
{
    if (!plan.canExecute)
    {
        RebuildStatus(plan.error.empty() ? "The current page-reticle selection cannot be extracted yet." : plan.error, true);
        return false;
    }

    UndoSnapshot undoSnapshot {documentState_.loaded, documentState_.files, documentState_.selection, layoutState_.pagePreviewView, layoutState_.libraryPreviewView};
    editor::ReticleExtractionResult result;
    std::string error;
    if (!services_.reticleExtraction.Execute(plan, documentState_.loaded, documentState_.files, &result, &error))
    {
        RebuildStatus(error.empty() ? "Extracting the selected page reticles failed." : error, true);
        return false;
    }
    PushUndoSnapshot(std::move(undoSnapshot));

    SelectPageReticle(plan.pageIndex, result.insertedReticleIndex);
    InvalidateReticleUsageHighlightCache();

    std::string status = "Extracted " + std::to_string(result.extractedPrimitiveCount) + " primitive";
    if (result.extractedPrimitiveCount != 1U)
    {
        status += "s";
    }
    status += " into reticle template '" + result.templateId + "'. Use File > Save to persist the staged JSON asset.";
    RebuildStatus(status, false);
    return true;
}

bool EditorApplication::ExecutePageDeletePlan(const editor::PageDeletePlan& plan)
{
    if (!plan.canExecute)
    {
        RebuildStatus(plan.error.empty() ? "The page asset cannot be deleted." : plan.error, true);
        return false;
    }

    UndoSnapshot undoSnapshot {documentState_.loaded, documentState_.files, documentState_.selection, layoutState_.pagePreviewView, layoutState_.libraryPreviewView};
    std::string error;
    if (!services_.pageManagement.Execute(plan, documentState_.loaded, documentState_.files, &error))
    {
        RebuildStatus("Delete page failed: " + error, true);
        return false;
    }
    PushUndoSnapshot(std::move(undoSnapshot));

    if (documentState_.loaded.document.pages.empty())
    {
        documentState_.selection = {};
    }
    else
    {
        documentState_.selection.pageIndex = plan.nextSelectedPageIndex;
        documentState_.selection.pageReticleIndex = -1;
        documentState_.selection.pageReticleIndices.clear();
        SelectPage(plan.nextSelectedPageIndex);
    }

    if (plan.replacementPageName.empty())
    {
        RebuildStatus("Page '" + plan.pageName + "' removed and marked for deletion on the next save.", false);
    }
    else
    {
        RebuildStatus("Page '" + plan.pageName + "' removed and marked for deletion. Default page switched to '" +
                          plan.replacementPageName + "'.",
                      false);
    }

    return true;
}

void EditorApplication::OpenNewPagePopup()
{
    SeedNewPageAssetDraftPath();
    workflowState_.showNewPagePopup = true;
}

void EditorApplication::OpenNewWindowPopup()
{
    SeedNewWindowAssetDraftPaths();
    workflowState_.showNewWindowPopup = true;
}

bool EditorApplication::OpenWindowAssetFromFileExplorer()
{
    const std::filesystem::path initialFolder =
        HasOpenWindow() && documentState_.windowFile.has_parent_path() ? documentState_.windowFile.parent_path()
                                                         : documentState_.assetPaths.DefaultAssetPath("assets/windows");
    std::string error;
    const std::optional<std::filesystem::path> selectedFile = editor::OpenWindowAssetFileDialog(initialFolder, &error);
    if (!selectedFile.has_value())
    {
        if (!error.empty())
        {
            RebuildStatus(error, true);
        }
        return false;
    }

    return LoadWindowConfiguration(*selectedFile);
}

bool EditorApplication::OpenPageAssetImportFromFileExplorer()
{
    if (!HasOpenWindow())
    {
        RebuildStatus("Open or create one window before importing a page asset.", true);
        return false;
    }

    const std::filesystem::path initialFolder = documentState_.assetPaths.CurrentPageImportTargetFolder(documentState_.windowFile, documentState_.files);
    std::string error;
    const std::optional<std::filesystem::path> selectedFile = editor::OpenPageAssetFileDialog(initialFolder, &error);
    if (!selectedFile.has_value())
    {
        if (!error.empty())
        {
            RebuildStatus(error, true);
        }
        return false;
    }

    OpenPageImportPopup(*selectedFile);
    return true;
}

void EditorApplication::BrowseNewWindowFile()
{
    const std::filesystem::path currentFile =
        std::filesystem::path(workflowState_.newWindowDraft.windowFile.data()).lexically_normal();
    const std::filesystem::path suggestedFile =
        currentFile.empty() ? documentState_.assetPaths.DefaultAssetPath("assets/windows/new_window.json") : currentFile;

    std::string error;
    const std::optional<std::filesystem::path> selectedFile =
        editor::SaveJsonAssetFileDialog(suggestedFile, "Select MFD window JSON file", &error);
    if (selectedFile.has_value())
    {
        CopyTextBuffer(workflowState_.newWindowDraft.windowFile, selectedFile->lexically_normal().string());
    }
    else if (!error.empty())
    {
        RebuildStatus(error, true);
    }
    workflowState_.showNewWindowPopup = true;
}

void EditorApplication::BrowseNewWindowFontFile()
{
    const std::filesystem::path currentFile =
        std::filesystem::path(workflowState_.newWindowDraft.fontFile.data()).lexically_normal();
    const std::filesystem::path initialFolder =
        currentFile.empty() ? documentState_.assetPaths.DefaultAssetPath("assets/fonts")
                            : editor::EditorAssetPathService::ConfiguredPathFolder(currentFile);

    std::string error;
    const std::optional<std::filesystem::path> selectedFile = editor::OpenFontAssetFileDialog(initialFolder, &error);
    if (selectedFile.has_value())
    {
        CopyTextBuffer(workflowState_.newWindowDraft.fontFile, selectedFile->lexically_normal().string());
    }
    else if (!error.empty())
    {
        RebuildStatus(error, true);
    }
    workflowState_.showNewWindowPopup = true;
}

void EditorApplication::BrowseNewWindowReticleLibraryFolder()
{
    std::filesystem::path initialFolder = editor::EditorAssetPathService::ConfiguredPathFolder(
        std::filesystem::path(workflowState_.newWindowDraft.reticleLibraryFolder.data()).lexically_normal());
    if (initialFolder.empty())
    {
        initialFolder = documentState_.assetPaths.DefaultAssetPath("assets/reticles");
    }

    std::string error;
    const std::optional<std::filesystem::path> selectedFolder =
        editor::OpenFolderDialog(initialFolder, "Select reticle library folder", &error);
    if (selectedFolder.has_value())
    {
        CopyTextBuffer(workflowState_.newWindowDraft.reticleLibraryFolder, selectedFolder->lexically_normal().string());
    }
    else if (!error.empty())
    {
        RebuildStatus(error, true);
    }
    workflowState_.showNewWindowPopup = true;
}

void EditorApplication::BrowseNewWindowFirstPageFile()
{
    const std::filesystem::path currentFile =
        std::filesystem::path(workflowState_.newWindowDraft.firstPageFile.data()).lexically_normal();
    const std::filesystem::path suggestedFile =
        currentFile.empty() ? documentState_.assetPaths.DefaultAssetPath("assets/pages/page1.json") : currentFile;

    std::string error;
    const std::optional<std::filesystem::path> selectedFile =
        editor::SaveJsonAssetFileDialog(suggestedFile, "Select initial page JSON file", &error);
    if (selectedFile.has_value())
    {
        CopyTextBuffer(workflowState_.newWindowDraft.firstPageFile, selectedFile->lexically_normal().string());
    }
    else if (!error.empty())
    {
        RebuildStatus(error, true);
    }
    workflowState_.showNewWindowPopup = true;
}

void EditorApplication::BrowseNewPageFile()
{
    const std::filesystem::path currentFile = std::filesystem::path(workflowState_.newPageDraft.fileName.data()).lexically_normal();
    const std::filesystem::path suggestedFile =
        currentFile.empty() ? documentState_.assetPaths.DefaultAssetPath("assets/pages/new_page.json") : currentFile;

    std::string error;
    const std::optional<std::filesystem::path> selectedFile =
        editor::SaveJsonAssetFileDialog(suggestedFile, "Select page JSON file", &error);
    if (selectedFile.has_value())
    {
        CopyTextBuffer(workflowState_.newPageDraft.fileName, selectedFile->lexically_normal().string());
    }
    else if (!error.empty())
    {
        RebuildStatus(error, true);
    }
    workflowState_.showNewPagePopup = true;
}

void EditorApplication::BrowseWindowFontFile()
{
    if (!HasOpenWindow())
    {
        return;
    }

    const std::filesystem::path currentFile = documentState_.loaded.window.fontFile;
    const std::filesystem::path initialFolder =
        currentFile.empty() ? documentState_.assetPaths.DefaultAssetPath("assets/fonts")
                            : editor::EditorAssetPathService::ConfiguredPathFolder(currentFile);

    std::string error;
    const std::optional<std::filesystem::path> selectedFile = editor::OpenFontAssetFileDialog(initialFolder, &error);
    if (selectedFile.has_value())
    {
        PushUndoSnapshot();
        documentState_.loaded.window.fontFile = selectedFile->lexically_normal();
        ApplyPreviewFontFile(documentState_.loaded.window.fontFile);
    }
    else if (!error.empty())
    {
        RebuildStatus(error, true);
    }
}

void EditorApplication::BrowseWindowReticleLibraryFolder()
{
    if (!HasOpenWindow())
    {
        return;
    }

    std::filesystem::path initialFolder =
        editor::EditorAssetPathService::ConfiguredPathFolder(documentState_.loaded.window.reticleLibraryFolder);
    if (initialFolder.empty())
    {
        initialFolder = documentState_.assetPaths.DefaultAssetPath("assets/reticles");
    }

    std::string error;
    const std::optional<std::filesystem::path> selectedFolder =
        editor::OpenFolderDialog(initialFolder, "Select reticle library folder", &error);
    if (selectedFolder.has_value())
    {
        PushUndoSnapshot();
        documentState_.loaded.window.reticleLibraryFolder = selectedFolder->lexically_normal();
    }
    else if (!error.empty())
    {
        RebuildStatus(error, true);
    }
}

void EditorApplication::BrowseSelectedPrimitiveImageFile()
{
    mfd::Primitive* primitive = SelectedLibraryPrimitive();
    if (primitive == nullptr)
    {
        return;
    }

    auto* image = std::get_if<mfd::ImageGeometry>(&primitive->geometry);
    if (image == nullptr)
    {
        return;
    }

    const std::filesystem::path initialFolder =
        image->file.empty() ? documentState_.assetPaths.DefaultAssetPath("assets")
                            : editor::EditorAssetPathService::ConfiguredPathFolder(image->file);

    std::string error;
    const std::optional<std::filesystem::path> selectedFile = editor::OpenImageAssetFileDialog(initialFolder, &error);
    if (selectedFile.has_value())
    {
        PushUndoSnapshot();
        image->file = selectedFile->lexically_normal();
    }
    else if (!error.empty())
    {
        RebuildStatus(error, true);
    }
}

void EditorApplication::OpenNewLibraryReticlePopup()
{
    workflowState_.showNewLibraryReticlePopup = true;
}

void EditorApplication::OpenDuplicateLibraryReticlePopup()
{
    const mfd::ReticleGroup* reticle = SelectedLibraryReticle();
    if (reticle == nullptr)
    {
        return;
    }

    CopyTextBuffer(workflowState_.duplicateLibraryReticleDraft.id, MakeUniqueLibraryReticleId(reticle->id + "_copy"));
    workflowState_.showDuplicateLibraryReticlePopup = true;
}

void EditorApplication::DrawPageImportPopup()
{
    if (!ImGui::BeginPopupModal("Import page", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    const editor::PageImportPlan plan =
        services_.pageImport.BuildPlan(documentState_.loaded, documentState_.files, BuildPageImportRequest(workflowState_.pageImportPopup.sourcePageFile));

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Import page asset");
    ImGui::TextWrapped("Review the staged page and reticle-template import before it is added to the current window.");
    ImGui::Separator();

    ImGui::TextDisabled("Source");
    ImGui::TextWrapped("%s", plan.sourcePageFile.empty() ? "<none>" : plan.sourcePageFile.string().c_str());

    if (!plan.sourcePageName.empty())
    {
        ImGui::TextDisabled("Page name");
        if (mfd::PageNamesEqual(plan.sourcePageName, plan.targetPageName))
        {
            ImGui::TextWrapped("%s", plan.sourcePageName.c_str());
        }
        else
        {
            ImGui::TextWrapped("%s -> %s", plan.sourcePageName.c_str(), plan.targetPageName.c_str());
        }
    }

    if (!plan.targetPageFile.empty())
    {
        ImGui::TextDisabled("Target page file");
        ImGui::TextWrapped("%s", plan.targetPageFile.string().c_str());
        ImGui::TextColored(ImportDispositionColor(plan.pageDisposition),
                           "Page policy: %s",
                           ImportDispositionLabel(plan.pageDisposition));
    }

    ImGui::SeparatorText("Reticle dependencies");
    if (plan.reticles.empty())
    {
        ImGui::TextDisabled("No reticle template dependency detected for this page.");
    }
    else
    {
        for (const editor::ReticleImportPlan& reticlePlan : plan.reticles)
        {
            ImGui::PushID(reticlePlan.sourceTemplateId.c_str());
            ImGui::TextColored(ImportDispositionColor(reticlePlan.disposition),
                               "[%s] %s",
                               ImportDispositionLabel(reticlePlan.disposition),
                               reticlePlan.sourceTemplateId.c_str());
            if (mfd::PageNamesEqual(reticlePlan.sourceTemplateId, reticlePlan.targetTemplateId))
            {
                ImGui::TextWrapped("Target: %s", reticlePlan.targetFile.string().c_str());
            }
            else
            {
                ImGui::TextWrapped("%s -> %s", reticlePlan.sourceTemplateId.c_str(), reticlePlan.targetTemplateId.c_str());
                ImGui::TextWrapped("Target: %s", reticlePlan.targetFile.string().c_str());
            }
            ImGui::PopID();
        }
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Imported assets are staged in memory first and written with File > Save.");
    ImGui::TextDisabled("Deterministic collision policy: missing target = copy, identical target = keep existing, different target = rename copy.");

    if (!plan.error.empty())
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.42f, 1.0f), "%s", plan.error.c_str());
    }

    ImGui::Spacing();
    ImGui::BeginDisabled(!plan.canExecute);
    if (AccentButton("Import page"))
    {
        if (ExecutePageImportPlan(plan))
        {
            workflowState_.pageImportPopup = {};
            ImGui::CloseCurrentPopup();
            ImGui::EndDisabled();
            ImGui::EndPopup();
            return;
        }
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        workflowState_.pageImportPopup = {};
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void EditorApplication::DrawPageRenamePopup()
{
    if (!ImGui::BeginPopupModal("Rename page globally", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    const bool pageIndexValid = workflowState_.pageRenamePopup.pageIndex >= 0 &&
                                workflowState_.pageRenamePopup.pageIndex < static_cast<int>(documentState_.loaded.document.pages.size());
    if (!pageIndexValid)
    {
        ImGui::TextWrapped("The selected page is no longer available.");
        if (ImGui::Button("Close"))
        {
            workflowState_.pageRenamePopup = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    const mfd::PageDefinition& page = documentState_.loaded.document.pages[static_cast<std::size_t>(workflowState_.pageRenamePopup.pageIndex)];

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Rename page globally");
    ImGui::TextWrapped("Review every shared window reference before renaming this page asset across the current scanned asset tree.");
    ImGui::Separator();

    ImGui::TextDisabled("Old name");
    ImGui::TextWrapped("%s", page.name.c_str());

    ImGui::InputText("New name", workflowState_.pageRenamePopup.newName.data(), workflowState_.pageRenamePopup.newName.size());
    ShowItemTooltip("Use the safe rename workflow to update the page JSON and every scanned window defaultPage reference consistently.");

    const editor::RenamePagePlan plan =
        services_.pageRename.BuildPlan(documentState_.loaded,
                                     documentState_.files,
                                     BuildPageRenameRequest(workflowState_.pageRenamePopup.pageIndex, workflowState_.pageRenamePopup.newName.data()));

    ImGui::SeparatorText("References found");
    if (plan.references.empty())
    {
        ImGui::TextDisabled("No scanned window reference is currently eligible for this rename.");
    }
    else
    {
        for (const editor::RenamePageReference& reference : plan.references)
        {
            ImGui::PushID(reference.windowFile.string().c_str());
            ImGui::TextWrapped("%s", reference.windowFile.string().c_str());
            if (reference.updatesDefaultPage)
            {
                ImGui::TextDisabled("defaultPage will be updated in this window JSON");
            }
            ImGui::PopID();
        }
    }

    ImGui::SeparatorText("Files to modify");
    for (const std::filesystem::path& file : plan.filesToModify)
    {
        ImGui::TextWrapped("%s", file.string().c_str());
    }
    if (plan.filesToModify.empty())
    {
        ImGui::TextDisabled("No file would be rewritten.");
    }

    if (!plan.collisions.empty())
    {
        ImGui::SeparatorText("Collisions");
        for (const editor::RenamePageCollision& collision : plan.collisions)
        {
            ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.42f, 1.0f),
                               "%s already exposes page '%s'",
                               collision.windowFile.string().c_str(),
                               collision.conflictingPageName.c_str());
        }
    }

    if (!plan.warnings.empty())
    {
        ImGui::SeparatorText("Warnings");
        for (const std::string& warning : plan.warnings)
        {
            ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.38f, 1.0f), "%s", warning.c_str());
        }
    }

    ImGui::Spacing();
    ImGui::TextDisabled("This workflow updates the scanned JSON assets directly across the current asset tree.");
    ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.38f, 1.0f),
                       "These disk changes are not covered by Ctrl+Z once the rename starts.");

    if (!plan.error.empty())
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.42f, 1.0f), "%s", plan.error.c_str());
    }

    ImGui::Spacing();
    ImGui::BeginDisabled(!plan.canExecute);
    if (AccentButton("Rename page"))
    {
        if (ExecutePageRenamePlan(plan))
        {
            workflowState_.pageRenamePopup = {};
            ImGui::CloseCurrentPopup();
            ImGui::EndDisabled();
            ImGui::EndPopup();
            return;
        }
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        if (tutorial_->MatchesTarget("popup_page_rename_cancel"))
        {
            tutorial_->CompleteStep();
        }
        workflowState_.pageRenamePopup = {};
        ImGui::CloseCurrentPopup();
    }
    tutorial_->DrawHalo(
        "popup_page_rename_cancel",
        "Close the rename popup",
        "This discovery step only inspects the safe page-rename workflow. Close it without executing the rename.");

    ImGui::EndPopup();
}

void EditorApplication::DrawReticleRenamePopup()
{
    if (!ImGui::BeginPopupModal("Rename reticle globally", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    const auto reticleIterator = documentState_.loaded.document.reticleLibrary.find(workflowState_.reticleRenamePopup.currentTemplateId);
    if (reticleIterator == documentState_.loaded.document.reticleLibrary.end())
    {
        ImGui::TextWrapped("The selected reticle template is no longer available.");
        if (ImGui::Button("Close"))
        {
            workflowState_.reticleRenamePopup = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    const mfd::ReticleGroup& reticle = reticleIterator->second;

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Rename reticle globally");
    ImGui::TextWrapped(
        "Pages reference reticle templates by logical id. Review every scanned page reference before renaming this shared template across the current asset tree.");
    ImGui::Separator();

    ImGui::TextDisabled("Old id");
    ImGui::TextWrapped("%s", reticle.id.empty() ? reticleIterator->first.c_str() : reticle.id.c_str());

    ImGui::InputText("New id", workflowState_.reticleRenamePopup.newName.data(), workflowState_.reticleRenamePopup.newName.size());
    ShowItemTooltip("Use the safe rename workflow to update the template JSON id and every scanned page template reference consistently.");

    ImGui::Checkbox("Rename template JSON file too", &workflowState_.reticleRenamePopup.renameTemplateFile);
    ShowItemTooltip("Also move the template JSON file to the default file name derived from the new template id.");

    const editor::RenameReticlePlan plan =
        services_.reticleRename.BuildPlan(documentState_.loaded,
                                        documentState_.files,
                                        BuildReticleRenameRequest(
                                            workflowState_.reticleRenamePopup.currentTemplateId,
                                            workflowState_.reticleRenamePopup.newName.data(),
                                            workflowState_.reticleRenamePopup.renameTemplateFile));

    ImGui::SeparatorText("Template file");
    if (plan.currentTemplateFile.empty())
    {
        ImGui::TextDisabled("No tracked template file is currently available.");
    }
    else
    {
        ImGui::TextWrapped("Current: %s", plan.currentTemplateFile.string().c_str());
        if (workflowState_.reticleRenamePopup.renameTemplateFile)
        {
            ImGui::TextWrapped("Target: %s", plan.targetTemplateFile.string().c_str());
        }
        else
        {
            ImGui::TextDisabled("Logical rename only: keep the current template JSON file path.");
        }
    }

    ImGui::SeparatorText("References found");
    if (plan.references.empty())
    {
        ImGui::TextDisabled("No scanned page currently references this template. Only the template JSON will be rewritten.");
    }
    else
    {
        for (const editor::RenameReticleReference& reference : plan.references)
        {
            ImGui::PushID((reference.pageFile.string() + reference.ownerReticleId).c_str());
            ImGui::TextWrapped("%s", reference.pageFile.string().c_str());
            ImGui::TextDisabled("%s '%s' on page '%s'",
                                ReticleReferenceKindLabel(reference.kind),
                                reference.ownerReticleId.c_str(),
                                reference.pageName.c_str());
            ImGui::PopID();
        }
    }

    ImGui::SeparatorText("Files to modify");
    for (const std::filesystem::path& file : plan.filesToModify)
    {
        ImGui::TextWrapped("%s", file.string().c_str());
    }
    if (plan.filesToModify.empty())
    {
        ImGui::TextDisabled("No file would be rewritten.");
    }

    if (!plan.filesToDelete.empty())
    {
        ImGui::SeparatorText("Files to delete");
        for (const std::filesystem::path& file : plan.filesToDelete)
        {
            ImGui::TextWrapped("%s", file.string().c_str());
        }
    }

    if (!plan.collisions.empty())
    {
        ImGui::SeparatorText("Collisions");
        for (const editor::RenameReticleCollision& collision : plan.collisions)
        {
            ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.42f, 1.0f),
                               "%s already uses template '%s' through %s '%s' on page '%s'",
                               collision.pageFile.string().c_str(),
                               collision.conflictingTemplateId.c_str(),
                               ReticleReferenceKindLabel(collision.kind),
                               collision.conflictingReticleId.c_str(),
                               collision.pageName.c_str());
        }
    }

    if (!plan.warnings.empty())
    {
        ImGui::SeparatorText("Warnings");
        for (const std::string& warning : plan.warnings)
        {
            ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.38f, 1.0f), "%s", warning.c_str());
        }
    }

    ImGui::Spacing();
    ImGui::TextDisabled("This workflow updates the scanned JSON assets directly across the current asset tree.");
    ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.38f, 1.0f),
                       "These disk changes are not covered by Ctrl+Z once the rename starts.");
    ImGui::TextDisabled("After a successful rename, regenerate the generated client API if this template is exposed there.");

    if (!plan.error.empty())
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.42f, 1.0f), "%s", plan.error.c_str());
    }

    ImGui::Spacing();
    ImGui::BeginDisabled(!plan.canExecute);
    if (AccentButton("Rename reticle"))
    {
        if (ExecuteReticleRenamePlan(plan))
        {
            workflowState_.reticleRenamePopup = {};
            ImGui::CloseCurrentPopup();
            ImGui::EndDisabled();
            ImGui::EndPopup();
            return;
        }
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        if (tutorial_->MatchesTarget("popup_reticle_rename_cancel"))
        {
            tutorial_->CompleteStep();
        }
        workflowState_.reticleRenamePopup = {};
        ImGui::CloseCurrentPopup();
    }
    tutorial_->DrawHalo(
        "popup_reticle_rename_cancel",
        "Close the rename popup",
        "This discovery step only inspects the safe reticle-rename workflow. Close it without executing the rename.");

    ImGui::EndPopup();
}

void EditorApplication::DrawReticleExtractionPopup()
{
    if (!ImGui::BeginPopupModal("Extract as reticle", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Extract a reusable reticle");
    ImGui::TextWrapped(
        "Review the current page-reticle selection before replacing it with one reusable library template. The visual result should stay identical or nearly identical after extraction.");
    ImGui::Separator();

    ImGui::InputText("Template id", workflowState_.reticleExtractionPopup.templateId.data(), workflowState_.reticleExtractionPopup.templateId.size());
    ShowItemTooltip("Logical id assigned to the new shared reticle template. Planning resolves collisions automatically.");
    ImGui::InputText("Template file", workflowState_.reticleExtractionPopup.templateFile.data(), workflowState_.reticleExtractionPopup.templateFile.size());
    ShowItemTooltip("Optional JSON file path for the new shared template. Clear it to let the editor derive the default file from the target template id.");

    const editor::ReticleExtractionPlan plan =
        services_.reticleExtraction.BuildPlan(documentState_.loaded, documentState_.files, BuildReticleExtractionRequest());

    ImGui::SeparatorText("Selection");
    if (plan.sourceReticleIds.empty())
    {
        ImGui::TextDisabled("No page reticle is currently selected.");
    }
    else
    {
        for (const std::string& reticleId : plan.sourceReticleIds)
        {
            ImGui::BulletText("%s", reticleId.c_str());
        }
    }

    ImGui::SeparatorText("Extraction result");
    if (plan.targetTemplateId.empty())
    {
        ImGui::TextDisabled("No target template id is currently available.");
    }
    else
    {
        ImGui::TextWrapped("Template id: %s", plan.targetTemplateId.c_str());
        if (plan.templateIdAdjusted)
        {
            ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.38f, 1.0f),
                               "The requested id collides with the current library. The editor will stage '%s' instead.",
                               plan.targetTemplateId.c_str());
        }
    }

    if (plan.targetTemplateFile.empty())
    {
        ImGui::TextDisabled("No target template file is currently available.");
    }
    else
    {
        ImGui::TextWrapped("Template file: %s", plan.targetTemplateFile.string().c_str());
        if (plan.templateFileAdjusted)
        {
            ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.38f, 1.0f),
                               "The requested file collides with an existing staged or on-disk asset. A unique file name will be used.");
        }
    }

    ImGui::Text("Flattened primitives: %d", static_cast<int>(plan.extractedPrimitiveCount));
    ImGui::Text("Replacement reticle id: %s",
                plan.replacementInstanceId.empty() ? "<pending>" : plan.replacementInstanceId.c_str());
    ImGui::Text("Editor layer: %s", plan.targetLayerId.empty() ? "<none>" : plan.targetLayerId.c_str());
    ImGui::TextDisabled("Draw order: %s", plan.drawOnTop ? "draw on top" : "regular page reticle order");

    ImGui::Spacing();
    ImGui::TextDisabled("The new template is staged in memory first, then written with File > Save.");
    ImGui::TextDisabled("Unsupported cases are rejected here instead of partially mutating the page.");

    if (!plan.error.empty())
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.42f, 1.0f), "%s", plan.error.c_str());
    }

    ImGui::Spacing();
    ImGui::BeginDisabled(!plan.canExecute);
    if (AccentButton("Extract reticle"))
    {
        if (ExecuteReticleExtractionPlan(plan))
        {
            workflowState_.reticleExtractionPopup = {};
            ImGui::CloseCurrentPopup();
            ImGui::EndDisabled();
            ImGui::EndPopup();
            return;
        }
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        if (tutorial_->MatchesTarget("popup_reticle_extract_cancel"))
        {
            tutorial_->CompleteStep();
        }
        workflowState_.reticleExtractionPopup = {};
        ImGui::CloseCurrentPopup();
    }
    tutorial_->DrawHalo(
        "popup_reticle_extract_cancel",
        "Close the extraction popup",
        "This discovery step only inspects the extraction workflow. Close it without replacing the selected reticle.");

    ImGui::EndPopup();
}

void EditorApplication::DrawDesignExportPopup()
{
    if (!ImGui::BeginPopupModal("Export design", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    if (!HasOpenWindow())
    {
        ImGui::TextWrapped("Open one window before exporting design documentation.");
        if (ImGui::Button("Close"))
        {
            workflowState_.designExportPopup = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Export design");
    ImGui::TextWrapped(
        "Generate Markdown ICD files and exploded designer views for the currently loaded window without modifying authored JSON assets.");
    ImGui::Separator();

    if (workflowState_.designExportPopup.exportCompleted)
    {
        ImGui::TextWrapped("Design export created:");
        ImGui::TextWrapped("%s", workflowState_.designExportPopup.exportedFolder.string().c_str());

        if (!workflowState_.designExportPopup.warnings.empty())
        {
            ImGui::SeparatorText("Warnings");
            for (const std::string& warning : workflowState_.designExportPopup.warnings)
            {
                ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.38f, 1.0f), "%s", warning.c_str());
            }
        }

        if (AccentButton("Open folder"))
        {
            std::string error;
            if (!editor::OpenFolderInFileExplorer(workflowState_.designExportPopup.exportedFolder, &error))
            {
                RebuildStatus(error.empty() ? "Opening the export folder failed." : error, true);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Close"))
        {
            workflowState_.designExportPopup = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    ImGui::InputText("Output folder", workflowState_.designExportPopup.outputFolder.data(), workflowState_.designExportPopup.outputFolder.size());
    ImGui::SameLine();
    if (IconButton("##browse_design_output", EditorIcon::Search, "Browse for the design export output folder."))
    {
        std::string dialogError;
        const std::filesystem::path initialFolder =
            workflowState_.designExportPopup.outputFolder.front() == '\0' ? documentState_.windowFile.parent_path()
                                                            : std::filesystem::path(workflowState_.designExportPopup.outputFolder.data());
        if (const auto selectedFolder =
                editor::OpenFolderDialog(initialFolder, "Select design export folder", &dialogError);
            selectedFolder.has_value())
        {
            CopyTextBuffer(workflowState_.designExportPopup.outputFolder, selectedFolder->lexically_normal().string());
        }
        else if (!dialogError.empty())
        {
            RebuildStatus(dialogError, true);
        }
    }
    ShowItemTooltip("Choose the folder where the design export should be created.");

    ImGui::SeparatorText("Options");
    ImGui::Checkbox("Export Markdown ICD", &workflowState_.designExportPopup.exportMarkdownIcd);
    ImGui::Checkbox("Export exploded designer views", &workflowState_.designExportPopup.exportExplodedViews);
    ShowItemTooltip("Render, per page, one exploded designer view with reticle labels and one clean page view without labels.");
    ImGui::Checkbox("Include canvas coordinates", &workflowState_.designExportPopup.includeCanvasCoordinates);
    ImGui::Checkbox("Include generated C++ usage snippets", &workflowState_.designExportPopup.includeCppSnippets);
    ImGui::Checkbox("Include strobe section", &workflowState_.designExportPopup.includeStrobe);
    ImGui::Checkbox("Include blink section", &workflowState_.designExportPopup.includeBlink);
    ImGui::Checkbox("Include primitive ids when available", &workflowState_.designExportPopup.includePrimitiveIds);
    ImGui::Checkbox("Include mapping hash when available", &workflowState_.designExportPopup.includeMappingHash);

    const editor::DesignExportPlan plan = services_.designExport.BuildPlan(BuildDesignExportRequest());

    if (plan.canExecute)
    {
        ImGui::SeparatorText("Plan");
        ImGui::TextWrapped("Final output: %s", plan.outputFolder.string().c_str());
        ImGui::Text("Files: %d", static_cast<int>(plan.filesToWrite.size()));
    }

    if (!plan.warnings.empty())
    {
        ImGui::SeparatorText("Warnings");
        for (const std::string& warning : plan.warnings)
        {
            ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.38f, 1.0f), "%s", warning.c_str());
        }
    }

    if (!plan.error.empty())
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.42f, 1.0f), "%s", plan.error.c_str());
    }

    ImGui::Spacing();
    ImGui::BeginDisabled(!plan.canExecute);
    if (AccentButton("Export"))
    {
        ExecuteDesignExportPlan(plan);
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        if (tutorial_->MatchesTarget("popup_design_export_cancel"))
        {
            tutorial_->CompleteStep();
        }
        workflowState_.designExportPopup = {};
        ImGui::CloseCurrentPopup();
    }
    tutorial_->DrawHalo(
        "popup_design_export_cancel",
        "Close the export popup",
        "This discovery step only inspects the design-export workflow. Close it without generating files.");

    ImGui::EndPopup();
}

void EditorApplication::DrawPageManagementPopup()
{
    if (!ImGui::BeginPopupModal("Manage page", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    const bool deleteAsset = workflowState_.pageManagementPopup.action == PageManagementAction::DeleteAsset;
    const bool pageIndexValid = workflowState_.pageManagementPopup.pageIndex >= 0 &&
                                workflowState_.pageManagementPopup.pageIndex < static_cast<int>(documentState_.loaded.document.pages.size());
    if (!pageIndexValid)
    {
        ImGui::TextWrapped("The selected page is no longer available.");
        if (ImGui::Button("Close"))
        {
            workflowState_.pageManagementPopup = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    const mfd::PageDefinition& page = documentState_.loaded.document.pages[static_cast<std::size_t>(workflowState_.pageManagementPopup.pageIndex)];
    if (deleteAsset)
    {
        ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.42f, 1.0f), "Delete page asset");
        ImGui::TextWrapped("This removes the page from the current window and deletes its JSON file on the next save.");
    }
    else
    {
        ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "Remove page from window");
        ImGui::TextWrapped("This detaches the page from the current window but keeps its JSON file on disk.");
    }

    ImGui::Separator();
    ImGui::TextDisabled("Page");
    ImGui::TextWrapped("%s", page.name.c_str());

    if (page.defaultPage && documentState_.loaded.document.pages.size() > 1U)
    {
        const int currentReplacementIndex =
            workflowState_.pageManagementPopup.replacementPageIndex >= 0 ? workflowState_.pageManagementPopup.replacementPageIndex
                                                           : SuggestReplacementPageIndex(documentState_.loaded.document.pages, workflowState_.pageManagementPopup.pageIndex);
        workflowState_.pageManagementPopup.replacementPageIndex = currentReplacementIndex;

        std::string replacementLabel = "Select replacement";
        if (currentReplacementIndex >= 0 &&
            currentReplacementIndex < static_cast<int>(documentState_.loaded.document.pages.size()) &&
            currentReplacementIndex != workflowState_.pageManagementPopup.pageIndex)
        {
            replacementLabel = documentState_.loaded.document.pages[static_cast<std::size_t>(currentReplacementIndex)].name;
        }

        if (ImGui::BeginCombo("Replacement default page", replacementLabel.c_str()))
        {
            for (int index = 0; index < static_cast<int>(documentState_.loaded.document.pages.size()); ++index)
            {
                if (index == workflowState_.pageManagementPopup.pageIndex)
                {
                    continue;
                }

                const bool selected = workflowState_.pageManagementPopup.replacementPageIndex == index;
                if (ImGui::Selectable(documentState_.loaded.document.pages[static_cast<std::size_t>(index)].name.c_str(), selected))
                {
                    workflowState_.pageManagementPopup.replacementPageIndex = index;
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ShowItemTooltip("Choose the page that should become the new default after this operation.");
    }
    else
    {
        workflowState_.pageManagementPopup.replacementPageIndex = -1;
    }

    const std::optional<int> replacementPageIndex =
        workflowState_.pageManagementPopup.replacementPageIndex >= 0 ? std::optional<int> {workflowState_.pageManagementPopup.replacementPageIndex}
                                                       : std::nullopt;
    const editor::PageRemovePlan removePlan =
        services_.pageManagement.BuildRemovePlan(documentState_.loaded,
                                               documentState_.files,
                                               editor::PageRemoveRequest {workflowState_.pageManagementPopup.pageIndex, replacementPageIndex});
    const editor::PageDeletePlan deletePlan =
        services_.pageManagement.BuildDeletePlan(documentState_.loaded,
                                               documentState_.files,
                                               editor::PageDeleteRequest {workflowState_.pageManagementPopup.pageIndex,
                                                                          replacementPageIndex,
                                                                          documentState_.assetPaths.DefaultAssetPath("assets"),
                                                                          workflowState_.pageManagementPopup.allowOutsideAssetsRoot});

    if (deleteAsset)
    {
        ImGui::Separator();
        ImGui::TextDisabled("File");
        ImGui::TextWrapped("%s", deletePlan.pageFile.empty() ? "<unknown>" : deletePlan.pageFile.string().c_str());

        if (deletePlan.outsideAssetsRoot)
        {
            ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.42f, 1.0f),
                               "The page file is outside the protected source assets root.");
            ImGui::Checkbox("Allow delete outside source assets root", &workflowState_.pageManagementPopup.allowOutsideAssetsRoot);
        }

        ImGui::Checkbox("Confirm page asset deletion on next save", &workflowState_.pageManagementPopup.confirmDelete);
        ShowItemTooltip("This confirmation is required before the page file is marked for deletion.");
    }

    const std::string errorMessage = deleteAsset ? deletePlan.error : removePlan.error;
    if (!errorMessage.empty())
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.42f, 1.0f), "%s", errorMessage.c_str());
    }

    const bool canExecutePlan = deleteAsset ? (deletePlan.canExecute && workflowState_.pageManagementPopup.confirmDelete) : removePlan.canExecute;
    ImGui::Spacing();
    ImGui::BeginDisabled(!canExecutePlan);
    if (AccentButton(deleteAsset ? "Delete page asset" : "Remove page from window"))
    {
        const bool executed = deleteAsset ? ExecutePageDeletePlan(deletePlan) : ExecutePageRemovePlan(removePlan);
        if (executed)
        {
            workflowState_.pageManagementPopup = {};
            ImGui::CloseCurrentPopup();
            ImGui::EndDisabled();
            ImGui::EndPopup();
            return;
        }
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        workflowState_.pageManagementPopup = {};
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void EditorApplication::PrepareTutorialStep()
{
    using editor::tutorial::TutorialStepId;

    tutorial_->ClampStepIndex();
    tutorial_->ResetPhase();
    tutorial_->ClearFocusLayer();

    const auto setPrimitiveDraft = [this](const mfd::PrimitiveType primitiveType)
    {
        for (int index = 0; index < static_cast<int>(kPrimitiveTypes.size()); ++index)
        {
            if (kPrimitiveTypes[static_cast<std::size_t>(index)] == primitiveType)
            {
                workflowState_.newLibraryReticleDraft.primitiveTypeIndex = index;
                break;
            }
        }
    };
    const auto selectTutorialPageOrFallback = [this](const std::string_view pageName)
    {
        if (const int pageIndex = FindPageIndexByName(documentState_.loaded, pageName); pageIndex >= 0)
        {
            SelectPage(pageIndex);
            return;
        }

        if (!documentState_.loaded.document.pages.empty())
        {
            SelectPage(std::clamp(documentState_.selection.pageIndex, 0, static_cast<int>(documentState_.loaded.document.pages.size()) - 1));
        }
    };
    const auto focusLibraryReticleInBrowser = [this](const std::string_view templateId)
    {
        const auto iterator = documentState_.loaded.document.reticleLibrary.find(std::string {templateId});
        if (iterator == documentState_.loaded.document.reticleLibrary.end())
        {
            return;
        }

        documentState_.selection.libraryReticleId = iterator->first;
        documentState_.selection.libraryBrowserReticleId = iterator->first;
        documentState_.selection.primitiveIndex = -1;
    };
    const auto selectLibraryPrimitiveById = [this](const std::string_view templateId, const std::string_view primitiveId)
    {
        const auto reticleIt = documentState_.loaded.document.reticleLibrary.find(std::string {templateId});
        if (reticleIt == documentState_.loaded.document.reticleLibrary.end())
        {
            return;
        }

        const auto primitiveIt = std::find_if(
            reticleIt->second.primitives.begin(),
            reticleIt->second.primitives.end(),
            [primitiveId](const mfd::Primitive& primitive)
            {
                return primitive.id == primitiveId;
            });
        if (primitiveIt == reticleIt->second.primitives.end())
        {
            return;
        }

        SelectLibraryPrimitive(
            reticleIt->first,
            static_cast<int>(primitiveIt - reticleIt->second.primitives.begin()));
    };

    const auto tutorialSteps = editor::tutorial::Steps();
    const editor::tutorial::TutorialStepDefinition& step =
        tutorialSteps[static_cast<std::size_t>(tutorial_->StepIndex())];
    if (!editor::tutorial::IsUiStep(step))
    {
        return;
    }

    switch (tutorial_->StepIndex())
    {
    case static_cast<int>(TutorialStepId::CreateWindow):
        tutorial_->ClearTrackedReticle();
        CopyTextBuffer(workflowState_.newWindowDraft.windowFile, documentState_.assetPaths.DefaultAssetPath("assets/windows/mfd_tutorial.json").string());
        CopyTextBuffer(workflowState_.newWindowDraft.title, "MFD Tutorial");
        workflowState_.newWindowDraft.width = 480;
        workflowState_.newWindowDraft.height = 480;
        workflowState_.newWindowDraft.positionX = 120;
        workflowState_.newWindowDraft.positionY = 80;
        CopyTextBuffer(workflowState_.newWindowDraft.fontFile, "");
        CopyTextBuffer(workflowState_.newWindowDraft.reticleLibraryFolder, documentState_.assetPaths.DefaultAssetPath("assets/reticles").string());
        workflowState_.newWindowDraft.commandUdpExposed = true;
        workflowState_.newWindowDraft.commandUdpEnabled = true;
        CopyTextBuffer(workflowState_.newWindowDraft.commandAddress, "127.0.0.1");
        workflowState_.newWindowDraft.commandPort = 49000;
        workflowState_.newWindowDraft.commandMaxPacketSize = 65507;
        workflowState_.newWindowDraft.feedbackUdpExposed = true;
        workflowState_.newWindowDraft.feedbackUdpEnabled = true;
        CopyTextBuffer(workflowState_.newWindowDraft.feedbackAddress, "127.0.0.1");
        workflowState_.newWindowDraft.feedbackPort = 49001;
        workflowState_.newWindowDraft.feedbackMaxPacketSize = 65507;
        workflowState_.newWindowDraft.feedbackFastIntervalSeconds = 0.020f;
        workflowState_.newWindowDraft.feedbackHeartbeatIntervalSeconds = 0.350f;
        workflowState_.newWindowDraft.createInitialPage = false;
        CopyTextBuffer(workflowState_.newWindowDraft.firstPageName, "Page1");
        CopyTextBuffer(workflowState_.newWindowDraft.firstPageTitle, "Page 1");
        CopyTextBuffer(workflowState_.newWindowDraft.firstPageFile, documentState_.assetPaths.DefaultAssetPath("assets/pages/mfd_tutorial_page1.json").string());
        workflowState_.newWindowDraft.firstPageBackground = ImVec4(0.0f, 0.125f, 0.376f, 1.0f);
        break;
    case static_cast<int>(TutorialStepId::CreateRadarTrackReticle):
        CopyTextBuffer(workflowState_.newLibraryReticleDraft.id, "mfd_tutorial_radar_track");
        setPrimitiveDraft(mfd::PrimitiveType::Diamond);
        break;
    case static_cast<int>(TutorialStepId::CreateCircleReticle):
        CopyTextBuffer(workflowState_.newLibraryReticleDraft.id, "mfd_tutorial_circle");
        setPrimitiveDraft(mfd::PrimitiveType::Circle);
        break;
    case static_cast<int>(TutorialStepId::CreateStrobeCursorReticle):
        CopyTextBuffer(workflowState_.newLibraryReticleDraft.id, kTutorialStrobeCursorTemplateId);
        setPrimitiveDraft(mfd::PrimitiveType::Line);
        break;
    case static_cast<int>(TutorialStepId::CreateAircraftReticle):
        CopyTextBuffer(workflowState_.newLibraryReticleDraft.id, kTutorialAircraftTemplateId);
        setPrimitiveDraft(mfd::PrimitiveType::Triangle);
        break;
    case static_cast<int>(TutorialStepId::AppendAircraftLabelPrimitive):
        if (documentState_.loaded.document.reticleLibrary.find(std::string(kTutorialAircraftTemplateId)) !=
            documentState_.loaded.document.reticleLibrary.end())
        {
            SelectLibraryReticle(std::string(kTutorialAircraftTemplateId));
        }
        setPrimitiveDraft(mfd::PrimitiveType::Text);
        break;
    case static_cast<int>(TutorialStepId::CreatePage1):
        CopyTextBuffer(workflowState_.newPageDraft.name, "Page1");
        CopyTextBuffer(workflowState_.newPageDraft.title, "Page 1");
        CopyTextBuffer(workflowState_.newPageDraft.fileName, documentState_.assetPaths.DefaultAssetPath("assets/pages/mfd_tutorial_page1.json").string());
        workflowState_.newPageDraft.background = ImVec4(0.0f, 0.125f, 0.376f, 1.0f);
        break;
    case static_cast<int>(TutorialStepId::SelectPage1TitleChrome):
        selectTutorialPageOrFallback("Page1");
        break;
    case static_cast<int>(TutorialStepId::FramePage1Title):
        if (const int pageIndex = FindPageIndexByName(documentState_.loaded, "Page1"); pageIndex >= 0)
        {
            SelectPageTitle(pageIndex);
        }
        else
        {
            selectTutorialPageOrFallback("Page1");
        }
        break;
    case static_cast<int>(TutorialStepId::CreateRadarTrackLayerOnPage1):
    case static_cast<int>(TutorialStepId::AllowPage1DynamicReticleTemplate):
        selectTutorialPageOrFallback("Page1");
        break;
    case static_cast<int>(TutorialStepId::AddPage1DefaultStrobe):
    case static_cast<int>(TutorialStepId::AddPage1AlternativeStrobe):
        selectTutorialPageOrFallback("Page1");
        break;
    case static_cast<int>(TutorialStepId::ExposeAircraftLabelPrimitive):
    case static_cast<int>(TutorialStepId::DisableAircraftLabelTransformInheritance):
        selectLibraryPrimitiveById(kTutorialAircraftTemplateId, kTutorialAircraftLabelPrimitiveId);
        break;
    case static_cast<int>(TutorialStepId::AddAircraftReticleToPage1):
    {
        selectTutorialPageOrFallback("Page1");
        layoutState_.pagePreviewView.center = kTutorialPage1OwnshipAnchor;
        if (documentState_.loaded.document.reticleLibrary.find(std::string(kTutorialAircraftTemplateId)) !=
            documentState_.loaded.document.reticleLibrary.end())
        {
            SelectLibraryReticle(std::string(kTutorialAircraftTemplateId));
        }
        break;
    }
    case static_cast<int>(TutorialStepId::RenameAircraftReticleOnPage1):
    {
        if (const int pageIndex = FindPageIndexByName(documentState_.loaded, "Page1"); pageIndex >= 0)
        {
            SelectPage(pageIndex);
            if (!tutorial_->TrackedReticleId().empty())
            {
                const mfd::PageDefinition& page = documentState_.loaded.document.pages[static_cast<std::size_t>(pageIndex)];
                if (const int reticleIndex = FindPageReticleIndexById(page, tutorial_->TrackedReticleId()); reticleIndex >= 0)
                {
                    SelectPageReticle(pageIndex, reticleIndex);
                }
            }
        }
        else
        {
            selectTutorialPageOrFallback("Page1");
        }
        break;
    }
    case static_cast<int>(TutorialStepId::AddCircleReticleToPage1):
    {
        selectTutorialPageOrFallback("Page1");
        if (documentState_.loaded.document.reticleLibrary.find("mfd_tutorial_circle") != documentState_.loaded.document.reticleLibrary.end())
        {
            SelectLibraryReticle("mfd_tutorial_circle");
        }
        break;
    }
    case static_cast<int>(TutorialStepId::ClipCircleOutside):
    {
        if (const int pageIndex = FindPageIndexByName(documentState_.loaded, "Page1"); pageIndex >= 0)
        {
            SelectPage(pageIndex);
            if (!tutorial_->TrackedReticleId().empty())
            {
                const mfd::PageDefinition& page = documentState_.loaded.document.pages[static_cast<std::size_t>(pageIndex)];
                if (const int reticleIndex = FindPageReticleIndexById(page, tutorial_->TrackedReticleId()); reticleIndex >= 0)
                {
                    SelectPageReticle(pageIndex, reticleIndex);
                }
            }
        }
        break;
    }
    case static_cast<int>(TutorialStepId::AddAndHideEditorLayer):
        selectTutorialPageOrFallback("Page1");
        break;
    case static_cast<int>(TutorialStepId::CreatePage2):
        CopyTextBuffer(workflowState_.newPageDraft.name, "Page2");
        CopyTextBuffer(workflowState_.newPageDraft.title, "Page 2");
        CopyTextBuffer(workflowState_.newPageDraft.fileName, documentState_.assetPaths.DefaultAssetPath("assets/pages/mfd_tutorial_page2.json").string());
        workflowState_.newPageDraft.background = ImVec4(0.04f, 0.08f, 0.14f, 1.0f);
        break;
    case static_cast<int>(TutorialStepId::CreateProgressBarReticle):
        CopyTextBuffer(workflowState_.newLibraryReticleDraft.id, "mfd_tutorial_progress_bar");
        setPrimitiveDraft(mfd::PrimitiveType::Rectangle);
        break;
    case static_cast<int>(TutorialStepId::ExposeProgressBarFillPrimitive):
        if (documentState_.loaded.document.reticleLibrary.find("mfd_tutorial_progress_bar") != documentState_.loaded.document.reticleLibrary.end())
        {
            SelectLibraryPrimitive("mfd_tutorial_progress_bar", 0);
        }
        break;
    case static_cast<int>(TutorialStepId::AddProgressBarToPage2):
        selectTutorialPageOrFallback("Page2");
        if (documentState_.loaded.document.reticleLibrary.find("mfd_tutorial_progress_bar") !=
            documentState_.loaded.document.reticleLibrary.end())
        {
            SelectLibraryReticle("mfd_tutorial_progress_bar");
        }
        break;
    case static_cast<int>(TutorialStepId::ShowPageBorder):
        selectTutorialPageOrFallback("Page1");
        layoutState_.pagePreviewViewOptions.showPageBorder = false;
        break;
    case static_cast<int>(TutorialStepId::ShowLayerInspector):
        selectTutorialPageOrFallback("Page1");
        layoutState_.pagePreviewViewOptions.showLayerInspector = false;
        break;
    case static_cast<int>(TutorialStepId::ShowMinimap):
        selectTutorialPageOrFallback("Page1");
        layoutState_.pagePreviewViewOptions.showMinimap = false;
        break;
    case static_cast<int>(TutorialStepId::ShowReticleUsageHighlights):
        selectTutorialPageOrFallback("Page1");
        focusLibraryReticleInBrowser("mfd_tutorial_circle");
        layoutState_.pagePreviewViewOptions.highlightReticleUsages = false;
        break;
    case static_cast<int>(TutorialStepId::ShowProblemsPanel):
        selectTutorialPageOrFallback("Page1");
        layoutState_.pagePreviewViewOptions.showProblemsPanel = false;
        break;
    case static_cast<int>(TutorialStepId::ToggleFullscreenPreview):
        if (services_.fullscreenPreview.IsActive())
        {
            ToggleFullscreenPagePreview();
        }
        selectTutorialPageOrFallback("Page1");
        break;
    case static_cast<int>(TutorialStepId::InspectPageImportWorkflow):
    case static_cast<int>(TutorialStepId::InspectPageRenameWorkflow):
        selectTutorialPageOrFallback("Page1");
        break;
    case static_cast<int>(TutorialStepId::InspectReticleRenameWorkflow):
        if (documentState_.loaded.document.reticleLibrary.find("mfd_tutorial_circle") != documentState_.loaded.document.reticleLibrary.end())
        {
            SelectLibraryReticle("mfd_tutorial_circle");
        }
        break;
    case static_cast<int>(TutorialStepId::InspectDesignExportWorkflow):
        if (services_.fullscreenPreview.IsActive())
        {
            ToggleFullscreenPagePreview();
        }
        selectTutorialPageOrFallback("Page1");
        break;
    default:
        break;
    }
}

bool EditorApplication::CreateNewWindow()
{
    using editor::tutorial::TutorialStepId;

    const std::filesystem::path windowFile = std::filesystem::path(workflowState_.newWindowDraft.windowFile.data()).lexically_normal();
    if (windowFile.empty())
    {
        RebuildStatus("Window file cannot be empty.", true);
        return false;
    }
    if (editor::EditorAssetPathService::IsExecStagingPath(windowFile))
    {
        RebuildStatus("Choose a source assets folder for the window JSON, not a staged _Exec folder.", true);
        return false;
    }

    const std::string windowTitle = workflowState_.newWindowDraft.title.data();
    if (windowTitle.empty())
    {
        RebuildStatus("Window title cannot be empty.", true);
        return false;
    }

    if (workflowState_.newWindowDraft.width <= 0 || workflowState_.newWindowDraft.height <= 0)
    {
        RebuildStatus("Window size must be strictly positive.", true);
        return false;
    }

    if (workflowState_.newWindowDraft.feedbackFastIntervalSeconds <= 0.0f)
    {
        RebuildStatus("Feedback fast interval must be strictly positive.", true);
        return false;
    }

    if (workflowState_.newWindowDraft.feedbackHeartbeatIntervalSeconds < workflowState_.newWindowDraft.feedbackFastIntervalSeconds)
    {
        RebuildStatus("Feedback heartbeat interval must stay greater than or equal to the fast interval.", true);
        return false;
    }

    const std::filesystem::path windowBaseFolder = windowFile.parent_path();

    mfd::LoadedWindowConfiguration next {};
    next.window.sourceFile = windowFile;
    next.window.title = windowTitle;
    next.window.width = std::max(1, workflowState_.newWindowDraft.width);
    next.window.height = std::max(1, workflowState_.newWindowDraft.height);
    next.window.positionX = workflowState_.newWindowDraft.positionX;
    next.window.positionY = workflowState_.newWindowDraft.positionY;
    next.window.targetFps = 60;
    next.window.feedbackFastIntervalSeconds =
        ClampFeedbackFastIntervalSeconds(workflowState_.newWindowDraft.feedbackFastIntervalSeconds);
    next.window.feedbackHeartbeatIntervalSeconds =
        ClampFeedbackHeartbeatIntervalSeconds(
            workflowState_.newWindowDraft.feedbackHeartbeatIntervalSeconds,
            next.window.feedbackFastIntervalSeconds);

    const std::filesystem::path fontPath = std::filesystem::path(workflowState_.newWindowDraft.fontFile.data()).lexically_normal();
    if (!fontPath.empty())
    {
        next.window.fontFile = fontPath.is_relative() ? (windowBaseFolder / fontPath).lexically_normal() : fontPath;
    }

    const std::filesystem::path reticleFolder =
        std::filesystem::path(workflowState_.newWindowDraft.reticleLibraryFolder.data()).lexically_normal();
    const std::filesystem::path effectiveReticleFolder =
        reticleFolder.empty() ? documentState_.assetPaths.DefaultSiblingAssetFile(windowFile, "reticles", "") : reticleFolder;
    if (editor::EditorAssetPathService::IsExecStagingPath(effectiveReticleFolder))
    {
        RebuildStatus("Choose a source assets folder for the reticle library, not a staged _Exec folder.", true);
        return false;
    }
    next.window.reticleLibraryFolder = effectiveReticleFolder.is_relative()
                                           ? (windowBaseFolder / effectiveReticleFolder).lexically_normal()
                                           : effectiveReticleFolder;
    next.document.reticleLibraryFolder = next.window.reticleLibraryFolder;

    if (workflowState_.newWindowDraft.commandUdpExposed)
    {
        mfd::WindowUdpCommandTransport commandUdp {};
        commandUdp.enabled = workflowState_.newWindowDraft.commandUdpEnabled;
        commandUdp.address = workflowState_.newWindowDraft.commandAddress.data();
        commandUdp.port = static_cast<std::uint16_t>(
            std::clamp(workflowState_.newWindowDraft.commandPort, 0, static_cast<int>(std::numeric_limits<std::uint16_t>::max())));
        commandUdp.maxPacketSize = std::max(512, workflowState_.newWindowDraft.commandMaxPacketSize);
        next.window.commandTransports.udp = commandUdp;
    }

    if (workflowState_.newWindowDraft.feedbackUdpExposed)
    {
        mfd::WindowUdpFeedbackTransport feedbackUdp {};
        feedbackUdp.enabled = workflowState_.newWindowDraft.feedbackUdpEnabled;
        feedbackUdp.address = workflowState_.newWindowDraft.feedbackAddress.data();
        feedbackUdp.port = static_cast<std::uint16_t>(
            std::clamp(workflowState_.newWindowDraft.feedbackPort, 0, static_cast<int>(std::numeric_limits<std::uint16_t>::max())));
        feedbackUdp.maxPacketSize = std::max(512, workflowState_.newWindowDraft.feedbackMaxPacketSize);
        next.window.feedbackTransports.udp = feedbackUdp;
    }

    editor::EditorFileLayout nextFiles {};
    if (std::filesystem::exists(next.window.reticleLibraryFolder))
    {
        std::string discoverError;
        if (!editor::DiscoverReticleTemplateFiles(next.window.reticleLibraryFolder, nextFiles, &discoverError))
        {
            RebuildStatus(discoverError, true);
            return false;
        }
    }

    if (tutorial_ != nullptr && tutorial_->IsStep(static_cast<int>(TutorialStepId::CreateWindow)))
    {
        const std::string templateId {kTutorialSteeringCueTemplateId};
        next.document.reticleLibrary.insert_or_assign(templateId, MakeTutorialSteeringCueReticle());
        nextFiles.templateFiles.insert_or_assign(
            templateId,
            editor::DefaultTemplateFilePath(next.window.reticleLibraryFolder, templateId));
    }

    if (workflowState_.newWindowDraft.createInitialPage)
    {
        const std::string pageName = workflowState_.newWindowDraft.firstPageName.data();
        if (pageName.empty())
        {
            RebuildStatus("Initial page name cannot be empty.", true);
            return false;
        }

        mfd::PageDefinition page {};
        page.name = pageName;
        page.normalizedName = mfd::NormalizePageName(pageName);
        page.title = workflowState_.newWindowDraft.firstPageTitle.data();
        page.backgroundColor = ToColorRgba(workflowState_.newWindowDraft.firstPageBackground);
        page.defaultPage = true;
        page.layers.push_back(mfd::PageLayerDefinition {std::string(mfd::kDefaultPageLayerId)});
        BootstrapEditorLayersForPage(page);
        next.document.pages.push_back(page);

        std::filesystem::path pageFile = std::filesystem::path(workflowState_.newWindowDraft.firstPageFile.data()).lexically_normal();
        if (pageFile.empty())
        {
            pageFile = editor::DefaultPageFilePath(windowFile, page.name);
        }
        if (pageFile.is_relative())
        {
            pageFile = (windowBaseFolder / pageFile).lexically_normal();
        }
        if (editor::EditorAssetPathService::IsExecStagingPath(pageFile))
        {
            RebuildStatus("Choose a source assets folder for the first page JSON, not a staged _Exec folder.", true);
            return false;
        }
        nextFiles.pageFiles.push_back(pageFile);
        next.window.pageFiles = nextFiles.pageFiles;
    }

    PushUndoSnapshot();
    documentState_.loaded = std::move(next);
    documentState_.files = std::move(nextFiles);
    documentState_.windowFile = documentState_.loaded.window.sourceFile;
    ApplyPreviewFontFile(documentState_.loaded.window.fontFile);
    SelectPage(DefaultPageIndex(documentState_.loaded.document.pages));
    RebuildStatus("New window draft created. Use File > Save to write JSON files on disk.", false);
    return true;
}

bool EditorApplication::CreateNewPage()
{
    using editor::tutorial::TutorialStepId;

    const std::string pageName = workflowState_.newPageDraft.name.data();
    const std::string normalizedPageName = mfd::NormalizePageName(pageName);
    if (normalizedPageName.empty())
    {
        RebuildStatus("Page name cannot be empty.", true);
        return false;
    }

    const bool duplicatePageName = std::any_of(
        documentState_.loaded.document.pages.begin(),
        documentState_.loaded.document.pages.end(),
        [&normalizedPageName](const mfd::PageDefinition& page)
        {
            return mfd::NormalizePageName(page.name) == normalizedPageName;
        });
    if (duplicatePageName)
    {
        RebuildStatus("Page name '" + pageName + "' already exists after normalization.", true);
        return false;
    }

    std::filesystem::path pageFile = std::filesystem::path(workflowState_.newPageDraft.fileName.data()).lexically_normal();
    if (pageFile.empty())
    {
        pageFile = editor::DefaultPageFilePath(documentState_.loaded.window.sourceFile, pageName);
    }
    if (pageFile.is_relative())
    {
        pageFile = (documentState_.loaded.window.sourceFile.parent_path() / pageFile).lexically_normal();
    }
    if (pageFile.extension().empty())
    {
        pageFile += ".json";
    }
    if (editor::EditorAssetPathService::IsExecStagingPath(pageFile))
    {
        RebuildStatus("Choose a source assets folder for the page JSON, not a staged _Exec folder.", true);
        return false;
    }

    PushUndoSnapshot();

    mfd::PageDefinition page;
    page.name = pageName;
    page.normalizedName = normalizedPageName;
    page.title = workflowState_.newPageDraft.title.data();
    page.backgroundColor = ToColorRgba(workflowState_.newPageDraft.background);
    page.layers.push_back(mfd::PageLayerDefinition {std::string(mfd::kDefaultPageLayerId)});
    if (tutorial_ != nullptr &&
        tutorial_->IsStep(static_cast<int>(TutorialStepId::CreatePage1)) &&
        page.name == "Page1")
    {
        page.layers.push_back(mfd::PageLayerDefinition {"overlay"});
        page.dynamicReticleBindings.push_back(mfd::DynamicReticleLayerBinding {
            std::string(kTutorialSteeringCueTemplateId),
            "overlay",
            0});
    }
    BootstrapEditorLayersForPage(page);

    documentState_.loaded.document.pages.push_back(page);
    documentState_.files.pageFiles.push_back(pageFile.lexically_normal());
    documentState_.loaded.window.pageFiles = documentState_.files.pageFiles;

    SelectPage(static_cast<int>(documentState_.loaded.document.pages.size()) - 1);
    RebuildStatus("Page '" + page.name + "' created.", false);
    return true;
}

bool EditorApplication::CreateNewLibraryReticleFromPrimitive()
{
    const std::string reticleId = workflowState_.newLibraryReticleDraft.id.data();
    if (reticleId.empty())
    {
        RebuildStatus("Library reticle id cannot be empty.", true);
        return false;
    }

    const mfd::PrimitiveType primitiveType =
        kPrimitiveTypes[static_cast<std::size_t>(workflowState_.newLibraryReticleDraft.primitiveTypeIndex)];
    std::string tutorialError;
    if (!tutorial_->ValidateNewLibraryReticleDraft(reticleId, primitiveType, tutorialError))
    {
        RebuildStatus(tutorialError, true);
        return false;
    }

    if (ReticleLibraryIdExistsNormalized(documentState_.loaded.document.reticleLibrary, reticleId))
    {
        RebuildStatus("Library reticle id '" + reticleId + "' already exists after normalization.", true);
        return false;
    }

    PushUndoSnapshot();
    mfd::ReticleGroup reticle = MakePrimitiveReticle(
        reticleId,
        primitiveType);
    tutorial_->ConfigureCreatedLibraryReticle(reticle);
    const std::string createdReticleId = reticle.id;
    const auto [createdIterator, inserted] =
        documentState_.loaded.document.reticleLibrary.emplace(createdReticleId, std::move(reticle));
    if (!inserted)
    {
        RebuildStatus("Library reticle id '" + createdReticleId + "' already exists.", true);
        return false;
    }

    documentState_.files.templateFiles[createdReticleId] =
        editor::DefaultTemplateFilePath(documentState_.loaded.window.reticleLibraryFolder, createdReticleId);
    SelectLibraryReticle(createdIterator->first);
    CopyTextBuffer(workflowState_.newLibraryReticleDraft.id, MakeUniqueLibraryReticleId(createdReticleId));
    RebuildStatus("Library reticle '" + createdReticleId + "' created.", false);
    return true;
}

void EditorApplication::DuplicateSelectedLibraryReticle()
{
    const mfd::ReticleGroup* source = SelectedLibraryReticle();
    if (source == nullptr)
    {
        RebuildStatus("No library reticle selected.", true);
        return;
    }

    const std::string newId = workflowState_.duplicateLibraryReticleDraft.id.data();
    if (newId.empty())
    {
        RebuildStatus("Duplicate id cannot be empty.", true);
        return;
    }

    if (ReticleLibraryIdExistsNormalized(documentState_.loaded.document.reticleLibrary, newId))
    {
        RebuildStatus("Library reticle id '" + newId + "' already exists after normalization.", true);
        return;
    }

    PushUndoSnapshot();
    mfd::ReticleGroup copy = *source;
    copy.id = newId;
    copy.sourceTemplateId.clear();
    const auto [createdIterator, inserted] = documentState_.loaded.document.reticleLibrary.emplace(newId, std::move(copy));
    if (!inserted)
    {
        RebuildStatus("Library reticle id '" + newId + "' already exists.", true);
        return;
    }

    documentState_.files.templateFiles[newId] = editor::DefaultTemplateFilePath(documentState_.loaded.window.reticleLibraryFolder, newId);
    SelectLibraryReticle(createdIterator->first);
    RebuildStatus("Library reticle duplicated as '" + newId + "'.", false);
}

void EditorApplication::CopySelectedLibraryReticle()
{
    const mfd::ReticleGroup* source = SelectedLibraryReticle();
    if (source == nullptr)
    {
        RebuildStatus("No library reticle selected.", true);
        return;
    }

    clipboardState_.libraryReticleClipboard = *source;
    RebuildStatus("Library reticle '" + source->id + "' copied.", false);
}

void EditorApplication::PasteCopiedLibraryReticle()
{
    if (!clipboardState_.libraryReticleClipboard.has_value())
    {
        RebuildStatus("No copied library reticle is available yet.", true);
        return;
    }

    PushUndoSnapshot();

    mfd::ReticleGroup copy = *clipboardState_.libraryReticleClipboard;
    const std::string baseId = copy.id.empty() ? std::string {"reticle_copy"} : copy.id + "_copy";
    copy.id = MakeUniqueLibraryReticleId(baseId);
    copy.sourceTemplateId.clear();
    const std::string pastedReticleId = copy.id;
    const auto [createdIterator, inserted] =
        documentState_.loaded.document.reticleLibrary.emplace(pastedReticleId, std::move(copy));
    if (!inserted)
    {
        RebuildStatus("Library reticle id '" + pastedReticleId + "' already exists.", true);
        return;
    }

    documentState_.files.templateFiles[pastedReticleId] =
        editor::DefaultTemplateFilePath(documentState_.loaded.window.reticleLibraryFolder, pastedReticleId);
    SelectLibraryReticle(createdIterator->first);
    RebuildStatus("Library reticle pasted as '" + pastedReticleId + "'.", false);
}

void EditorApplication::CopySelectedLibraryPrimitive()
{
    const mfd::Primitive* source = SelectedLibraryPrimitive();
    const mfd::ReticleGroup* reticle = SelectedLibraryReticle();
    if (source == nullptr || reticle == nullptr)
    {
        RebuildStatus("No library primitive selected.", true);
        return;
    }

    clipboardState_.libraryPrimitiveClipboard = *source;
    clipboardState_.libraryPrimitivePasteSerial = 0;
    const std::string primitiveLabel =
        source->id.empty() ? std::string {"unnamed primitive"} : std::string {"'"} + source->id + "'";
    RebuildStatus("Primitive " + primitiveLabel + " copied from library reticle '" + reticle->id + "'.", false);
}

void EditorApplication::PasteCopiedLibraryPrimitive()
{
    mfd::ReticleGroup* reticle = SelectedLibraryReticle();
    if (reticle == nullptr)
    {
        RebuildStatus("Select a library reticle before pasting one primitive.", true);
        return;
    }

    if (!clipboardState_.libraryPrimitiveClipboard.has_value())
    {
        RebuildStatus("No copied library primitive is available yet.", true);
        return;
    }

    PushUndoSnapshot();

    ++clipboardState_.libraryPrimitivePasteSerial;
    const float offset = 0.018f * static_cast<float>(clipboardState_.libraryPrimitivePasteSerial);
    const editor::PrimitivePastePlan plan = services_.primitiveClipboard.BuildPastePlan(
        editor::PrimitivePasteRequest {
            *reticle,
            *clipboardState_.libraryPrimitiveClipboard,
            documentState_.selection.kind == SelectionKind::LibraryPrimitive ? documentState_.selection.primitiveIndex : -1,
            mfd::Vec2 {offset, -offset}});

    reticle->primitives.insert(reticle->primitives.begin() + plan.insertionIndex, plan.primitive);
    SelectLibraryPrimitive(reticle->id, plan.insertionIndex);
    RebuildStatus("Primitive '" + plan.primitive.id + "' pasted in library reticle '" + reticle->id + "'.", false);
}

mfd::ReticleGroup EditorApplication::MakePrimitiveReticle(std::string id, const mfd::PrimitiveType primitiveType)
{
    mfd::ReticleGroup reticle;
    reticle.id = std::move(id);

    mfd::Primitive primitive;
    primitive.id = "primitive_01";
    primitive.type = primitiveType;

    switch (primitiveType)
    {
    case mfd::PrimitiveType::Text:
        primitive.geometry = mfd::TextGeometry {"TXT", 0.06f};
        break;
    case mfd::PrimitiveType::Time:
        primitive.geometry = mfd::TimeGeometry {"%H:%M:%S", false, 0.06f};
        break;
    case mfd::PrimitiveType::Line:
        primitive.geometry = mfd::LineGeometry {{-0.10f, 0.0f}, {0.10f, 0.0f}};
        break;
    case mfd::PrimitiveType::Circle:
        primitive.geometry = mfd::CircleGeometry {0.10f};
        break;
    case mfd::PrimitiveType::Ring:
        primitive.geometry = mfd::RingGeometry {0.06f, 0.10f, 64};
        primitive.style.filled = true;
        primitive.style.fillColor = mfd::ColorRgba {0, 255, 102, 48};
        break;
    case mfd::PrimitiveType::Rectangle:
        primitive.geometry = mfd::RectangleGeometry {0.24f, 0.14f};
        break;
    case mfd::PrimitiveType::Ellipse:
        primitive.geometry = mfd::EllipseGeometry {0.24f, 0.14f};
        break;
    case mfd::PrimitiveType::Square:
        primitive.geometry = mfd::SquareGeometry {0.20f, 0.20f};
        break;
    case mfd::PrimitiveType::Diamond:
        primitive.geometry = mfd::DiamondGeometry {0.22f, 0.22f};
        break;
    case mfd::PrimitiveType::Triangle:
        primitive.geometry = mfd::TriangleGeometry {{{{-0.10f, -0.08f}, {0.10f, -0.08f}, {0.0f, 0.12f}}}};
        break;
    case mfd::PrimitiveType::Polyline:
        primitive.geometry = mfd::PolylineGeometry {{{{-0.10f, -0.10f}, {0.0f, 0.12f}, {0.10f, -0.10f}}}, false};
        break;
    case mfd::PrimitiveType::Bezier:
        primitive.geometry = mfd::BezierGeometry {{{{-0.12f, -0.12f}, {-0.04f, 0.12f}, {0.04f, -0.12f}, {0.12f, 0.12f}}}, 32};
        break;
    case mfd::PrimitiveType::Arc:
        primitive.geometry = mfd::ArcGeometry {0.12f, -45.0f, 135.0f, 48};
        break;
    case mfd::PrimitiveType::Image:
        primitive.geometry = mfd::ImageGeometry {};
        break;
    }

    reticle.primitives.push_back(std::move(primitive));
    return reticle;
}


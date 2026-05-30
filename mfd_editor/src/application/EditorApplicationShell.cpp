/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorApplication.h"

/**
 * @file
 * @brief Editor shell and workspace layout implementation extracted from `EditorApplication`.
 */

#include <algorithm>
#include <cmath>
#include <filesystem>

#include "internal/application/EditorApplicationInternal.h"
#include "EditorTutorialController.h"
#include "EditorTutorialData.h"
#include "EditorUiTheme.h"
#include "EditorWorkspaceLayout.h"

namespace
{
using editor::ui::AccentButton;
using editor::ui::DrawVerticalSplitter;
using editor::ui::ShowItemTooltip;

constexpr float kMinSidebarWidth = 220.0f;
constexpr float kMinInspectorWidth = 280.0f;
constexpr float kMinWorkspaceWidth = 420.0f;
constexpr float kMinPageContextWidth = 320.0f;
constexpr float kMinReticleStudioWidth = 320.0f;
constexpr float kLayerInspectorDockWidth = 248.0f;
constexpr float kPreviewProblemsDockHeight = 176.0f;
constexpr const char* kReticleStudioDisplayPopupId = "ReticleStudioDisplayPopup";
}

void EditorApplication::DrawMenuBar()
{
    using editor::tutorial::TutorialStepId;
    const bool hasOpenWindow = HasOpenWindow();

    if (!ImGui::BeginMainMenuBar())
    {
        return;
    }

    const bool fileMenuOpen = ImGui::BeginMenu("File");
    tutorial_->DrawHalo("menu_file", "Click File", "Open the top-level document actions used by this tutorial step.");
    if (ImGui::IsItemClicked() && tutorial_->MatchesTarget("menu_file"))
    {
        tutorial_->AdvancePhase();
    }
    if (fileMenuOpen)
    {
        const bool newWindowRequested = ImGui::MenuItem("New window from scratch");
        ShowItemTooltip("Create a brand-new window JSON and optional first page directly from the editor.");
        tutorial_->DrawHalo(
            "menu_file_new_window",
            "Click New window from scratch",
            "Open the creation dialog prefilled with the tutorial window settings.");
        if (newWindowRequested)
        {
            if (tutorial_->MatchesTarget("menu_file_new_window"))
            {
                tutorial_->AdvancePhase();
            }
            OpenNewWindowPopup();
        }

        const bool openWindowRequested = ImGui::MenuItem("Open window asset...");
        ShowItemTooltip("Browse to one authored window JSON through the native file explorer.");
        if (openWindowRequested)
        {
            OpenWindowAssetFromFileExplorer();
        }

        const bool exportMenuOpen = ImGui::BeginMenu("Export", hasOpenWindow);
        ShowItemTooltip("Open export workflows for designer-facing deliverables.");
        tutorial_->DrawHalo(
            "menu_file_export",
            "Open Export",
            "Open the export workflows exposed by the editor before reviewing the design-export popup.");
        if (ImGui::IsItemClicked() && tutorial_->MatchesTarget("menu_file_export"))
        {
            tutorial_->AdvancePhase();
        }
        if (exportMenuOpen)
        {
            const bool exportDesignRequested = ImGui::MenuItem("Export design...");
            ShowItemTooltip("Generate Markdown ICD files and exploded designer views for the current window.");
            tutorial_->DrawHalo(
                "menu_file_export_design",
                "Click Export design...",
                "Open the design export popup and review its options without writing anything yet.");
            if (exportDesignRequested)
            {
                if (tutorial_->MatchesTarget("menu_file_export_design"))
                {
                    tutorial_->AdvancePhase();
                }
                OpenDesignExportPopup();
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();
        const bool saveRequested = ImGui::MenuItem("Save", "Ctrl+S", false, hasOpenWindow);
        ShowItemTooltip("Write the window file, page files and reticle template files back to disk.");
        tutorial_->DrawHalo(
            "menu_file_save",
            "Click Save",
            "Persist the authored tutorial assets before moving to the code review steps.");
        if (saveRequested)
        {
            const bool saveSucceeded = SaveAll();
            const bool tutorialSaveMatched = tutorial_->MatchesTarget("menu_file_save");
            if (editor::detail::ShouldAdvanceTutorialOnSuccessfulSave(
                    saveSucceeded,
                    tutorial_->IsStep(static_cast<int>(TutorialStepId::SaveTutorialAssets)),
                    tutorialSaveMatched))
            {
                tutorial_->CompleteStep();
            }
        }

        const bool canReloadCurrent = hasOpenWindow && std::filesystem::exists(documentState_.windowFile);
        const bool reloadRequested = ImGui::MenuItem("Reload current", nullptr, false, canReloadCurrent);
        ShowItemTooltip("Reload the current window asset from disk and discard unsaved editor changes.");
        if (reloadRequested)
        {
            LoadWindowConfiguration(documentState_.windowFile);
        }
        ImGui::EndMenu();
    }
    else if (tutorial_->ShouldResetFileMenuPhaseOnClose())
    {
        tutorial_->ResetPhase();
    }

    if (ImGui::BeginMenu("Edit"))
    {
        const bool undoRequested = ImGui::MenuItem("Undo", "Ctrl+Z", false, !documentState_.undoStack.empty());
        ShowItemTooltip("Restore the previous editor snapshot.");
        if (undoRequested)
        {
            Undo();
        }

        const bool hasPageReticleSelection =
            !SelectedPageReticleIndices().empty() || documentState_.selection.kind == SelectionKind::PageStrobe;
        const bool copyReticlesRequested =
            ImGui::MenuItem("Copy selected page reticles", "Ctrl+C", false, hasPageReticleSelection);
        ShowItemTooltip("Copy the selected page reticle instances or the selected page strobe so they can be pasted on the active page.");
        if (copyReticlesRequested)
        {
            CopySelectedPageReticles();
        }

        const bool cutReticlesRequested =
            ImGui::MenuItem("Cut selected page reticles", "Ctrl+X", false, hasPageReticleSelection);
        ShowItemTooltip("Copy the selected page reticle instances or the selected page strobe into the clipboard, then remove them from the page.");
        if (cutReticlesRequested)
        {
            CutSelectedPageReticles();
        }

        const bool canPastePageReticles = ActivePage() != nullptr && !clipboardState_.pageReticleClipboard.empty();
        const bool pasteReticlesRequested =
            ImGui::MenuItem("Paste page reticles", "Ctrl+V", false, canPastePageReticles);
        ShowItemTooltip("Paste copied page reticles onto the active page.");
        if (pasteReticlesRequested)
        {
            PasteCopiedPageReticles();
        }

        const bool deleteReticlesRequested =
            ImGui::MenuItem("Delete selected page reticles", "Del", false, hasPageReticleSelection);
        ShowItemTooltip("Remove the selected page reticle instances or the selected page strobe from the active page.");
        if (deleteReticlesRequested)
        {
            DeleteSelection();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window", hasOpenWindow))
    {
        const bool editWindowRequested = ImGui::MenuItem("Window settings");
        ShowItemTooltip("Reopen the window-level inspector to tune transports, cadence and display metadata.");
        if (editWindowRequested)
        {
            SelectWindow();
        }
        ImGui::EndMenu();
    }

    const bool pageMenuOpen = ImGui::BeginMenu("Page", hasOpenWindow);
    tutorial_->DrawHalo("menu_page", "Click Page", "Open the page-authoring actions used by the current tutorial step.");
    if (ImGui::IsItemClicked() && tutorial_->MatchesTarget("menu_page"))
    {
        tutorial_->AdvancePhase();
    }
    if (pageMenuOpen)
    {
        const bool newPageRequested = ImGui::MenuItem("New page");
        ShowItemTooltip("Create a new page and its backing JSON file.");
        tutorial_->DrawHalo(
            "menu_page_new",
            "Click New page",
            "Open the page dialog with the tutorial page values already prepared for you.");
        if (newPageRequested)
        {
            if (tutorial_->MatchesTarget("menu_page_new"))
            {
                tutorial_->AdvancePhase();
            }
            OpenNewPagePopup();
        }

        const bool importPageRequested = ImGui::MenuItem("Import page...");
        ShowItemTooltip("Import one external page JSON and stage its reticle dependencies into the current window.");
        tutorial_->DrawHalo(
            "menu_page_import",
            "Click Import page...",
            "Open the import workflow so you know where shared page ingestion starts. You can cancel the native file picker.");
        if (importPageRequested)
        {
            if (tutorial_->MatchesTarget("menu_page_import"))
            {
                tutorial_->CompleteStep();
            }
            OpenPageAssetImportFromFileExplorer();
        }

        const bool renamePageRequested = ImGui::MenuItem("Rename current page globally...", nullptr, false, ActivePage() != nullptr);
        ShowItemTooltip("Rename the current page asset safely across the current asset tree and update shared window references.");
        tutorial_->DrawHalo(
            "menu_page_rename",
            "Click Rename current page globally...",
            "Open the safe page-rename popup, review the scanned references, then close it without executing the rename.");
        if (renamePageRequested)
        {
            if (tutorial_->MatchesTarget("menu_page_rename"))
            {
                tutorial_->AdvancePhase();
            }
            OpenPageRenamePopup(documentState_.selection.pageIndex);
        }

        const bool removePageRequested = ImGui::MenuItem("Remove current page from window", nullptr, false, ActivePage() != nullptr);
        ShowItemTooltip("Detach the current page from the window while keeping its JSON file.");
        if (removePageRequested)
        {
            OpenPageManagementPopup(PageManagementAction::RemoveFromWindow, documentState_.selection.pageIndex);
        }

        const bool deletePageRequested = ImGui::MenuItem("Delete current page asset...", "Del", false, ActivePage() != nullptr);
        ShowItemTooltip("Open the confirmation flow that removes the page and marks its JSON file for deletion on the next save.");
        if (deletePageRequested)
        {
            OpenPageManagementPopup(PageManagementAction::DeleteAsset, documentState_.selection.pageIndex);
        }
        ImGui::EndMenu();
    }
    else if (tutorial_->ShouldResetPageMenuPhaseOnClose())
    {
        tutorial_->ResetPhase();
    }

    const bool reticleMenuOpen = ImGui::BeginMenu("Reticle", hasOpenWindow);
    tutorial_->DrawHalo("menu_reticle", "Click Reticle", "Open the reticle-template actions used by the tutorial.");
    if (ImGui::IsItemClicked() && tutorial_->MatchesTarget("menu_reticle"))
    {
        tutorial_->AdvancePhase();
    }
    if (reticleMenuOpen)
    {
        const bool newLibraryReticleRequested = ImGui::MenuItem("New library reticle from primitive");
        ShowItemTooltip("Create a new shared reticle template.");
        tutorial_->DrawHalo(
            "menu_reticle_new",
            "Click New library reticle from primitive",
            "Open the reticle dialog and create the tutorial template shown in this step.");
        if (newLibraryReticleRequested)
        {
            if (tutorial_->MatchesTarget("menu_reticle_new"))
            {
                tutorial_->AdvancePhase();
            }
            OpenNewLibraryReticlePopup();
        }

        const bool hasFocusedLibraryReticle =
            (documentState_.selection.kind == SelectionKind::LibraryReticle || documentState_.selection.kind == SelectionKind::LibraryPrimitive) &&
            SelectedLibraryReticle() != nullptr;
        const bool hasSelectedLibraryPrimitive =
            documentState_.selection.kind == SelectionKind::LibraryPrimitive && SelectedLibraryPrimitive() != nullptr;
        const bool copyLibrarySelectionRequested = ImGui::MenuItem(
            hasSelectedLibraryPrimitive ? "Copy selected primitive" : "Copy selected library reticle",
            "Ctrl+C",
            false,
            hasSelectedLibraryPrimitive || hasFocusedLibraryReticle);
        ShowItemTooltip(
            hasSelectedLibraryPrimitive
                ? "Copy the focused primitive into the reticle-studio clipboard."
                : "Copy the focused shared reticle template into the editor clipboard.");
        if (copyLibrarySelectionRequested)
        {
            if (hasSelectedLibraryPrimitive)
            {
                CopySelectedLibraryPrimitive();
            }
            else
            {
                CopySelectedLibraryReticle();
            }
        }

        const bool pasteLibrarySelectionRequested = ImGui::MenuItem(
            hasSelectedLibraryPrimitive ? "Paste copied primitive" : "Paste copied library reticle",
            "Ctrl+V",
            false,
            hasSelectedLibraryPrimitive ? clipboardState_.libraryPrimitiveClipboard.has_value() : clipboardState_.libraryReticleClipboard.has_value());
        ShowItemTooltip(
            hasSelectedLibraryPrimitive
                ? "Paste the copied primitive into the focused reticle template."
                : "Paste the copied shared reticle template as one new library entry.");
        if (pasteLibrarySelectionRequested)
        {
            if (hasSelectedLibraryPrimitive)
            {
                PasteCopiedLibraryPrimitive();
            }
            else
            {
                PasteCopiedLibraryReticle();
            }
        }

        const bool duplicateReticleRequested =
            ImGui::MenuItem("Duplicate selected library reticle", nullptr, false, hasFocusedLibraryReticle);
        ShowItemTooltip("Duplicate the focused library reticle under a new template id.");
        if (duplicateReticleRequested)
        {
            OpenDuplicateLibraryReticlePopup();
        }

        const bool renameReticleRequested =
            ImGui::MenuItem("Rename selected library reticle globally...", nullptr, false, hasFocusedLibraryReticle);
        ShowItemTooltip("Rename the focused library reticle template safely across the current asset tree and every page that references it.");
        tutorial_->DrawHalo(
            "menu_reticle_rename",
            "Click Rename selected library reticle globally...",
            "Open the safe reticle-rename popup, review the shared references, then close it without executing the rename.");
        if (renameReticleRequested)
        {
            if (tutorial_->MatchesTarget("menu_reticle_rename"))
            {
                tutorial_->AdvancePhase();
            }
            OpenReticleRenamePopup(documentState_.selection.libraryReticleId);
        }

        const bool deleteReticleRequested =
            ImGui::MenuItem("Delete selected library reticle", "Del", false, hasFocusedLibraryReticle);
        ShowItemTooltip("Delete the focused library reticle template from the shared library.");
        if (deleteReticleRequested)
        {
            DeleteSelectedLibraryReticle();
        }
        ImGui::EndMenu();
    }
    else if (tutorial_->ShouldResetReticleMenuPhaseOnClose())
    {
        tutorial_->ResetPhase();
    }

    if (ImGui::BeginMenu("Help"))
    {
        const bool tutorialRequested = ImGui::MenuItem("Tutorial", nullptr, tutorial_->IsCoachVisible());
        ShowItemTooltip("Open the guided discovery mode for the editor and tutorial assets.");
        if (tutorialRequested)
        {
            tutorial_->OpenFlow();
        }
        ImGui::EndMenu();
    }

    ImGui::TextDisabled("|");
    const char* titleLabel = hasOpenWindow && !documentState_.loaded.window.title.empty() ? documentState_.loaded.window.title.c_str() : "No asset open";
    ImGui::Text("%s", titleLabel);
    if (hasOpenWindow)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("[%s]", documentState_.windowFile.filename().string().c_str());
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", workflowState_.statusMessage.c_str());

    ImGui::EndMainMenuBar();
}

void EditorApplication::DrawRootLayout()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin(
        "MFD Editor Root",
        nullptr,
        ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar(2);

    const float totalWidth = ImGui::GetContentRegionAvail().x;
    const float totalHeight = ImGui::GetContentRegionAvail().y;
    const float splitterCount = static_cast<float>((layoutState_.sidebarVisible ? 1 : 0) + (layoutState_.inspectorVisible ? 1 : 0));
    const float reservedInspectorWidth = layoutState_.inspectorVisible ? layoutState_.inspectorWidth : 0.0f;

    if (layoutState_.sidebarVisible)
    {
        const float maxSidebarWidth = std::max(
            kMinSidebarWidth, totalWidth - reservedInspectorWidth - kMinWorkspaceWidth - splitterCount * editor::ui::kPaneSplitterWidth);
        layoutState_.sidebarWidth = std::floor(std::clamp(layoutState_.sidebarWidth, kMinSidebarWidth, maxSidebarWidth));
    }

    if (layoutState_.inspectorVisible)
    {
        const float maxInspectorWidth = std::max(
            kMinInspectorWidth, totalWidth - (layoutState_.sidebarVisible ? layoutState_.sidebarWidth : 0.0f) - kMinWorkspaceWidth -
                                   splitterCount * editor::ui::kPaneSplitterWidth);
        layoutState_.inspectorWidth = std::floor(std::clamp(layoutState_.inspectorWidth, kMinInspectorWidth, maxInspectorWidth));
    }

    if (layoutState_.sidebarVisible)
    {
        ImGui::BeginChild("Sidebar", ImVec2(layoutState_.sidebarWidth, 0.0f), true);
        DrawSidebar();
        ImGui::EndChild();

        ImGui::SameLine();
        if (DrawVerticalSplitter("##SidebarSplitter", totalHeight))
        {
            const float nextSidebarWidth = layoutState_.sidebarWidth + ImGui::GetIO().MouseDelta.x;
            const float nextMaxSidebarWidth =
                std::max(kMinSidebarWidth,
                         totalWidth - (layoutState_.inspectorVisible ? layoutState_.inspectorWidth : 0.0f) - kMinWorkspaceWidth -
                             splitterCount * editor::ui::kPaneSplitterWidth);
            layoutState_.sidebarWidth = std::floor(std::clamp(nextSidebarWidth, kMinSidebarWidth, nextMaxSidebarWidth));
        }

        ImGui::SameLine();
    }

    const float workspaceWidth = std::floor(std::max(kMinWorkspaceWidth,
                                                     totalWidth - (layoutState_.sidebarVisible ? layoutState_.sidebarWidth : 0.0f) -
                                                         (layoutState_.inspectorVisible ? layoutState_.inspectorWidth : 0.0f) -
                                                         splitterCount * editor::ui::kPaneSplitterWidth));
    ImGui::BeginChild("Workspace", ImVec2(workspaceWidth, 0.0f), true);
    DrawWorkspace();
    ImGui::EndChild();

    if (layoutState_.inspectorVisible)
    {
        ImGui::SameLine();
        if (DrawVerticalSplitter("##InspectorSplitter", totalHeight))
        {
            const float nextInspectorWidth = layoutState_.inspectorWidth - ImGui::GetIO().MouseDelta.x;
            const float nextMaxInspectorWidth =
                std::max(kMinInspectorWidth,
                         totalWidth - (layoutState_.sidebarVisible ? layoutState_.sidebarWidth : 0.0f) - kMinWorkspaceWidth -
                             splitterCount * editor::ui::kPaneSplitterWidth);
            layoutState_.inspectorWidth = std::floor(std::clamp(nextInspectorWidth, kMinInspectorWidth, nextMaxInspectorWidth));
        }

        ImGui::SameLine();
        const float inspectorPanelWidth = std::max(0.0f, std::floor(ImGui::GetContentRegionAvail().x));
        ImGui::BeginChild("Inspector", ImVec2(inspectorPanelWidth, 0.0f), true);
        DrawInspector();
        ImGui::EndChild();
    }

    ImGui::End();
}

bool EditorApplication::IsLibraryStudioWorkspaceVisible() const
{
    using editor::tutorial::TutorialStepId;

    const bool forcePagePreviewTutorialWorkspace =
        tutorial_->IsStep(static_cast<int>(TutorialStepId::ShowLayerInspector)) ||
        tutorial_->IsStep(static_cast<int>(TutorialStepId::ShowMinimap)) ||
        tutorial_->IsStep(static_cast<int>(TutorialStepId::ShowReticleUsageHighlights)) ||
        tutorial_->IsStep(static_cast<int>(TutorialStepId::ShowProblemsPanel)) ||
        tutorial_->IsStep(static_cast<int>(TutorialStepId::ToggleFullscreenPreview)) ||
        tutorial_->IsStep(static_cast<int>(TutorialStepId::InspectReticleRenameWorkflow)) ||
        tutorial_->IsStep(static_cast<int>(TutorialStepId::InspectDesignExportWorkflow));
    return !forcePagePreviewTutorialWorkspace &&
           (documentState_.selection.kind == SelectionKind::LibraryReticle || documentState_.selection.kind == SelectionKind::LibraryPrimitive);
}

bool EditorApplication::CanToggleFullscreenPagePreview() const
{
    return services_.fullscreenPreview.IsActive() || (HasOpenWindow() && !IsLibraryStudioWorkspaceVisible());
}

void EditorApplication::DrawSidebar()
{
    ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "MFD Editor");
    ImGui::TextDisabled("Work directly in the page visualization.");
    ImGui::Separator();

    if (!HasOpenWindow())
    {
        ImGui::TextWrapped("No authored window is open yet.");
        ImGui::Spacing();
        ImGui::TextDisabled("Use File > Open window asset... or File > New window from scratch.");

        if (!workflowState_.lastRuntimeError.empty())
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Runtime: %s", workflowState_.lastRuntimeError.c_str());
        }
        return;
    }

    DrawPageTree();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    DrawLibraryTree();

    if (!workflowState_.lastRuntimeError.empty())
    {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Runtime: %s", workflowState_.lastRuntimeError.c_str());
    }
}

void EditorApplication::DrawWorkspace()
{
    if (!HasOpenWindow())
    {
        tutorial_->DrawCoach();
        DrawEmptyWorkspacePlaceholder();
        return;
    }

    const std::vector<std::string> pagePreviewProblems = BuildPagePreviewProblemMessages();
    const bool hasPagePreviewProblems = !pagePreviewProblems.empty();
    const bool libraryStudioVisible = IsLibraryStudioWorkspaceVisible();
    const bool fullscreenPreviewActive = services_.fullscreenPreview.IsActive();

    if (fullscreenPreviewActive)
    {
        ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Page preview");
        DrawPagePreviewHeaderControls("##FullscreenPagePreviewViewMenu", hasPagePreviewProblems);
        ImGui::TextDisabled("Fullscreen preview keeps the page canvas interactive. Press F11 or Esc to restore the editor layout.");
        tutorial_->DrawCoach();
        ImGui::Separator();

        DrawPagePreviewWorkspace(
            pagePreviewProblems,
            "FullscreenPagePreviewPanel", "FullscreenPageLayersPanel", "FullscreenPageProblemsPanel", true, true);
        return;
    }

    if (libraryStudioVisible)
    {
        const float totalWidth = ImGui::GetContentRegionAvail().x;
        const float totalHeight = ImGui::GetContentRegionAvail().y;

        if (!layoutState_.pagePreviewViewOptions.showPageContext)
        {
            DrawReticleStudioPanel();
            return;
        }

        if (layoutState_.libraryStudioPageWidth <= 0.0f)
        {
            layoutState_.libraryStudioPageWidth = std::max(kMinPageContextWidth, totalWidth * 0.56f);
        }

        const float maxPageWidth = std::max(kMinPageContextWidth,
                                            totalWidth - kMinReticleStudioWidth - editor::ui::kPaneSplitterWidth);
        layoutState_.libraryStudioPageWidth = std::floor(std::clamp(layoutState_.libraryStudioPageWidth, kMinPageContextWidth, maxPageWidth));
        const float pageWidth = layoutState_.libraryStudioPageWidth;
        const float studioWidth = std::max(kMinReticleStudioWidth,
                                           std::floor(totalWidth - pageWidth - editor::ui::kPaneSplitterWidth));

        ImGui::BeginChild("PageContextPanel", ImVec2(pageWidth, 0.0f), true);
        ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Page context");
        DrawPagePreviewHeaderControls("##PageContextViewMenu", hasPagePreviewProblems, false);
        ImGui::TextDisabled("Keep drag & drop and page composition visible while editing the library reticle.");
        tutorial_->DrawCoach();
        ImGui::Separator();

        DrawPagePreviewWorkspace(pagePreviewProblems,
                                 "PageContextPreviewPanel",
                                 "PageContextLayersPanel",
                                 "PageContextProblemsPanel",
                                 documentState_.selection.kind == SelectionKind::PageReticle ||
                                     documentState_.selection.kind == SelectionKind::PageTitle ||
                                     documentState_.selection.kind == SelectionKind::PageStrobe,
                                 documentState_.selection.kind == SelectionKind::PageReticle ||
                                     documentState_.selection.kind == SelectionKind::PageTitle ||
                                     documentState_.selection.kind == SelectionKind::PageStrobe);
        ImGui::EndChild();

        ImGui::SameLine();
        if (DrawVerticalSplitter("##WorkspaceSplitter", totalHeight))
        {
            const float nextPageWidth = layoutState_.libraryStudioPageWidth + ImGui::GetIO().MouseDelta.x;
            const float nextMaxPageWidth = std::max(kMinPageContextWidth,
                                                    totalWidth - kMinReticleStudioWidth - editor::ui::kPaneSplitterWidth);
            layoutState_.libraryStudioPageWidth = std::floor(std::clamp(nextPageWidth, kMinPageContextWidth, nextMaxPageWidth));
        }

        ImGui::SameLine();
        DrawReticleStudioPanel(studioWidth);
        return;
    }

    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Page preview");
    DrawPagePreviewHeaderControls("##MainPagePreviewViewMenu", hasPagePreviewProblems);
    ImGui::TextDisabled("Use View to toggle preview-only overlays without touching authored JSON assets.");
    tutorial_->DrawCoach();
    ImGui::Separator();

    DrawPagePreviewWorkspace(
        pagePreviewProblems,
        "MainPagePreviewPanel", "MainPageLayersPanel", "MainPageProblemsPanel", true, true);
}

void EditorApplication::DrawPagePreviewWorkspace(const std::vector<std::string>& pagePreviewProblems,
                                                 const char* previewChildId,
                                                 const char* layersChildId,
                                                 const char* problemsChildId,
                                                 const bool drawPreviewOverlays,
                                                 const bool handlePreviewInteraction)
{
    const ImVec2 available = ImGui::GetContentRegionAvail();
    editor::WorkspaceLayoutRequest layoutRequest;
    layoutRequest.width = std::max(0.0f, std::floor(available.x));
    layoutRequest.height = std::max(0.0f, std::floor(available.y));
    layoutRequest.spacing = ImGui::GetStyle().ItemSpacing.y;
    layoutRequest.showLeadingPanel = layoutState_.pagePreviewViewOptions.showLayerInspector;
    layoutRequest.leadingPanelWidth = kLayerInspectorDockWidth;
    layoutRequest.minLeadingPanelWidth = 196.0f;
    layoutRequest.showBottomPanel = layoutState_.pagePreviewViewOptions.showProblemsPanel;
    layoutRequest.bottomPanelHeight = kPreviewProblemsDockHeight;
    layoutRequest.minBottomPanelHeight = 88.0f;
    layoutRequest.minCenterWidth = 220.0f;
    layoutRequest.minCenterHeight = 168.0f;
    const editor::WorkspaceLayoutResult layout = editor::ComputeWorkspaceLayout(layoutRequest);
    const mfd::PageDefinition* activePage = ActivePage();

    if (layout.leadingPanel.IsVisible())
    {
        ImGui::BeginChild(layersChildId, ImVec2(layout.leadingPanel.width, layout.leadingPanel.height), true);
        if (activePage != nullptr)
        {
            DrawLayerInspectorPanel(*activePage);
        }
        else
        {
            ImGui::TextColored(ImVec4(0.85f, 0.91f, 0.96f, 1.0f), "Layer Inspector");
            ImGui::TextDisabled("Open one page to inspect editor layers.");
        }
        ImGui::EndChild();
        ImGui::SameLine();
    }

    ImGui::BeginGroup();
    if (layout.previewPanel.IsVisible())
    {
        ImGui::BeginChild(previewChildId, ImVec2(layout.previewPanel.width, layout.previewPanel.height), true);

        ViewportState pageViewport;
        pageViewport.origin = ImGui::GetCursorScreenPos();
        pageViewport.size = ImGui::GetContentRegionAvail();
        pageViewport.valid = pageViewport.size.x > 8.0f && pageViewport.size.y > 8.0f;

        if (activePage != nullptr)
        {
            pageViewport.view = layoutState_.pagePreviewView;
        }

        if (pageViewport.valid && activePage != nullptr)
        {
            DrawPagePreview(pageViewport);
            if (drawPreviewOverlays)
            {
                DrawPreviewOverlays(pageViewport);
            }
            if (handlePreviewInteraction)
            {
                HandlePreviewInteraction(pageViewport);
            }
            DrawPageReticleContextMenu();
        }
        else
        {
            if (HasOpenWindow() && documentState_.loaded.document.pages.empty())
            {
                ImGui::TextDisabled("No page yet.");
                ImGui::TextWrapped("Create the first page to start previewing and authoring this window.");
                if (AccentButton("Create first page##preview"))
                {
                    OpenNewPagePopup();
                }
                ShowItemTooltip("Open the page-creation workflow for this window.");
            }
            else
            {
                ImGui::TextDisabled("No active page to preview.");
            }
        }

        ImGui::EndChild();
    }

    if (layout.bottomPanel.IsVisible())
    {
        ImGui::BeginChild(problemsChildId, ImVec2(layout.bottomPanel.width, layout.bottomPanel.height), true);
        DrawProblemsPanel(pagePreviewProblems);
        ImGui::EndChild();
    }
    ImGui::EndGroup();
}

void EditorApplication::DrawReticleStudioPanel(const float width)
{
    const float panelWidth = width > 0.0f ? width : std::max(0.0f, std::floor(ImGui::GetContentRegionAvail().x));
    ImGui::BeginChild("ReticleStudioPanel", ImVec2(panelWidth, 0.0f), true);
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "Reticle studio");

    const ImGuiStyle& style = ImGui::GetStyle();
    const float buttonWidth = ImGui::CalcTextSize("View").x + style.FramePadding.x * 2.0f;
    ImGui::SameLine();
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - buttonWidth));
    if (ImGui::Button("View##ReticleStudioDisplay"))
    {
        ImGui::OpenPopup(kReticleStudioDisplayPopupId);
    }

    if (ImGui::BeginPopup(kReticleStudioDisplayPopupId))
    {
        ImGui::Checkbox("Show page context", &layoutState_.pagePreviewViewOptions.showPageContext);
        ImGui::Checkbox("Show primitive names", &layoutState_.libraryStudioShowPrimitiveLabels);
        ImGui::Checkbox("Show gizmos", &layoutState_.libraryStudioShowGizmos);
        ImGui::EndPopup();
    }

    ImGui::TextDisabled("Click a primitive to focus it, drag the handles to edit its geometry, then use Ctrl+C / Ctrl+V to duplicate it.");
    ImGui::Separator();

    ViewportState studioViewport;
    studioViewport.origin = ImGui::GetCursorScreenPos();
    studioViewport.size = ImGui::GetContentRegionAvail();
    studioViewport.valid = studioViewport.size.x > 8.0f && studioViewport.size.y > 8.0f;
    studioViewport.view = layoutState_.libraryPreviewView;

    if (studioViewport.valid)
    {
        DrawLibraryPreview(studioViewport);
        DrawLibraryPreviewOverlays(studioViewport);
        HandleLibraryPreviewInteraction(studioViewport);
    }
    ImGui::EndChild();
}

void EditorApplication::DrawEmptyWorkspacePlaceholder()
{
    const ImVec2 available = ImGui::GetContentRegionAvail();
    if (available.x <= 8.0f || available.y <= 8.0f)
    {
        return;
    }

    const char* headline = "Open or create assets";
    const char* description =
        "Start with a new authored window or browse to an existing window JSON.\n"
        "Nothing is loaded automatically when the editor starts, but you can launch the guided tutorial.";

    const ImVec2 headlineSize = ImGui::CalcTextSize(headline);
    const ImVec2 descriptionSize = ImGui::CalcTextSize(description);
    const float buttonRowWidth = 420.0f;
    const float totalHeight = headlineSize.y + descriptionSize.y + 126.0f;
    const ImVec2 start(
        std::max(0.0f, (available.x - std::max(std::max(headlineSize.x, descriptionSize.x), buttonRowWidth)) * 0.5f),
        std::max(0.0f, (available.y - totalHeight) * 0.5f));

    ImGui::SetCursorPos(start);
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "%s", headline);
    ImGui::SetCursorPosX(start.x);
    ImGui::TextDisabled("%s", description);
    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::SetCursorPosX(start.x);
    if (AccentButton("Open window asset..."))
    {
        OpenWindowAssetFromFileExplorer();
    }

    ImGui::SetCursorPosX(start.x);
    if (ImGui::Button("New window from scratch", ImVec2(220.0f, 0.0f)))
    {
        OpenNewWindowPopup();
    }

    ImGui::SetCursorPosX(start.x);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.54f, 0.61f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.66f, 0.73f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.44f, 0.52f, 1.00f));
    if (ImGui::Button("Launch the tutorial", ImVec2(220.0f, 0.0f)))
    {
        tutorial_->OpenFlow();
    }
    ImGui::PopStyleColor(3);
}

void EditorApplication::DrawInspector()
{
    if (!HasOpenWindow())
    {
        ImGui::TextDisabled("No asset is open.");
        ImGui::TextWrapped("Open one existing window asset or create a new window to edit pages, reticles and strobe settings.");
        return;
    }

    switch (documentState_.selection.kind)
    {
    case SelectionKind::Window:
        DrawWindowInspector();
        break;
    case SelectionKind::Page:
        DrawPageInspector();
        break;
    case SelectionKind::PageReticle:
        DrawPageReticleInspector();
        break;
    case SelectionKind::PageTitle:
        DrawSelectedPageTitleInspector();
        break;
    case SelectionKind::PageStrobe:
        DrawSelectedPageStrobeInspector();
        break;
    case SelectionKind::LibraryReticle:
        DrawLibraryReticleInspector();
        break;
    case SelectionKind::LibraryPrimitive:
        DrawLibraryReticleInspector();
        ImGui::Separator();
        DrawLibraryPrimitiveInspector();
        break;
    }
}

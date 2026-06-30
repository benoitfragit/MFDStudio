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
#include <string>
#include <vector>

#include "internal/application/EditorApplicationInternal.h"
#include "internal/application/EditorSidebarLayout.h"
#include "internal/application/EditorViewportGrid.h"
#include "EditorResponsiveLayout.h"
#include "EditorTutorialController.h"
#include "EditorTutorialData.h"
#include "EditorIcons.h"
#include "EditorUiTheme.h"
#include "EditorWorkspaceLayout.h"

namespace
{
using editor::ui::AccentButton;
using editor::ui::DisabledTextWrapped;
using editor::ui::DrawVerticalSplitter;
using editor::ui::EditorIcon;
using editor::ui::IconButton;
using editor::ui::IconText;
using editor::ui::ShowItemTooltip;

// Auxiliary panels expose a usable technical minimum, not a hard resize floor: when
// the window shrinks they compress to these widths and then auto-collapse so the
// central workspace always keeps `kMinWorkspaceWidth` of editable space.
constexpr float kMinSidebarWidth = 200.0f;
constexpr float kMinInspectorWidth = 280.0f;
constexpr float kMinWorkspaceWidth = 360.0f;
constexpr float kLayerInspectorDockWidth = 248.0f;
constexpr float kPreviewProblemsDockHeight = 176.0f;
constexpr float kSidebarSectionMinVisibleRows = 3.0f;

void DrawRuntimeErrorBanner(const std::string& runtimeError)
{
    if (runtimeError.empty())
    {
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.55f, 0.55f, 1.0f));
    ImGui::TextWrapped("Runtime error: %s", runtimeError.c_str());
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();
}
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

        const std::vector<std::filesystem::path> recentWindowFiles = recentWindows_.ExistingEntries();
        const bool recentMenuOpen = ImGui::BeginMenu("Open recent window", !recentWindowFiles.empty());
        ShowItemTooltip("Reopen one of the recently authored windows.");
        if (recentMenuOpen)
        {
            for (const std::filesystem::path& entry : recentWindowFiles)
            {
                const std::string label = entry.filename().string() + "##recent_" + entry.generic_string();
                if (ImGui::MenuItem(label.c_str()))
                {
                    LoadWindowConfiguration(entry);
                }
                ShowItemTooltip(entry.generic_string().c_str());
            }
            ImGui::EndMenu();
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
            if (workflowState_.documentDirty)
            {
                workflowState_.reloadConfirmRequested = true;
            }
            else
            {
                LoadWindowConfiguration(documentState_.windowFile);
            }
        }
        ImGui::EndMenu();
    }
    else if (tutorial_->ShouldResetFileMenuPhaseOnClose())
    {
        tutorial_->ResetPhase();
    }

    if (ImGui::BeginMenu("Edit"))
    {
        const bool undoRequested = ImGui::MenuItem("Undo", "Ctrl+Z", false, documentState_.history.CanUndo());
        ShowItemTooltip("Restore the previous editor snapshot.");
        if (undoRequested)
        {
            Undo();
        }

        const bool redoRequested = ImGui::MenuItem("Redo", "Ctrl+Y", false, documentState_.history.CanRedo());
        ShowItemTooltip("Reapply the editor snapshot that was just undone.");
        if (redoRequested)
        {
            Redo();
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

    DrawViewMenu();

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

        // Remove-from-window and delete-asset now live on the page tree row and page inspector
        // icon bars, so the menu keeps only creation, import and the safe global rename.
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

        // Duplicate and delete now live on the library tree row and library inspector icon bars,
        // so the menu keeps creation, clipboard (which also serves primitives) and global rename.
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
    const bool showDirtyMarker = hasOpenWindow && workflowState_.documentDirty;
    ImGui::Text("%s%s", showDirtyMarker ? "* " : "", titleLabel);
    if (showDirtyMarker)
    {
        ShowItemTooltip("This window has unsaved changes. Save with Ctrl+S.");
    }
    if (hasOpenWindow)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("[%s]", documentState_.windowFile.filename().string().c_str());
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", workflowState_.statusMessage.c_str());

    if (layoutState_.shellLayoutMode != editor::ShellLayoutMode::Wide)
    {
        ImGui::SameLine();
        const char* layoutModeLabel =
            layoutState_.shellLayoutMode == editor::ShellLayoutMode::Compact ? "Compact layout" : "Focus layout";
        ImGui::TextColored(ImVec4(0.40f, 0.74f, 0.95f, 1.0f), "[%s]", layoutModeLabel);
        ShowItemTooltip(
            "The window is narrow: auxiliary panels auto-collapse to protect the workspace. "
            "Use the View menu or widen the window to restore them.");
    }

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

    // Compute the validation diagnostics once per frame so the sidebar (drawn first) and the
    // workspace problems panel share a single source of truth instead of recomputing them. The
    // sidebar folds them into per-entity badges; the menu-bar View entry reads the stash next frame.
    const std::vector<editor::PagePreviewProblem> pagePreviewProblems = BuildPagePreviewProblems();
    layoutState_.pagePreviewHasProblems = !pagePreviewProblems.empty();
    const editor::SidebarProblemSummary sidebarProblems =
        editor::SummarizeSidebarProblems(pagePreviewProblems, CollectNormalizedPageIds());

    // Resolve the responsive arrangement from the user's width and visibility
    // preferences. The helper never mutates those preferences: it returns effective
    // widths and reports transient auto-collapse separately, so a narrow window never
    // overwrites what the user expects when the window grows back.
    editor::ShellLayoutRequest layoutRequest;
    layoutRequest.totalWidth = std::max(0.0f, totalWidth);
    layoutRequest.splitterWidth = editor::ui::kPaneSplitterWidth;
    layoutRequest.minWorkspaceWidth = kMinWorkspaceWidth;
    layoutRequest.sidebar.wantVisible = layoutState_.sidebarVisible;
    layoutRequest.sidebar.preferredWidth = layoutState_.sidebarWidth;
    layoutRequest.sidebar.minWidth = kMinSidebarWidth;
    layoutRequest.inspector.wantVisible = layoutState_.inspectorVisible;
    layoutRequest.inspector.preferredWidth = layoutState_.inspectorWidth;
    layoutRequest.inspector.minWidth = kMinInspectorWidth;
    const editor::ShellLayoutResult layout = editor::ComputeShellLayout(layoutRequest);

    // Stash transient layout state so the menu-bar indicator can surface auto-collapse
    // (read by `DrawMenuBar` on the next frame, where mode changes are rare and benign).
    layoutState_.shellLayoutMode = layout.mode;
    layoutState_.sidebarAutoCollapsed = layout.sidebar.autoCollapsed;
    layoutState_.inspectorAutoCollapsed = layout.inspector.autoCollapsed;

    const float splitterTotal = editor::ui::kPaneSplitterWidth *
        static_cast<float>((layout.sidebar.visible ? 1 : 0) + (layout.inspector.visible ? 1 : 0));

    if (layout.sidebar.visible)
    {
        // The sidebar itself must never scroll: its Pages and Reticle sections are each sized to
        // fit the available height and own their own scroll regions. Without these flags rounding
        // can spill a few pixels and raise a spurious outer scrollbar over the two inner ones.
        ImGui::BeginChild("Sidebar",
                          ImVec2(std::floor(layout.sidebar.width), 0.0f),
                          true,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        DrawSidebar(sidebarProblems);
        ImGui::EndChild();

        ImGui::SameLine();
        if (DrawVerticalSplitter("##SidebarSplitter", totalHeight))
        {
            const float maxSidebarWidth =
                std::max(kMinSidebarWidth, totalWidth - splitterTotal - kMinWorkspaceWidth - layout.inspector.width);
            const float nextSidebarWidth = layoutState_.sidebarWidth + ImGui::GetIO().MouseDelta.x;
            layoutState_.sidebarWidth = std::floor(std::clamp(nextSidebarWidth, kMinSidebarWidth, maxSidebarWidth));
        }

        ImGui::SameLine();
    }

    ImGui::BeginChild("Workspace", ImVec2(std::floor(std::max(0.0f, layout.workspaceWidth)), 0.0f), true);
    DrawWorkspace(pagePreviewProblems);
    ImGui::EndChild();

    if (layout.inspector.visible)
    {
        ImGui::SameLine();
        if (DrawVerticalSplitter("##InspectorSplitter", totalHeight))
        {
            const float maxInspectorWidth =
                std::max(kMinInspectorWidth, totalWidth - splitterTotal - kMinWorkspaceWidth - layout.sidebar.width);
            const float nextInspectorWidth = layoutState_.inspectorWidth - ImGui::GetIO().MouseDelta.x;
            layoutState_.inspectorWidth = std::floor(std::clamp(nextInspectorWidth, kMinInspectorWidth, maxInspectorWidth));
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
        tutorial_->IsStep(static_cast<int>(TutorialStepId::HideWorkspacePanels)) ||
        tutorial_->IsStep(static_cast<int>(TutorialStepId::ToggleFullscreenPreview)) ||
        tutorial_->IsStep(static_cast<int>(TutorialStepId::InspectReticleRenameWorkflow)) ||
        tutorial_->IsStep(static_cast<int>(TutorialStepId::InspectDesignExportWorkflow));
    return !forcePagePreviewTutorialWorkspace &&
           (documentState_.selection.kind == SelectionKind::LibraryReticle || documentState_.selection.kind == SelectionKind::LibraryPrimitive);
}

bool EditorApplication::CanToggleFullscreenPagePreview() const
{
    // Fullscreen preview only rearranges editor-only panel visibility and pane sizes; it never reads
    // document state, so it stays available in every workspace, including before a window is opened.
    return true;
}

void EditorApplication::DrawSidebar(const editor::SidebarProblemSummary& problems)
{
    if (!HasOpenWindow())
    {
        ImGui::TextColored(ImVec4(0.33f, 0.86f, 0.78f, 1.0f), "MFD Editor");
        DisabledTextWrapped("Work directly in the page visualization.");
        ImGui::Separator();
        ImGui::TextWrapped("No authored window is open yet.");
        ImGui::Spacing();
        DisabledTextWrapped("Use File > Open window asset... or File > New window from scratch.");
        ImGui::Spacing();
        DrawRuntimeErrorBanner(workflowState_.lastRuntimeError);
        return;
    }

    // Surface runtime failures at the top of the panel where they cannot be missed.
    DrawRuntimeErrorBanner(workflowState_.lastRuntimeError);

    const bool tutorialActive = tutorial_->IsCoachVisible();

    // A filter keeps long page and library lists scannable. It is intentionally
    // disabled and cleared during the guided tutorial so every halo target stays visible.
    ImGui::BeginDisabled(tutorialActive);
    IconText(EditorIcon::Search);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint(
        "##SidebarFilter",
        "Filter pages and reticles...",
        layoutState_.sidebarFilter.data(),
        layoutState_.sidebarFilter.size());
    ImGui::EndDisabled();
    ShowItemTooltip(
        "Type to filter pages and reticle templates by name. Start with 'page:', 'reticle:' or "
        "'problem:' to aim the same filter at one section. Disabled while the tutorial is running.");
    if (tutorialActive && layoutState_.sidebarFilter[0] != '\0')
    {
        layoutState_.sidebarFilter[0] = '\0';
    }

    // Surface a single, navigable count of pending validation problems. Clicking it opens the
    // Problems panel, which already resolves each diagnostic to the offending entity.
    if (problems.total > 0)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
                           "! %zu validation problem%s",
                           problems.total,
                           problems.total == 1U ? "" : "s");
        ShowItemTooltip("Click to open the Problems panel and jump to each issue.");
        if (ImGui::IsItemClicked())
        {
            layoutState_.pagePreviewViewOptions.showProblemsPanel = true;
        }
    }
    ImGui::Spacing();

    const editor::SidebarFilterQuery filter = ResolveSidebarFilter();
    const int visiblePages = CountVisibleSidebarPages(filter, problems);
    const int visibleReticles = CountVisibleSidebarReticles(filter, problems);

    const float sidebarBodyHeight = std::max(0.0f, ImGui::GetContentRegionAvail().y);
    const float sidebarBodyWidth = std::max(0.0f, ImGui::GetContentRegionAvail().x);
    const float sectionRowHeight = ImGui::GetFrameHeightWithSpacing();
    const float sectionChromeHeight =
        ImGui::GetFrameHeightWithSpacing() +
        ImGui::GetFrameHeightWithSpacing() +
        ImGui::GetStyle().ItemSpacing.y;
    const float minSectionHeight =
        sectionChromeHeight + sectionRowHeight * kSidebarSectionMinVisibleRows;

    // The Pages/Reticle split is user-driven: a stored fraction (0.5 = even) drives the preferred
    // Pages height, and the draggable splitter below feeds the fraction back. The splitter gap is
    // reserved as the section spacing so both sections plus the handle exactly fill the body.
    const float splitterThickness = editor::ui::kPaneSplitterWidth;
    const float distributableHeight = std::max(0.0f, sidebarBodyHeight - splitterThickness);
    const float pagesFraction = std::clamp(layoutState_.sidebarPagesSplitFraction, 0.0f, 1.0f);
    const float pagesPreferredHeight = distributableHeight * pagesFraction;
    const editor::app::SidebarSectionLayoutResult sectionLayout =
        editor::app::ComputeSidebarSectionLayout(
            {sidebarBodyHeight, splitterThickness, pagesPreferredHeight, minSectionHeight, minSectionHeight});

    DrawSidebarPagesSection(filter, problems, visiblePages, sectionLayout.pagesHeight);
    if (sectionLayout.sectionSpacing > 0.0f)
    {
        if (editor::ui::DrawHorizontalSplitter("##SidebarSectionSplitter", sidebarBodyWidth, sectionLayout.sectionSpacing))
        {
            layoutState_.sidebarPagesSplitFraction = editor::app::ResolveSidebarPagesFraction(
                sectionLayout.pagesHeight, ImGui::GetIO().MouseDelta.y, distributableHeight);
        }
        ShowItemTooltip("Drag to resize the Pages and Reticle library sections.");
    }
    DrawSidebarReticleSection(filter, problems, visibleReticles, sectionLayout.reticlesHeight);
}

void EditorApplication::DrawSidebarPagesSection(const editor::SidebarFilterQuery& filter,
                                                const editor::SidebarProblemSummary& problems,
                                                const int visiblePages,
                                                const float sectionHeight)
{
    const bool tutorialActive = tutorial_->IsCoachVisible();
    // ImGui treats a zero-height child as "take the remaining height", which would break the split.
    const float clampedSectionHeight = sectionHeight > 0.0f ? sectionHeight : 1.0f;
    ImGui::BeginChild("SidebarPagesSection", ImVec2(0.0f, clampedSectionHeight), false);

    if (tutorialActive)
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    }

    // The visible count lives in the label while the stable id (after '###') keeps the open state
    // from resetting whenever the filter changes that count.
    const std::string pagesHeader = "Pages (" + std::to_string(visiblePages) + ")###SidebarPagesHeader";
    if (ImGui::CollapsingHeader(pagesHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawPageActionToolbar();
        ImGui::Spacing();

        const float scrollRegionHeight = std::max(1.0f, ImGui::GetContentRegionAvail().y);
        ImGui::BeginChild("SidebarPagesScrollRegion", ImVec2(0.0f, scrollRegionHeight), false);
        DrawPageTree(filter, problems);
        ImGui::EndChild();
    }

    ImGui::EndChild();
}

void EditorApplication::DrawSidebarReticleSection(const editor::SidebarFilterQuery& filter,
                                                  const editor::SidebarProblemSummary& problems,
                                                  const int visibleReticles,
                                                  const float sectionHeight)
{
    const bool tutorialActive = tutorial_->IsCoachVisible();
    // ImGui treats a zero-height child as "take the remaining height", which would break the split.
    const float clampedSectionHeight = sectionHeight > 0.0f ? sectionHeight : 1.0f;
    ImGui::BeginChild("SidebarReticleSection", ImVec2(0.0f, clampedSectionHeight), false);

    if (tutorialActive)
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    }

    const std::string libraryHeader =
        "Reticle library (" + std::to_string(visibleReticles) + ")###SidebarLibraryHeader";
    if (ImGui::CollapsingHeader(libraryHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawLibraryActionToolbar();
        ImGui::Spacing();

        const float scrollRegionHeight = std::max(1.0f, ImGui::GetContentRegionAvail().y);
        ImGui::BeginChild("SidebarReticleScrollRegion", ImVec2(0.0f, scrollRegionHeight), false);
        DrawLibraryTree(filter, problems);
        ImGui::EndChild();
    }

    ImGui::EndChild();
}

void EditorApplication::DrawPageActionToolbar()
{
    // The add-page affordance now speaks the same white-glyph icon language as the rest of the editor
    // instead of a text accent button.
    if (IconButton("##sidebar_add_page", EditorIcon::Add, "Create a new page and its backing JSON file."))
    {
        OpenNewPagePopup();
    }

    // The rename/remove/delete actions ride next to the add button rather than crowding every page row,
    // so they only surface while a page is the active selection.
    const int pageCount = static_cast<int>(documentState_.loaded.document.pages.size());
    const bool pageSelected =
        documentState_.selection.kind == SelectionKind::Page &&
        documentState_.selection.pageIndex >= 0 &&
        documentState_.selection.pageIndex < pageCount;
    if (!pageSelected)
    {
        return;
    }

    const int pageIndex = documentState_.selection.pageIndex;
    ImGui::SameLine();
    if (IconButton("##sidebar_rename_page", EditorIcon::Rename, "Rename the selected page globally."))
    {
        OpenPageRenamePopup(pageIndex);
    }
    ImGui::SameLine();
    if (IconButton("##sidebar_remove_page", EditorIcon::RemoveFromWindow, "Remove the selected page from the window."))
    {
        OpenPageManagementPopup(PageManagementAction::RemoveFromWindow, pageIndex);
    }
    ImGui::SameLine();
    if (IconButton("##sidebar_delete_page", EditorIcon::Delete, "Delete the selected page asset."))
    {
        OpenPageManagementPopup(PageManagementAction::DeleteAsset, pageIndex);
    }
}

void EditorApplication::DrawLibraryActionToolbar()
{
    if (IconButton("##sidebar_add_reticle", EditorIcon::Add, "Create a new library reticle seeded with one primitive."))
    {
        OpenNewLibraryReticlePopup();
    }

    // Resolve the template the row actions should target: the browser highlight wins, otherwise the
    // template currently edited in the studio. Captured by value so a delete cannot invalidate it.
    std::string selectedReticleId = documentState_.selection.libraryBrowserReticleId;
    if (selectedReticleId.empty() &&
        (documentState_.selection.kind == SelectionKind::LibraryReticle ||
         documentState_.selection.kind == SelectionKind::LibraryPrimitive))
    {
        selectedReticleId = documentState_.selection.libraryReticleId;
    }

    const bool reticleSelected =
        !selectedReticleId.empty() &&
        documentState_.loaded.document.reticleLibrary.count(selectedReticleId) > 0;
    if (!reticleSelected)
    {
        return;
    }

    ImGui::SameLine();
    if (IconButton("##sidebar_rename_reticle", EditorIcon::Rename, "Rename the selected template globally."))
    {
        SelectLibraryReticle(selectedReticleId);
        OpenReticleRenamePopup(selectedReticleId);
    }
    ImGui::SameLine();
    if (IconButton("##sidebar_duplicate_reticle", EditorIcon::Duplicate, "Duplicate the selected template under a new id."))
    {
        SelectLibraryReticle(selectedReticleId);
        OpenDuplicateLibraryReticlePopup();
    }
    ImGui::SameLine();
    if (IconButton("##sidebar_delete_reticle", EditorIcon::Delete, "Delete the selected template from the library."))
    {
        SelectLibraryReticle(selectedReticleId);
        DeleteSelectedLibraryReticle();
    }
}

void EditorApplication::DrawWorkspace(const std::vector<editor::PagePreviewProblem>& pagePreviewProblems)
{
    if (!HasOpenWindow())
    {
        tutorial_->DrawCoach();
        DrawEmptyWorkspacePlaceholder();
        return;
    }

    const bool libraryStudioVisible = IsLibraryStudioWorkspaceVisible();
    const bool fullscreenPreviewActive = services_.fullscreenPreview.IsActive();

    if (fullscreenPreviewActive)
    {
        // The fullscreen preview no longer opens with a "Page preview" header banner: it only
        // restated the F11/Esc shortcut already advertised in the menu and ate vertical space that
        // fullscreen is meant to reclaim. The tutorial coach still renders inline when a guided
        // step is active.
        tutorial_->DrawCoach();

        DrawPagePreviewWorkspace(
            pagePreviewProblems,
            "FullscreenPagePreviewPanel", "FullscreenPageLayersPanel", "FullscreenPageProblemsPanel", true, true);
        return;
    }

    if (libraryStudioVisible)
    {
        // Editing a library reticle hands the whole workspace row to the reticle studio.
        DrawReticleStudioPanel();
        return;
    }

    // The former "Page preview" header banner only restated that overlays live in the View menu.
    // It carried no tutorial dependency and ate vertical space, so the workspace now starts directly
    // with the canvas; the tutorial coach still renders inline when a guided step is active.
    tutorial_->DrawCoach();

    DrawPagePreviewWorkspace(
        pagePreviewProblems,
        "MainPagePreviewPanel", "MainPageLayersPanel", "MainPageProblemsPanel", true, true);
}

void EditorApplication::DrawPagePreviewWorkspace(const std::vector<editor::PagePreviewProblem>& pagePreviewProblems,
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
    mfd::PageDefinition* activePage = ActivePage();

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
    // The reticle studio no longer opens with a "Reticle studio" header banner: the title and the
    // primitive-editing hint only consumed vertical space above the canvas. The tutorial coach still
    // renders inline when a guided step is active.
    tutorial_->DrawCoach();

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

    // A fourth "Recent" action joins the trio only when a recently opened window still exists,
    // turning the empty workspace into a resume hub without disturbing the default visual identity.
    const std::vector<std::filesystem::path> recentEntries = recentWindows_.ExistingEntries();
    const int actionCount = recentEntries.empty() ? 3 : 4;

    const char* headline = "Open or create assets";
    const char* description =
        "Start with a new authored window or browse to an existing window JSON.\n"
        "Nothing is loaded automatically when the editor starts, but you can launch the guided tutorial.";

    const ImVec2 headlineSize = ImGui::CalcTextSize(headline);
    const ImVec2 descriptionSize = ImGui::CalcTextSize(description);
    const float bigIconSize = 72.0f;
    const float cellSpacing = 40.0f;
    const float iconRowWidth =
        static_cast<float>(actionCount) * bigIconSize + static_cast<float>(actionCount - 1) * cellSpacing;
    const float captionLineHeight = ImGui::GetTextLineHeightWithSpacing();
    const float totalHeight = headlineSize.y + descriptionSize.y + 48.0f + bigIconSize + captionLineHeight;
    const ImVec2 start(
        std::max(0.0f, (available.x - std::max(std::max(headlineSize.x, descriptionSize.x), iconRowWidth)) * 0.5f),
        std::max(0.0f, (available.y - totalHeight) * 0.5f));

    ImGui::SetCursorPos(start);
    ImGui::TextColored(ImVec4(0.72f, 0.86f, 0.95f, 1.0f), "%s", headline);
    ImGui::SetCursorPosX(start.x);
    ImGui::TextDisabled("%s", description);
    ImGui::Spacing();
    ImGui::Spacing();

    // Large, centered glyph actions on one line, each with a caption underneath.
    const EditorIcon icons[4] = {EditorIcon::Search, EditorIcon::Add, EditorIcon::Tutorial, EditorIcon::Recent};
    const char* const buttonIds[4] = {"##empty_open", "##empty_new", "##empty_tutorial", "##empty_recent"};
    const char* const tooltips[4] = {
        "Browse to an existing window JSON.",
        "Create a brand-new authored window.",
        "Launch the guided tutorial.",
        "Reopen a recently authored window."};
    const char* const captions[4] = {"Open window", "New window", "Tutorial", "Recent"};

    const float blockLeftX = std::max(0.0f, (available.x - iconRowWidth) * 0.5f);
    const float iconRowTopY = ImGui::GetCursorPosY();
    int pressedAction = -1;
    for (int cell = 0; cell < actionCount; ++cell)
    {
        if (cell == 0)
        {
            ImGui::SetCursorPosX(blockLeftX);
        }
        else
        {
            ImGui::SameLine(0.0f, cellSpacing);
        }

        if (IconButton(buttonIds[cell], icons[cell], tooltips[cell], true, bigIconSize))
        {
            pressedAction = cell;
        }
    }

    const float captionY = iconRowTopY + bigIconSize + 6.0f;
    for (int cell = 0; cell < actionCount; ++cell)
    {
        const ImVec2 captionSize = ImGui::CalcTextSize(captions[cell]);
        const float cellX = blockLeftX + static_cast<float>(cell) * (bigIconSize + cellSpacing);
        ImGui::SetCursorPos(ImVec2(cellX + (bigIconSize - captionSize.x) * 0.5f, captionY));
        ImGui::TextDisabled("%s", captions[cell]);
    }

    switch (pressedAction)
    {
    case 0:
        OpenWindowAssetFromFileExplorer();
        break;
    case 1:
        OpenNewWindowPopup();
        break;
    case 2:
        tutorial_->OpenFlow();
        break;
    case 3:
        ImGui::OpenPopup("EmptyWorkspaceRecentWindows");
        break;
    default:
        break;
    }

    if (ImGui::BeginPopup("EmptyWorkspaceRecentWindows"))
    {
        ImGui::TextDisabled("Recent windows");
        ImGui::Separator();
        for (const std::filesystem::path& entry : recentEntries)
        {
            const std::string label = entry.filename().string() + "##" + entry.generic_string();
            if (ImGui::MenuItem(label.c_str()))
            {
                LoadWindowConfiguration(entry);
                ImGui::CloseCurrentPopup();
            }
            ShowItemTooltip(entry.generic_string().c_str());
        }
        ImGui::EndPopup();
    }
}

void EditorApplication::DrawInspector()
{
    if (!HasOpenWindow())
    {
        ImGui::TextDisabled("No asset is open.");
        ImGui::TextWrapped("Open one existing window asset or create a new window to edit pages, reticles and strobe settings.");
        return;
    }

    const char* contextLabel = "Selection";
    switch (documentState_.selection.kind)
    {
    case SelectionKind::Window:
        contextLabel = "Window settings";
        break;
    case SelectionKind::Page:
        contextLabel = "Page";
        break;
    case SelectionKind::PageReticle:
        contextLabel = "Page > Reticle";
        break;
    case SelectionKind::PageTitle:
        contextLabel = "Page > Title chrome";
        break;
    case SelectionKind::PageStrobe:
        contextLabel = "Page > Strobe";
        break;
    case SelectionKind::LibraryReticle:
        contextLabel = "Library > Reticle";
        break;
    case SelectionKind::LibraryPrimitive:
        contextLabel = "Library > Reticle > Primitive";
        break;
    }
    ImGui::TextColored(ImVec4(0.55f, 0.62f, 0.70f, 1.0f), "%s", contextLabel);
    ImGui::Separator();
    ImGui::Spacing();

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

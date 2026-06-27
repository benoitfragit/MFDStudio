/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief ImGui-based authoring tool used to edit pages, reticles and primitive layouts.
 */

#include <array>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <raylib.h>

#include "EditorAssetPathService.h"
#include "EditorAssetWatcher.h"
#include "EditorAutosaveScheduler.h"
#include "EditorDocumentSerializer.h"
#include "EditorDesignExportService.h"
#include "EditorFullscreenPreviewController.h"
#include "EditorLayerFocusController.h"
#include "EditorPageImportService.h"
#include "EditorPagePreviewHit.h"
#include "EditorProblemNavigation.h"
#include "EditorSidebarFilter.h"
#include "EditorPageManagementService.h"
#include "EditorPageRenameService.h"
#include "EditorPagePreviewViewOptions.h"
#include "EditorPrimitiveClipboardService.h"
#include "EditorRecentWindowsService.h"
#include "EditorReticleExtractionService.h"
#include "EditorReticleUsageHighlightService.h"
#include "EditorReticleRenameService.h"
#include "internal/application/EditorApplicationState.h"
#include "mfd/io/JsonLoader.h"
#include "mfd/model/Reticle.h"
#include "BezierPolylineCache.h"
#include "ImageTextureCache.h"
#include "TextLayoutCache.h"

class EditorTutorialController;

namespace mfd
{
class Canvas2D;
} // namespace mfd

/**
 * @brief Interactive editor for authored MFD assets.
 *
 * @details `EditorApplication` owns the loaded window document, the preview
 * viewport, the property inspectors and the authoring interaction state used to
 * move, rotate and scale reticles or primitives directly in logical space.
 */
class EditorApplication
{
    friend class EditorTutorialController;

public:
    /**
     * @brief Builds the editor shell with its default UI state.
     * @param assetDirectory Optional authored asset root used for default paths.
     */
    explicit EditorApplication(std::filesystem::path assetDirectory = {});
    /** @brief Releases runtime preview resources such as off-screen textures. */
    ~EditorApplication();

    /**
     * @brief Runs the full editor until the user closes it.
     * @return Process exit code.
     */
    int Run();

private:
    /** @brief Input buffer capacity used for editable filesystem paths in the editor UI. */
    static constexpr std::size_t kPathTextCapacity = editor::app::kPathTextCapacity;
    using SelectionKind = editor::app::SelectionKind;
    using InteractionMode = editor::app::InteractionMode;
    using PrimitiveHandleKind = editor::app::PrimitiveHandleKind;
    using Selection = editor::app::Selection;
    using ViewportState = editor::app::ViewportState;
    using ReticleScreenBounds = editor::app::ReticleScreenBounds;
    using PageClipTarget = editor::app::PageClipTarget;
    using PageReticleHit = editor::app::PageReticleHit;
    using UndoSnapshot = editor::app::UndoSnapshot;
    using NewPageDraft = editor::app::NewPageDraft;
    using NewWindowDraft = editor::app::NewWindowDraft;
    using NewLibraryReticleDraft = editor::app::NewLibraryReticleDraft;
    using DuplicateLibraryReticleDraft = editor::app::DuplicateLibraryReticleDraft;
    using PageManagementAction = editor::app::PageManagementAction;
    using PageManagementPopupState = editor::app::PageManagementPopupState;
    using PageImportPopupState = editor::app::PageImportPopupState;
    using PageRenamePopupState = editor::app::PageRenamePopupState;
    using ReticleRenamePopupState = editor::app::ReticleRenamePopupState;
    using ReticleExtractionPopupState = editor::app::ReticleExtractionPopupState;
    using DesignExportPopupState = editor::app::DesignExportPopupState;
    using LayerPreviewTextureSlot = editor::app::LayerPreviewTextureSlot;

    /** @brief Loads a root window file plus its referenced authored assets into the editor. */
    bool LoadWindowConfiguration(const std::filesystem::path& path);
    /** @brief Serializes every modified file back to disk. */
    bool SaveAll();
    /** @brief Writes a crash-recovery snapshot of the current document next to the assets. */
    void WriteRecoverySnapshot();
    /** @brief Deletes the crash-recovery snapshot once it is no longer needed. */
    void ClearRecoverySnapshot();
    /** @brief Restores the document from a previous-session recovery snapshot, then reloads it. */
    void RecoverPreviousSession();
    /** @brief Returns the authored files (window, pages, templates) watched for external edits. */
    [[nodiscard]] std::vector<std::filesystem::path> CollectWatchedAssetFiles() const;
    /** @brief Re-baselines the asset watcher after the editor itself loaded or wrote the files. */
    void RearmAssetWatcher();
    /** @brief Restores the latest undo snapshot when available. */
    void Undo();
    /** @brief Restores the next redo snapshot when available. */
    void Redo();
    /** @brief Captures the current document, file layout, selection and preview views as one snapshot. */
    [[nodiscard]] UndoSnapshot CaptureCurrentSnapshot() const;
    /** @brief Restores one snapshot into the live editor state and resynchronizes derived caches. */
    void RestoreSnapshot(UndoSnapshot snapshot);
    /** @brief Captures the current document state into the undo stack. */
    void PushUndoSnapshot();
    /** @brief Pushes one pre-captured undo snapshot into the undo stack. */
    void PushUndoSnapshot(UndoSnapshot snapshot);
    /** @brief Moves the current page selection (reticles, strobe, or title) by a logical delta. */
    void NudgeSelection(mfd::Vec2 delta);
    /** @brief Handles global keyboard shortcuts such as save, undo and delete. */
    void HandleShortcuts();
    /** @brief Deletes the current selection when that selection supports deletion. */
    void DeleteSelection();
    /** @brief Handles native file-drop events routed to the editor window. */
    void HandleDroppedFiles();
    /** @brief Opens the page-management popup for the provided page and action. */
    void OpenPageManagementPopup(PageManagementAction action, int pageIndex);
    /** @brief Applies one page-removal plan to the live editor state. */
    bool ExecutePageRemovePlan(const editor::PageRemovePlan& plan);
    /** @brief Applies one page-delete plan to the live editor state. */
    bool ExecutePageDeletePlan(const editor::PageDeletePlan& plan);
    /** @brief Opens the page-import popup for the provided external JSON file. */
    void OpenPageImportPopup(std::filesystem::path sourcePageFile);
    /** @brief Builds the service request used by the page-import popup. */
    [[nodiscard]] editor::PageImportRequest BuildPageImportRequest(const std::filesystem::path& sourcePageFile) const;
    /** @brief Applies one page-import plan to the live editor state. */
    bool ExecutePageImportPlan(const editor::PageImportPlan& plan);
    /** @brief Opens the global page-rename popup for the provided page. */
    void OpenPageRenamePopup(int pageIndex);
    /** @brief Builds the service request used by the page-rename popup. */
    [[nodiscard]] editor::RenamePageRequest BuildPageRenameRequest(int pageIndex, std::string_view newPageName) const;
    /** @brief Applies one global page-rename plan to disk and the live editor state. */
    bool ExecutePageRenamePlan(const editor::RenamePagePlan& plan);
    /** @brief Opens the global reticle-template rename popup for the provided template id. */
    void OpenReticleRenamePopup(std::string templateId);
    /** @brief Builds the service request used by the reticle-rename popup. */
    [[nodiscard]] editor::RenameReticleRequest BuildReticleRenameRequest(std::string_view oldTemplateId,
                                                                         std::string_view newTemplateId,
                                                                         bool renameTemplateFile) const;
    /** @brief Applies one global reticle-template rename plan to disk and the live editor state. */
    bool ExecuteReticleRenamePlan(const editor::RenameReticlePlan& plan);
    /** @brief Opens the reusable-reticle extraction popup for the current page-reticle selection. */
    void OpenReticleExtractionPopup();
    /** @brief Builds the service request used by the reticle-extraction popup. */
    [[nodiscard]] editor::ReticleExtractionRequest BuildReticleExtractionRequest() const;
    /** @brief Applies one staged reticle extraction to the live editor state. */
    bool ExecuteReticleExtractionPlan(const editor::ReticleExtractionPlan& plan);
    /** @brief Toggles the editor-only fullscreen page-preview mode. */
    void ToggleFullscreenPagePreview();
    /** @brief Captures the current layout state used by the fullscreen controller. */
    [[nodiscard]] editor::FullscreenPreviewLayoutState CaptureFullscreenPreviewLayoutState() const;
    /** @brief Applies one layout state restored by the fullscreen controller. */
    void ApplyFullscreenPreviewLayoutState(const editor::FullscreenPreviewLayoutState& state);
    /** @brief Opens the design-export popup and seeds its default output folder. */
    void OpenDesignExportPopup();
    /** @brief Builds the service request used by the design-export popup. */
    [[nodiscard]] editor::DesignExportRequest BuildDesignExportRequest() const;
    /** @brief Executes one design-export plan and updates the popup feedback state. */
    bool ExecuteDesignExportPlan(const editor::DesignExportPlan& plan);
    /** @brief Deletes the currently selected reticle from the shared library. */
    void DeleteSelectedLibraryReticle();

    /** @brief Draws the top-level menu bar and file actions. */
    void DrawMenuBar();
    /** @brief Draws the root multi-pane editor layout. */
    void DrawRootLayout();
    /** @brief Draws the navigation sidebar. */
    void DrawSidebar(const editor::SidebarProblemSummary& problems);
    /** @brief Draws the central preview workspace. */
    void DrawWorkspace(const std::vector<editor::PagePreviewProblem>& problems);
    /** @brief Draws the empty-state placeholder when no authored window is open. */
    void DrawEmptyWorkspacePlaceholder();
    /** @brief Draws the right-hand inspector for the current selection. */
    void DrawInspector();
    /** @brief Returns whether the reticle-studio split workspace is the active editor mode. */
    [[nodiscard]] bool IsLibraryStudioWorkspaceVisible() const;
    /** @brief Returns whether the fullscreen page preview can be toggled from the current workspace. */
    [[nodiscard]] bool CanToggleFullscreenPagePreview() const;

    /** @brief Draws the authored page tree, filtered and badged by the resolved sidebar filter. */
    void DrawPageTree(const editor::SidebarFilterQuery& filter, const editor::SidebarProblemSummary& problems);
    /** @brief Draws the reticle-library tree, filtered and badged by the resolved sidebar filter. */
    void DrawLibraryTree(const editor::SidebarFilterQuery& filter, const editor::SidebarProblemSummary& problems);
    /** @brief Draws the Pages header toolbar: the add-page glyph plus the selected page's rename/remove/delete actions. */
    void DrawPageActionToolbar();
    /** @brief Draws the Reticle-library header toolbar: the add-reticle glyph plus the selected template's rename/duplicate/delete actions. */
    void DrawLibraryActionToolbar();
    /** @brief Resolves the live sidebar filter, returning a neutral query while the tutorial runs. */
    [[nodiscard]] editor::SidebarFilterQuery ResolveSidebarFilter() const;
    /** @brief Normalized page identifiers indexed like the authored pages, matching problem contexts. */
    [[nodiscard]] std::vector<std::string> CollectNormalizedPageIds() const;
    /** @brief Returns whether one page passes the resolved sidebar filter (text, scope and problems). */
    [[nodiscard]] bool PageMatchesSidebarFilter(int pageIndex,
                                                const mfd::PageDefinition& page,
                                                const editor::SidebarFilterQuery& filter,
                                                const editor::SidebarProblemSummary& problems) const;
    /** @brief Returns whether one library reticle passes the resolved sidebar filter. */
    [[nodiscard]] bool LibraryReticleMatchesSidebarFilter(const std::string& templateId,
                                                          const editor::SidebarFilterQuery& filter,
                                                          const editor::SidebarProblemSummary& problems) const;
    /** @brief Counts the authored pages that currently pass the resolved sidebar filter. */
    [[nodiscard]] int CountVisibleSidebarPages(const editor::SidebarFilterQuery& filter,
                                               const editor::SidebarProblemSummary& problems) const;
    /** @brief Counts the library reticles that currently pass the resolved sidebar filter. */
    [[nodiscard]] int CountVisibleSidebarReticles(const editor::SidebarFilterQuery& filter,
                                                  const editor::SidebarProblemSummary& problems) const;
    /** @brief Draws window-level properties such as transports and feedback cadence. */
    void DrawWindowInspector();
    /** @brief Draws page-level properties such as view and blink types. */
    void DrawPageInspector();
    /** @brief Draws the page-level strobe selector and its basic configuration. */
    void DrawPageStrobeInspector(mfd::PageDefinition& page);
    /** @brief Draws the editor-only layer manager for the active page. */
    void DrawPageLayerInspector(mfd::PageDefinition& page);
    /**
     * @brief Reorders one active-page layer by a single slot through the shared reorder helper.
     * @param page Active page whose runtime layer order is updated.
     * @param layerIndex Index of the layer to move inside `page.layers`.
     * @param moveUp `true` moves the layer one slot earlier, `false` one slot later.
     * @return `true` when the layer actually moved; an undo snapshot and a preview refresh happen
     *         only in that case, so a no-op leaves the history and caches untouched.
     */
    bool ReorderActivePageLayer(mfd::PageDefinition& page, std::size_t layerIndex, bool moveUp);
    /** @brief Draws the editor-only generated dynamic-template selection for the active page. */
    void DrawPageDynamicTemplateInspector(mfd::PageDefinition& page);
    /** @brief Draws the inspector for one page reticle instance. */
    void DrawPageReticleInspector();
    /** @brief Returns the current visual center of the page-title preview reticle. */
    [[nodiscard]] mfd::Vec2 PageTitleVisualCenterLocal(const mfd::PageDefinition& page) const;
    /** @brief Draws the inspector for the selected page title chrome. */
    void DrawSelectedPageTitleInspector();
    /** @brief Draws the inspector for the selected page strobe instance. */
    void DrawSelectedPageStrobeInspector();
    /** @brief Draws the page-local blink-type editor. */
    void DrawPageBlinkInspector(mfd::PageDefinition& page);
    /** @brief Draws the page-reticle blink assignment editor. */
    void DrawPageReticleBlinkInspector(mfd::PageDefinition& page, mfd::ReticleGroup& reticle);
    /** @brief Moves the selected page reticle to one validated index inside its owning page. */
    void MoveSelectedPageReticleToIndex(mfd::PageDefinition& page,
                                        mfd::ReticleGroup& reticle,
                                        int targetIndex,
                                        const char* action);
    /** @brief Applies one validated page-reticle id edit and keeps tutorial tracking coherent. */
    void ApplyPageReticleIdEdit(mfd::PageDefinition& page, mfd::ReticleGroup& reticle, std::string_view requestedId);
    /** @brief Draws the inspector for one reticle template from the library. */
    void DrawLibraryReticleInspector();
    /** @brief Draws the inspector for one primitive inside a library reticle. */
    void DrawLibraryPrimitiveInspector();
    /** @brief Applies one clipping-mode change to the selected page strobe. */
    void ApplySelectedPageStrobeClipping(mfd::ReticleGroup& reticle,
                                         mfd::ReticleClipMode mode,
                                         std::string primitiveId);
    /** @brief Draws one editable 2D point field and records undo activation when needed. */
    void EditPointArrayField(const char* label, mfd::Vec2& value);
    /** @brief Edits one time-format field and only commits complete supported directives when editing finishes. */
    void EditTimeFormatField(const std::string& label, const char* tooltip, mfd::TimeGeometry& geometry);

    /** @brief Draws the current page into the main preview viewport. */
    void DrawPagePreview(const ViewportState& viewport);
    /**
     * @brief Repaints the editor preview background guides (page background, grid and page border).
     * @param viewport Active preview viewport.
     * @param page Active page providing the background color.
     *
     * @note Reused as the base of the Canvas2D clipping restore callback so the editor guides
     * survive reticle clipping.
     */
    void DrawPagePreviewBackgroundGuides(const ViewportState& viewport, const mfd::PageDefinition& page);
    /**
     * @brief Repaints visible reticles of strictly earlier layers during a layer-local clip restore.
     * @param page Active page being previewed.
     * @param canvas Canvas drawing into the active stencil region.
     * @param currentLayerOrder Render order of the layer whose clipping reticle is being restored.
     *
     * @note Earlier-layer reticles are redrawn without their own clipping so the restore never
     * recurses into another clip mask. Reticles of the current and later layers stay erased.
     */
    void RedrawEarlierLayerReticlesForClipRestore(const mfd::PageDefinition& page,
                                                  mfd::Canvas2D& canvas,
                                                  std::size_t currentLayerOrder);
    /** @brief Draws the selected library reticle into the studio preview viewport. */
    void DrawLibraryPreview(const ViewportState& viewport);
    /** @brief Draws the shared page-preview workspace used by the page view and the fullscreen preview. */
    void DrawPagePreviewWorkspace(const std::vector<editor::PagePreviewProblem>& pagePreviewProblems,
                                  const char* previewChildId,
                                  const char* layersChildId,
                                  const char* problemsChildId,
                                  bool drawPreviewOverlays,
                                  bool handlePreviewInteraction);
    /** @brief Draws the reticle-studio panel with one optional explicit width. */
    void DrawReticleStudioPanel(float width = 0.0f);
    /** @brief Draws the always-visible menu-bar View entry whose content adapts to the active workspace. */
    void DrawViewMenu();
    /** @brief Draws the sidebar and inspector visibility toggles shared by every View-menu context. */
    void DrawShellPanelVisibilityMenuItems();
    /** @brief Draws the View-menu items for the page-preview and fullscreen workspaces. */
    void DrawPagePreviewViewMenuItems(bool showProblemsIndicator);
    /** @brief Draws the View-menu items for the reticle-studio workspace. */
    void DrawReticleStudioViewMenuItems();
    /** @brief Draws the fullscreen-preview toggle shared by every View-menu context. */
    void DrawFullscreenPreviewMenuItem();
    /** @brief Draws guides, selection boxes and coordinate overlays on the page preview. */
    void DrawPreviewOverlays(const ViewportState& viewport);
    /** @brief Draws the optional page-preview minimap overlay. */
    void DrawPagePreviewMinimap(const ViewportState& viewport, const mfd::PageDefinition& page);
    /** @brief Draws optional reticle-name labels over the page preview. */
    void DrawPagePreviewReticleNames(const ViewportState& viewport, const mfd::PageDefinition& page);
    /** @brief Draws the selection bounds and transform handles for the active page preview. */
    void DrawPagePreviewGizmos(const ViewportState& viewport, const mfd::PageDefinition& page);
    /** @brief Draws the docked layer-inspector panel shown next to the page preview. */
    void DrawLayerInspectorPanel(mfd::PageDefinition& page);
    /**
     * @brief Applies the layer-inspector focus selection for one strip entry.
     * @param page Active page being inspected.
     * @param entry Strip entry whose layer (or synthetic Full View) becomes focused.
     */
    void ApplyLayerInspectorEntryFocus(mfd::PageDefinition& page, const editor::LayerFocusStripEntry& entry);
    /** @brief Draws the docked validation panel shown under the page preview. */
    void DrawProblemsPanel(const std::vector<editor::PagePreviewProblem>& problems);
    /** @brief Selects the editor entity referenced by one validation problem context. */
    void NavigateToProblem(const std::string& contextId);
    /** @brief Draws page and instance highlights for the currently selected reticle template. */
    void DrawReticleUsageHighlightPlaceholder(const ViewportState& viewport);
    /** @brief Returns the cached reticle-template highlight result when the workflow is active. */
    const editor::ReticleUsageHighlightResult* ResolveReticleUsageHighlight();
    /** @brief Invalidates every cached reticle-usage highlight derived from the asset graph. */
    void InvalidateReticleUsageHighlightCache() noexcept;
    /** @brief Clears the current layer focus and optionally reports it in the status bar. */
    void ClearLayerFocus(bool announceStatus);
    /** @brief Clears invalid layer-focus ids after page or layer changes. */
    void SanitizeLayerFocusForActivePage();
    /** @brief Drops any page-reticle selection that no longer stays editable in focus mode. */
    void SanitizePageReticleSelectionForCurrentFocus();
    /** @brief Returns whether one page reticle is currently editable under layer focus. */
    bool IsPageReticleSelectableInCurrentFocus(const mfd::PageDefinition& page, const mfd::ReticleGroup& reticle) const;
    /** @brief Returns whether one page reticle should stay visible but dimmed under layer focus. */
    bool ShouldDimPageReticleInCurrentFocus(const mfd::PageDefinition& page, const mfd::ReticleGroup& reticle) const;
    /** @brief Returns the editor layer id used for new page-reticle insertions. */
    std::string ActiveInsertionLayerId(const mfd::PageDefinition& page) const;
    /** @brief Handles drag interactions in the page preview. */
    void HandlePreviewInteraction(const ViewportState& viewport);
    /** @brief Draws the page-preview context menu for reticle-specific actions. */
    void DrawPageReticleContextMenu();
    /** @brief Draws overlays specific to the library-studio preview. */
    void DrawLibraryPreviewOverlays(const ViewportState& viewport);
    /** @brief Handles drag interactions in the library-studio preview. */
    void HandleLibraryPreviewInteraction(const ViewportState& viewport);
    /** @brief Clears the page-preview drag interaction state. */
    void CancelPreviewInteraction() noexcept;
    /** @brief Begins a rotate/scale handle interaction for the selected page reticle. */
    void BeginReticleHandleInteraction(InteractionMode mode,
                                       ImVec2 cornerScreen,
                                       const ViewportState& viewport,
                                       ImVec2 mouse,
                                       const mfd::Transform2D& selectedTransform,
                                       const mfd::ReticleGroup& selectedPreviewReticle);
    /** @brief Clears the library-preview primitive handle interaction state. */
    void CancelLibraryPreviewInteraction() noexcept;
    /** @brief Draws the clip menu items for one hovered page-reticle primitive. */
    void DrawReticleClipMenuItems(mfd::PageDefinition& page, int reticleIndex, int primitiveIndex);
    /** @brief Returns the hovered clip targets that belong to one page reticle. */
    std::vector<PageClipTarget> CollectClipTargetsForReticle(int reticleIndex) const;
    /** @brief Draws the per-reticle page-preview context menu content. */
    void DrawReticleContextContent(mfd::PageDefinition& page, int reticleIndex);

    /** @brief Ensures the off-screen preview texture matches the requested size. */
    void EnsurePreviewTexture(int width, int height);
    /** @brief Releases the off-screen preview texture. */
    void ReleasePreviewTexture();
    /** @brief Ensures the hover-preview texture matches the requested size. */
    void EnsureTooltipPreviewTexture(int width, int height);
    /** @brief Releases the hover-preview texture. */
    void ReleaseTooltipPreviewTexture();
    /** @brief Releases every cached layer-preview thumbnail texture. */
    void ReleaseLayerPreviewTextures() noexcept;
    /** @brief Releases every preview-side GPU resource before shutdown. */
    void ReleasePreviewGpuResources() noexcept;
    /** @brief Applies the font file declared by the loaded window configuration. */
    void ApplyPreviewFontFile(std::filesystem::path fontFile);
    /** @brief Lazily loads the configured preview font once raylib is ready. */
    void EnsurePreviewFont();
    /** @brief Releases the preview font override. */
    void ReleasePreviewFont() noexcept;
    /** @brief Returns the preview font override when one is currently loaded. */
    const Font* PreviewTextFont() const noexcept;
    /** @brief Returns the renderer-aligned logical width of one preview text primitive. */
    [[nodiscard]] float MeasurePreviewTextWidthLogical(const mfd::TextGeometry& geometry);
    /** @brief Returns the renderer-aligned logical width of one preview time primitive. */
    [[nodiscard]] float MeasurePreviewTextWidthLogical(const mfd::TimeGeometry& geometry);
    /** @brief Resets the page preview camera from the currently active authored page view. */
    void ResetPagePreviewView() noexcept;
    /** @brief Resets the library preview camera to its neutral editor-only view. */
    void ResetLibraryPreviewView() noexcept;
    /** @brief Returns the current page-preview problem messages derived from validation. */
    std::vector<editor::PagePreviewProblem> BuildPagePreviewProblems() const;
    /** @brief Updates the footer status message. */
    void RebuildStatus(std::string message, bool isError);

    /** @brief Opens the "new page" popup and seeds its draft values. */
    void OpenNewPagePopup();
    /** @brief Opens the "new window" popup and seeds its draft values. */
    void OpenNewWindowPopup();
    /** @brief Opens a native file-explorer dialog to load one existing window asset. */
    bool OpenWindowAssetFromFileExplorer();
    /** @brief Opens the "new library reticle" popup and seeds its draft values. */
    void OpenNewLibraryReticlePopup();
    /** @brief Opens the "duplicate reticle" popup and seeds its draft values. */
    void OpenDuplicateLibraryReticlePopup();
    /** @brief Opens a native file-explorer dialog to import one external page JSON asset. */
    bool OpenPageAssetImportFromFileExplorer();
    /** @brief Opens a native save-file dialog for the new-window root JSON field. */
    void BrowseNewWindowFile();
    /** @brief Opens a native open-file dialog for the optional new-window font file field. */
    void BrowseNewWindowFontFile();
    /** @brief Opens a native folder picker for the new-window reticle-library field. */
    void BrowseNewWindowReticleLibraryFolder();
    /** @brief Opens a native save-file dialog for the new-window initial page JSON field. */
    void BrowseNewWindowFirstPageFile();
    /** @brief Opens a native save-file dialog for the new-page JSON field. */
    void BrowseNewPageFile();
    /** @brief Opens a native open-file dialog for the loaded-window font file field in the inspector. */
    void BrowseWindowFontFile();
    /** @brief Opens a native folder picker for the loaded-window reticle-library field in the inspector. */
    void BrowseWindowReticleLibraryFolder();
    /** @brief Opens a native open-file dialog for the selected image primitive file field in the inspector. */
    void BrowseSelectedPrimitiveImageFile();
    /** @brief Draws and resolves all modal popups owned by the editor. */
    void DrawPopups();
    /** @brief Draws the remove/delete page confirmation popup. */
    void DrawPageManagementPopup();
    /** @brief Draws the page-import planning popup. */
    void DrawPageImportPopup();
    /** @brief Draws the global page-rename planning popup. */
    void DrawPageRenamePopup();
    /** @brief Draws the global reticle-template rename planning popup. */
    void DrawReticleRenamePopup();
    /** @brief Draws the reusable-reticle extraction planning popup. */
    void DrawReticleExtractionPopup();
    /** @brief Draws the design-export planning popup. */
    void DrawDesignExportPopup();
    /** @brief Seeds the editor state expected by the current tutorial step. */
    void PrepareTutorialStep();

    /** @brief Creates a new page from the popup draft. */
    bool CreateNewPage();
    /** @brief Creates a brand-new window document from the popup draft. */
    bool CreateNewWindow();
    /** @brief Creates a new reticle template initialized with one primitive. */
    bool CreateNewLibraryReticleFromPrimitive();
    /** @brief Duplicates the selected library reticle under a new id. */
    void DuplicateSelectedLibraryReticle();
    /** @brief Copies the selected library reticle into the editor clipboard. */
    void CopySelectedLibraryReticle();
    /** @brief Pastes the copied library reticle as one new template. */
    void PasteCopiedLibraryReticle();
    /** @brief Copies the selected library primitive into the reticle-studio clipboard. */
    void CopySelectedLibraryPrimitive();
    /** @brief Pastes the copied library primitive into the focused reticle template. */
    void PasteCopiedLibraryPrimitive();
    /** @brief Instantiates one page reticle from a library template at a logical position. */
    bool CreatePageReticleInstanceFromTemplate(std::string_view templateId, mfd::Vec2 position);
    /** @brief Seeds default save locations for the window-creation popup. */
    void SeedNewWindowAssetDraftPaths();
    /** @brief Seeds a default save location for the page-creation popup. */
    void SeedNewPageAssetDraftPath();

    /** @brief Selects the window-level inspector without changing the active page context. */
    void SelectWindow();
    /** @brief Selects one page and updates the inspector focus accordingly. */
    void SelectPage(int pageIndex, bool resetPreviewView = true);
    /** @brief Selects one page reticle instance. */
    void SelectPageReticle(int pageIndex, int reticleIndex);
    /** @brief Selects the generated page title chrome on the active page. */
    void SelectPageTitle(int pageIndex);
    /** @brief Selects one page-level strobe instance. Defaults to the active authored strobe. */
    void SelectPageStrobe(int pageIndex, int strobeIndex = -1);
    /** @brief Toggles one page reticle inside the multi-selection. */
    void TogglePageReticleSelection(int pageIndex, int reticleIndex);
    /** @brief Selects one reticle template in the library tree. */
    void SelectLibraryReticle(std::string templateId, bool resetPreviewView = true);
    /** @brief Selects one primitive inside the selected library reticle. */
    void SelectLibraryPrimitive(std::string templateId, int primitiveIndex);

    /** @brief Returns the currently active page when the selection targets one. */
    mfd::PageDefinition* ActivePage() noexcept;
    /** @brief Returns the currently active page when the selection targets one. */
    const mfd::PageDefinition* ActivePage() const noexcept;
    /** @brief Returns the selected page reticle instance when available. */
    mfd::ReticleGroup* SelectedPageReticle() noexcept;
    /** @brief Returns the selected page reticle instance when available. */
    const mfd::ReticleGroup* SelectedPageReticle() const noexcept;
    /** @brief Returns the selected page strobe reticle instance when available. */
    mfd::ReticleGroup* SelectedPageStrobeReticle() noexcept;
    /** @brief Returns the selected page strobe reticle instance when available. */
    const mfd::ReticleGroup* SelectedPageStrobeReticle() const noexcept;
    /** @brief Returns the selected page strobe definition when available. */
    mfd::PageStrobeDefinition* SelectedPageStrobe() noexcept;
    /** @brief Returns the selected page strobe definition when available. */
    const mfd::PageStrobeDefinition* SelectedPageStrobe() const noexcept;
    /** @brief Returns the selected page title styling state when available. */
    mfd::PageTitleDisplayDefinition* SelectedPageTitleDisplay() noexcept;
    /** @brief Returns the selected page title styling state when available. */
    const mfd::PageTitleDisplayDefinition* SelectedPageTitleDisplay() const noexcept;
    /** @brief Returns the currently selected editable page reticle, including the page strobe. */
    mfd::ReticleGroup* SelectedEditablePageReticle() noexcept;
    /** @brief Returns the currently selected editable page reticle, including the page strobe. */
    const mfd::ReticleGroup* SelectedEditablePageReticle() const noexcept;
    /** @brief Returns the selected reticle template when available. */
    mfd::ReticleGroup* SelectedLibraryReticle() noexcept;
    /** @brief Returns the selected reticle template when available. */
    const mfd::ReticleGroup* SelectedLibraryReticle() const noexcept;
    /** @brief Returns the selected primitive when available. */
    mfd::Primitive* SelectedLibraryPrimitive() noexcept;
    /** @brief Returns the selected primitive when available. */
    const mfd::Primitive* SelectedLibraryPrimitive() const noexcept;
    /** @brief Renders or refreshes one layer-preview thumbnail texture. */
    const RenderTexture2D* RenderLayerPreviewThumbnail(std::size_t thumbnailIndex,
                                                       const mfd::PageDefinition& page,
                                                       const editor::LayerFocusStripEntry& entry,
                                                       int width,
                                                       int height);
    /** @brief Draws a hover tooltip preview for one reticle item inside the tree views. */
    void DrawReticleHoverPreviewTooltip(const mfd::ReticleGroup& reticle,
                                        std::string_view label,
                                        Color backgroundColor);
    /** @brief Returns `true` when one page reticle is part of the current selection. */
    bool HasSelectedPageReticle(int pageIndex, int reticleIndex) const noexcept;
    /** @brief Returns `true` when the page-level strobe currently owns the selection. */
    bool IsPageStrobeSelected() const noexcept;
    /** @brief Returns `true` when the generated page title currently owns the selection. */
    bool IsPageTitleSelected() const noexcept;
    /** @brief Returns the selected page reticle indices on the active page. */
    std::vector<int> SelectedPageReticleIndices() const;
    /** @brief Returns the current number of selected page reticles. */
    int SelectedPageReticleCount() const;
    /** @brief Returns `true` when a window document is currently open in the editor. */
    bool HasOpenWindow() const noexcept;
    /** @brief Copies the selected page reticles into the editor clipboard. */
    void CopySelectedPageReticles();
    /** @brief Cuts the selected page reticles into the editor clipboard. */
    void CutSelectedPageReticles();
    /** @brief Pastes the page-reticle clipboard into the active page. */
    void PasteCopiedPageReticles();
    /** @brief Returns one unique template id for the library clipboard and duplication flows. */
    std::string MakeUniqueLibraryReticleId(std::string_view baseId) const;

    /** @brief Updates the current selection from a click inside the page preview. */
    void UpdateReticleSelectionFromClick(const ViewportState& viewport, bool additiveSelection);
    /** @brief Returns all page reticles hit by the mouse, ordered from the most specific to the broadest hit. */
    std::vector<int> CollectPageReticlesAt(const ViewportState& viewport, ImVec2 mousePosition) const;
    /** @brief Finds the nearest page reticle to the mouse for hit-testing. */
    std::optional<int> FindNearestPageReticle(const ViewportState& viewport, ImVec2 mousePosition) const;
    /** @brief Builds one ranked page-preview hit candidate for static reticles, the active strobe or the page title. */
    std::optional<PageReticleHit> BuildPageReticleHit(const mfd::PageDefinition& page,
                                                      const ViewportState& viewport,
                                                      ImVec2 mousePosition,
                                                      editor::PagePreviewHitTarget target,
                                                      const mfd::ReticleGroup& reticle,
                                                      editor::PagePreviewDrawOrderKey drawOrder) const;
    /** @brief Returns `true` when the left candidate should outrank the right one for page-preview selection. */
    static bool PreferPageReticleHit(const PageReticleHit& lhs, const PageReticleHit& rhs) noexcept;
    /** @brief Returns all clip-capable page primitives hit by the mouse, ordered by hit quality. */
    std::vector<PageClipTarget> CollectPageClipTargetsAt(const ViewportState& viewport, ImVec2 mousePosition) const;
    /** @brief Finds the nearest supported convex page primitive to the mouse for clipping actions. */
    std::optional<PageClipTarget> FindNearestPageClipPrimitive(const ViewportState& viewport, ImVec2 mousePosition) const;
    /** @brief Finds the nearest library primitive to the mouse for hit-testing. */
    std::optional<int> FindNearestLibraryPrimitive(const ViewportState& viewport, ImVec2 mousePosition) const;
    /** @brief Applies the current direct-manipulation transform from mouse movement. */
    void ApplyMouseTransform(const ViewportState& viewport);
    /** @brief Computes screen-space bounds for one reticle instance. */
    ReticleScreenBounds ComputeReticleScreenBounds(const mfd::ReticleGroup& reticle, const ViewportState& viewport) const;
    /** @brief Computes screen-space bounds for one primitive inside a reticle. */
    ReticleScreenBounds ComputePrimitiveScreenBounds(const mfd::ReticleGroup& reticle,
                                                     const mfd::Primitive& primitive,
                                                     const ViewportState& viewport) const;
    /** @brief Returns the hit distance in pixels between the mouse and one page reticle. */
    float ReticleHitDistancePixels(const mfd::ReticleGroup& reticle, const ViewportState& viewport, ImVec2 mousePosition) const;
    /** @brief Returns one cached preview reticle representing the page title chrome. */
    const mfd::ReticleGroup& BuildPageTitlePreviewReticle(const mfd::PageDefinition& page) const;
    /** @brief Returns the hit distance in pixels between the mouse and one primitive. */
    float PrimitiveHitDistancePixels(const mfd::ReticleGroup& reticle,
                                     const mfd::Primitive& primitive,
                                     const ViewportState& viewport,
                                     ImVec2 mousePosition) const;
    /** @brief Applies one clipping mode change to a page reticle and reports status. */
    bool ApplyPageReticleClipping(int reticleIndex, mfd::ReticleClipMode mode, std::string primitiveId);

    /** @brief Builds a minimal reticle template containing one primitive of the requested type. */
    static mfd::ReticleGroup MakePrimitiveReticle(std::string id, mfd::PrimitiveType primitiveType);
    /** @brief Instantiates or replaces one page strobe from the selected library template. */
    static mfd::PageStrobeDefinition MakePageStrobeFromTemplate(const mfd::PageDefinition& page,
                                                                const mfd::ReticleGroup& templ,
                                                                const std::optional<mfd::PageStrobeDefinition>& previousStrobe,
                                                                std::size_t* unmappedPrimitiveOverrideCount = nullptr);
    /** @brief Generates a reticle id that does not collide inside the provided container. */
    static std::string MakeUniqueReticleId(const std::vector<mfd::ReticleGroup>& groups, std::string_view baseId);
    /** @brief Generates a page reticle id that stays unique against static reticles and authored strobes after normalization. */
    static std::string MakeUniquePageReticleId(const mfd::PageDefinition& page,
                                               std::string_view baseId,
                                               std::string_view excludedReticleId = {},
                                               std::string_view excludedStrobeId = {});
    /** @brief Generates a unique strobe name inside one page. */
    static std::string MakeUniqueStrobeName(const mfd::PageDefinition& page,
                                            std::string_view baseName,
                                            std::string_view excludedStrobeName = {});
    /** @brief Generates a unique editor layer id inside one page. */
    static std::string MakeUniqueLayerId(const mfd::PageDefinition& page, std::string_view baseId);
    /** @brief Grouped document state: asset roots, loaded JSON, file layout, selection and undo history. */
    editor::app::DocumentState documentState_ {};
    /** @brief Grouped preview resources: GPU render targets, caches and derived preview state. */
    editor::app::PreviewState previewState_ {};
    /** @brief Grouped workflow UI state: status bar, drafts and modal popup state. */
    editor::app::WorkflowState workflowState_ {};
    /** @brief Grouped clipboard state shared by page-reticle and reticle-studio copy/paste flows. */
    editor::app::ClipboardState clipboardState_ {};
    /** @brief Private controller that owns the guided tutorial state and workflow. */
    std::unique_ptr<EditorTutorialController> tutorial_ {};
    /** @brief Cadence control deciding when to write the crash-recovery snapshot. */
    editor::EditorAutosaveScheduler autosave_ {};
    /** @brief Recently opened window assets surfaced by the empty workspace resume hub. */
    editor::EditorRecentWindowsService recentWindows_ {};
    /** @brief Tracks the authored files for edits made outside the editor. */
    editor::EditorAssetWatcher assetWatcher_ {};
    /** @brief Seconds accumulated since the last external-change poll. */
    float assetWatchAccumulatorSeconds_ = 0.0f;
    /** @brief Grouped stateless services and controllers shared across editor responsibilities. */
    editor::app::ServicesState services_ {};
    /** @brief Grouped direct-manipulation state used by page-preview and reticle-studio gestures. */
    editor::app::InteractionState interactionState_ {};
    /** @brief Grouped layout and view state for the main editor workspace. */
    editor::app::LayoutState layoutState_ {};
};


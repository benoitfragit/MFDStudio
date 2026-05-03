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
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <raylib.h>

#include "EditorDocumentSerializer.h"
#include "EditorLayerFocusController.h"
#include "EditorPageImportService.h"
#include "EditorPageManagementService.h"
#include "EditorPageRenameService.h"
#include "EditorPagePreviewViewOptions.h"
#include "EditorReticleExtractionService.h"
#include "EditorReticleUsageHighlightService.h"
#include "EditorReticleRenameService.h"
#include "mfd/io/JsonLoader.h"
#include "mfd/model/Reticle.h"
#include "ImageTextureCache.h"

class EditorTutorialController;

namespace editor::automation
{
class EditorApplicationAutomationBridge;
class IEditorAutomationFacade;
}

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
    friend class editor::automation::EditorApplicationAutomationBridge;

public:
    /** @brief Builds the editor shell with its default startup file and UI state. */
    EditorApplication();
    /** @brief Releases runtime preview resources such as off-screen textures. */
    ~EditorApplication();

    /**
     * @brief Runs the full editor until the user closes it.
     * @return Process exit code.
     */
    int Run();

private:
    /** @brief Input buffer capacity used for editable filesystem paths in the editor UI. */
    static constexpr std::size_t kPathTextCapacity = 512;

    /** @brief Current high-level selection type shown in the inspector. */
    enum class SelectionKind
    {
        Page,
        PageReticle,
        LibraryReticle,
        LibraryPrimitive
    };

    /** @brief Current direct-manipulation mode active in the preview viewport. */
    enum class InteractionMode
    {
        None,
        PanPage,
        NavigateMinimap,
        MoveReticle,
        RotateReticle,
        ScaleReticle,
        MovePrimitive,
        EditPrimitiveHandle
    };

    /** @brief Type of primitive handle currently manipulated inside the studio preview. */
    enum class PrimitiveHandleKind
    {
        None,
        Point,
        Radius,
        RectangleCorner,
        DiamondAxis
    };

    /** @brief Asset path field currently driven by the folder-picker popup. */
    enum class AssetFolderPickerTarget
    {
        None,
        WindowFile,
        ReticleLibraryFolder,
        FirstPageFile,
        NewPageFile
    };

    /** @brief Current tree selection routed to the inspector and the preview overlays. */
    struct Selection
    {
        SelectionKind kind = SelectionKind::Page;
        int pageIndex = 0;
        int pageReticleIndex = -1;
        std::vector<int> pageReticleIndices {};
        std::string libraryReticleId {};
        std::string libraryBrowserReticleId {};
        int primitiveIndex = -1;
    };

    /**
     * @brief Screen-to-logical conversion state of one preview viewport.
     *
     * @note The editor uses the same normalized `[-1, 1]` space as the
     * runtime, so these helpers are central when dragging or measuring content.
     */
    struct ViewportState
    {
        ImVec2 origin {};
        ImVec2 size {};
        mfd::PageViewState view {};
        bool valid = false;

        float LogicalScale() const noexcept;
        ImVec2 ToScreen(mfd::Vec2 logical) const noexcept;
        mfd::Vec2 ToLogical(ImVec2 screen) const noexcept;
    };

    /** @brief Screen-space bounds cache used for hit-testing and overlay drawing. */
    struct ReticleScreenBounds
    {
        ImVec2 min {};
        ImVec2 max {};
        ImVec2 center {};
        bool valid = false;
    };

    /** @brief Primitive-level target used by the page clipping context menu. */
    struct PageClipTarget
    {
        int reticleIndex = -1;
        int primitiveIndex = -1;
    };

    /** @brief Full undo snapshot storing the loaded document, file layout and current selection. */
    struct UndoSnapshot
    {
        mfd::LoadedWindowConfiguration loaded;
        editor::EditorFileLayout files;
        Selection selection;
        mfd::PageViewState pagePreviewView;
        mfd::PageViewState libraryPreviewView;
    };

    /** @brief Draft values used by the "new page" popup before the page is created. */
    struct NewPageDraft
    {
        std::array<char, 64> name {};
        std::array<char, 64> title {};
        std::array<char, kPathTextCapacity> fileName {};
        ImVec4 background {0.03f, 0.10f, 0.03f, 1.0f};
    };

    /** @brief Draft values used by the "new window" popup before a window is created from scratch. */
    struct NewWindowDraft
    {
        std::array<char, kPathTextCapacity> windowFile {};
        std::array<char, 64> title {};
        int width = 640;
        int height = 480;
        int positionX = 120;
        int positionY = 80;
        std::array<char, kPathTextCapacity> fontFile {};
        std::array<char, kPathTextCapacity> reticleLibraryFolder {};
        bool commandUdpEnabled = true;
        std::array<char, 64> commandAddress {};
        int commandPort = 49000;
        int commandMaxPacketSize = 65507;
        bool feedbackUdpEnabled = false;
        std::array<char, 64> feedbackAddress {};
        int feedbackPort = 49001;
        int feedbackMaxPacketSize = 65507;
        bool createInitialPage = true;
        std::array<char, 64> firstPageName {};
        std::array<char, 64> firstPageTitle {};
        std::array<char, kPathTextCapacity> firstPageFile {};
        ImVec4 firstPageBackground {0.03f, 0.10f, 0.03f, 1.0f};
    };

    /** @brief Draft values used by the "new library reticle" popup. */
    struct NewLibraryReticleDraft
    {
        std::array<char, 64> id {};
        int primitiveTypeIndex = 0;
    };

    /** @brief Draft values used when duplicating one library reticle under a new id. */
    struct DuplicateLibraryReticleDraft
    {
        std::array<char, 64> id {};
    };

    /** @brief Page-management operation currently staged through the confirmation popup. */
    enum class PageManagementAction
    {
        None,
        RemoveFromWindow,
        DeleteAsset
    };

    /** @brief UI state driving the remove/delete page confirmation popup. */
    struct PageManagementPopupState
    {
        PageManagementAction action = PageManagementAction::None;
        bool openRequested = false;
        int pageIndex = -1;
        int replacementPageIndex = -1;
        bool allowOutsideAssetsRoot = false;
        bool confirmDelete = false;
    };

    /** @brief UI state driving the page-import review popup. */
    struct PageImportPopupState
    {
        bool openRequested = false;
        std::filesystem::path sourcePageFile {};
    };

    /** @brief UI state driving the global page-rename review popup. */
    struct PageRenamePopupState
    {
        bool openRequested = false;
        int pageIndex = -1;
        std::array<char, 128> newName {};
    };

    /** @brief UI state driving the global reticle-template rename review popup. */
    struct ReticleRenamePopupState
    {
        bool openRequested = false;
        std::string currentTemplateId {};
        std::array<char, 128> newName {};
        bool renameTemplateFile = true;
    };

    /** @brief UI state driving the reticle-extraction review popup. */
    struct ReticleExtractionPopupState
    {
        bool openRequested = false;
        std::array<char, 128> templateId {};
        std::array<char, kPathTextCapacity> templateFile {};
    };

    /** @brief Loads a root window file plus its referenced authored assets into the editor. */
    bool LoadWindowConfiguration(const std::filesystem::path& path);
    /** @brief Serializes every modified file back to disk. */
    bool SaveAll();
    /** @brief Restores the latest undo snapshot when available. */
    void Undo();
    /** @brief Captures the current document state into the undo stack. */
    void PushUndoSnapshot();
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
    /** @brief Deletes the currently selected reticle from the shared library. */
    void DeleteSelectedLibraryReticle();

    /** @brief Draws the top-level menu bar and file actions. */
    void DrawMenuBar();
    /** @brief Draws the root multi-pane editor layout. */
    void DrawRootLayout();
    /** @brief Draws the navigation sidebar. */
    void DrawSidebar();
    /** @brief Draws the central preview workspace. */
    void DrawWorkspace();
    /** @brief Draws the empty-state placeholder when no authored window is open. */
    void DrawEmptyWorkspacePlaceholder();
    /** @brief Draws the right-hand inspector for the current selection. */
    void DrawInspector();

    /** @brief Draws the authored page tree. */
    void DrawPageTree();
    /** @brief Draws the reticle-library tree. */
    void DrawLibraryTree();
    /** @brief Draws page-level properties such as view and blink types. */
    void DrawPageInspector();
    /** @brief Draws the page-level strobe selector and its basic configuration. */
    void DrawPageStrobeInspector(mfd::PageDefinition& page);
    /** @brief Draws the editor-only layer manager for the active page. */
    void DrawPageLayerInspector(mfd::PageDefinition& page);
    /** @brief Draws the inspector for one page reticle instance. */
    void DrawPageReticleInspector();
    /** @brief Draws the page-local blink-type editor. */
    void DrawPageBlinkInspector(mfd::PageDefinition& page);
    /** @brief Draws the page-reticle blink assignment editor. */
    void DrawPageReticleBlinkInspector(mfd::PageDefinition& page, mfd::ReticleGroup& reticle);
    /** @brief Draws the inspector for one reticle template from the library. */
    void DrawLibraryReticleInspector();
    /** @brief Draws the inspector for one primitive inside a library reticle. */
    void DrawLibraryPrimitiveInspector();

    /** @brief Draws the current page into the main preview viewport. */
    void DrawPagePreview(const ViewportState& viewport);
    /** @brief Draws the selected library reticle into the studio preview viewport. */
    void DrawLibraryPreview(const ViewportState& viewport);
    /** @brief Draws the shared page-preview view menu button and popup. */
    void DrawPagePreviewViewMenuButton(const char* buttonId, bool showProblemsIndicator);
    /** @brief Draws guides, selection boxes and coordinate overlays on the page preview. */
    void DrawPreviewOverlays(const ViewportState& viewport);
    /** @brief Draws the optional page-preview minimap overlay. */
    void DrawPagePreviewMinimap(const ViewportState& viewport, const mfd::PageDefinition& page);
    /** @brief Draws optional reticle-name labels over the page preview. */
    void DrawPagePreviewReticleNames(const ViewportState& viewport, const mfd::PageDefinition& page);
    /** @brief Draws the selection bounds and transform handles for the active page preview. */
    void DrawPagePreviewGizmos(const ViewportState& viewport, const mfd::PageDefinition& page);
    /** @brief Draws the layer-inspector strip or its placeholder when enabled. */
    void DrawLayerInspectorStrip(const ViewportState& viewport, const mfd::PageDefinition& page);
    /** @brief Draws the lightweight problems panel overlay for the page preview. */
    void DrawProblemsPanel(const ViewportState& viewport);
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

    /** @brief Ensures the off-screen preview texture matches the requested size. */
    void EnsurePreviewTexture(int width, int height);
    /** @brief Releases the off-screen preview texture. */
    void ReleasePreviewTexture();
    /** @brief Ensures the hover-preview texture matches the requested size. */
    void EnsureTooltipPreviewTexture(int width, int height);
    /** @brief Releases the hover-preview texture. */
    void ReleaseTooltipPreviewTexture();
    /** @brief Applies the font file declared by the loaded window configuration. */
    void ApplyPreviewFontFile(std::filesystem::path fontFile);
    /** @brief Lazily loads the configured preview font once raylib is ready. */
    void EnsurePreviewFont();
    /** @brief Releases the preview font override. */
    void ReleasePreviewFont() noexcept;
    /** @brief Returns the preview font override when one is currently loaded. */
    const Font* PreviewTextFont() const noexcept;
    /** @brief Resets the page preview camera from the currently active authored page view. */
    void ResetPagePreviewView() noexcept;
    /** @brief Resets the library preview camera to its neutral editor-only view. */
    void ResetLibraryPreviewView() noexcept;
    /** @brief Returns the current page-preview problem messages derived from validation. */
    std::vector<std::string> BuildPagePreviewProblemMessages() const;
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
    /** @brief Opens the guided folder-picker popup for one asset-location field. */
    void OpenAssetFolderPicker(AssetFolderPickerTarget target);
    /** @brief Draws and resolves all modal popups owned by the editor. */
    void DrawPopups();
    /** @brief Draws the guided folder-picker popup used by new-asset dialogs. */
    void DrawAssetFolderPickerPopup();
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
    /** @brief Instantiates one page reticle from a library template at a logical position. */
    bool CreatePageReticleInstanceFromTemplate(std::string_view templateId, mfd::Vec2 position);
    /** @brief Seeds default save locations for the window-creation popup. */
    void SeedNewWindowAssetDraftPaths();
    /** @brief Seeds a default save location for the page-creation popup. */
    void SeedNewPageAssetDraftPath();

    /** @brief Selects one page and updates the inspector focus accordingly. */
    void SelectPage(int pageIndex);
    /** @brief Selects one page reticle instance. */
    void SelectPageReticle(int pageIndex, int reticleIndex);
    /** @brief Toggles one page reticle inside the multi-selection. */
    void TogglePageReticleSelection(int pageIndex, int reticleIndex);
    /** @brief Selects one reticle template in the library tree. */
    void SelectLibraryReticle(std::string templateId);
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
    /** @brief Returns the selected reticle template when available. */
    mfd::ReticleGroup* SelectedLibraryReticle() noexcept;
    /** @brief Returns the selected reticle template when available. */
    const mfd::ReticleGroup* SelectedLibraryReticle() const noexcept;
    /** @brief Returns the selected primitive when available. */
    mfd::Primitive* SelectedLibraryPrimitive() noexcept;
    /** @brief Returns the selected primitive when available. */
    const mfd::Primitive* SelectedLibraryPrimitive() const noexcept;
    /** @brief Draws a hover tooltip preview for one reticle item inside the tree views. */
    void DrawReticleHoverPreviewTooltip(const mfd::ReticleGroup& reticle,
                                        std::string_view label,
                                        Color backgroundColor);
    /** @brief Returns `true` when one page reticle is part of the current selection. */
    bool HasSelectedPageReticle(int pageIndex, int reticleIndex) const noexcept;
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

    /** @brief Updates the current selection from a click inside the page preview. */
    void UpdateReticleSelectionFromClick(const ViewportState& viewport, bool additiveSelection);
    /** @brief Returns all page reticles hit by the mouse, ordered from the most specific to the broadest hit. */
    std::vector<int> CollectPageReticlesAt(const ViewportState& viewport, ImVec2 mousePosition) const;
    /** @brief Finds the nearest page reticle to the mouse for hit-testing. */
    std::optional<int> FindNearestPageReticle(const ViewportState& viewport, ImVec2 mousePosition) const;
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
                                                                const std::optional<mfd::PageStrobeDefinition>& previousStrobe);
    /** @brief Generates a reticle id that does not collide inside the provided container. */
    static std::string MakeUniqueReticleId(const std::vector<mfd::ReticleGroup>& groups, std::string_view baseId);
    /** @brief Generates a unique editor layer id inside one page. */
    static std::string MakeUniqueLayerId(const mfd::PageDefinition& page, std::string_view baseId);
    /** @brief Root window file currently open in the editor, or empty when no asset is loaded yet. */
    std::filesystem::path windowFile_ {};
    /** @brief Loader used to resolve the root window file and its referenced assets. */
    mfd::JsonLoader loader_ {};
    /** @brief In-memory authored document currently being edited. */
    mfd::LoadedWindowConfiguration loaded_ {};
    /** @brief Layout metadata tracking which authored object belongs to which file. */
    editor::EditorFileLayout files_ {};
    /** @brief Current tree and inspector selection. */
    Selection selection_ {};
    /** @brief Undo history of full document snapshots. */
    std::vector<UndoSnapshot> undoStack_ {};
    /** @brief Off-screen preview texture used by the viewports. */
    RenderTexture2D previewTexture_ {};
    /** @brief Indicates whether the preview texture currently owns GPU resources. */
    bool previewTextureReady_ = false;
    /** @brief Indicates whether the preview texture was successfully created with stencil support. */
    bool previewTextureStencilReady_ = false;
    /** @brief Shared texture cache used by image primitives in preview canvases. */
    mfd::ImageTextureCache previewImageCache_ {};
    /** @brief Off-screen texture dedicated to tree-item hover previews. */
    RenderTexture2D tooltipPreviewTexture_ {};
    /** @brief Indicates whether the hover-preview texture currently owns GPU resources. */
    bool tooltipPreviewTextureReady_ = false;
    /** @brief Indicates whether the hover-preview texture was successfully created with stencil support. */
    bool tooltipPreviewTextureStencilReady_ = false;
    /** @brief Font file declared by the current window configuration. */
    std::filesystem::path previewFontFile_ {};
    /** @brief GPU font override used by the preview canvases when available. */
    Font previewFont_ {};
    /** @brief Indicates whether the preview font currently owns GPU resources. */
    bool previewFontReady_ = false;
    /** @brief Indicates whether the current preview font file has already been attempted. */
    bool previewFontLoadAttempted_ = false;
    /** @brief Current status message shown in the footer. */
    std::string statusMessage_ {};
    /** @brief Indicates whether the footer status is an error. */
    bool statusIsError_ = false;
    /** @brief Last runtime exception surfaced by the editor shell. */
    std::string lastRuntimeError_ {};
    /** @brief Popup visibility flag for page creation. */
    bool showNewPagePopup_ = false;
    /** @brief Popup visibility flag for window creation. */
    bool showNewWindowPopup_ = false;
    /** @brief Popup visibility flag for library reticle creation. */
    bool showNewLibraryReticlePopup_ = false;
    /** @brief Popup visibility flag for library reticle duplication. */
    bool showDuplicateLibraryReticlePopup_ = false;
    /** @brief Popup visibility flag for the guided asset-folder picker. */
    bool showAssetFolderPickerPopup_ = false;
    /** @brief Page-creation draft values. */
    NewPageDraft newPageDraft_ {};
    /** @brief Window-creation draft values. */
    NewWindowDraft newWindowDraft_ {};
    /** @brief Reticle-creation draft values. */
    NewLibraryReticleDraft newLibraryReticleDraft_ {};
    /** @brief Reticle-duplication draft values. */
    DuplicateLibraryReticleDraft duplicateLibraryReticleDraft_ {};
    /** @brief Confirmation-popup state used by remove/delete page actions. */
    PageManagementPopupState pageManagementPopup_ {};
    /** @brief Confirmation-popup state used by page imports. */
    PageImportPopupState pageImportPopup_ {};
    /** @brief Confirmation-popup state used by global page renames. */
    PageRenamePopupState pageRenamePopup_ {};
    /** @brief Confirmation-popup state used by global reticle-template renames. */
    ReticleRenamePopupState reticleRenamePopup_ {};
    /** @brief Confirmation-popup state used by reusable-reticle extractions. */
    ReticleExtractionPopupState reticleExtractionPopup_ {};
    /** @brief Asset field currently edited through the guided folder picker. */
    AssetFolderPickerTarget assetFolderPickerTarget_ = AssetFolderPickerTarget::None;
    /** @brief Current folder displayed by the guided asset-folder picker. */
    std::filesystem::path assetFolderPickerCurrentFolder_ {};
    /** @brief Internal clipboard used by copy/paste on page reticles. */
    std::vector<mfd::ReticleGroup> pageReticleClipboard_ {};
    /** @brief Paste counter used to offset successive pasted copies. */
    int pageReticlePasteSerial_ = 0;
    /** @brief Private controller that owns the guided tutorial state and workflow. */
    std::unique_ptr<EditorTutorialController> tutorial_ {};
    /** @brief Hidden bridge mapping the live editor state to the automation subsystem. */
    std::unique_ptr<editor::automation::EditorApplicationAutomationBridge> automationBridge_ {};
    /** @brief Hidden facade exposing generic editor automation services to future in-process automation plugins. */
    std::unique_ptr<editor::automation::IEditorAutomationFacade> automationFacade_ {};
    /** @brief Stateless service owning the page remove/delete planning logic. */
    editor::PageManagementService pageManagementService_ {};
    /** @brief Stateless service owning the page import planning logic. */
    editor::PageImportService pageImportService_ {};
    /** @brief Stateless service owning the global page-rename planning logic. */
    editor::PageRenameService pageRenameService_ {};
    /** @brief Stateless service owning the global reticle-template rename planning logic. */
    editor::ReticleRenameService reticleRenameService_ {};
    /** @brief Stateless controller owning page-preview layer-focus decisions. */
    editor::LayerFocusController layerFocusController_ {};
    /** @brief Stateless service computing page highlights for the selected reticle template. */
    editor::ReticleUsageHighlightService reticleUsageHighlightService_ {};
    /** @brief Stateless service extracting one selected page-reticle block into a reusable template. */
    editor::ReticleExtractionService reticleExtractionService_ {};
    /** @brief Cached highlight result reused while the document and selected template stay unchanged. */
    struct ReticleUsageHighlightCacheState
    {
        bool dirty = true;
        std::string templateId {};
        std::filesystem::path assetsRoot {};
        bool includeExecAssets = false;
        editor::ReticleUsageHighlightResult result {};
    } reticleUsageHighlightCache_ {};
    /** @brief Future hidden conversation-surface state kept invisible until one UI consumer is explicitly added. */
    struct HiddenConversationSurfaceState
    {
        /** @brief Enables one future docked conversation panel once the product decides to expose it. */
        bool panelEnabled = false;
        /** @brief Enables one future detached overlay window once the product decides to expose it. */
        bool overlayWindowEnabled = false;
        /** @brief Stores the last external-agent session id for future hidden conversation adapters. */
        std::string lastExternalAgentSessionId {};
    } hiddenConversationSurface_ {};
    /** @brief Current direct-manipulation mode active in the preview. */
    InteractionMode interactionMode_ = InteractionMode::None;
    /** @brief Reticle currently manipulated by the user, when relevant. */
    int interactionReticleIndex_ = -1;
    /** @brief Ordered list of reticles manipulated together during one group move gesture. */
    std::vector<int> interactionReticleIndices_ {};
    /** @brief Primitive currently manipulated by the user, when relevant. */
    int interactionPrimitiveIndex_ = -1;
    /** @brief Handle index currently manipulated on the selected primitive. */
    int interactionHandleIndex_ = -1;
    /** @brief Kind of handle currently manipulated. */
    PrimitiveHandleKind interactionHandleKind_ = PrimitiveHandleKind::None;
    /** @brief Reticle transform snapshot captured at interaction start. */
    mfd::Transform2D interactionStartTransform_ {};
    /** @brief Reticle transform snapshots captured for every moved reticle at gesture start. */
    std::vector<mfd::Transform2D> interactionStartReticleTransforms_ {};
    /** @brief Primitive snapshot captured at interaction start. */
    mfd::Primitive interactionStartPrimitive_ {};
    /** @brief Mouse position in logical coordinates at interaction start. */
    mfd::Vec2 interactionStartMouseLogical_ {};
    /** @brief Mouse position in reticle-local coordinates at interaction start. */
    mfd::Vec2 interactionStartMouseReticleLocal_ {};
    /** @brief Mouse position in primitive-local coordinates at interaction start. */
    mfd::Vec2 interactionStartMousePrimitiveLocal_ {};
    /** @brief Starting angle used by rotate gestures. */
    float interactionStartAngleDegrees_ = 0.0f;
    /** @brief Starting distance used by scale gestures. */
    float interactionStartDistance_ = 0.0f;
    /** @brief Local visual center of the reticle when a direct manipulation starts. */
    mfd::Vec2 interactionStartReticleVisualCenterLocal_ {};
    /** @brief Screen-space center snapshot used by interactive overlays. */
    ImVec2 interactionStartCenterScreen_ {};
    /** @brief Screen-space corner snapshot used by interactive overlays. */
    ImVec2 interactionStartCornerScreen_ {};
    /** @brief Width of the left navigation pane. */
    float sidebarWidth_ = 320.0f;
    /** @brief Width of the right inspector pane. */
    float inspectorWidth_ = 360.0f;
    /** @brief Split ratio used by the library-studio sub-layout. */
    float libraryStudioPageWidth_ = 0.0f;
    /** @brief Indicates whether primitive labels stay visible in the reticle-studio overlay. */
    bool libraryStudioShowPrimitiveLabels_ = true;
    /** @brief Indicates whether reticle-studio gizmos such as bounds and handles stay visible. */
    bool libraryStudioShowGizmos_ = true;
    /** @brief Session-scoped display preferences for the page preview panel. */
    editor::PagePreviewViewOptions pagePreviewViewOptions_ {};
    /** @brief Editor-only layer-focus state used by the page-preview inspector strip. */
    editor::LayerFocusState layerFocusState_ {};
    /** @brief Editor-only page preview camera independent from the authored page view. */
    mfd::PageViewState pagePreviewView_ {};
    /** @brief Editor-only reticle-studio preview camera. */
    mfd::PageViewState libraryPreviewView_ {};
    /** @brief Logical offset preserved while dragging the viewport rectangle inside the minimap. */
    mfd::Vec2 minimapDragOffsetLogical_ {};
    /** @brief Reticles currently listed by the page-preview context menu. */
    std::vector<int> pagePreviewContextReticleIndices_ {};
    /** @brief Clip-capable reticle primitives currently listed by the page-preview context menu. */
    std::vector<PageClipTarget> pagePreviewContextTargets_ {};
};

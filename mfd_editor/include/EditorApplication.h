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
#include "mfd/io/JsonLoader.h"
#include "mfd/model/Reticle.h"

class EditorTutorialController;

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
    };

    /** @brief Draft values used by the "new page" popup before the page is created. */
    struct NewPageDraft
    {
        std::array<char, 64> name {};
        std::array<char, 64> title {};
        std::array<char, 64> fileName {};
        ImVec4 background {0.03f, 0.10f, 0.03f, 1.0f};
    };

    /** @brief Draft values used by the "new window" popup before a window is created from scratch. */
    struct NewWindowDraft
    {
        std::array<char, 128> windowFile {};
        std::array<char, 64> title {};
        int width = 640;
        int height = 480;
        int positionX = 120;
        int positionY = 80;
        std::array<char, 128> fontFile {};
        std::array<char, 128> reticleLibraryFolder {};
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
        std::array<char, 128> firstPageFile {};
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
    /** @brief Deletes the currently active page from the window definition. */
    void DeleteActivePage();
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
    /** @brief Draws the right-hand inspector for the current selection. */
    void DrawInspector();

    /** @brief Draws the authored page tree. */
    void DrawPageTree();
    /** @brief Draws the reticle-library tree. */
    void DrawLibraryTree();
    /** @brief Draws page-level properties such as view and blink types. */
    void DrawPageInspector();
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
    /** @brief Draws guides, selection boxes and coordinate overlays on the page preview. */
    void DrawPreviewOverlays(const ViewportState& viewport);
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
    /** @brief Updates the footer status message. */
    void RebuildStatus(std::string message, bool isError);

    /** @brief Opens the "new page" popup and seeds its draft values. */
    void OpenNewPagePopup();
    /** @brief Opens the "new window" popup and seeds its draft values. */
    void OpenNewWindowPopup();
    /** @brief Opens the "new library reticle" popup and seeds its draft values. */
    void OpenNewLibraryReticlePopup();
    /** @brief Opens the "duplicate reticle" popup and seeds its draft values. */
    void OpenDuplicateLibraryReticlePopup();
    /** @brief Draws and resolves all modal popups owned by the editor. */
    void DrawPopups();
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
    /** @brief Copies the selected page reticles into the editor clipboard. */
    void CopySelectedPageReticles();
    /** @brief Pastes the page-reticle clipboard into the active page. */
    void PasteCopiedPageReticles();

    /** @brief Updates the current selection from a click inside the page preview. */
    void UpdateReticleSelectionFromClick(const ViewportState& viewport, bool additiveSelection);
    /** @brief Finds the nearest page reticle to the mouse for hit-testing. */
    std::optional<int> FindNearestPageReticle(const ViewportState& viewport, ImVec2 mousePosition) const;
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
    /** @brief Generates a reticle id that does not collide inside the provided container. */
    static std::string MakeUniqueReticleId(const std::vector<mfd::ReticleGroup>& groups, std::string_view baseId);
    /** @brief Generates a unique editor layer id inside one page. */
    static std::string MakeUniqueLayerId(const mfd::PageDefinition& page, std::string_view baseId);
    /** @brief Root window file currently open in the editor. */
    std::filesystem::path windowFile_ {"assets/windows/demo_pages.json"};
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
    /** @brief Page-creation draft values. */
    NewPageDraft newPageDraft_ {};
    /** @brief Window-creation draft values. */
    NewWindowDraft newWindowDraft_ {};
    /** @brief Reticle-creation draft values. */
    NewLibraryReticleDraft newLibraryReticleDraft_ {};
    /** @brief Reticle-duplication draft values. */
    DuplicateLibraryReticleDraft duplicateLibraryReticleDraft_ {};
    /** @brief Internal clipboard used by copy/paste on page reticles. */
    std::vector<mfd::ReticleGroup> pageReticleClipboard_ {};
    /** @brief Paste counter used to offset successive pasted copies. */
    int pageReticlePasteSerial_ = 0;
    /** @brief Private controller that owns the guided tutorial state and workflow. */
    std::unique_ptr<EditorTutorialController> tutorial_ {};
    /** @brief Current direct-manipulation mode active in the preview. */
    InteractionMode interactionMode_ = InteractionMode::None;
    /** @brief Reticle currently manipulated by the user, when relevant. */
    int interactionReticleIndex_ = -1;
    /** @brief Primitive currently manipulated by the user, when relevant. */
    int interactionPrimitiveIndex_ = -1;
    /** @brief Handle index currently manipulated on the selected primitive. */
    int interactionHandleIndex_ = -1;
    /** @brief Kind of handle currently manipulated. */
    PrimitiveHandleKind interactionHandleKind_ = PrimitiveHandleKind::None;
    /** @brief Reticle transform snapshot captured at interaction start. */
    mfd::Transform2D interactionStartTransform_ {};
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
    /** @brief Editor-only page preview camera independent from the authored page view. */
    mfd::PageViewState pagePreviewView_ {};
    /** @brief Editor-only reticle-studio preview camera. */
    mfd::PageViewState libraryPreviewView_ {};
    /** @brief Logical offset preserved while dragging the viewport rectangle inside the minimap. */
    mfd::Vec2 minimapDragOffsetLogical_ {};
    /** @brief Reticle currently targeted by the page-preview context menu. */
    int pagePreviewContextReticleIndex_ = -1;
    /** @brief Primitive currently targeted by the page-preview clipping context menu. */
    int pagePreviewContextPrimitiveIndex_ = -1;
};

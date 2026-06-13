/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Private state and type groupings used to keep `EditorApplication` responsibility-oriented.
 */

#include <array>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <imgui.h>
#include <raylib.h>

#include "EditorAssetPathService.h"
#include "EditorDesignExportService.h"
#include "EditorDocumentSerializer.h"
#include "EditorFullscreenPreviewController.h"
#include "EditorLayerFocusController.h"
#include "internal/application/EditorPagePreviewDrawOrderController.h"
#include "EditorPageImportService.h"
#include "EditorPageManagementService.h"
#include "EditorPagePreviewHit.h"
#include "EditorPagePreviewViewOptions.h"
#include "EditorPageRenameService.h"
#include "EditorPrimitiveClipboardService.h"
#include "EditorReticleExtractionService.h"
#include "EditorReticleRenameService.h"
#include "EditorReticleUsageHighlightService.h"
#include "mfd/io/JsonLoader.h"
#include "mfd/model/Reticle.h"
#include "BezierPolylineCache.h"
#include "ImageTextureCache.h"
#include "TextLayoutCache.h"

namespace editor::app
{
/** @brief Input buffer capacity used for editable filesystem paths in the editor UI. */
inline constexpr std::size_t kPathTextCapacity = 512U;

/** @brief Current high-level selection type shown in the inspector. */
enum class SelectionKind
{
    Window,
    Page,
    PageReticle,
    PageTitle,
    PageStrobe,
    LibraryReticle,
    LibraryPrimitive
};

/** @brief Current direct-manipulation mode active in one preview viewport. */
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

/** @brief Type of primitive handle currently manipulated inside the reticle studio. */
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
 * @note The editor uses the same normalized `[-1, 1]` space as the runtime.
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

/** @brief Ranked hit candidate used by page-preview selection. */
struct PageReticleHit
{
    editor::PagePreviewHitTarget target {};
    float distance = std::numeric_limits<float>::max();
    float area = std::numeric_limits<float>::max();
    bool directHit = false;
    bool boundsHit = false;
    editor::PagePreviewDrawOrderKey drawOrder {};
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
    bool commandUdpExposed = true;
    bool commandUdpEnabled = true;
    std::array<char, 64> commandAddress {};
    int commandPort = 49000;
    int commandMaxPacketSize = 65507;
    bool feedbackUdpExposed = false;
    bool feedbackUdpEnabled = false;
    std::array<char, 64> feedbackAddress {};
    int feedbackPort = 49001;
    int feedbackMaxPacketSize = 65507;
    float feedbackFastIntervalSeconds = 0.020f;
    float feedbackHeartbeatIntervalSeconds = 0.350f;
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

/** @brief UI state driving the design-export popup and its execution feedback. */
struct DesignExportPopupState
{
    bool openRequested = false;
    std::array<char, kPathTextCapacity> outputFolder {};
    bool exportMarkdownIcd = true;
    bool exportExplodedViews = true;
    bool includeCanvasCoordinates = true;
    bool includeCppSnippets = true;
    bool includeStrobe = true;
    bool includeBlink = true;
    bool includePrimitiveIds = true;
    bool includeMappingHash = true;
    bool exportCompleted = false;
    std::filesystem::path exportedFolder {};
    std::vector<std::string> warnings {};
};

/** @brief One cached render target reused by one layer-preview thumbnail slot. */
struct LayerPreviewTextureSlot
{
    RenderTexture2D texture {};
    bool ready = false;
    bool stencilReady = false;
    int width = 0;
    int height = 0;
};

/** @brief Cached highlight result reused while the document and selected template stay unchanged. */
struct ReticleUsageHighlightCacheState
{
    bool dirty = true;
    std::string templateId {};
    std::filesystem::path assetsRoot {};
    editor::ReticleUsageHighlightResult result {};
};

/** @brief Cached generated page-title reticle reused while the current authored title state stays unchanged. */
struct PageTitlePreviewReticleCacheState
{
    bool valid = false;
    std::string pageName {};
    std::string pageTitle {};
    mfd::PageTitleDisplayDefinition display {};
    mfd::ReticleGroup reticle {};
};

/** @brief Document, file-layout and selection state owned by the editor shell. */
struct DocumentState
{
    std::filesystem::path windowFile {};
    mfd::JsonLoader loader {};
    editor::EditorAssetPathService assetPaths {};
    mfd::LoadedWindowConfiguration loaded {};
    editor::EditorFileLayout files {};
    Selection selection {};
    std::vector<UndoSnapshot> undoStack {};
};

/** @brief GPU resources, preview caches and per-canvas derived caches. */
struct PreviewState
{
    RenderTexture2D previewTexture {};
    bool previewTextureReady = false;
    bool previewTextureStencilReady = false;
    std::vector<int> orderedStaticReticleIndices {};
    mfd::BezierPolylineCache previewBezierCache {};
    mfd::ImageTextureCache previewImageCache {};
    mfd::TextLayoutCache previewTextLayoutCache {};
    RenderTexture2D tooltipPreviewTexture {};
    bool tooltipPreviewTextureReady = false;
    bool tooltipPreviewTextureStencilReady = false;
    std::vector<LayerPreviewTextureSlot> layerPreviewTextures {};
    std::filesystem::path previewFontFile {};
    Font previewFont {};
    bool previewFontReady = false;
    bool previewFontLoadAttempted = false;
    ReticleUsageHighlightCacheState reticleUsageHighlightCache {};
    mutable PageTitlePreviewReticleCacheState pageTitlePreviewReticleCache {};
};

/** @brief Transient authoring workflow state, drafts and footer notifications. */
struct WorkflowState
{
    std::string statusMessage {};
    bool statusIsError = false;
    std::string lastRuntimeError {};
    bool showNewPagePopup = false;
    bool showNewWindowPopup = false;
    bool showNewLibraryReticlePopup = false;
    bool showDuplicateLibraryReticlePopup = false;
    NewPageDraft newPageDraft {};
    NewWindowDraft newWindowDraft {};
    NewLibraryReticleDraft newLibraryReticleDraft {};
    DuplicateLibraryReticleDraft duplicateLibraryReticleDraft {};
    PageManagementPopupState pageManagementPopup {};
    PageImportPopupState pageImportPopup {};
    PageRenamePopupState pageRenamePopup {};
    ReticleRenamePopupState reticleRenamePopup {};
    ReticleExtractionPopupState reticleExtractionPopup {};
    DesignExportPopupState designExportPopup {};
};

/** @brief Editor clipboards and paste serials used by page and library authoring flows. */
struct ClipboardState
{
    std::vector<mfd::ReticleGroup> pageReticleClipboard {};
    std::optional<mfd::ReticleGroup> libraryReticleClipboard {};
    std::optional<mfd::Primitive> libraryPrimitiveClipboard {};
    int pageReticlePasteSerial = 0;
    int libraryPrimitivePasteSerial = 0;
};

/** @brief Stateless services and controllers consumed across editor responsibilities. */
struct ServicesState
{
    editor::PageManagementService pageManagement {};
    editor::PageImportService pageImport {};
    editor::PageRenameService pageRename {};
    editor::PrimitiveClipboardService primitiveClipboard {};
    editor::ReticleRenameService reticleRename {};
    editor::EditorDesignExportService designExport {};
    editor::FullscreenPreviewController fullscreenPreview {};
    editor::LayerFocusController layerFocus {};
    editor::PagePreviewDrawOrderController pagePreviewDrawOrder {};
    editor::ReticleUsageHighlightService reticleUsageHighlight {};
    editor::ReticleExtractionService reticleExtraction {};
};

/** @brief Current direct-manipulation interaction state active in the page or library previews. */
struct InteractionState
{
    InteractionMode mode = InteractionMode::None;
    int reticleIndex = -1;
    std::vector<int> reticleIndices {};
    int primitiveIndex = -1;
    int handleIndex = -1;
    PrimitiveHandleKind handleKind = PrimitiveHandleKind::None;
    mfd::Transform2D startTransform {};
    std::vector<mfd::Transform2D> startReticleTransforms {};
    mfd::Primitive startPrimitive {};
    mfd::Vec2 startMouseLogical {};
    mfd::Vec2 startMouseReticleLocal {};
    mfd::Vec2 startMousePrimitiveLocal {};
    float startAngleDegrees = 0.0f;
    float startDistance = 0.0f;
    mfd::Vec2 startReticleVisualCenterLocal {};
    ImVec2 startCenterScreen {};
    ImVec2 startCornerScreen {};
};

/** @brief Session-scoped layout, view-mode and context-menu state for the editor workspace. */
struct LayoutState
{
    float sidebarWidth = 320.0f;
    bool sidebarVisible = true;
    float inspectorWidth = 360.0f;
    bool inspectorVisible = true;
    std::array<char, 96> sidebarFilter {};
    std::map<std::string, bool> inspectorSectionOpen {};
    float libraryStudioPageWidth = 0.0f;
    bool libraryStudioShowPrimitiveLabels = true;
    bool libraryStudioShowGizmos = true;
    editor::PagePreviewViewOptions pagePreviewViewOptions {};
    editor::LayerFocusState layerFocusState {};
    mfd::PageViewState pagePreviewView {};
    mfd::PageViewState libraryPreviewView {};
    mfd::Vec2 minimapDragOffsetLogical {};
    bool suppressNextPagePreviewContextMenu = false;
    bool suppressNextLibraryPreviewContextMenu = false;
    std::vector<int> pagePreviewContextReticleIndices {};
    std::vector<PageClipTarget> pagePreviewContextTargets {};
};
} // namespace editor::app

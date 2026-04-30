/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Strongly-typed automation contracts used by hidden editor automation plugins and tests.
 */

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "EditorDocumentSerializer.h"
#include "mfd/io/JsonLoader.h"

namespace editor::automation
{
/**
 * @brief Opaque window identifier exposed to external automation clients.
 */
struct WindowId
{
    std::string value;

    [[nodiscard]] bool empty() const noexcept
    {
        return value.empty();
    }
};

/**
 * @brief Opaque page identifier exposed to external automation clients.
 */
struct PageId
{
    std::string value;

    [[nodiscard]] bool empty() const noexcept
    {
        return value.empty();
    }
};

/**
 * @brief Opaque reticle-template identifier exposed to external automation clients.
 */
struct ReticleAssetId
{
    std::string value;

    [[nodiscard]] bool empty() const noexcept
    {
        return value.empty();
    }
};

/**
 * @brief Opaque page-reticle-instance identifier exposed to external automation clients.
 */
struct PageReticleInstanceId
{
    std::string value;

    [[nodiscard]] bool empty() const noexcept
    {
        return value.empty();
    }
};

/**
 * @brief Opaque primitive identifier exposed to external automation clients.
 */
struct PrimitiveId
{
    std::string value;

    [[nodiscard]] bool empty() const noexcept
    {
        return value.empty();
    }
};

/**
 * @brief Opaque editor-layer identifier exposed to external automation clients.
 */
struct LayerId
{
    std::string value;

    [[nodiscard]] bool empty() const noexcept
    {
        return value.empty();
    }
};

/**
 * @brief Opaque automation-session identifier exposed to external automation clients.
 */
struct AutomationSessionId
{
    std::string value;

    [[nodiscard]] bool empty() const noexcept
    {
        return value.empty();
    }
};

/**
 * @brief Normalized error categories returned by automation services.
 */
enum class AutomationErrorCode
{
    None,
    InvalidArgument,
    InvalidState,
    NotFound,
    Conflict,
    ValidationFailed,
    IoFailure,
    TransportFailure,
    Unsupported,
    InternalFailure
};

/**
 * @brief Structured error returned by automation services and the transport boundary.
 */
struct AutomationError
{
    AutomationErrorCode code = AutomationErrorCode::None;
    std::string message;

    [[nodiscard]] bool ok() const noexcept
    {
        return code == AutomationErrorCode::None;
    }
};

/**
 * @brief Success/error wrapper returned by commands that do not need a payload.
 */
struct AutomationStatus
{
    AutomationError error;

    [[nodiscard]] bool ok() const noexcept
    {
        return error.ok();
    }

    static AutomationStatus Success() noexcept
    {
        return {};
    }
};

/**
 * @brief Success/error wrapper returned by commands that produce a payload.
 * @tparam T Result payload type.
 */
template <typename T>
struct AutomationResult
{
    T value {};
    AutomationError error {};

    [[nodiscard]] bool ok() const noexcept
    {
        return error.ok();
    }

    static AutomationResult<T> Success(T payload)
    {
        AutomationResult<T> result;
        result.value = std::move(payload);
        return result;
    }
};

/**
 * @brief Current high-level UI selection known by the hidden automation bridge.
 */
enum class AutomationSelectionKind
{
    None,
    Page,
    PageReticleInstance,
    ReticleAsset,
    Primitive
};

/**
 * @brief Primitive owner category targeted by authoring actions.
 */
enum class AutomationPrimitiveOwnerKind
{
    ReticleAsset,
    PageReticleInstance
};

/**
 * @brief Reticle target category used by generic reticle-level automation actions.
 */
enum class AutomationReticleTargetKind
{
    ReticleAsset,
    PageReticleInstance,
    PageStrobe
};

/**
 * @brief Event kinds emitted by the automation subsystem after successful mutations.
 */
enum class AutomationEventKind
{
    DocumentChanged,
    SelectionChanged,
    ActivePageChanged,
    ValidationChanged,
    AssetSaved,
    SessionCommitted,
    SessionRolledBack
};

/**
 * @brief Current editor UI state exposed through automation snapshots.
 */
struct AutomationUiState
{
    AutomationSelectionKind selectionKind = AutomationSelectionKind::None;
    PageId activePageId {};
    std::vector<PageReticleInstanceId> selectedPageReticleIds {};
    ReticleAssetId selectedReticleAssetId {};
    PrimitiveId selectedPrimitiveId {};
    AutomationPrimitiveOwnerKind selectedPrimitiveOwnerKind = AutomationPrimitiveOwnerKind::ReticleAsset;
    std::string selectedPrimitiveOwnerId {};
    int selectedPrimitiveIndex = -1;
    mfd::PageViewState pagePreviewView {};
    mfd::PageViewState libraryPreviewView {};
};

/**
 * @brief One primitive entry exposed through an automation snapshot.
 */
struct PrimitiveSnapshot
{
    PrimitiveId id {};
    AutomationPrimitiveOwnerKind ownerKind = AutomationPrimitiveOwnerKind::ReticleAsset;
    std::string ownerId {};
    int index = -1;
    mfd::Primitive primitive {};
};

/**
 * @brief One page-reticle instance exposed through an automation snapshot.
 */
struct PageReticleInstanceSnapshot
{
    PageReticleInstanceId id {};
    int index = -1;
    mfd::ReticleGroup reticle {};
    std::vector<PrimitiveSnapshot> primitives {};
};

/**
 * @brief One reticle-library template exposed through an automation snapshot.
 */
struct ReticleAssetSnapshot
{
    ReticleAssetId id {};
    std::filesystem::path sourceFile {};
    mfd::ReticleGroup reticle {};
    std::vector<PrimitiveSnapshot> primitives {};
};

/**
 * @brief One editor layer exposed through an automation snapshot.
 */
struct LayerSnapshot
{
    LayerId id {};
    int index = -1;
    mfd::EditorLayerDefinition layer {};
};

/**
 * @brief One page entry exposed through an automation snapshot.
 */
struct PageSnapshot
{
    PageId id {};
    int index = -1;
    std::filesystem::path sourceFile {};
    mfd::PageDefinition page {};
    std::vector<PageReticleInstanceSnapshot> reticles {};
    std::vector<LayerSnapshot> layers {};
};

/**
 * @brief Full editor snapshot returned by the query service.
 */
struct DocumentSnapshot
{
    bool hasOpenWindow = false;
    WindowId windowId {};
    mfd::WindowAssetDefinition window {};
    std::vector<PageSnapshot> pages {};
    std::vector<ReticleAssetSnapshot> reticleAssets {};
    editor::EditorFileLayout fileLayout {};
    AutomationUiState uiState {};
    bool sessionActive = false;
    AutomationSessionId sessionId {};
};

/**
 * @brief Snapshot of the live editor state used internally for preview sessions and rollback.
 */
struct EditorStateSnapshot
{
    std::filesystem::path windowFile {};
    mfd::LoadedWindowConfiguration loaded {};
    editor::EditorFileLayout files {};
    AutomationUiState uiState {};
};

/**
 * @brief Human-readable diagnostic reported by validation passes.
 */
struct ValidationDiagnostic
{
    AutomationErrorCode code = AutomationErrorCode::None;
    std::string entityId {};
    std::string message {};
};

/**
 * @brief Validation report returned by session and persistence services.
 */
struct ValidationReport
{
    bool valid = true;
    std::vector<ValidationDiagnostic> diagnostics {};
};

/**
 * @brief Event emitted by the hidden automation subsystem after one state transition.
 */
struct AutomationEvent
{
    AutomationEventKind kind = AutomationEventKind::DocumentChanged;
    std::string entityId {};
    std::string message {};
};

/**
 * @brief Save result returned by persistence commands.
 */
struct SaveResult
{
    std::vector<std::filesystem::path> savedFiles {};
};

/**
 * @brief JSON preview exported for one automation-visible entity.
 */
struct ExportJsonPreviewResult
{
    std::filesystem::path sourcePath {};
    std::string json {};
};

/**
 * @brief Creates a brand-new authored window document and optional initial page.
 */
struct CreateWindowDocumentRequest
{
    mfd::WindowAssetDefinition window {};
    std::optional<mfd::PageDefinition> initialPage {};
    std::optional<std::filesystem::path> windowFile {};
    std::optional<std::filesystem::path> reticleLibraryFolder {};
    std::optional<std::filesystem::path> initialPageFile {};
};

/**
 * @brief Replaces the authored window settings while keeping the current document content.
 */
struct ReplaceWindowAssetRequest
{
    mfd::WindowAssetDefinition window {};
    std::optional<std::filesystem::path> windowFile {};
    std::optional<std::filesystem::path> reticleLibraryFolder {};
};

/**
 * @brief Creates one new page asset inside the current document.
 */
struct CreatePageAssetRequest
{
    mfd::PageDefinition page {};
    std::optional<std::filesystem::path> filePathHint {};
};

/**
 * @brief Replaces one existing page asset with a full page definition.
 */
struct ReplacePageAssetRequest
{
    PageId pageId {};
    mfd::PageDefinition page {};
    std::optional<std::filesystem::path> filePathHint {};
};

/**
 * @brief Deletes one existing page asset from the current document.
 */
struct DeletePageAssetRequest
{
    PageId pageId {};
};

/**
 * @brief Creates one new reticle template in the shared reticle library.
 */
struct CreateReticleAssetRequest
{
    mfd::ReticleGroup reticle {};
    std::optional<std::filesystem::path> filePathHint {};
};

/**
 * @brief Replaces one existing reticle template with a full reticle definition.
 */
struct ReplaceReticleAssetRequest
{
    ReticleAssetId reticleId {};
    mfd::ReticleGroup reticle {};
    std::optional<std::filesystem::path> filePathHint {};
};

/**
 * @brief Deletes one reticle template from the shared reticle library.
 */
struct DeleteReticleAssetRequest
{
    ReticleAssetId reticleId {};
};

/**
 * @brief Instantiates one library reticle onto one page as a page-local reticle instance.
 */
struct InstantiateReticleOnPageRequest
{
    PageId pageId {};
    ReticleAssetId reticleAssetId {};
    std::optional<std::string> instanceId {};
    mfd::Transform2D transform {};
    bool assignDefaultLayer = true;
};

/**
 * @brief Replaces one page-reticle instance with a full reticle definition.
 */
struct ReplacePageReticleInstanceRequest
{
    PageReticleInstanceId pageReticleId {};
    mfd::ReticleGroup reticle {};
};

/**
 * @brief Deletes one page-reticle instance from one page.
 */
struct DeletePageReticleInstanceRequest
{
    PageReticleInstanceId pageReticleId {};
};

/**
 * @brief Reorders one page-reticle instance inside its parent page draw order.
 */
struct MovePageReticleRequest
{
    PageReticleInstanceId pageReticleId {};
    int targetIndex = 0;
};

/**
 * @brief Selects one primitive inside a reticle asset or a page-reticle instance.
 */
struct PrimitiveSelector
{
    std::string primitiveId {};
    int primitiveIndex = -1;
};

/**
 * @brief Appends one primitive to a reticle asset or page-reticle instance.
 */
struct AddPrimitiveRequest
{
    AutomationPrimitiveOwnerKind ownerKind = AutomationPrimitiveOwnerKind::ReticleAsset;
    std::string ownerId {};
    mfd::Primitive primitive {};
    std::optional<int> insertIndex {};
};

/**
 * @brief Replaces one primitive inside a reticle asset or page-reticle instance.
 */
struct ReplacePrimitiveRequest
{
    AutomationPrimitiveOwnerKind ownerKind = AutomationPrimitiveOwnerKind::ReticleAsset;
    std::string ownerId {};
    PrimitiveSelector primitive {};
    mfd::Primitive replacement {};
};

/**
 * @brief Deletes one primitive from a reticle asset or page-reticle instance.
 */
struct DeletePrimitiveRequest
{
    AutomationPrimitiveOwnerKind ownerKind = AutomationPrimitiveOwnerKind::ReticleAsset;
    std::string ownerId {};
    PrimitiveSelector primitive {};
};

/**
 * @brief Toggles the visibility of one reticle template.
 */
struct SetReticleAssetVisibilityRequest
{
    ReticleAssetId reticleId {};
    bool visible = true;
};

/**
 * @brief Toggles draw-on-top ordering for one reticle template.
 */
struct SetReticleAssetDrawOnTopRequest
{
    ReticleAssetId reticleId {};
    bool drawOnTop = false;
};

/**
 * @brief Toggles the visibility of one page-reticle instance.
 */
struct SetReticleVisibilityRequest
{
    PageReticleInstanceId pageReticleId {};
    bool visible = true;
};

/**
 * @brief Toggles draw-on-top ordering for one page-reticle instance.
 */
struct SetReticleDrawOnTopRequest
{
    PageReticleInstanceId pageReticleId {};
    bool drawOnTop = false;
};

/**
 * @brief Replaces the clipping state of one reticle asset, page-reticle instance or page strobe.
 */
struct SetReticleClippingRequest
{
    AutomationReticleTargetKind targetKind = AutomationReticleTargetKind::PageReticleInstance;
    std::string targetId {};
    mfd::ReticleClipState clipping {};
};

/**
 * @brief Updates the transform of one reticle asset, page-reticle instance or page strobe.
 */
struct SetReticleTransformRequest
{
    AutomationReticleTargetKind targetKind = AutomationReticleTargetKind::PageReticleInstance;
    std::string targetId {};
    mfd::Transform2D transform {};
};

/**
 * @brief Updates the transform of one primitive.
 */
struct SetPrimitiveTransformRequest
{
    AutomationPrimitiveOwnerKind ownerKind = AutomationPrimitiveOwnerKind::ReticleAsset;
    std::string ownerId {};
    PrimitiveSelector primitive {};
    mfd::Transform2D transform {};
};

/**
 * @brief Toggles the visibility of one primitive.
 */
struct SetPrimitiveVisibilityRequest
{
    AutomationPrimitiveOwnerKind ownerKind = AutomationPrimitiveOwnerKind::ReticleAsset;
    std::string ownerId {};
    PrimitiveSelector primitive {};
    bool visible = true;
};

/**
 * @brief Toggles whether one primitive is exposed through the generated client API.
 */
struct SetPrimitiveExposedRequest
{
    AutomationPrimitiveOwnerKind ownerKind = AutomationPrimitiveOwnerKind::ReticleAsset;
    std::string ownerId {};
    PrimitiveSelector primitive {};
    bool exposed = true;
};

/**
 * @brief Updates the stroke style of one primitive.
 */
struct SetPrimitiveLineStyleRequest
{
    AutomationPrimitiveOwnerKind ownerKind = AutomationPrimitiveOwnerKind::ReticleAsset;
    std::string ownerId {};
    PrimitiveSelector primitive {};
    mfd::LineStyle lineStyle = mfd::LineStyle::Solid;
};

/**
 * @brief Assigns one page-reticle instance to one editor layer inside its page.
 */
struct SetPageReticleLayerRequest
{
    PageReticleInstanceId pageReticleId {};
    LayerId layerId {};
};

/**
 * @brief Appends one new editor layer to one page.
 */
struct CreateLayerRequest
{
    PageId pageId {};
    mfd::EditorLayerDefinition layer {};
    std::optional<int> insertIndex {};
};

/**
 * @brief Replaces one existing editor layer and updates page-reticle references when renamed.
 */
struct ReplaceLayerRequest
{
    PageId pageId {};
    LayerId layerId {};
    mfd::EditorLayerDefinition layer {};
};

/**
 * @brief Deletes one editor layer from one page and clears reticle assignments that referenced it.
 */
struct DeleteLayerRequest
{
    PageId pageId {};
    LayerId layerId {};
};

/**
 * @brief Replaces the full blink-type catalog of one page.
 */
struct ReplacePageBlinkTypesRequest
{
    PageId pageId {};
    std::vector<mfd::PageBlinkDefinition> blinkTypes {};
};

/**
 * @brief Updates the default page blink type used by reticles that enable blink without naming one.
 */
struct SetPageDefaultBlinkTypeRequest
{
    PageId pageId {};
    std::string blinkTypeName {};
};

/**
 * @brief Updates the blink binding of one page-reticle instance.
 */
struct SetPageReticleBlinkRequest
{
    PageReticleInstanceId pageReticleId {};
    mfd::ReticleBlinkState blink {};
};

/**
 * @brief Assigns one reticle template as the page strobe, preserving prior authored settings when possible.
 */
struct AssignPageStrobeTemplateRequest
{
    PageId pageId {};
    ReticleAssetId reticleAssetId {};
    std::optional<std::string> strobeId {};
};

/**
 * @brief Replaces one page strobe with a fully specified authored definition.
 */
struct ReplacePageStrobeRequest
{
    PageId pageId {};
    mfd::PageStrobeDefinition strobe {};
};

/**
 * @brief Removes the page strobe from one page.
 */
struct DeletePageStrobeRequest
{
    PageId pageId {};
};

/**
 * @brief Toggles the visibility of one editor layer inside one page.
 */
struct SetLayerVisibilityRequest
{
    PageId pageId {};
    LayerId layerId {};
    bool visible = true;
};

/**
 * @brief Updates the live editor selection without exposing raw ImGui widgets.
 */
struct SelectEntityRequest
{
    AutomationSelectionKind selectionKind = AutomationSelectionKind::None;
    std::string targetId {};
};

/**
 * @brief Strongly-typed semantic action applied during one preview session.
 */
using AutomationAction = std::variant<CreateWindowDocumentRequest,
                                      ReplaceWindowAssetRequest,
                                      CreatePageAssetRequest,
                                      ReplacePageAssetRequest,
                                      DeletePageAssetRequest,
                                      CreateReticleAssetRequest,
                                      ReplaceReticleAssetRequest,
                                      DeleteReticleAssetRequest,
                                      InstantiateReticleOnPageRequest,
                                      ReplacePageReticleInstanceRequest,
                                      DeletePageReticleInstanceRequest,
                                      MovePageReticleRequest,
                                      AddPrimitiveRequest,
                                      ReplacePrimitiveRequest,
                                      DeletePrimitiveRequest,
                                      SetReticleAssetVisibilityRequest,
                                      SetReticleAssetDrawOnTopRequest,
                                      SetReticleVisibilityRequest,
                                      SetReticleDrawOnTopRequest,
                                      SetReticleClippingRequest,
                                      SetReticleTransformRequest,
                                      SetPrimitiveTransformRequest,
                                      SetPrimitiveVisibilityRequest,
                                      SetPrimitiveExposedRequest,
                                      SetPrimitiveLineStyleRequest,
                                      SetPageReticleLayerRequest,
                                      CreateLayerRequest,
                                      ReplaceLayerRequest,
                                      DeleteLayerRequest,
                                      ReplacePageBlinkTypesRequest,
                                      SetPageDefaultBlinkTypeRequest,
                                      SetPageReticleBlinkRequest,
                                      AssignPageStrobeTemplateRequest,
                                      ReplacePageStrobeRequest,
                                      DeletePageStrobeRequest,
                                      SetLayerVisibilityRequest,
                                      SelectEntityRequest>;

/**
 * @brief Opens one new preview session.
 */
struct BeginSessionRequest
{
    std::string label {};
};

/**
 * @brief Applies one semantic action to one active preview session.
 */
struct ApplyActionRequest
{
    AutomationSessionId sessionId {};
    AutomationAction action {};
};

/**
 * @brief Validates one active preview session.
 */
struct ValidateSessionRequest
{
    AutomationSessionId sessionId {};
};

/**
 * @brief Commits one active preview session and records one undo step.
 */
struct CommitSessionRequest
{
    AutomationSessionId sessionId {};
};

/**
 * @brief Rolls back one active preview session to its captured pre-session snapshot.
 */
struct RollbackSessionRequest
{
    AutomationSessionId sessionId {};
};

/**
 * @brief Requests a JSON preview for one window/page/reticle entity.
 */
struct ExportJsonPreviewRequest
{
    /**
     * @brief JSON-producing entity categories supported by the persistence service.
     */
    enum class Kind
    {
        Window,
        Page,
        ReticleAsset,
        PageReticleInstance
    };

    Kind kind = Kind::Window;
    std::string entityId {};
};

/**
 * @brief Requests one save restricted to one specific entity when supported.
 */
struct SaveAssetRequest
{
    /**
     * @brief Saved entity categories supported by the persistence service.
     */
    enum class Kind
    {
        Page,
        ReticleAsset
    };

    Kind kind = Kind::Page;
    std::string entityId {};
};
} // namespace editor::automation

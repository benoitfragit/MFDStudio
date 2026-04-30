/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Stable native C ABI used by external in-process editor automation plugins.
 *
 * @details This boundary intentionally avoids C++ classes, STL containers,
 * exceptions, COM, and raw UI objects across the DLL edge. The editor keeps
 * ownership of the live document, undo/redo, validation, JSON serialization,
 * and disk persistence. Plugins consume only the typed callback tables defined
 * below.
 */

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#    define MFD_EDITOR_AUTOMATION_CALL __cdecl
#    if defined(MFD_EDITOR_AUTOMATION_PLUGIN_EXPORTS)
#        define MFD_EDITOR_AUTOMATION_EXPORT __declspec(dllexport)
#    else
#        define MFD_EDITOR_AUTOMATION_EXPORT
#    endif
#else
#    define MFD_EDITOR_AUTOMATION_CALL
#    if defined(__GNUC__) || defined(__clang__)
#        define MFD_EDITOR_AUTOMATION_EXPORT __attribute__((visibility("default")))
#    else
#        define MFD_EDITOR_AUTOMATION_EXPORT
#    endif
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief ABI revision implemented by the editor automation plugin contract. */
enum
{
    MFD_EDITOR_AUTOMATION_PLUGIN_ABI_VERSION = 4u
};

/** @brief Exported symbol name returning the plugin function table. */
#define MFD_EDITOR_AUTOMATION_PLUGIN_ENTRY_POINT "MfdGetEditorAutomationPluginApi"

/**
 * @brief Status codes exchanged across the stable automation plugin ABI.
 */
typedef enum MfdEditorAutomationResultCode
{
    MfdEditorAutomationResultCode_Success = 0,
    MfdEditorAutomationResultCode_BufferTooSmall = 1,
    MfdEditorAutomationResultCode_InvalidArgument = 2,
    MfdEditorAutomationResultCode_InvalidState = 3,
    MfdEditorAutomationResultCode_NotFound = 4,
    MfdEditorAutomationResultCode_Conflict = 5,
    MfdEditorAutomationResultCode_ValidationFailed = 6,
    MfdEditorAutomationResultCode_IoFailure = 7,
    MfdEditorAutomationResultCode_TransportFailure = 8,
    MfdEditorAutomationResultCode_Unsupported = 9,
    MfdEditorAutomationResultCode_InternalFailure = 10,
    MfdEditorAutomationResultCode_AbiMismatch = 11
} MfdEditorAutomationResultCode;

/**
 * @brief Borrowed UTF-8 string slice crossing the plugin ABI.
 *
 * @note The memory is owned by the caller of the ABI function.
 */
typedef struct MfdEditorStringView
{
    const char* data;
    size_t size;
} MfdEditorStringView;

/**
 * @brief Mutable UTF-8 output buffer used to return strings and human-readable errors.
 *
 * @note `size` receives the full string length excluding the trailing null
 * terminator. Implementations should write at most `capacity - 1` bytes,
 * null-terminate when `capacity > 0`, and return
 * `MfdEditorAutomationResultCode_BufferTooSmall` when truncation occurred.
 */
typedef struct MfdEditorUtf8Buffer
{
    char* data;
    size_t capacity;
    size_t size;
} MfdEditorUtf8Buffer;

/** @brief Opaque preview-session handle issued by the host to one plugin. */
typedef uint64_t MfdEditorAutomationSessionHandle;

/**
 * @brief Two-dimensional vector expressed in logical editor coordinates.
 */
typedef struct MfdEditorAutomationVec2
{
    float x;
    float y;
} MfdEditorAutomationVec2;

/**
 * @brief RGBA color expressed on 8 bits per channel.
 */
typedef struct MfdEditorAutomationColorRgba
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} MfdEditorAutomationColorRgba;

/**
 * @brief Editor page view state exposed to automation callers.
 */
typedef struct MfdEditorAutomationPageView
{
    MfdEditorAutomationVec2 center;
    float zoom;
    float reserved_padding;
} MfdEditorAutomationPageView;

/**
 * @brief Fixed-size 2D transform expressed in logical editor coordinates.
 */
typedef struct MfdEditorAutomationTransform2D
{
    float position_x;
    float position_y;
    float rotation_degrees;
    float scale_x;
    float scale_y;
} MfdEditorAutomationTransform2D;

/**
 * @brief Stable save-target kinds supported by the host-owned persistence service.
 */
typedef enum MfdEditorAutomationSaveAssetKind
{
    MfdEditorAutomationSaveAssetKind_Page = 0,
    MfdEditorAutomationSaveAssetKind_ReticleAsset = 1
} MfdEditorAutomationSaveAssetKind;

/**
 * @brief Stable automation event kinds emitted by the editor host.
 */
typedef enum MfdEditorAutomationEventKind
{
    MfdEditorAutomationEventKind_DocumentChanged = 0,
    MfdEditorAutomationEventKind_SelectionChanged = 1,
    MfdEditorAutomationEventKind_ActivePageChanged = 2,
    MfdEditorAutomationEventKind_ValidationChanged = 3,
    MfdEditorAutomationEventKind_AssetSaved = 4,
    MfdEditorAutomationEventKind_SessionCommitted = 5,
    MfdEditorAutomationEventKind_SessionRolledBack = 6
} MfdEditorAutomationEventKind;

/**
 * @brief High-level selection kinds exposed through the automation UI snapshot.
 */
typedef enum MfdEditorAutomationSelectionKind
{
    MfdEditorAutomationSelectionKind_None = 0,
    MfdEditorAutomationSelectionKind_Page = 1,
    MfdEditorAutomationSelectionKind_PageReticleInstance = 2,
    MfdEditorAutomationSelectionKind_ReticleAsset = 3,
    MfdEditorAutomationSelectionKind_Primitive = 4
} MfdEditorAutomationSelectionKind;

/**
 * @brief Primitive owner categories supported by automation queries and actions.
 */
typedef enum MfdEditorAutomationPrimitiveOwnerKind
{
    MfdEditorAutomationPrimitiveOwnerKind_ReticleAsset = 0,
    MfdEditorAutomationPrimitiveOwnerKind_PageReticleInstance = 1
} MfdEditorAutomationPrimitiveOwnerKind;

/**
 * @brief Reticle target categories supported by generic reticle-level actions.
 */
typedef enum MfdEditorAutomationReticleTargetKind
{
    MfdEditorAutomationReticleTargetKind_ReticleAsset = 0,
    MfdEditorAutomationReticleTargetKind_PageReticleInstance = 1,
    MfdEditorAutomationReticleTargetKind_PageStrobe = 2
} MfdEditorAutomationReticleTargetKind;

/**
 * @brief Stable clipping modes supported by generic reticle-level actions.
 */
typedef enum MfdEditorAutomationReticleClipMode
{
    MfdEditorAutomationReticleClipMode_None = 0,
    MfdEditorAutomationReticleClipMode_Inner = 1,
    MfdEditorAutomationReticleClipMode_Outer = 2
} MfdEditorAutomationReticleClipMode;

/**
 * @brief Primitive kinds exposed by automation queries and asset-seeding requests.
 */
typedef enum MfdEditorAutomationPrimitiveType
{
    MfdEditorAutomationPrimitiveType_Text = 0,
    MfdEditorAutomationPrimitiveType_Time = 1,
    MfdEditorAutomationPrimitiveType_Line = 2,
    MfdEditorAutomationPrimitiveType_Circle = 3,
    MfdEditorAutomationPrimitiveType_Ring = 4,
    MfdEditorAutomationPrimitiveType_Rectangle = 5,
    MfdEditorAutomationPrimitiveType_Ellipse = 6,
    MfdEditorAutomationPrimitiveType_Square = 7,
    MfdEditorAutomationPrimitiveType_Diamond = 8,
    MfdEditorAutomationPrimitiveType_Triangle = 9,
    MfdEditorAutomationPrimitiveType_Polyline = 10,
    MfdEditorAutomationPrimitiveType_Bezier = 11,
    MfdEditorAutomationPrimitiveType_Arc = 12,
    MfdEditorAutomationPrimitiveType_Image = 13
} MfdEditorAutomationPrimitiveType;

/**
 * @brief Stroke styles exposed for outline-capable primitives.
 */
typedef enum MfdEditorAutomationLineStyle
{
    MfdEditorAutomationLineStyle_Solid = 0,
    MfdEditorAutomationLineStyle_Dotted = 1,
    MfdEditorAutomationLineStyle_Dashed = 2
} MfdEditorAutomationLineStyle;

/**
 * @brief JSON-producing entity categories supported by the host-owned preview exporter.
 */
typedef enum MfdEditorAutomationExportJsonPreviewKind
{
    MfdEditorAutomationExportJsonPreviewKind_Window = 0,
    MfdEditorAutomationExportJsonPreviewKind_Page = 1,
    MfdEditorAutomationExportJsonPreviewKind_ReticleAsset = 2,
    MfdEditorAutomationExportJsonPreviewKind_PageReticleInstance = 3
} MfdEditorAutomationExportJsonPreviewKind;

/**
 * @brief Compact snapshot summary exposed to one automation plugin.
 */
typedef struct MfdEditorAutomationSnapshotSummary
{
    size_t struct_size;
    uint32_t page_count;
    uint32_t reticle_asset_count;
    int32_t active_page_index;
    uint8_t has_open_window;
    uint8_t session_active;
    uint8_t reserved[6];
    MfdEditorUtf8Buffer window_id;
    MfdEditorUtf8Buffer window_title;
} MfdEditorAutomationSnapshotSummary;

/**
 * @brief Full window metadata exposed through the automation ABI.
 */
typedef struct MfdEditorAutomationWindowInfo
{
    size_t struct_size;
    uint32_t page_count;
    uint32_t reticle_asset_count;
    int32_t active_page_index;
    int32_t width;
    int32_t height;
    int32_t position_x;
    int32_t position_y;
    int32_t target_fps;
    uint8_t has_open_window;
    uint8_t session_active;
    uint8_t reserved[6];
    MfdEditorUtf8Buffer window_id;
    MfdEditorUtf8Buffer window_title;
    MfdEditorUtf8Buffer source_path;
    MfdEditorUtf8Buffer font_file;
    MfdEditorUtf8Buffer icon_file;
    MfdEditorUtf8Buffer reticle_library_folder;
} MfdEditorAutomationWindowInfo;

/**
 * @brief UI state snapshot exposed to one automation plugin.
 */
typedef struct MfdEditorAutomationUiState
{
    size_t struct_size;
    uint32_t selection_kind;
    uint32_t selected_page_reticle_count;
    int32_t selected_primitive_index;
    uint32_t selected_primitive_owner_kind;
    uint8_t reserved[4];
    MfdEditorAutomationPageView page_preview_view;
    MfdEditorAutomationPageView library_preview_view;
    MfdEditorUtf8Buffer active_page_id;
    MfdEditorUtf8Buffer selected_reticle_asset_id;
    MfdEditorUtf8Buffer selected_primitive_id;
    MfdEditorUtf8Buffer selected_primitive_owner_id;
} MfdEditorAutomationUiState;

/**
 * @brief Per-page information returned by the host through the stable ABI.
 */
typedef struct MfdEditorAutomationPageInfo
{
    size_t struct_size;
    uint32_t index;
    uint32_t reticle_count;
    uint32_t layer_count;
    uint32_t blink_type_count;
    uint8_t is_default_page;
    uint8_t is_active_page;
    uint8_t has_strobe;
    uint8_t reserved[5];
    MfdEditorAutomationColorRgba background_color;
    MfdEditorAutomationPageView view;
    MfdEditorUtf8Buffer page_id;
    MfdEditorUtf8Buffer page_name;
    MfdEditorUtf8Buffer page_title;
    MfdEditorUtf8Buffer source_path;
} MfdEditorAutomationPageInfo;

/**
 * @brief One page blink-type entry exposed through the automation ABI.
 */
typedef struct MfdEditorAutomationPageBlinkTypeInfo
{
    size_t struct_size;
    uint32_t page_index;
    uint32_t blink_index;
    uint32_t duration_ms;
    uint8_t is_default;
    uint8_t reserved[7];
    MfdEditorUtf8Buffer blink_type_name;
} MfdEditorAutomationPageBlinkTypeInfo;

/**
 * @brief Minimal validation summary returned by one preview-session validation pass.
 */
typedef struct MfdEditorAutomationValidationSummary
{
    size_t struct_size;
    uint32_t diagnostic_count;
    uint8_t valid;
    uint8_t reserved[7];
} MfdEditorAutomationValidationSummary;

/**
 * @brief Minimal save summary returned by one host-owned persistence operation.
 */
typedef struct MfdEditorAutomationSaveSummary
{
    size_t struct_size;
    uint32_t saved_file_count;
    uint8_t reserved[4];
} MfdEditorAutomationSaveSummary;

/**
 * @brief Stable information returned for one reticle template stored in the shared library.
 */
typedef struct MfdEditorAutomationReticleAssetInfo
{
    size_t struct_size;
    uint32_t index;
    uint32_t primitive_count;
    uint32_t clipping_mode;
    uint8_t visible;
    uint8_t draw_on_top;
    uint8_t reserved[6];
    MfdEditorAutomationTransform2D transform;
    MfdEditorUtf8Buffer reticle_asset_id;
    MfdEditorUtf8Buffer template_id;
    MfdEditorUtf8Buffer label;
    MfdEditorUtf8Buffer category;
    MfdEditorUtf8Buffer clipping_primitive_id;
    MfdEditorUtf8Buffer source_path;
} MfdEditorAutomationReticleAssetInfo;

/**
 * @brief Stable information returned for one page reticle instance.
 */
typedef struct MfdEditorAutomationPageReticleInfo
{
    size_t struct_size;
    uint32_t page_index;
    uint32_t reticle_index;
    uint32_t primitive_count;
    uint32_t clipping_mode;
    uint8_t visible;
    uint8_t draw_on_top;
    uint8_t blink_enabled;
    uint8_t reserved[5];
    MfdEditorAutomationTransform2D transform;
    MfdEditorUtf8Buffer page_reticle_id;
    MfdEditorUtf8Buffer instance_id;
    MfdEditorUtf8Buffer source_reticle_asset_id;
    MfdEditorUtf8Buffer layer_id;
    MfdEditorUtf8Buffer blink_type_name;
    MfdEditorUtf8Buffer clipping_primitive_id;
} MfdEditorAutomationPageReticleInfo;

/**
 * @brief Stable information returned for one editor layer inside one page.
 */
typedef struct MfdEditorAutomationLayerInfo
{
    size_t struct_size;
    uint32_t page_index;
    uint32_t layer_index;
    uint32_t assigned_reticle_count;
    uint8_t visible;
    uint8_t reserved[7];
    MfdEditorUtf8Buffer layer_id;
    MfdEditorUtf8Buffer layer_name;
} MfdEditorAutomationLayerInfo;

/**
 * @brief Stable information returned for one primitive.
 */
typedef struct MfdEditorAutomationPrimitiveInfo
{
    size_t struct_size;
    uint32_t owner_kind;
    uint32_t primitive_index;
    uint32_t primitive_type;
    uint8_t visible;
    uint8_t exposed;
    uint8_t filled;
    uint8_t reserved[5];
    float thickness;
    MfdEditorAutomationTransform2D transform;
    MfdEditorAutomationColorRgba color;
    MfdEditorAutomationColorRgba fill_color;
    MfdEditorUtf8Buffer primitive_id;
    MfdEditorUtf8Buffer owner_id;
    MfdEditorUtf8Buffer content;
    uint32_t line_style;
    uint8_t reserved_extension[4];
} MfdEditorAutomationPrimitiveInfo;

/**
 * @brief One validation diagnostic returned after a session validation pass.
 */
typedef struct MfdEditorAutomationValidationDiagnostic
{
    size_t struct_size;
    uint32_t index;
    int32_t diagnostic_code;
    uint8_t reserved[4];
    MfdEditorUtf8Buffer entity_id;
    MfdEditorUtf8Buffer message;
} MfdEditorAutomationValidationDiagnostic;

/**
 * @brief One structured automation event consumed from the host event queue.
 */
typedef struct MfdEditorAutomationEvent
{
    size_t struct_size;
    uint32_t kind;
    uint8_t reserved[4];
    MfdEditorUtf8Buffer entity_id;
    MfdEditorUtf8Buffer message;
} MfdEditorAutomationEvent;

/**
 * @brief Stable request used to create one new page asset through the host.
 */
typedef struct MfdEditorAutomationCreatePageAssetRequest
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    MfdEditorStringView name;
    MfdEditorStringView title;
    MfdEditorStringView relative_source_path;
    uint8_t make_default;
    uint8_t reserved[7];
    MfdEditorUtf8Buffer created_page_id;
} MfdEditorAutomationCreatePageAssetRequest;

/**
 * @brief Stable request used to delete one existing page asset.
 */
typedef struct MfdEditorAutomationDeletePageAssetRequest
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    MfdEditorStringView page_id;
} MfdEditorAutomationDeletePageAssetRequest;

/**
 * @brief Stable request used to create one seed reticle asset in the shared reticle library.
 */
typedef struct MfdEditorAutomationCreateReticleAssetRequest
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    MfdEditorStringView template_id;
    MfdEditorStringView relative_source_path;
    MfdEditorStringView label;
    MfdEditorStringView category;
    uint32_t seed_primitive_type;
    uint8_t visible;
    uint8_t draw_on_top;
    uint8_t reserved[2];
    MfdEditorAutomationTransform2D transform;
    MfdEditorUtf8Buffer created_reticle_asset_id;
} MfdEditorAutomationCreateReticleAssetRequest;

/**
 * @brief Stable request used to delete one existing reticle asset from the shared library.
 */
typedef struct MfdEditorAutomationDeleteReticleAssetRequest
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    MfdEditorStringView reticle_asset_id;
} MfdEditorAutomationDeleteReticleAssetRequest;

/**
 * @brief Stable request used to instantiate one library reticle onto one page.
 */
typedef struct MfdEditorAutomationInstantiateReticleOnPageRequest
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    MfdEditorStringView page_id;
    MfdEditorStringView reticle_asset_id;
    MfdEditorStringView requested_instance_id;
    MfdEditorAutomationTransform2D transform;
    uint8_t assign_default_layer;
    uint8_t reserved[7];
    MfdEditorUtf8Buffer created_page_reticle_id;
} MfdEditorAutomationInstantiateReticleOnPageRequest;

/**
 * @brief Stable request used to delete one page-reticle instance.
 */
typedef struct MfdEditorAutomationDeletePageReticleRequest
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    MfdEditorStringView page_reticle_id;
} MfdEditorAutomationDeletePageReticleRequest;

/**
 * @brief Stable request used to reorder one page reticle instance.
 */
typedef struct MfdEditorAutomationMovePageReticleRequest
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    MfdEditorStringView page_reticle_id;
    int32_t target_index;
    uint8_t reserved[4];
} MfdEditorAutomationMovePageReticleRequest;

/**
 * @brief Stable request used to toggle one page reticle visibility.
 */
typedef struct MfdEditorAutomationSetPageReticleVisibilityRequest
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    MfdEditorStringView page_reticle_id;
    uint8_t visible;
    uint8_t reserved[7];
} MfdEditorAutomationSetPageReticleVisibilityRequest;

/**
 * @brief Stable request used to toggle one page reticle draw-on-top flag.
 */
typedef struct MfdEditorAutomationSetPageReticleDrawOnTopRequest
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    MfdEditorStringView page_reticle_id;
    uint8_t draw_on_top;
    uint8_t reserved[7];
} MfdEditorAutomationSetPageReticleDrawOnTopRequest;

/**
 * @brief Stable request used to replace one page reticle transform.
 */
typedef struct MfdEditorAutomationSetPageReticleTransformRequest
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    MfdEditorStringView page_reticle_id;
    MfdEditorAutomationTransform2D transform;
} MfdEditorAutomationSetPageReticleTransformRequest;

/**
 * @brief Stable request used to assign one page reticle to one editor layer.
 *
 * @note An empty `layer_id` clears the current layer assignment.
 */
typedef struct MfdEditorAutomationSetPageReticleLayerRequest
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    MfdEditorStringView page_reticle_id;
    MfdEditorStringView layer_id;
} MfdEditorAutomationSetPageReticleLayerRequest;

/**
 * @brief Stable request used to create one new editor layer on one page.
 *
 * @note A negative `insert_index` appends the layer at the end of the page layer list.
 */
typedef struct MfdEditorAutomationCreateLayerRequest
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    MfdEditorStringView page_id;
    MfdEditorStringView layer_name;
    int32_t insert_index;
    uint8_t visible;
    uint8_t reserved[3];
    MfdEditorUtf8Buffer created_layer_id;
} MfdEditorAutomationCreateLayerRequest;

/**
 * @brief Stable request used to replace one existing editor layer.
 */
typedef struct MfdEditorAutomationReplaceLayerRequest
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    MfdEditorStringView page_id;
    MfdEditorStringView layer_id;
    MfdEditorStringView layer_name;
    uint8_t visible;
    uint8_t reserved[7];
} MfdEditorAutomationReplaceLayerRequest;

/**
 * @brief Stable request used to delete one editor layer.
 */
typedef struct MfdEditorAutomationDeleteLayerRequest
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    MfdEditorStringView page_id;
    MfdEditorStringView layer_id;
} MfdEditorAutomationDeleteLayerRequest;

/**
 * @brief Stable request used to toggle one editor layer visibility.
 */
typedef struct MfdEditorAutomationSetLayerVisibilityRequest
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    MfdEditorStringView page_id;
    MfdEditorStringView layer_id;
    uint8_t visible;
    uint8_t reserved[7];
} MfdEditorAutomationSetLayerVisibilityRequest;

/**
 * @brief Stable request used to toggle one reticle template visibility.
 */
typedef struct MfdEditorAutomationSetReticleAssetVisibilityRequest
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    MfdEditorStringView reticle_asset_id;
    uint8_t visible;
    uint8_t reserved[7];
} MfdEditorAutomationSetReticleAssetVisibilityRequest;

/**
 * @brief Stable request used to toggle one reticle template draw-on-top flag.
 */
typedef struct MfdEditorAutomationSetReticleAssetDrawOnTopRequest
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    MfdEditorStringView reticle_asset_id;
    uint8_t draw_on_top;
    uint8_t reserved[7];
} MfdEditorAutomationSetReticleAssetDrawOnTopRequest;

/**
 * @brief Stable request used to apply one generic reticle transform update.
 */
typedef struct MfdEditorAutomationSetReticleTransformRequest
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    uint32_t target_kind;
    uint8_t reserved[4];
    MfdEditorStringView target_id;
    MfdEditorAutomationTransform2D transform;
} MfdEditorAutomationSetReticleTransformRequest;

/**
 * @brief Stable request used to apply one generic reticle clipping update.
 */
typedef struct MfdEditorAutomationSetReticleClippingRequest
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    uint32_t target_kind;
    uint32_t clipping_mode;
    MfdEditorStringView target_id;
    MfdEditorStringView primitive_id;
} MfdEditorAutomationSetReticleClippingRequest;

/**
 * @brief Stable request used to update the stroke style of one primitive.
 */
typedef struct MfdEditorAutomationSetPrimitiveLineStyleRequest
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    uint32_t owner_kind;
    uint32_t line_style;
    MfdEditorStringView owner_id;
    MfdEditorStringView primitive_id;
} MfdEditorAutomationSetPrimitiveLineStyleRequest;

/**
 * @brief Stable request used to create or update one page blink type.
 *
 * @note Leave `existing_blink_type_name` empty to append a new blink type.
 */
typedef struct MfdEditorAutomationUpsertPageBlinkTypeRequest
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    MfdEditorStringView page_id;
    MfdEditorStringView existing_blink_type_name;
    MfdEditorStringView blink_type_name;
    uint32_t duration_ms;
    uint8_t make_default;
    uint8_t reserved[7];
} MfdEditorAutomationUpsertPageBlinkTypeRequest;

/**
 * @brief Stable request used to delete one page blink type.
 */
typedef struct MfdEditorAutomationDeletePageBlinkTypeRequest
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    MfdEditorStringView page_id;
    MfdEditorStringView blink_type_name;
} MfdEditorAutomationDeletePageBlinkTypeRequest;

/**
 * @brief Stable request used to set or clear the default blink type of one page.
 *
 * @note An empty `blink_type_name` clears the current default blink type.
 */
typedef struct MfdEditorAutomationSetPageDefaultBlinkTypeRequest
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    MfdEditorStringView page_id;
    MfdEditorStringView blink_type_name;
} MfdEditorAutomationSetPageDefaultBlinkTypeRequest;

/**
 * @brief Stable request used to set the blink binding of one page reticle.
 */
typedef struct MfdEditorAutomationSetPageReticleBlinkRequest
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    MfdEditorStringView page_reticle_id;
    MfdEditorStringView blink_type_name;
    uint8_t enabled;
    uint8_t reserved[7];
} MfdEditorAutomationSetPageReticleBlinkRequest;

/**
 * @brief Stable request used to assign one shared reticle template as the page strobe.
 */
typedef struct MfdEditorAutomationAssignPageStrobeTemplateRequest
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    MfdEditorStringView page_id;
    MfdEditorStringView reticle_asset_id;
    MfdEditorStringView requested_strobe_id;
} MfdEditorAutomationAssignPageStrobeTemplateRequest;

/**
 * @brief Stable request used to delete one page strobe definition.
 */
typedef struct MfdEditorAutomationDeletePageStrobeRequest
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    MfdEditorStringView page_id;
} MfdEditorAutomationDeletePageStrobeRequest;

/**
 * @brief Stable request used to update the live editor selection.
 */
typedef struct MfdEditorAutomationSelectEntityRequest
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    uint32_t selection_kind;
    uint8_t reserved[4];
    MfdEditorStringView target_id;
} MfdEditorAutomationSelectEntityRequest;

/**
 * @brief Stable request used to save one specific page or reticle asset.
 */
typedef struct MfdEditorAutomationSaveAssetRequest
{
    size_t struct_size;
    uint32_t kind;
    uint8_t reserved[4];
    MfdEditorStringView entity_id;
} MfdEditorAutomationSaveAssetRequest;

/**
 * @brief Stable request used to export one host-owned JSON preview.
 */
typedef struct MfdEditorAutomationExportJsonPreviewRequest
{
    size_t struct_size;
    uint32_t kind;
    uint8_t reserved[4];
    MfdEditorStringView entity_id;
} MfdEditorAutomationExportJsonPreviewRequest;

/**
 * @brief Stable JSON preview result returned through the automation ABI.
 */
typedef struct MfdEditorAutomationJsonPreviewResult
{
    size_t struct_size;
    MfdEditorUtf8Buffer source_path;
    MfdEditorUtf8Buffer json;
} MfdEditorAutomationJsonPreviewResult;

struct MfdEditorAutomationHostApi;

/**
 * @brief Function type filling one stable plugin API descriptor.
 */
typedef MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* MfdGetEditorAutomationPluginApiFn)(
    struct MfdEditorAutomationPluginApi* out_api,
    MfdEditorUtf8Buffer* error);

/**
 * @brief Versioned host callbacks exposed by the editor to one plugin instance.
 */
typedef struct MfdEditorAutomationHostApi
{
    size_t struct_size;
    uint32_t abi_version;
    void* host_context;
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* get_snapshot_summary)(
        void* host_context,
        MfdEditorAutomationSnapshotSummary* out_summary,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* get_page_info)(
        void* host_context,
        uint32_t page_index,
        MfdEditorAutomationPageInfo* out_page,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* begin_session)(
        void* host_context,
        MfdEditorStringView label,
        MfdEditorAutomationSessionHandle* out_session,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* validate_session)(
        void* host_context,
        MfdEditorAutomationSessionHandle session,
        MfdEditorAutomationValidationSummary* out_validation,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* create_page_asset)(
        void* host_context,
        MfdEditorAutomationCreatePageAssetRequest* request,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* commit_session)(
        void* host_context,
        MfdEditorAutomationSessionHandle session,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* rollback_session)(
        void* host_context,
        MfdEditorAutomationSessionHandle session,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* save_all)(
        void* host_context,
        MfdEditorAutomationSaveSummary* out_save,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* get_reticle_asset_info)(
        void* host_context,
        uint32_t reticle_index,
        MfdEditorAutomationReticleAssetInfo* out_reticle,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* get_page_reticle_info)(
        void* host_context,
        uint32_t page_index,
        uint32_t reticle_index,
        MfdEditorAutomationPageReticleInfo* out_reticle,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* get_layer_info)(
        void* host_context,
        uint32_t page_index,
        uint32_t layer_index,
        MfdEditorAutomationLayerInfo* out_layer,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* get_session_validation_diagnostic)(
        void* host_context,
        MfdEditorAutomationSessionHandle session,
        uint32_t diagnostic_index,
        MfdEditorAutomationValidationDiagnostic* out_diagnostic,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* instantiate_reticle_on_page)(
        void* host_context,
        MfdEditorAutomationInstantiateReticleOnPageRequest* request,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* move_page_reticle)(
        void* host_context,
        MfdEditorAutomationMovePageReticleRequest* request,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* set_page_reticle_visibility)(
        void* host_context,
        MfdEditorAutomationSetPageReticleVisibilityRequest* request,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* set_page_reticle_draw_on_top)(
        void* host_context,
        MfdEditorAutomationSetPageReticleDrawOnTopRequest* request,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* set_page_reticle_transform)(
        void* host_context,
        MfdEditorAutomationSetPageReticleTransformRequest* request,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* set_page_reticle_layer)(
        void* host_context,
        MfdEditorAutomationSetPageReticleLayerRequest* request,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* create_layer)(
        void* host_context,
        MfdEditorAutomationCreateLayerRequest* request,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* set_layer_visibility)(
        void* host_context,
        MfdEditorAutomationSetLayerVisibilityRequest* request,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* save_asset)(
        void* host_context,
        const MfdEditorAutomationSaveAssetRequest* request,
        MfdEditorAutomationSaveSummary* out_save,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* consume_pending_event)(
        void* host_context,
        MfdEditorAutomationEvent* out_event,
        MfdEditorUtf8Buffer* error);

    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* get_window_info)(
        void* host_context,
        MfdEditorAutomationWindowInfo* out_window,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* get_ui_state)(
        void* host_context,
        MfdEditorAutomationUiState* out_state,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* get_page_blink_type_info)(
        void* host_context,
        uint32_t page_index,
        uint32_t blink_index,
        MfdEditorAutomationPageBlinkTypeInfo* out_blink_type,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* get_reticle_asset_primitive_info)(
        void* host_context,
        uint32_t reticle_index,
        uint32_t primitive_index,
        MfdEditorAutomationPrimitiveInfo* out_primitive,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* get_page_reticle_primitive_info)(
        void* host_context,
        uint32_t page_index,
        uint32_t reticle_index,
        uint32_t primitive_index,
        MfdEditorAutomationPrimitiveInfo* out_primitive,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* delete_page_asset)(
        void* host_context,
        MfdEditorAutomationDeletePageAssetRequest* request,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* create_reticle_asset)(
        void* host_context,
        MfdEditorAutomationCreateReticleAssetRequest* request,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* delete_reticle_asset)(
        void* host_context,
        MfdEditorAutomationDeleteReticleAssetRequest* request,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* delete_page_reticle)(
        void* host_context,
        MfdEditorAutomationDeletePageReticleRequest* request,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* set_reticle_asset_visibility)(
        void* host_context,
        MfdEditorAutomationSetReticleAssetVisibilityRequest* request,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* set_reticle_asset_draw_on_top)(
        void* host_context,
        MfdEditorAutomationSetReticleAssetDrawOnTopRequest* request,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* set_reticle_transform)(
        void* host_context,
        MfdEditorAutomationSetReticleTransformRequest* request,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* set_reticle_clipping)(
        void* host_context,
        MfdEditorAutomationSetReticleClippingRequest* request,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* replace_layer)(
        void* host_context,
        MfdEditorAutomationReplaceLayerRequest* request,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* delete_layer)(
        void* host_context,
        MfdEditorAutomationDeleteLayerRequest* request,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* upsert_page_blink_type)(
        void* host_context,
        MfdEditorAutomationUpsertPageBlinkTypeRequest* request,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* delete_page_blink_type)(
        void* host_context,
        MfdEditorAutomationDeletePageBlinkTypeRequest* request,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* set_page_default_blink_type)(
        void* host_context,
        MfdEditorAutomationSetPageDefaultBlinkTypeRequest* request,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* set_page_reticle_blink)(
        void* host_context,
        MfdEditorAutomationSetPageReticleBlinkRequest* request,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* assign_page_strobe_template)(
        void* host_context,
        MfdEditorAutomationAssignPageStrobeTemplateRequest* request,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* delete_page_strobe)(
        void* host_context,
        MfdEditorAutomationDeletePageStrobeRequest* request,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* select_entity)(
        void* host_context,
        MfdEditorAutomationSelectEntityRequest* request,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* export_json_preview)(
        void* host_context,
        const MfdEditorAutomationExportJsonPreviewRequest* request,
        MfdEditorAutomationJsonPreviewResult* out_preview,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* set_primitive_line_style)(
        void* host_context,
        MfdEditorAutomationSetPrimitiveLineStyleRequest* request,
        MfdEditorUtf8Buffer* error);
} MfdEditorAutomationHostApi;

/**
 * @brief Returns whether one optional host callback appended to `MfdEditorAutomationHostApi` is available.
 *
 * @details Even after the ABI version is bumped, the host may append new
 * callbacks in future revisions while preserving the leading layout. This
 * helper lets plugins probe one callback safely through `struct_size`.
 */
#define MFD_EDITOR_AUTOMATION_HOST_HAS_CALLBACK(host, callback_member)                                                 \
    ((host) != NULL &&                                                                                                 \
     (host)->struct_size >= (offsetof(MfdEditorAutomationHostApi, callback_member) + sizeof((host)->callback_member)) && \
     (host)->callback_member != NULL)

/**
 * @brief Immutable metadata describing one automation plugin instance.
 */
typedef struct MfdEditorAutomationPluginInfo
{
    size_t struct_size;
    uint32_t abi_version;
    MfdEditorStringView plugin_id;
    MfdEditorStringView display_name;
} MfdEditorAutomationPluginInfo;

/**
 * @brief Versioned plugin callbacks exported by one automation plugin DLL.
 */
typedef struct MfdEditorAutomationPluginApi
{
    size_t struct_size;
    MfdEditorAutomationPluginInfo info;
    void* plugin_context;
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* start)(
        void* plugin_context,
        const MfdEditorAutomationHostApi* host,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* tick)(
        void* plugin_context,
        MfdEditorUtf8Buffer* error);
    void (MFD_EDITOR_AUTOMATION_CALL* stop)(void* plugin_context);
    void (MFD_EDITOR_AUTOMATION_CALL* destroy)(void* plugin_context);
} MfdEditorAutomationPluginApi;

#ifdef __cplusplus
} // extern "C"
#endif

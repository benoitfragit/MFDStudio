/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Stable C ABI used by external in-process editor automation plugins.
 *
 * @details This boundary intentionally avoids C++ classes, STL containers and
 * exceptions across the DLL edge. The editor keeps ownership of the live
 * document, undo/redo, validation and persistence. Plugins consume only the
 * versioned function tables defined below.
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

/** @brief ABI revision implemented by this editor automation plugin contract. */
enum
{
    MFD_EDITOR_AUTOMATION_PLUGIN_ABI_VERSION = 1u
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
 * null-terminate when `capacity > 0` and return
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
 * @brief Compact snapshot summary exposed to one automation plugin.
 */
typedef struct MfdEditorAutomationSnapshotSummaryV1
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
} MfdEditorAutomationSnapshotSummaryV1;

/**
 * @brief Per-page information returned by the host through the stable ABI.
 */
typedef struct MfdEditorAutomationPageInfoV1
{
    size_t struct_size;
    uint32_t index;
    uint32_t reticle_count;
    uint32_t layer_count;
    uint8_t is_default_page;
    uint8_t reserved[7];
    MfdEditorUtf8Buffer page_id;
    MfdEditorUtf8Buffer page_name;
    MfdEditorUtf8Buffer page_title;
    MfdEditorUtf8Buffer source_path;
} MfdEditorAutomationPageInfoV1;

/**
 * @brief Minimal validation summary returned by one preview-session validation pass.
 */
typedef struct MfdEditorAutomationValidationSummaryV1
{
    size_t struct_size;
    uint32_t diagnostic_count;
    uint8_t valid;
    uint8_t reserved[7];
} MfdEditorAutomationValidationSummaryV1;

/**
 * @brief Minimal save summary returned by one host-owned persistence operation.
 */
typedef struct MfdEditorAutomationSaveSummaryV1
{
    size_t struct_size;
    uint32_t saved_file_count;
    uint8_t reserved[4];
} MfdEditorAutomationSaveSummaryV1;

/**
 * @brief Stable request used to create one new page asset through the host.
 */
typedef struct MfdEditorAutomationCreatePageAssetRequestV1
{
    size_t struct_size;
    MfdEditorAutomationSessionHandle session;
    MfdEditorStringView name;
    MfdEditorStringView title;
    MfdEditorStringView relative_source_path;
    uint8_t make_default;
    uint8_t reserved[7];
    MfdEditorUtf8Buffer created_page_id;
} MfdEditorAutomationCreatePageAssetRequestV1;

struct MfdEditorAutomationHostApiV1;

/**
 * @brief Function type filling one stable plugin API descriptor.
 */
typedef MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* MfdGetEditorAutomationPluginApiFn)(
    struct MfdEditorAutomationPluginApiV1* out_api,
    MfdEditorUtf8Buffer* error);

/**
 * @brief Versioned host callbacks exposed by the editor to one plugin instance.
 */
typedef struct MfdEditorAutomationHostApiV1
{
    size_t struct_size;
    uint32_t abi_version;
    void* host_context;
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* get_snapshot_summary)(
        void* host_context,
        MfdEditorAutomationSnapshotSummaryV1* out_summary,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* get_page_info)(
        void* host_context,
        uint32_t page_index,
        MfdEditorAutomationPageInfoV1* out_page,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* begin_session)(
        void* host_context,
        MfdEditorStringView label,
        MfdEditorAutomationSessionHandle* out_session,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* validate_session)(
        void* host_context,
        MfdEditorAutomationSessionHandle session,
        MfdEditorAutomationValidationSummaryV1* out_validation,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* create_page_asset)(
        void* host_context,
        MfdEditorAutomationCreatePageAssetRequestV1* request,
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
        MfdEditorAutomationSaveSummaryV1* out_save,
        MfdEditorUtf8Buffer* error);
} MfdEditorAutomationHostApiV1;

/**
 * @brief Immutable metadata describing one automation plugin instance.
 */
typedef struct MfdEditorAutomationPluginInfoV1
{
    size_t struct_size;
    uint32_t abi_version;
    MfdEditorStringView plugin_id;
    MfdEditorStringView display_name;
} MfdEditorAutomationPluginInfoV1;

/**
 * @brief Versioned plugin callbacks exported by one automation plugin DLL.
 */
typedef struct MfdEditorAutomationPluginApiV1
{
    size_t struct_size;
    MfdEditorAutomationPluginInfoV1 info;
    void* plugin_context;
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* start)(
        void* plugin_context,
        const MfdEditorAutomationHostApiV1* host,
        MfdEditorUtf8Buffer* error);
    MfdEditorAutomationResultCode(MFD_EDITOR_AUTOMATION_CALL* tick)(
        void* plugin_context,
        MfdEditorUtf8Buffer* error);
    void (MFD_EDITOR_AUTOMATION_CALL* stop)(void* plugin_context);
    void (MFD_EDITOR_AUTOMATION_CALL* destroy)(void* plugin_context);
} MfdEditorAutomationPluginApiV1;

#ifdef __cplusplus
} // extern "C"
#endif

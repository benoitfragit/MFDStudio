#pragma once
/** @file @brief Size/version-checked widget inspector facade; host owns every UI callback. */
#include <stddef.h>
#include <stdint.h>
#if defined(_WIN32)
#define MFD_WIDGET_EDITOR_CALL __cdecl
#if defined(MFD_EDITOR_WIDGETS_EXPORTS)
#define MFD_WIDGET_EDITOR_EXPORT __declspec(dllexport)
#else
#define MFD_WIDGET_EDITOR_EXPORT __declspec(dllimport)
#endif
#else
#define MFD_WIDGET_EDITOR_CALL
#define MFD_WIDGET_EDITOR_EXPORT
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define MFD_WIDGET_EDITOR_ABI_VERSION 1U
#define MFD_WIDGET_EDITOR_CAPABILITY_DESIGNER 1U
/** @brief Callback strings are borrowed for this call only; callbacks must not throw. */
typedef struct MfdWidgetEditorUiV1
{
    size_t struct_size;
    uint32_t version;
    void* context;
    void(MFD_WIDGET_EDITOR_CALL* text)(void*, const char*);
    uint8_t(MFD_WIDGET_EDITOR_CALL* input_text)(void*, const char*, char*, size_t);
    uint8_t(MFD_WIDGET_EDITOR_CALL* checkbox)(void*, const char*, uint8_t*);
    uint8_t(MFD_WIDGET_EDITOR_CALL* number)(void*, const char*, double*, double, double);
    uint8_t(MFD_WIDGET_EDITOR_CALL* combo)(void*, const char*, int32_t*, const char* const*, size_t);
    uint8_t(MFD_WIDGET_EDITOR_CALL* button)(void*, const char*);
} MfdWidgetEditorUiV1;
/** @brief Mutable bounded draft, copied and validated by the host before committing. */
typedef struct MfdWidgetEditorDraftV1
{
    size_t struct_size;
    char id[257];
    char parent[257];
    char text[4097];
    uint8_t visible, enabled, exposed, checked, warning, override_size;
    uint8_t override_checked, override_warning, override_value, override_text;
    double x, y, width, height, value;
    int32_t resize_mode, horizontal_alignment, vertical_alignment;
    double row, column, row_span, column_span, render_order;
    uint8_t edit_definition, definition_only;
    char definition_id[257];
    int32_t type, visual_state;
    double minimum, maximum, step, decimals, hit_x, hit_y, hit_width, hit_height;
    double preferred_width, preferred_height, default_value;
    uint8_t default_checked, default_warning;
    char default_text[4097];
    char binding_primitive[257];
    int32_t binding_target;
    double binding_from, binding_to;
} MfdWidgetEditorDraftV1;
/** @brief Result flags: 1 instance edit, 2 definition edit, 4 edit visual, 8 apply visual, 16 reset overrides. */
typedef struct MfdWidgetEditorApiV1
{
    size_t struct_size;
    uint32_t abi_version;
    uint32_t capabilities;
    uint32_t(MFD_WIDGET_EDITOR_CALL* draw)(const MfdWidgetEditorUiV1*, MfdWidgetEditorDraftV1*);
} MfdWidgetEditorApiV1;
MFD_WIDGET_EDITOR_EXPORT uint32_t MFD_WIDGET_EDITOR_CALL mfd_get_widgets_editor_api_v1(MfdWidgetEditorApiV1* output);
#ifdef __cplusplus
}
#endif

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
    /**
     * @brief Optional tail: draws a host-themed collapsible section.
     * @return Non-zero when the editor should draw the section body.
     * @note Check `struct_size` before reading; a host built before section support omits it.
     */
    uint8_t(MFD_WIDGET_EDITOR_CALL* begin_section)(void*, const char* section_id, const char* label, uint8_t default_open);
    /**
     * @brief Optional tail: host-drawn picker listing the Page root and the existing containers.
     * @param parent In/out null-terminated container id; empty selects the Page root.
     * @return Non-zero when the user picked another parent.
     * @note Check `struct_size` before reading; without it the editor falls back to `input_text`.
     */
    uint8_t(MFD_WIDGET_EDITOR_CALL* parent_picker)(void*, const char* label, char* parent, size_t capacity);
} MfdWidgetEditorUiV1;
/** @brief `parent_type` value when the host does not report the parent container type. */
#define MFD_WIDGET_EDITOR_PARENT_TYPE_UNKNOWN (-1)
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
    /**
     * @brief Optional tail: type of the current parent container (0 Canvas, 1 Linear, 2 Grid, 3 Overlay).
     * @note Meaningful only when `parent` is not empty. Editors check `struct_size` before reading it and
     * treat an absent or `MFD_WIDGET_EDITOR_PARENT_TYPE_UNKNOWN` value as "show every placement field".
     */
    int32_t parent_type;
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

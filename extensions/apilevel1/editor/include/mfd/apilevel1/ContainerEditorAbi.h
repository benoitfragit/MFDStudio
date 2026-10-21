/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/** @file @brief Stable editor-host UI facade for API LEVEL 1 containers. */

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#define MFD_CONTAINER_EDITOR_CALL __cdecl
#if defined(MFD_EDITOR_CONTAINERS_API1_EXPORTS)
#define MFD_CONTAINER_EDITOR_EXPORT __declspec(dllexport)
#else
#define MFD_CONTAINER_EDITOR_EXPORT __declspec(dllimport)
#endif
#else
#define MFD_CONTAINER_EDITOR_CALL
#define MFD_CONTAINER_EDITOR_EXPORT
#endif
#ifdef __cplusplus
extern "C"
{
#endif

#define MFD_CONTAINER_EDITOR_ABI_VERSION 1U
#define MFD_CONTAINER_EDITOR_UI_FACADE_VERSION 1U

typedef void(MFD_CONTAINER_EDITOR_CALL* MfdEditorUiTextV1)(void* host_context, const char* text);
/**
 * @brief Starts a host-themed collapsible inspector section.
 * @param host_context Opaque context supplied by the host facade.
 * @param section_id Stable identifier used to persist the section state.
 * @param label Human-readable section caption.
 * @param default_open Non-zero when the section should open on first use.
 * @return Non-zero when the extension should draw the section body.
 */
typedef uint8_t(MFD_CONTAINER_EDITOR_CALL* MfdEditorUiBeginSectionV1)(
    void* host_context, const char* section_id, const char* label, uint8_t default_open);
typedef uint8_t(MFD_CONTAINER_EDITOR_CALL* MfdEditorUiCheckboxV1)(
    void* host_context, const char* label, uint8_t* value);
typedef uint8_t(MFD_CONTAINER_EDITOR_CALL* MfdEditorUiDragFloatV1)(
    void* host_context, const char* label, float* value, float speed, float minimum, float maximum);
typedef uint8_t(MFD_CONTAINER_EDITOR_CALL* MfdEditorUiDragFloat2V1)(
    void* host_context, const char* label, float* values, float speed, float minimum, float maximum);
typedef uint8_t(MFD_CONTAINER_EDITOR_CALL* MfdEditorUiComboV1)(
    void* host_context, const char* label, int32_t* current, const char* const* labels, size_t label_count);
typedef uint8_t(MFD_CONTAINER_EDITOR_CALL* MfdEditorUiColorEdit4V1)(
    void* host_context, const char* label, float* rgba);

/**
 * @brief Host-owned bounded UI facade invoked only during the active ImGui frame.
 *
 * No ImGui object, allocator or STL owner crosses the DLL boundary. The host
 * owns context, theme, scaling, input and rendering.
 */
typedef struct MfdContainerEditorUiFacadeV1
{
    size_t struct_size;
    uint32_t version;
    void* host_context;
    MfdEditorUiTextV1 text;
    MfdEditorUiCheckboxV1 checkbox;
    MfdEditorUiDragFloatV1 drag_float;
    MfdEditorUiDragFloat2V1 drag_float2;
    MfdEditorUiComboV1 combo;
    MfdEditorUiColorEdit4V1 color_edit4;
    /**
     * @brief Draws a host-themed collapsible section and reports whether its body is visible.
     * @note Optional tail extension. Check `struct_size` before reading this callback so a
     *       facade produced before section support remains binary compatible.
     */
    MfdEditorUiBeginSectionV1 begin_section;
} MfdContainerEditorUiFacadeV1;

/** @brief Mutable POD projection of the selected authored container. */
typedef struct MfdContainerEditorSelectionV1
{
    size_t struct_size;
    const char* id;
    uint32_t* type;
    uint32_t* size_mode;
    float* position_xy;
    float* size_xy;
    float* minimum_size_xy;
    uint8_t* visible;
    uint8_t* enabled;
    uint8_t* exposed;
    float* padding_ltrb;
    float* spacing;
    uint32_t* orientation;
    uint32_t* alignment;
    uint32_t* rows;
    uint32_t* columns;
    uint32_t* horizontal_scroll_mode;
    uint32_t* vertical_scroll_mode;
    float* scroll_offset_xy;
    float* scrollbar_thickness;
    float* scrollbar_minimum_thumb_size;
    uint8_t* clip_content;
    uint8_t* background_visible;
    uint8_t* border_visible;
    float* background_rgba;
    float* border_rgba;
    float* track_rgba;
    float* thumb_rgba;
    float* thumb_hover_rgba;
    float* thumb_active_rgba;
    float* border_thickness;
    float* corner_radius;
    float* scrollbar_corner_radius;
    float* scrollbar_margins_ltrb;
    const char* parent_id;
    uint8_t has_parent;
    uint32_t parent_type;
    uint32_t* child_resize_mode;
    uint32_t* child_horizontal_alignment;
    uint32_t* child_vertical_alignment;
    uint32_t* child_row;
    uint32_t* child_column;
    uint32_t* child_row_span;
    uint32_t* child_column_span;
    int32_t* child_render_order;
} MfdContainerEditorSelectionV1;

typedef uint8_t(MFD_CONTAINER_EDITOR_CALL* MfdDrawContainerInspectorV1)(
    const MfdContainerEditorUiFacadeV1* ui,
    MfdContainerEditorSelectionV1* selection);

/** @brief Versioned function table returned by the editor extension. */
typedef struct MfdContainerEditorApiV1
{
    size_t struct_size;
    uint32_t abi_version;
    uint32_t required_ui_facade_version;
    MfdDrawContainerInspectorV1 draw_inspector;
} MfdContainerEditorApiV1;

/** @brief Returns the compatible editor extension function table. */
MFD_CONTAINER_EDITOR_EXPORT uint32_t MFD_CONTAINER_EDITOR_CALL mfd_get_container_editor_api_v1(
    MfdContainerEditorApiV1* api);

#ifdef __cplusplus
}
#endif

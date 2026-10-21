/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/** @file @brief Stable C ABI exposed by mfd_containers_api1. */

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#define MFD_APILEVEL1_CALL __cdecl
#if defined(MFD_CONTAINERS_API1_EXPORTS)
#define MFD_APILEVEL1_EXPORT __declspec(dllexport)
#else
#define MFD_APILEVEL1_EXPORT __declspec(dllimport)
#endif
#else
#define MFD_APILEVEL1_CALL
#define MFD_APILEVEL1_EXPORT
#endif
#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Current ABI version of the LEVEL 1 runtime extension. */
#define MFD_CONTAINERS_RUNTIME_ABI_VERSION 1U
/** @brief Capability bit indicating recursive deterministic layout support. */
#define MFD_CONTAINERS_CAPABILITY_LAYOUT 0x01U
/** @brief Capability bit indicating nested scrolling geometry support. */
#define MFD_CONTAINERS_CAPABILITY_SCROLLING 0x02U
/** @brief Capability bit indicating effective visibility/input inheritance. */
#define MFD_CONTAINERS_CAPABILITY_STATE 0x04U

typedef struct MfdContainerVec2V1
{
    float x;
    float y;
} MfdContainerVec2V1;

typedef struct MfdContainerInsetsV1
{
    float left;
    float top;
    float right;
    float bottom;
} MfdContainerInsetsV1;

/** @brief Flattened child edge. Strings remain caller-owned for the duration of one call. */
typedef struct MfdContainerChildV1
{
    const char* child_id;
    uint32_t resize_mode;
    uint32_t horizontal_alignment;
    uint32_t vertical_alignment;
    uint32_t row;
    uint32_t column;
    uint32_t row_span;
    uint32_t column_span;
    int32_t render_order;
} MfdContainerChildV1;

/** @brief Flattened authored container passed to the extension without STL ownership. */
typedef struct MfdContainerDefinitionV1
{
    const char* id;
    uint32_t type;
    uint32_t size_mode;
    MfdContainerVec2V1 position;
    MfdContainerVec2V1 size;
    MfdContainerVec2V1 minimum_size;
    MfdContainerVec2V1 layout_reference_size;
    uint8_t visible;
    uint8_t enabled;
    MfdContainerInsetsV1 padding;
    float spacing;
    uint32_t orientation;
    uint32_t alignment;
    uint32_t rows;
    uint32_t columns;
    uint32_t horizontal_scroll_mode;
    uint32_t vertical_scroll_mode;
    MfdContainerVec2V1 scroll_offset;
    float scrollbar_thickness;
    float scrollbar_minimum_thumb_size;
    MfdContainerInsetsV1 scrollbar_margins;
    size_t first_child;
    size_t child_count;
} MfdContainerDefinitionV1;

typedef struct MfdContainerRectV1
{
    MfdContainerVec2V1 center;
    MfdContainerVec2V1 size;
} MfdContainerRectV1;

/** @brief One resolved layout entry indexed back to its source definition. */
typedef struct MfdContainerLayoutV1
{
    size_t source_index;
    size_t parent_source_index;
    MfdContainerRectV1 bounds;
    MfdContainerRectV1 viewport;
    MfdContainerVec2V1 content_size;
    MfdContainerVec2V1 scroll_offset;
    uint8_t effective_visible;
    uint8_t effective_enabled;
    uint8_t horizontal_scrollbar_visible;
    uint8_t vertical_scrollbar_visible;
    MfdContainerRectV1 horizontal_track;
    MfdContainerRectV1 horizontal_thumb;
    MfdContainerRectV1 vertical_track;
    MfdContainerRectV1 vertical_thumb;
    uint32_t depth;
    int32_t render_order;
} MfdContainerLayoutV1;

typedef struct MfdContainerErrorBufferV1
{
    char* data;
    size_t capacity;
} MfdContainerErrorBufferV1;

typedef uint32_t(MFD_APILEVEL1_CALL* MfdEvaluateContainerLayoutV1)(
    const MfdContainerDefinitionV1* definitions,
    size_t definition_count,
    const MfdContainerChildV1* children,
    size_t child_count,
    MfdContainerLayoutV1* output,
    size_t output_capacity,
    size_t* output_count,
    MfdContainerErrorBufferV1 error);

/** @brief Versioned function table returned by the extension handshake. */
typedef struct MfdContainersRuntimeApiV1
{
    size_t struct_size;
    uint32_t abi_version;
    uint32_t capabilities;
    MfdEvaluateContainerLayoutV1 evaluate_layout;
} MfdContainersRuntimeApiV1;

/**
 * @brief Retrieves the LEVEL 1 runtime API after validating caller structure size.
 * @param api Caller-owned structure whose `struct_size` must be initialized.
 * @param error Optional bounded diagnostic buffer.
 * @return Zero on success, non-zero on incompatibility.
 */
MFD_APILEVEL1_EXPORT uint32_t MFD_APILEVEL1_CALL mfd_get_containers_runtime_api_v1(
    MfdContainersRuntimeApiV1* api,
    MfdContainerErrorBufferV1 error);

#ifdef __cplusplus
}
#endif

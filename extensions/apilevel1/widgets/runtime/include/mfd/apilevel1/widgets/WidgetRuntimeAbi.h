#pragma once
/** @file @brief Versioned allocation-free C ABI; no STL or host graphics cross the DLL. */
#include <stdint.h>
#if defined(_WIN32)
#define MFD_WIDGET_CALL __cdecl
#if defined(MFD_WIDGETS_EXPORTS)
#define MFD_WIDGET_EXPORT __declspec(dllexport)
#else
#define MFD_WIDGET_EXPORT
#endif
#else
#define MFD_WIDGET_CALL
#define MFD_WIDGET_EXPORT __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define MFD_WIDGET_ABI_VERSION 1U
#define MFD_WIDGET_CAPABILITY_INPUT 1U
struct MfdWidgetInputV1
{
    uint32_t struct_size;
    uint32_t type;
    double minimum, maximum, step, value, normalized_position;
    uint8_t checked, pressed, hovered, pending, down, began, released, cancel;
};
struct MfdWidgetOutputV1
{
    uint32_t struct_size;
    uint8_t pressed, pending, proposed_checked;
    double proposed_value;
    uint32_t event_count;
    uint32_t events[6];
};
struct MfdWidgetRuntimeApiV1
{
    uint32_t struct_size, abi_version, capabilities;
    uint32_t (MFD_WIDGET_CALL* evaluate_input)(const struct MfdWidgetInputV1*, struct MfdWidgetOutputV1*);
};
MFD_WIDGET_EXPORT uint32_t MFD_WIDGET_CALL mfd_get_widgets_runtime_api_v1(struct MfdWidgetRuntimeApiV1* api);
#ifdef __cplusplus
}
#endif

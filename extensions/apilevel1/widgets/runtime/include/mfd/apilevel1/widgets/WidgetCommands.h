#pragma once
/** @file @brief Typed widget commands and input intent on the existing transport. */
#include <optional>
#include <string>
#include "mfd/runtime/GeneratedTransportMap.h"
namespace mfd::api1
{
struct WidgetHandle
{
    std::string page;
    std::string widgetId;
    TransportId pageId = 0U;
    TransportId widgetTransportId = 0U;
};
struct WidgetStatePatch
{
    std::optional<bool> visible;
    std::optional<bool> enabled;
    std::optional<bool> checked;
    std::optional<bool> warning;
    std::optional<double> value;
    std::optional<std::string> text;
    /** @brief Acknowledge/reject the matching pending request; values remain client-authoritative. */
    std::optional<std::uint64_t> requestId;
};
struct UpdateWidgetCommand { WidgetHandle target; WidgetStatePatch patch; };
enum class WidgetEvent : std::uint32_t
{
    Pressed = 0, Released = 1, Activated = 2, CheckedChangeRequested = 3,
    SelectionChangeRequested = 4, ValueChangeRequested = 5, DragBegin = 6,
    DragEnd = 7, Cancelled = 8, Snapshot = 9
};
struct WidgetFeedback
{
    std::uint64_t session = 0;
    std::uint64_t sequence = 0;
    TransportId pageId = 0;
    TransportId widgetId = 0;
    std::uint64_t requestId = 0;
    WidgetEvent event = WidgetEvent::Snapshot;
    bool pressed = false;
    bool checked = false;
    double value = 0;
};
}

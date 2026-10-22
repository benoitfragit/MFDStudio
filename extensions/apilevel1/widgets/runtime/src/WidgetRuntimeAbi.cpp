#include "mfd/apilevel1/widgets/WidgetRuntimeAbi.h"
#include <algorithm>
#include <cmath>
namespace
{
void AddEvent(MfdWidgetOutputV1& result, const uint32_t event) noexcept
{
    // At most five events in a combined begin/update/release sample.
    if (result.event_count < 6U) result.events[result.event_count++] = event;
}
uint32_t MFD_WIDGET_CALL Evaluate(const MfdWidgetInputV1* input, MfdWidgetOutputV1* output) noexcept
{
    if (input == nullptr || output == nullptr || input->struct_size < sizeof(MfdWidgetInputV1) ||
        output->struct_size < sizeof(MfdWidgetOutputV1)) return 1U;
    const auto& in = *input;
    if (in.type > 10U || !std::isfinite(in.minimum) || !std::isfinite(in.maximum) ||
        !std::isfinite(in.maximum-in.minimum) || in.minimum >= in.maximum ||
        !std::isfinite(in.step) || in.step < 0.0 || !std::isfinite(in.value) ||
        in.value < in.minimum || in.value > in.maximum || !std::isfinite(in.normalized_position) ||
        in.checked > 1U || in.pressed > 1U || in.hovered > 1U || in.pending > 1U ||
        in.down > 1U || in.began > 1U || in.released > 1U || in.cancel > 1U) return 2U;
    MfdWidgetOutputV1 result {};
    result.struct_size = sizeof(result);
    result.pressed = in.pressed;
    result.pending = in.pending;
    result.proposed_checked = in.checked;
    result.proposed_value = in.value;
    const bool numeric = in.type == 4U || in.type == 5U;
    if (in.cancel || in.type > 5U)
    {
        if (result.pressed) AddEvent(result, 8U);
        result.pressed = 0U;
        result.pending = 0U;
    }
    else
    {
        if (in.began && in.hovered && !result.pressed)
        {
            result.pressed = 1U;
            AddEvent(result, 0U);
            if (numeric) AddEvent(result, 6U);
        }
        if (result.pressed && numeric && (in.down || in.released))
        {
            double value = in.minimum + std::clamp(in.normalized_position, 0.0, 1.0) * (in.maximum-in.minimum);
            if (in.step > 0.0)
            {
                const double ticks = (value-in.minimum)/in.step;
                if (std::isfinite(ticks)) value = in.minimum + std::round(ticks)*in.step;
            }
            result.proposed_value = std::clamp(value, in.minimum, in.maximum);
            if (result.proposed_value != in.value)
            {
                result.pending = 1U;
                AddEvent(result, 5U);
            }
        }
        if (result.pressed && (in.released || !in.down))
        {
            result.pressed = 0U;
            AddEvent(result, 1U);
            if (numeric) AddEvent(result, 7U);
            else if (in.hovered)
            {
                result.pending = 1U;
                if (in.type == 0U) AddEvent(result, 2U);
                else
                {
                    result.proposed_checked = in.type == 3U ? 1U : !in.checked;
                    AddEvent(result, in.type == 3U ? 4U : 3U);
                }
            }
            else AddEvent(result, 8U);
        }
    }
    *output = result;
    return 0U;
}
}
extern "C" MFD_WIDGET_EXPORT uint32_t MFD_WIDGET_CALL mfd_get_widgets_runtime_api_v1(MfdWidgetRuntimeApiV1* api)
{
    if (api == nullptr || api->struct_size < sizeof(MfdWidgetRuntimeApiV1)) return 1U;
    *api = {sizeof(MfdWidgetRuntimeApiV1), MFD_WIDGET_ABI_VERSION, MFD_WIDGET_CAPABILITY_INPUT, Evaluate};
    return 0U;
}

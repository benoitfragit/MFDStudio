#pragma once

/**
 * @file
 * @brief Shared, allocation-free widget input state transition used by the runtime and editor preview.
 */

#include "mfd/apilevel1/widgets/WidgetRuntimeAbi.h"

#include <algorithm>
#include <cmath>

namespace mfd::api1::detail
{
/** @brief Appends one ABI event while preserving the fixed output bound. */
inline void AddWidgetInteractionEvent(MfdWidgetOutputV1& output, const std::uint32_t event) noexcept
{
    if (output.event_count < 6U)
    {
        output.events[output.event_count] = event;
        ++output.event_count;
    }
}

/**
 * @brief Evaluates one bounded widget pointer sample.
 * @param input Validated ABI input describing the current confirmed state and pointer transition.
 * @param output Receives the proposed state and ordered semantic events.
 * @return Zero on success, `1` for invalid pointers/structure sizes, or `2` for invalid field values.
 * @note This helper is internal and deliberately shared by the runtime DLL and the editor test mode so
 *       both surfaces keep exactly the same button, toggle, slider and knob semantics.
 */
inline std::uint32_t EvaluateWidgetInteraction(const MfdWidgetInputV1* input,
                                               MfdWidgetOutputV1* output) noexcept
{
    if (input == nullptr || output == nullptr || input->struct_size < sizeof(MfdWidgetInputV1) ||
        output->struct_size < sizeof(MfdWidgetOutputV1))
    {
        return 1U;
    }

    const MfdWidgetInputV1& sample = *input;
    if (sample.type > 10U || !std::isfinite(sample.minimum) || !std::isfinite(sample.maximum) ||
        !std::isfinite(sample.maximum - sample.minimum) || sample.minimum >= sample.maximum ||
        !std::isfinite(sample.step) || sample.step < 0.0 || !std::isfinite(sample.value) ||
        sample.value < sample.minimum || sample.value > sample.maximum ||
        !std::isfinite(sample.normalized_position) || sample.checked > 1U || sample.pressed > 1U ||
        sample.hovered > 1U || sample.pending > 1U || sample.down > 1U || sample.began > 1U ||
        sample.released > 1U || sample.cancel > 1U)
    {
        return 2U;
    }

    MfdWidgetOutputV1 result {};
    result.struct_size = sizeof(result);
    result.pressed = sample.pressed;
    result.pending = sample.pending;
    result.proposed_checked = sample.checked;
    result.proposed_value = sample.value;

    const bool numeric = sample.type == 4U || sample.type == 5U;
    if (sample.cancel != 0U || sample.type > 5U)
    {
        if (result.pressed != 0U)
        {
            AddWidgetInteractionEvent(result, 8U);
        }
        result.pressed = 0U;
        result.pending = 0U;
    }
    else
    {
        if (sample.began != 0U && sample.hovered != 0U && result.pressed == 0U)
        {
            result.pressed = 1U;
            AddWidgetInteractionEvent(result, 0U);
            if (numeric)
            {
                AddWidgetInteractionEvent(result, 6U);
            }
        }
        if (result.pressed != 0U && numeric && (sample.down != 0U || sample.released != 0U))
        {
            double value = sample.minimum + std::clamp(sample.normalized_position, 0.0, 1.0) *
                                                (sample.maximum - sample.minimum);
            if (sample.step > 0.0)
            {
                const double ticks = (value - sample.minimum) / sample.step;
                if (std::isfinite(ticks))
                {
                    value = sample.minimum + std::round(ticks) * sample.step;
                }
            }
            result.proposed_value = std::clamp(value, sample.minimum, sample.maximum);
            if (result.proposed_value != sample.value)
            {
                result.pending = 1U;
                AddWidgetInteractionEvent(result, 5U);
            }
        }
        if (result.pressed != 0U && (sample.released != 0U || sample.down == 0U))
        {
            result.pressed = 0U;
            AddWidgetInteractionEvent(result, 1U);
            if (numeric)
            {
                AddWidgetInteractionEvent(result, 7U);
            }
            else if (sample.hovered != 0U)
            {
                result.pending = 1U;
                if (sample.type == 0U)
                {
                    AddWidgetInteractionEvent(result, 2U);
                }
                else
                {
                    const bool selectsRadio = sample.type == 3U;
                    result.proposed_checked = selectsRadio || sample.checked == 0U ? 1U : 0U;
                    AddWidgetInteractionEvent(result, selectsRadio ? 4U : 3U);
                }
            }
            else
            {
                AddWidgetInteractionEvent(result, 8U);
            }
        }
    }

    *output = result;
    return 0U;
}
} // namespace mfd::api1::detail

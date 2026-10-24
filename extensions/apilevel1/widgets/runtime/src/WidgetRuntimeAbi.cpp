#include "mfd/apilevel1/widgets/WidgetRuntimeAbi.h"
#include "WidgetInteractionEvaluator.h"
namespace
{
uint32_t MFD_WIDGET_CALL Evaluate(const MfdWidgetInputV1* input, MfdWidgetOutputV1* output) noexcept
{
    return mfd::api1::detail::EvaluateWidgetInteraction(input, output);
}
}
extern "C" MFD_WIDGET_EXPORT uint32_t MFD_WIDGET_CALL mfd_get_widgets_runtime_api_v1(MfdWidgetRuntimeApiV1* api)
{
    if (api == nullptr || api->struct_size < sizeof(MfdWidgetRuntimeApiV1)) return 1U;
    *api = {sizeof(MfdWidgetRuntimeApiV1), MFD_WIDGET_ABI_VERSION, MFD_WIDGET_CAPABILITY_INPUT, Evaluate};
    return 0U;
}

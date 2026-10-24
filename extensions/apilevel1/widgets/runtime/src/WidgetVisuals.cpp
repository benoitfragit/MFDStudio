#include "mfd/apilevel1/widgets/WidgetVisuals.h"
#include "mfd/apilevel1/widgets/WidgetModel.h"
#include "mfd/model/internal/RuntimeModelValidation.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <system_error>
namespace mfd::api1
{
namespace
{
void FormatWidgetValue(const double value, const std::uint32_t decimals, std::string& output)
{
    std::array<char, 512U> buffer {};
    const std::to_chars_result result = std::to_chars(
        buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::fixed,
        static_cast<int>(decimals));
    if (result.ec != std::errc {})
    {
        output.clear();
        return;
    }
    output.assign(buffer.data(), result.ptr);
}
} // namespace

ContainerRect WidgetHitRegion(const WidgetInstance& widget, const ContainerRect& bounds) noexcept
{
    if (!widget.definition) return {};
    const auto& definition = *widget.definition;
    const Vec2 scale {bounds.size.x / definition.preferredSize.x, bounds.size.y / definition.preferredSize.y};
    return {{bounds.center.x + definition.hitRegion.center.x * scale.x,
             bounds.center.y + definition.hitRegion.center.y * scale.y},
            {definition.hitRegion.size.x * scale.x, definition.hitRegion.size.y * scale.y}};
}
ReticleGroup BuildWidgetVisual(const WidgetInstance& widget, const WidgetState& state, const ContainerRect& bounds)
{
    if (!widget.definition) return {};
    const auto& definition = *widget.definition;
    const WidgetVisualState selected = ResolveWidgetVisualState(state);
    const WidgetVisual* source = nullptr;
    for (const auto& visual : definition.visuals)
    {
        if (visual.state == selected) { source = &visual; break; }
        if (visual.state == WidgetVisualState::Normal) source = &visual;
    }
    if (source == nullptr) return {};
    ReticleGroup result = source->composition;
    const double ratio = std::clamp((state.confirmed.value-definition.range.minimum) /
                                    (definition.range.maximum-definition.range.minimum), 0.0, 1.0);
    for (const WidgetBinding& binding : source->bindings)
    {
        const auto primitive = std::find_if(result.primitives.begin(), result.primitives.end(),
            [&binding](const Primitive& value) { return value.id == binding.primitiveId; });
        if (primitive == result.primitives.end()) continue;
        const float value = static_cast<float>(binding.from + ratio * (binding.to-binding.from));
        switch (binding.target)
        {
        case WidgetBindingTarget::Text:
            if (auto* text = std::get_if<TextGeometry>(&primitive->geometry))
            {
                if (definition.type == WidgetType::NumericDisplay || definition.type == WidgetType::Slider ||
                    definition.type == WidgetType::Knob || definition.type == WidgetType::Gauge || definition.type == WidgetType::ProgressBar)
                {
                    FormatWidgetValue(state.confirmed.value, definition.range.decimals, text->text);
                }
                else text->text = state.confirmed.text;
            }
            break;
        case WidgetBindingTarget::Rotation: primitive->transform.rotationDegrees = value; break;
        case WidgetBindingTarget::ScaleX: primitive->transform.scale.x = value; break;
        case WidgetBindingTarget::ScaleY: primitive->transform.scale.y = value; break;
        case WidgetBindingTarget::PositionX: primitive->transform.position.x = value; break;
        case WidgetBindingTarget::PositionY: primitive->transform.position.y = value; break;
        case WidgetBindingTarget::Visible: primitive->style.visible = value >= 0.5f; break;
        }
    }
    const Vec2 scale {bounds.size.x / definition.preferredSize.x, bounds.size.y / definition.preferredSize.y};
    result.transform.position = {bounds.center.x + result.transform.position.x * scale.x,
                                 bounds.center.y + result.transform.position.y * scale.y};
    result.transform.scale = {result.transform.scale.x * scale.x, result.transform.scale.y * scale.y};
    if (!runtime_validation::internal::IsValidReticle(result)) return {};
    result.visible = result.visible && state.visible;
    return result;
}
}

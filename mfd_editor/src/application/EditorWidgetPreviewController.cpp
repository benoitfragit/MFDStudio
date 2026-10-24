#include "internal/application/EditorWidgetPreviewController.h"

#include "WidgetInteractionEvaluator.h"
#include "mfd/apilevel1/widgets/WidgetModel.h"
#include "mfd/apilevel1/widgets/WidgetVisuals.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace editor::app
{
namespace
{
constexpr std::size_t kNoWidget = std::numeric_limits<std::size_t>::max();

bool Contains(const mfd::api1::ContainerRect& rectangle, const mfd::Vec2 point) noexcept
{
    return rectangle.size.x > 0.0f && rectangle.size.y > 0.0f &&
           std::abs(point.x - rectangle.center.x) <= rectangle.size.x * 0.5f &&
           std::abs(point.y - rectangle.center.y) <= rectangle.size.y * 0.5f;
}

bool OverScrollbar(const mfd::api1::ContainerLayoutNode& layout,
                   const mfd::Vec2 pointer) noexcept
{
    return (layout.horizontalScrollbar.visible && Contains(layout.horizontalScrollbar.track, pointer)) ||
           (layout.verticalScrollbar.visible && Contains(layout.verticalScrollbar.track, pointer));
}

bool VisibleThroughAncestors(const std::vector<mfd::api1::ContainerDefinition>& containers,
                             const std::vector<const mfd::api1::ContainerLayoutNode*>& layoutsBySource,
                             const mfd::api1::ContainerLayoutNode& layout,
                             const mfd::Vec2 pointer) noexcept
{
    if (!layout.effectiveVisible || layout.sourceIndex >= layoutsBySource.size())
    {
        return false;
    }

    const mfd::api1::ContainerLayoutNode* current = &layout;
    for (std::size_t depth = 0U; current != nullptr && depth <= mfd::api1::kMaximumContainerDepth + 1U; ++depth)
    {
        if (current->sourceIndex >= layoutsBySource.size())
        {
            return false;
        }
        if (current->sourceIndex < containers.size() &&
            containers[current->sourceIndex].scrolling.clipContent &&
            !Contains(current->viewport, pointer))
        {
            return false;
        }
        if (current->parentSourceIndex == std::numeric_limits<std::size_t>::max())
        {
            return true;
        }
        if (current->parentSourceIndex >= layoutsBySource.size())
        {
            return false;
        }
        current = layoutsBySource[current->parentSourceIndex];
    }
    return false;
}

bool AncestorScrollbarOwnsPointer(const std::vector<const mfd::api1::ContainerLayoutNode*>& layoutsBySource,
                                  const mfd::api1::ContainerLayoutNode& layout,
                                  const mfd::Vec2 pointer) noexcept
{
    std::size_t parent = layout.parentSourceIndex;
    for (std::size_t depth = 0U;
         parent < layoutsBySource.size() && depth < mfd::api1::kMaximumContainerDepth;
         ++depth)
    {
        const mfd::api1::ContainerLayoutNode* ancestor = layoutsBySource[parent];
        if (ancestor == nullptr)
        {
            return false;
        }
        if (OverScrollbar(*ancestor, pointer))
        {
            return true;
        }
        parent = ancestor->parentSourceIndex;
    }
    return false;
}

double NormalizedPointerPosition(const mfd::api1::WidgetInstance& widget,
                                 const mfd::api1::ContainerLayoutNode& layout,
                                 const mfd::Vec2 pointer) noexcept
{
    if (layout.bounds.size.x <= 0.0f)
    {
        return 0.0;
    }
    if (widget.definition->type == mfd::api1::WidgetType::Knob)
    {
        constexpr double kPi = 3.14159265358979323846;
        return (std::atan2(pointer.y - layout.bounds.center.y, pointer.x - layout.bounds.center.x) + kPi) /
               (2.0 * kPi);
    }
    return (pointer.x - layout.bounds.center.x) / layout.bounds.size.x + 0.5;
}

bool OutputRequestsConfirmation(const MfdWidgetOutputV1& output) noexcept
{
    for (std::uint32_t index = 0U; index < output.event_count && index < 6U; ++index)
    {
        if (output.events[index] >= 2U && output.events[index] <= 5U)
        {
            return true;
        }
    }
    return false;
}
} // namespace

bool EditorWidgetPreviewController::Enabled() const noexcept
{
    return enabled_;
}

void EditorWidgetPreviewController::SetEnabled(const bool enabled) noexcept
{
    if (enabled_ == enabled)
    {
        return;
    }
    enabled_ = enabled;
    Reset();
}

void EditorWidgetPreviewController::Reset() noexcept
{
    capturedWidgetId_.clear();
    entries_.clear();
    layoutsBySource_.clear();
}

void EditorWidgetPreviewController::CancelPointerInteraction() noexcept
{
    capturedWidgetId_.clear();
    for (Entry& entry : entries_)
    {
        entry.state.hovered = false;
        entry.state.pressed = false;
        entry.state.pending = false;
    }
}

void EditorWidgetPreviewController::Synchronize(const std::vector<mfd::api1::WidgetInstance>& widgets)
{
    if (!enabled_)
    {
        return;
    }

    bool unchanged = entries_.size() == widgets.size();
    for (std::size_t index = 0U; unchanged && index < widgets.size(); ++index)
    {
        unchanged = entries_[index].id == widgets[index].id;
    }
    if (unchanged)
    {
        return;
    }

    capturedWidgetId_.clear();
    entries_.clear();
    entries_.reserve(widgets.size());
    for (const mfd::api1::WidgetInstance& widget : widgets)
    {
        entries_.push_back({widget.id, mfd::api1::InitialWidgetState(widget)});
    }
}

bool EditorWidgetPreviewController::Update(
    const std::vector<mfd::api1::ContainerDefinition>& containers,
    const std::vector<mfd::api1::ContainerLayoutNode>& layouts,
    const std::vector<mfd::api1::WidgetInstance>& widgets,
    const WidgetPreviewPointerSample& sample)
{
    Synchronize(widgets);
    if (!enabled_ || widgets.size() != entries_.size() ||
        containers.size() > mfd::api1::kMaximumContainerCount ||
        widgets.size() > mfd::api1::kMaximumWidgetCount ||
        !std::isfinite(sample.pointer.x) || !std::isfinite(sample.pointer.y))
    {
        return false;
    }

    const std::size_t projectedNodeCount = containers.size() + widgets.size();
    if (projectedNodeCount < containers.size() || layouts.size() > projectedNodeCount)
    {
        return false;
    }

    layoutsBySource_.assign(projectedNodeCount, nullptr);
    for (const mfd::api1::ContainerLayoutNode& layout : layouts)
    {
        if (layout.sourceIndex >= layoutsBySource_.size() || layoutsBySource_[layout.sourceIndex] != nullptr)
        {
            return false;
        }
        layoutsBySource_[layout.sourceIndex] = &layout;
    }

    const std::size_t firstWidgetSource = containers.size();
    std::size_t hoveredWidget = kNoWidget;
    for (auto iterator = layouts.rbegin(); !sample.blocked && iterator != layouts.rend(); ++iterator)
    {
        const mfd::api1::ContainerLayoutNode& layout = *iterator;
        if (!VisibleThroughAncestors(containers, layoutsBySource_, layout, sample.pointer))
        {
            continue;
        }
        if (layout.sourceIndex >= firstWidgetSource)
        {
            const std::size_t index = layout.sourceIndex - firstWidgetSource;
            if (index >= widgets.size() ||
                !Contains(mfd::api1::WidgetHitRegion(widgets[index], layout.bounds), sample.pointer))
            {
                continue;
            }
            if (!AncestorScrollbarOwnsPointer(layoutsBySource_, layout, sample.pointer))
            {
                hoveredWidget = index;
            }
            break;
        }
        if (OverScrollbar(layout, sample.pointer) ||
            (containers[layout.sourceIndex].appearance.backgroundVisible &&
             containers[layout.sourceIndex].appearance.backgroundColor.a != 0U &&
             Contains(layout.bounds, sample.pointer)))
        {
            break;
        }
    }

    if (sample.began && !sample.cancel && capturedWidgetId_.empty() && hoveredWidget < widgets.size())
    {
        const mfd::api1::ContainerLayoutNode* layout = layoutsBySource_[firstWidgetSource + hoveredWidget];
        if (layout != nullptr && layout->effectiveEnabled && widgets[hoveredWidget].definition != nullptr &&
            mfd::api1::IsInteractiveWidget(widgets[hoveredWidget].definition->type))
        {
            capturedWidgetId_ = widgets[hoveredWidget].id;
        }
    }

    const bool consumed = hoveredWidget != kNoWidget || !capturedWidgetId_.empty();
    for (std::size_t index = 0U; index < widgets.size(); ++index)
    {
        const mfd::api1::WidgetInstance& widget = widgets[index];
        Entry& entry = entries_[index];
        const mfd::api1::ContainerLayoutNode* layout = layoutsBySource_[firstWidgetSource + index];
        const bool ownsCapture = capturedWidgetId_ == widget.id;
        const bool unavailable = layout == nullptr || !layout->effectiveVisible || !layout->effectiveEnabled ||
                                 widget.definition == nullptr;

        entry.state.visible = layout != nullptr && layout->effectiveVisible;
        entry.state.enabled = layout != nullptr && layout->effectiveEnabled;
        entry.state.hovered = hoveredWidget == index && !unavailable;

        if (widget.definition == nullptr)
        {
            entry.state.pressed = false;
            entry.state.pending = false;
            continue;
        }

        const mfd::api1::WidgetRange& range = widget.definition->range;
        MfdWidgetInputV1 input {};
        input.struct_size = sizeof(input);
        input.type = static_cast<std::uint32_t>(widget.definition->type);
        input.minimum = range.minimum;
        input.maximum = range.maximum;
        input.step = range.step;
        input.value = entry.state.confirmed.value;
        input.normalized_position = layout == nullptr ? 0.0 : NormalizedPointerPosition(widget, *layout, sample.pointer);
        input.checked = entry.state.confirmed.checked ? 1U : 0U;
        input.pressed = entry.state.pressed ? 1U : 0U;
        input.hovered = entry.state.hovered ? 1U : 0U;
        input.pending = entry.state.pending ? 1U : 0U;
        input.down = sample.down && ownsCapture ? 1U : 0U;
        input.began = sample.began && ownsCapture ? 1U : 0U;
        input.released = sample.released && ownsCapture ? 1U : 0U;
        input.cancel = sample.cancel || unavailable || (entry.state.pressed && !ownsCapture) ? 1U : 0U;

        MfdWidgetOutputV1 output {};
        output.struct_size = sizeof(output);
        if (mfd::api1::detail::EvaluateWidgetInteraction(&input, &output) != 0U)
        {
            entry.state.pressed = false;
            entry.state.pending = false;
            if (ownsCapture)
            {
                capturedWidgetId_.clear();
            }
            continue;
        }

        entry.state.pressed = output.pressed != 0U;
        entry.state.pending = output.pending != 0U;
        if (OutputRequestsConfirmation(output))
        {
            entry.state.confirmed.checked = output.proposed_checked != 0U;
            entry.state.confirmed.value = output.proposed_value;
            entry.state.pending = false;
        }
        if (ownsCapture && !entry.state.pressed)
        {
            capturedWidgetId_.clear();
        }
    }

    if (sample.cancel)
    {
        capturedWidgetId_.clear();
    }
    return consumed;
}

mfd::api1::WidgetState EditorWidgetPreviewController::PreviewState(
    const std::size_t index,
    const mfd::api1::WidgetInstance& widget,
    const bool effectiveVisible,
    const bool effectiveEnabled) const
{
    mfd::api1::WidgetState state = index < entries_.size() && entries_[index].id == widget.id
                                            ? entries_[index].state
                                            : mfd::api1::InitialWidgetState(widget);
    state.visible = effectiveVisible;
    state.enabled = effectiveEnabled;
    return state;
}
} // namespace editor::app

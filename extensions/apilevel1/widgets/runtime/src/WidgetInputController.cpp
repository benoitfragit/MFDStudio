#include "mfd/apilevel1/widgets/WidgetInputController.h"
#include "mfd/apilevel1/widgets/WidgetModel.h"
#include "mfd/apilevel1/widgets/WidgetVisuals.h"
#include "mfd/runtime/SceneRegistry.h"
#include "ContainerPointerQuery.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <unordered_set>
namespace mfd::api1
{
namespace
{
bool Contains(const ContainerRect& rect, const Vec2 point) noexcept
{
    return rect.size.x > 0.0f && rect.size.y > 0.0f && std::abs(point.x-rect.center.x) <= rect.size.x*0.5f &&
           std::abs(point.y-rect.center.y) <= rect.size.y*0.5f;
}
bool OverScrollbar(const ContainerLayoutNode& node, const Vec2 pointer) noexcept
{
    return (node.horizontalScrollbar.visible && Contains(node.horizontalScrollbar.track,pointer)) ||
           (node.verticalScrollbar.visible && Contains(node.verticalScrollbar.track,pointer));
}
}
WidgetInputController::WidgetInputController()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    session_ = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
    if (session_ == 0U) session_ = 1U;
}
bool WidgetInputController::NeedsUpdate(const std::string& page) const noexcept
{ return !captured_.empty() || !pendingAges_.empty() || (!page_.empty() && page_ != page); }
void WidgetInputController::Emit(SceneRegistry& scene, const TransportId pageId, const TransportId widgetId,
    const std::uint64_t requestId, const WidgetEvent event, const bool pressed, const bool checked, const double value)
{
    if (pageId == 0U || widgetId == 0U) return;
    if (sequence_ == std::numeric_limits<std::uint64_t>::max()) { ++session_; sequence_ = 0U; }
    scene.QueueWidgetFeedback({session_, ++sequence_, pageId, widgetId, requestId, event, pressed, checked, value});
}
bool WidgetInputController::Update(SceneRegistry& scene, const WidgetRuntimeHost& runtime,
    const std::vector<ContainerDefinition>& nodes, const std::vector<ContainerLayoutNode>& layouts,
    const std::vector<WidgetInstance>& widgets, const std::vector<WidgetState>& states,
    const Vec2 pointer, const bool down, const bool began, const bool released, const bool cancel, const float elapsed, const bool pointerBlocked)
{
    if (widgets.size() != states.size() || widgets.size() > kMaximumWidgetCount || nodes.size() < widgets.size()) return false;
    const float dt = std::isfinite(elapsed) ? std::clamp(elapsed,0.0f,1.0f) : 0.0f;
    if (page_ != scene.ActivePageName())
    {
        if (!captured_.empty()) Emit(scene,captureSnapshot_.pageId,captureSnapshot_.widgetId,
            captureSnapshot_.requestId,WidgetEvent::Cancelled,false,captureSnapshot_.checked,captureSnapshot_.value);
        captured_.clear();
        page_ = scene.ActivePageName();
        capturedSeconds_ = 0;
        pendingAges_.clear();
        snapshotIndex_ = 0;
    }
    const std::size_t firstWidget = nodes.size()-widgets.size();
    const std::size_t noTarget = std::numeric_limits<std::size_t>::max();
    std::size_t hovered = noTarget;
    const detail::ContainerPointerQuery query(nodes,layouts,pointer);
    std::vector<const ContainerLayoutNode*> bySource(nodes.size(),nullptr);
    for (const auto& node : layouts) if (node.sourceIndex < bySource.size()) bySource[node.sourceIndex] = &node;
    for (auto it = layouts.rbegin(); !pointerBlocked && it != layouts.rend(); ++it)
    {
        if (!query.VisibleAt(*it,false)) continue;
        if (it->sourceIndex >= firstWidget)
        {
            const auto index = it->sourceIndex-firstWidget;
            if (index >= widgets.size() || !Contains(WidgetHitRegion(widgets[index],it->bounds),pointer)) continue;
            bool scrollbar = false;
            auto parent = it->parentSourceIndex;
            for (std::size_t depth = 0; parent < bySource.size() && depth < kMaximumContainerDepth; ++depth)
            {
                const auto* ancestor = bySource[parent];
                if (ancestor == nullptr) break;
                scrollbar = scrollbar || OverScrollbar(*ancestor,pointer);
                parent = ancestor->parentSourceIndex;
            }
            if (scrollbar) break;
            hovered = index; // Disabled instances consume their hit region without accepting input.
            break;
        }
        if (OverScrollbar(*it,pointer) || (nodes[it->sourceIndex].appearance.backgroundVisible &&
            nodes[it->sourceIndex].appearance.backgroundColor.a != 0U && Contains(it->bounds,pointer))) break;
    }
    if (began && !cancel && hovered < widgets.size() && captured_.empty())
    {
        const auto* layout = bySource[firstWidget+hovered];
        if (layout && layout->effectiveEnabled && IsInteractiveWidget(widgets[hovered].definition->type))
        {
            captured_ = widgets[hovered].id;
            capturedSeconds_ = 0;
        }
    }
    const bool consumed = hovered != noTarget || !captured_.empty();
    capturedSeconds_ += dt;
    std::unordered_set<std::string_view> activeIds;
    activeIds.reserve(widgets.size());
    for (const auto& widget : widgets) activeIds.insert(widget.id);
    for (auto age = pendingAges_.begin(); age != pendingAges_.end();)
    {
        if (activeIds.count(age->first) == 0)
            age = pendingAges_.erase(age);
        else { age->second += dt; ++age; }
    }
    bool captureFound = false;
    for (std::size_t index = 0; index < widgets.size(); ++index)
    {
        const auto& widget = widgets[index];
        const auto& state = states[index];
        const auto* layout = bySource[firstWidget+index];
        const bool ownsCapture = captured_ == widget.id;
        captureFound = captureFound || ownsCapture;
        const bool unavailable = layout == nullptr || !layout->effectiveVisible || !layout->effectiveEnabled;
        const bool cancelInput = cancel || unavailable || (ownsCapture && capturedSeconds_ >= 30.0f);
        const auto& range = widget.definition->range;
        double position = 0.0;
        if (layout && layout->bounds.size.x > 0.0f)
        {
            position = (pointer.x-layout->bounds.center.x)/layout->bounds.size.x+0.5;
            if (widget.definition->type == WidgetType::Knob)
                position = (std::atan2(pointer.y-layout->bounds.center.y,pointer.x-layout->bounds.center.x)+3.141592653589793)/6.283185307179586;
        }
        MfdWidgetInputV1 input {};
        input.struct_size = sizeof(input);
        input.type = static_cast<std::uint32_t>(widget.definition->type);
        input.minimum = range.minimum; input.maximum = range.maximum; input.step = range.step;
        input.value = state.confirmed.value; input.normalized_position = position;
        input.checked = state.confirmed.checked; input.pressed = state.pressed;
        input.hovered = hovered == index && !unavailable;
        if (!state.pending) pendingAges_.erase(widget.id);
        const auto age = state.pending ? pendingAges_.emplace(widget.id, 0.0f).first : pendingAges_.end();
        input.pending = state.pending && age != pendingAges_.end() && age->second < 2.0f;
        input.down = down && ownsCapture; input.began = began && ownsCapture;
        input.released = released && ownsCapture; input.cancel = cancelInput || (state.pressed && !ownsCapture);
        MfdWidgetOutputV1 output {};
        if (!runtime.Evaluate(input,output))
        {
            scene.SetActiveWidgetInteraction(widget.id, false, false, false, state.requestId);
            if (ownsCapture) captured_.clear();
            pendingAges_.erase(widget.id);
            continue;
        }
        std::uint64_t requestId = state.requestId;
        for (std::uint32_t eventIndex = 0; eventIndex < output.event_count; ++eventIndex)
            if (output.events[eventIndex] >= 2U && output.events[eventIndex] <= 5U)
            {
                if (request_ == std::numeric_limits<std::uint64_t>::max()) { ++session_; request_ = 0; }
                requestId = ++request_; pendingAges_[widget.id] = 0; break;
            }
        scene.SetActiveWidgetInteraction(widget.id,input.hovered != 0U,output.pressed != 0U,output.pending != 0U,requestId);
        const auto route = scene.ActiveWidgetTransportId(widget.id);
        for (std::uint32_t eventIndex = 0; eventIndex < output.event_count; ++eventIndex)
            Emit(scene,scene.ActivePageTransportId(),route,requestId,static_cast<WidgetEvent>(output.events[eventIndex]),
                 output.pressed != 0U,output.proposed_checked != 0U,output.proposed_value);
        if (ownsCapture)
        {
            captureSnapshot_ = {session_, sequence_, scene.ActivePageTransportId(), route, requestId,
                WidgetEvent::Snapshot, output.pressed != 0U, state.confirmed.checked, state.confirmed.value};
            if (output.pressed == 0U) captured_.clear();
        }
    }
    if (!captured_.empty() && !captureFound)
    {
        Emit(scene,captureSnapshot_.pageId,captureSnapshot_.widgetId,captureSnapshot_.requestId,
            WidgetEvent::Cancelled,false,captureSnapshot_.checked,captureSnapshot_.value);
        captured_.clear();
    }
    snapshotSeconds_ += dt;
    if (snapshotSeconds_ >= 0.5f && !widgets.empty())
    {
        const std::size_t end = std::min(widgets.size(),snapshotIndex_+64U);
        for (; snapshotIndex_ < end; ++snapshotIndex_)
        {
            const auto& state = states[snapshotIndex_];
            Emit(scene,scene.ActivePageTransportId(),scene.ActiveWidgetTransportId(widgets[snapshotIndex_].id),
                state.requestId,WidgetEvent::Snapshot,state.pressed,state.confirmed.checked,state.confirmed.value);
        }
        if (snapshotIndex_ >= widgets.size()) { snapshotIndex_ = 0; snapshotSeconds_ = 0; }
    }
    return consumed;
}
}

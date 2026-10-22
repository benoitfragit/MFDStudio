#include "mfd/apilevel1/widgets/WidgetModel.h"
#include "mfd/apilevel1/ContainerLayoutEngine.h"
#include "mfd/apilevel1/internal/LayoutKernel.h"
#include "mfd/model/PageName.h"
#include "mfd/model/internal/RuntimeModelValidation.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace mfd::api1
{
namespace
{
bool ValidId(const std::string& id)
{
    return !id.empty() && id.size() <= 256U && id.find('\0') == std::string::npos && !NormalizePageName(id).empty();
}
bool ValidSize(const Vec2 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && value.x > 0.0f && value.y > 0.0f &&
           value.x <= 10000.0f && value.y <= 10000.0f;
}
bool ValidValue(const WidgetValue& value, const WidgetRange& range)
{
    return std::isfinite(value.value) && value.value >= range.minimum && value.value <= range.maximum &&
           value.text.size() <= kMaximumWidgetTextBytes && value.text.find('\0') == std::string::npos;
}
} // namespace

bool IsInteractiveWidget(const WidgetType type) noexcept
{
    return static_cast<std::uint32_t>(type) <= static_cast<std::uint32_t>(WidgetType::Knob);
}
WidgetState InitialWidgetState(const WidgetInstance& instance)
{
    WidgetState state;
    if (instance.definition) state.confirmed = instance.definition->defaults;
    if (instance.overrides.checked) state.confirmed.checked = *instance.overrides.checked;
    if (instance.overrides.warning) state.confirmed.warning = *instance.overrides.warning;
    if (instance.overrides.value) state.confirmed.value = *instance.overrides.value;
    if (instance.overrides.text) state.confirmed.text = *instance.overrides.text;
    state.visible = instance.visible;
    state.enabled = instance.enabled;
    return state;
}
WidgetVisualState ResolveWidgetVisualState(const WidgetState& state) noexcept
{
    if (!state.enabled) return WidgetVisualState::Disabled;
    if (state.pressed) return WidgetVisualState::Pressed;
    if (state.pending) return WidgetVisualState::Pending;
    if (state.confirmed.warning) return WidgetVisualState::Warning;
    if (state.confirmed.checked) return WidgetVisualState::Checked;
    if (state.hovered) return WidgetVisualState::Hover;
    return WidgetVisualState::Normal;
}
bool ValidateWidgetDefinition(const WidgetDefinition& definition, std::string& error)
{
    error.clear();
    const WidgetRange& range = definition.range;
    if (definition.schemaVersion != 1U || !ValidId(definition.id) ||
        static_cast<std::uint32_t>(definition.type) > 10U || !ValidSize(definition.preferredSize) ||
        !ValidSize(definition.hitRegion.size) ||
        !runtime_validation::IsValidVec2(definition.hitRegion.center) ||
        !std::isfinite(range.minimum) || !std::isfinite(range.maximum) ||
        !std::isfinite(range.maximum - range.minimum) || range.minimum >= range.maximum ||
        !std::isfinite(range.step) || range.step < 0.0 || range.decimals > 9U ||
        !ValidValue(definition.defaults, range))
    {
        error = "Widget '" + definition.id + "' has invalid schema, type, geometry, range or defaults.";
        return false;
    }
    if (definition.visuals.empty() || definition.visuals.size() > 7U)
    {
        error = "A widget requires a normal visual and at most seven state visuals.";
        return false;
    }
    std::unordered_set<std::uint32_t> states;
    for (const WidgetVisual& visual : definition.visuals)
    {
        const auto state = static_cast<std::uint32_t>(visual.state);
        if (state > 6U || !states.insert(state).second || visual.composition.primitives.size() > 256U ||
            visual.bindings.size() > 64U || !runtime_validation::internal::IsValidReticle(visual.composition))
        {
            error = "Invalid or duplicate widget visual state or over-budget visual.";
            return false;
        }
        if (visual.composition.clipping.mode != ReticleClipMode::None)
        {
            error = "Widget visuals cannot erase Page layers; use container viewport clipping.";
            return false;
        }
        std::unordered_map<std::string, PrimitiveType> primitives;
        for (const Primitive& primitive : visual.composition.primitives)
        {
            if (!ValidId(primitive.id) || !primitives.emplace(primitive.id, primitive.type).second)
            {
                error = "Widget visuals require unique nonempty primitive IDs.";
                return false;
            }
        }
        for (const WidgetBinding& binding : visual.bindings)
        {
            const auto primitive = primitives.find(binding.primitiveId);
            if (primitive == primitives.end() || static_cast<std::uint32_t>(binding.target) > 6U ||
                !std::isfinite(binding.from) || !std::isfinite(binding.to) ||
                std::abs(binding.from) > 10000.0 || std::abs(binding.to) > 10000.0 ||
                (binding.target == WidgetBindingTarget::Text && primitive->second != PrimitiveType::Text))
            {
                error = "Invalid widget binding target, primitive reference or range.";
                return false;
            }
        }
    }
    if (states.count(0U) == 0U)
    {
        error = "Widget requires a normal visual state.";
        return false;
    }
    return true;
}

std::vector<ContainerDefinition> ProjectWidgetLayoutNodes(const std::vector<ContainerDefinition>& containers,
                                                         const std::vector<WidgetInstance>& widgets)
{
    std::vector<ContainerDefinition> nodes = containers;
    nodes.reserve(containers.size() + widgets.size());
    for (ContainerDefinition& container : nodes)
    {
        container.id = "container/" + container.id;
        for (ContainerChildPlacement& child : container.children)
        {
            child.childId = (child.kind == ContainerChildKind::Widget ? "widget/" : "container/") + child.childId;
            child.kind = ContainerChildKind::Container;
        }
    }
    for (const WidgetInstance& widget : widgets)
    {
        ContainerDefinition leaf;
        leaf.id = "widget/" + widget.id;
        leaf.geometry = {widget.position, widget.size.value_or(widget.definition->preferredSize)};
        leaf.minimumSize = {0.0f, 0.0f};
        leaf.layoutReferenceSize = leaf.geometry.size;
        leaf.visible = widget.visible;
        leaf.enabled = widget.enabled;
        leaf.scrolling.clipContent = false;
        nodes.push_back(std::move(leaf));
    }
    return nodes;
}
bool ValidatePageWidgets(const std::vector<ContainerDefinition>& containers,
                         const std::vector<WidgetInstance>& widgets, std::string& error)
{
    if (widgets.size() > kMaximumWidgetCount || containers.size() > kMaximumContainerCount)
    {
        error = "Mixed page exceeds the container/widget count limit.";
        return false;
    }
    std::size_t edgeCount = 0;
    for (const auto& container : containers)
    {
        if (container.children.size() > kMaximumContainerCount + kMaximumWidgetCount - edgeCount)
        { error = "Mixed page exceeds the 8192 ownership-edge budget."; return false; }
        edgeCount += container.children.size();
    }
    std::unordered_set<std::string> ids;
    std::unordered_set<std::string> widgetIds;
    std::unordered_set<std::string> containerIds;
    for (const ContainerDefinition& container : containers)
    {
        if (!ids.insert(NormalizePageName(container.id)).second)
        {
            error = "Duplicate normalized container ID.";
            return false;
        }
        containerIds.insert(container.id);
    }
    for (const WidgetInstance& widget : widgets)
    {
        if (!ValidId(widget.id) || !ids.insert(NormalizePageName(widget.id)).second ||
            !widget.definition || !runtime_validation::IsValidVec2(widget.position) ||
            (widget.size && !ValidSize(*widget.size)))
        {
            error = "Invalid widget ID, missing definition, collision or geometry: " + widget.id;
            return false;
        }
        if (!ValidateWidgetDefinition(*widget.definition, error)) return false;
        if (!ValidValue(InitialWidgetState(widget).confirmed, widget.definition->range))
        {
            error = "Invalid widget instance override: " + widget.id;
            return false;
        }
        widgetIds.insert(widget.id);
    }
    auto containerOnly = containers;
    for (ContainerDefinition& container : containerOnly)
    {
        for (const ContainerChildPlacement& child : container.children)
        {
            if ((child.kind == ContainerChildKind::Container && !containerIds.count(child.childId)) ||
                (child.kind == ContainerChildKind::Widget && !widgetIds.count(child.childId)) ||
                static_cast<std::uint32_t>(child.kind) > 1U)
            {
                error = "Unknown typed container child: " + child.childId;
                return false;
            }
        }
        auto& children = container.children;
        children.erase(std::remove_if(children.begin(), children.end(), [](const ContainerChildPlacement& child)
        { return child.kind == ContainerChildKind::Widget; }), children.end());
    }
    // Preserve the published limit of 64 container levels; the final widget is a leaf.
    if (!ContainerLayoutEngine {}.Validate(containerOnly, error)) return false;
    return detail::ValidateLayoutNodes(ProjectWidgetLayoutNodes(containers, widgets),
                                      kMaximumContainerCount + kMaximumWidgetCount,
                                      kMaximumContainerDepth + 1U, error);
}
bool EvaluateWidgetLayout(const std::vector<ContainerDefinition>& containers,
                          const std::vector<WidgetInstance>& widgets,
                          std::vector<ContainerLayoutNode>& output, std::string& error)
{
    output.clear();
    if (!ValidatePageWidgets(containers, widgets, error)) return false;
    return detail::EvaluateLayoutNodes(ProjectWidgetLayoutNodes(containers, widgets),
                                      kMaximumContainerCount + kMaximumWidgetCount,
                                      kMaximumContainerDepth + 1U, output, error);
}
bool EvaluateContainersWithWidgets(const std::vector<ContainerDefinition>& containers,
                                  const std::vector<WidgetInstance>& widgets,
                                  std::vector<ContainerLayoutNode>& output, std::string& error)
{
    if (widgets.empty()) return ContainerLayoutEngine {}.Evaluate(containers, output, error);
    if (!EvaluateWidgetLayout(containers, widgets, output, error)) return false;
    output.erase(std::remove_if(output.begin(), output.end(), [count = containers.size()](const ContainerLayoutNode& node)
    { return node.sourceIndex >= count; }), output.end());
    for (ContainerLayoutNode& node : output)
    {
        node.id = containers[node.sourceIndex].id;
        if (node.parentSourceIndex < containers.size()) node.parentId = containers[node.parentSourceIndex].id;
    }
    return true;
}
} // namespace mfd::api1

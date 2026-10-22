#include "mfd/apilevel1/widgets/WidgetEditorModel.h"
#include "mfd/model/PageName.h"
#include "mfd/apilevel1/ContainerLayoutEngine.h"
#include <algorithm>
#include <cmath>
namespace mfd::api1
{
namespace
{
bool UsesId(const PageDefinition& page, const std::string& id)
{
    for (const auto& container : page.containers) if (PageNamesEqual(container.id, id)) return true;
    for (const auto& widget : page.widgets) if (PageNamesEqual(widget.id, id)) return true;
    return false;
}
bool SameGeometry(const WidgetInstance& first, const WidgetInstance& second) noexcept
{
    if (first.position.x != second.position.x || first.position.y != second.position.y ||
        first.size.has_value() != second.size.has_value()) return false;
    return !first.size || (first.size->x == second.size->x && first.size->y == second.size->y);
}
void RemoveEdges(PageDefinition& page, const std::string_view id)
{
    for (auto& container : page.containers)
        container.children.erase(std::remove_if(container.children.begin(), container.children.end(),
            [id](const ContainerChildPlacement& edge) { return edge.kind == ContainerChildKind::Widget && edge.childId == id; }),
            container.children.end());
}
}
WidgetInstance* WidgetEditorModel::Find(PageDefinition& page, const std::string_view id) noexcept
{
    for (auto& widget : page.widgets) if (widget.id == id) return &widget;
    return nullptr;
}
const WidgetInstance* WidgetEditorModel::Find(const PageDefinition& page, const std::string_view id) noexcept
{
    for (const auto& widget : page.widgets) if (widget.id == id) return &widget;
    return nullptr;
}
std::string WidgetEditorModel::UniqueId(const PageDefinition& page, const std::string_view base)
{
    const std::string prefix(base.empty() ? "Widget" : base);
    std::string id = prefix;
    for (std::size_t index = 2; UsesId(page, id) && index <= kMaximumWidgetCount + kMaximumContainerCount + 2; ++index)
        id = prefix + std::to_string(index);
    return id;
}
std::string WidgetEditorModel::Insert(PageDefinition& page, std::shared_ptr<const WidgetDefinition> definition,
                                      std::string reference, const std::string_view parent, std::string& error)
{
    if (!definition || page.widgets.size() >= kMaximumWidgetCount)
    { error = "A valid definition and room for another widget are required."; return {}; }
    PageDefinition candidate = page;
    WidgetInstance instance;
    instance.id = UniqueId(page, definition->id);
    instance.definition = std::move(definition);
    instance.definitionFile = std::move(reference);
    const std::string id = instance.id;
    candidate.widgets.push_back(instance);
    if (!Replace(candidate, id, std::move(instance), parent, nullptr, error)) return {};
    page = std::move(candidate);
    return id;
}
std::string WidgetEditorModel::Duplicate(PageDefinition& page, const std::string_view id, std::string& error)
{
    const auto* source = Find(page, id);
    if (!source || page.widgets.size() >= kMaximumWidgetCount)
    { error = "Select a valid widget with room for a duplicate."; return {}; }
    PageDefinition candidate = page;
    WidgetInstance copy = *source;
    copy.id = UniqueId(page, source->id + "Copy");
    const std::string duplicateId = copy.id;
    candidate.widgets.push_back(std::move(copy));
    for (auto& container : candidate.containers)
    {
        const auto edge = std::find_if(container.children.begin(), container.children.end(),
            [id](const ContainerChildPlacement& child) { return child.kind == ContainerChildKind::Widget && child.childId == id; });
        if (edge != container.children.end())
        { auto placement = *edge; placement.childId = duplicateId; container.children.push_back(std::move(placement)); break; }
    }
    if (!ValidatePageWidgets(candidate.containers, candidate.widgets, error)) return {};
    page = std::move(candidate);
    return duplicateId;
}
bool WidgetEditorModel::Remove(PageDefinition& page, const std::string_view id, std::string& error)
{
    if (!Find(page, id)) { error = "Unknown widget instance."; return false; }
    PageDefinition candidate = page;
    RemoveEdges(candidate, id);
    candidate.widgets.erase(std::remove_if(candidate.widgets.begin(), candidate.widgets.end(),
        [id](const WidgetInstance& widget) { return widget.id == id; }), candidate.widgets.end());
    if (!ValidatePageWidgets(candidate.containers, candidate.widgets, error)) return false;
    page = std::move(candidate);
    return true;
}
bool WidgetEditorModel::Replace(PageDefinition& page, const std::string_view previousId, WidgetInstance replacement,
                                const std::string_view parent, const ContainerChildPlacement* placement, std::string& error)
{
    PageDefinition candidate = page;
    auto* target = Find(candidate, previousId);
    if (!target) { error = "Unknown widget instance."; return false; }
    RemoveEdges(candidate, previousId);
    *target = std::move(replacement);
    if (!parent.empty())
    {
        auto* container = FindContainer(candidate.containers, parent);
        if (!container) { error = "Unknown widget parent container."; return false; }
        ContainerChildPlacement edge = placement ? *placement : ContainerChildPlacement {};
        edge.kind = ContainerChildKind::Widget;
        edge.childId = target->id;
        container->children.push_back(std::move(edge));
    }
    if (!ValidatePageWidgets(candidate.containers, candidate.widgets, error)) return false;
    page = std::move(candidate);
    return true;
}
bool WidgetEditorModel::ApplyGesture(PageDefinition& page, const WidgetInstance& initial, const Vec2 delta,
                                    const bool resize, std::string& error)
{
    if (!std::isfinite(delta.x) || !std::isfinite(delta.y) || !initial.definition)
    { error = "Widget gesture must have finite coordinates and a definition."; return false; }
    PageDefinition candidate = page;
    auto* target = Find(candidate, initial.id);
    if (!target) { error = "Unknown widget instance."; return false; }
    *target = initial;
    Vec2 scale {1.0f, 1.0f};
    bool resizeX = true, resizeY = true;
    for (std::size_t parentIndex = 0; parentIndex < candidate.containers.size(); ++parentIndex)
    {
        const auto& parent = candidate.containers[parentIndex];
        const auto edge = std::find_if(parent.children.begin(), parent.children.end(),
            [&initial](const ContainerChildPlacement& child)
            { return child.kind == ContainerChildKind::Widget && child.childId == initial.id; });
        if (edge == parent.children.end()) continue;
        const bool positioned = parent.type == ContainerType::Canvas || parent.type == ContainerType::Overlay;
        if ((!resize && !positioned) || (positioned && edge->resizeMode == ChildResizeMode::Stretch))
        { error = "The parent layout controls this widget geometry; edit its placement instead."; return false; }
        if (positioned && edge->resizeMode == ChildResizeMode::Proportional)
        {
            std::vector<ContainerLayoutNode> layouts;
            if (!EvaluateWidgetLayout(candidate.containers, candidate.widgets, layouts, error)) return false;
            const auto layout = std::find_if(layouts.begin(), layouts.end(),
                [parentIndex](const ContainerLayoutNode& node) { return node.sourceIndex == parentIndex; });
            if (layout == layouts.end()) { error = "Missing parent layout."; return false; }
            scale = {layout->viewport.size.x / parent.layoutReferenceSize.x,
                     layout->viewport.size.y / parent.layoutReferenceSize.y};
        }
        if (parent.type == ContainerType::Grid)
        {
            resizeX = edge->horizontalAlignment != ContainerAlignment::Stretch;
            resizeY = edge->verticalAlignment != ContainerAlignment::Stretch;
        }
        if (parent.type == ContainerType::Linear && parent.alignment == ContainerAlignment::Stretch)
        {
            resizeX = parent.orientation == LinearOrientation::Horizontal;
            resizeY = parent.orientation == LinearOrientation::Vertical;
        }
        break;
    }
    if (!std::isfinite(scale.x) || !std::isfinite(scale.y) || scale.x <= 0.0f || scale.y <= 0.0f)
    { error = "Cannot drag a widget inside an empty layout viewport."; return false; }
    if (resize)
    {
        const auto size = initial.size.value_or(initial.definition->preferredSize);
        const Vec2 changed {resizeX ? std::max(0.001f, size.x + 2.0f * delta.x / scale.x) : size.x,
                            resizeY ? std::max(0.001f, size.y - 2.0f * delta.y / scale.y) : size.y};
        if (changed.x != size.x || changed.y != size.y) target->size = changed;
    }
    else
    {
        target->position = {initial.position.x + delta.x / scale.x, initial.position.y + delta.y / scale.y};
    }
    if (SameGeometry(*target, *Find(page, initial.id))) return false;
    if (!ValidatePageWidgets(candidate.containers, candidate.widgets, error)) return false;
    page = std::move(candidate);
    return true;
}
bool WidgetEditorModel::ReplaceDefinition(MfdDocument& document, std::shared_ptr<const WidgetDefinition> replacement,
                                          std::string& error)
{
    if (!replacement || !ValidateWidgetDefinition(*replacement, error)) return false;
    for (const auto& definition : document.widgetLibrary)
        if (definition && definition->sourceFile != replacement->sourceFile && PageNamesEqual(definition->id, replacement->id))
        { error = "Another widget definition already uses this identifier."; return false; }
    MfdDocument candidate = document;
    bool found = false;
    for (auto& definition : candidate.widgetLibrary)
        if (definition->sourceFile == replacement->sourceFile) { definition = replacement; found = true; }
    if (!found) candidate.widgetLibrary.push_back(replacement);
    for (auto& page : candidate.pages)
    {
        for (auto& widget : page.widgets)
            if (widget.definition && widget.definition->sourceFile == replacement->sourceFile) widget.definition = replacement;
        if (!ValidatePageWidgets(page.containers, page.widgets, error)) return false;
    }
    document = std::move(candidate);
    return true;
}
std::shared_ptr<const WidgetDefinition> WidgetEditorModel::CreateDefinition(std::filesystem::path file,
                                                                           std::string id, const WidgetType type)
{
    auto definition = std::make_shared<WidgetDefinition>();
    definition->sourceFile = std::move(file);
    definition->id = std::move(id);
    definition->type = type;
    definition->defaults.text = definition->id;
    for (std::uint32_t state = 0; state < 7; ++state)
    {
        WidgetVisual visual;
        visual.state = static_cast<WidgetVisualState>(state);
        visual.composition.id = "Visual";
        Primitive background;
        background.id = "Background";
        background.type = PrimitiveType::Rectangle;
        background.geometry = RectangleGeometry {0.3f, 0.12f};
        background.style.filled = true;
        background.style.color = state == 2U ? ColorRgba {50, 140, 180, 255} : ColorRgba {24, 48, 64, 255};
        if (state == 3U) background.style.color = {30, 130, 80, 255};
        if (state == 4U) background.style.color = {45, 45, 45, 255};
        if (state == 5U) background.style.color = {180, 80, 20, 255};
        background.style.fillColor = background.style.color;
        Primitive label;
        label.id = "Text";
        label.type = PrimitiveType::Text;
        TextGeometry text;
        text.text = definition->id;
        text.fontSize = 0.04f;
        label.geometry = text;
        visual.composition.primitives = {background, label};
        visual.bindings.push_back({"Text", WidgetBindingTarget::Text});
        definition->visuals.push_back(std::move(visual));
    }
    return definition;
}
}

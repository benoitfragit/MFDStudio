/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

#include "mfd/apilevel1/ContainerLayoutEngine.h"
#include "mfd/apilevel1/internal/LayoutKernel.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>

#include "mfd/model/PageName.h"

namespace mfd::api1
{
namespace
{
using ContainerIndex = std::unordered_map<std::string, std::size_t>;
using OwnerIndex = std::unordered_map<std::string, std::string>;

bool IsFinite(const float value) noexcept
{
    return std::isfinite(value);
}

bool IsFinite(const Vec2 value) noexcept
{
    return IsFinite(value.x) && IsFinite(value.y);
}

bool IsNonNegativeFinite(const float value) noexcept
{
    return IsFinite(value) && value >= 0.0f;
}

bool HasKnownEnumValues(const ContainerDefinition& container) noexcept
{
    return static_cast<std::uint32_t>(container.type) <= static_cast<std::uint32_t>(ContainerType::Overlay) &&
           static_cast<std::uint32_t>(container.sizeMode) <=
               static_cast<std::uint32_t>(ContainerSizeMode::FitVertical) &&
           static_cast<std::uint32_t>(container.orientation) <=
               static_cast<std::uint32_t>(LinearOrientation::Vertical) &&
           static_cast<std::uint32_t>(container.alignment) <=
               static_cast<std::uint32_t>(ContainerAlignment::Stretch) &&
           static_cast<std::uint32_t>(container.scrolling.horizontal) <=
               static_cast<std::uint32_t>(ScrollMode::AlwaysVisible) &&
           static_cast<std::uint32_t>(container.scrolling.vertical) <=
               static_cast<std::uint32_t>(ScrollMode::AlwaysVisible);
}

bool HasKnownEnumValues(const ContainerChildPlacement& placement) noexcept
{
    return placement.kind == ContainerChildKind::Container &&
           static_cast<std::uint32_t>(placement.resizeMode) <=
               static_cast<std::uint32_t>(ChildResizeMode::Stretch) &&
           static_cast<std::uint32_t>(placement.horizontalAlignment) <=
               static_cast<std::uint32_t>(ContainerAlignment::Stretch) &&
           static_cast<std::uint32_t>(placement.verticalAlignment) <=
               static_cast<std::uint32_t>(ContainerAlignment::Stretch);
}

bool IsValidInsets(const ContainerInsets& value) noexcept
{
    return IsNonNegativeFinite(value.left) && IsNonNegativeFinite(value.top) &&
           IsNonNegativeFinite(value.right) && IsNonNegativeFinite(value.bottom);
}

ContainerRect InsetRect(const ContainerRect& source, const ContainerInsets& insets) noexcept
{
    const float width = std::max(0.0f, source.size.x - insets.left - insets.right);
    const float height = std::max(0.0f, source.size.y - insets.top - insets.bottom);
    return ContainerRect {
        {source.center.x + (insets.left - insets.right) * 0.5f,
         source.center.y + (insets.bottom - insets.top) * 0.5f},
        {width, height}};
}

float AlignOffset(const ContainerAlignment alignment, const float available, const float size) noexcept
{
    switch (alignment)
    {
    case ContainerAlignment::Start: return -(available - size) * 0.5f;
    case ContainerAlignment::End: return (available - size) * 0.5f;
    case ContainerAlignment::Center:
    case ContainerAlignment::Stretch: return 0.0f;
    }
    return 0.0f;
}

Vec2 ClampSize(const Vec2 requested, const Vec2 minimum) noexcept
{
    return Vec2 {std::max(requested.x, minimum.x), std::max(requested.y, minimum.y)};
}

ContainerRect ResolveCanvasChild(const ContainerDefinition& parent,
                                 const ContainerRect& content,
                                 const ContainerDefinition& child,
                                 const Vec2 preferredSize,
                                 const ContainerChildPlacement& placement) noexcept
{
    Vec2 size = ClampSize(preferredSize, child.minimumSize);
    Vec2 offset = child.geometry.center;

    if (placement.resizeMode == ChildResizeMode::Proportional &&
        parent.layoutReferenceSize.x > 0.0f && parent.layoutReferenceSize.y > 0.0f)
    {
        const Vec2 scale {
            content.size.x / parent.layoutReferenceSize.x,
            content.size.y / parent.layoutReferenceSize.y};
        size = ClampSize({size.x * scale.x, size.y * scale.y}, child.minimumSize);
        offset = {offset.x * scale.x, offset.y * scale.y};
    }
    else if (placement.resizeMode == ChildResizeMode::Stretch)
    {
        size = ClampSize(content.size, child.minimumSize);
        offset = {};
    }

    return ContainerRect {{content.center.x + offset.x, content.center.y + offset.y}, size};
}

ContainerRect ResolveLinearChild(const ContainerDefinition& parent,
                                 const ContainerRect& content,
                                 const ContainerDefinition& child,
                                 const Vec2 preferredSize,
                                 const float precedingMainExtent) noexcept
{
    Vec2 size = ClampSize(preferredSize, child.minimumSize);
    const bool horizontal = parent.orientation == LinearOrientation::Horizontal;
    const float availableMain = horizontal ? content.size.x : content.size.y;
    if (horizontal)
    {
        if (parent.alignment == ContainerAlignment::Stretch)
        {
            size.y = std::max(content.size.y, child.minimumSize.y);
        }
    }
    else
    {
        if (parent.alignment == ContainerAlignment::Stretch)
        {
            size.x = std::max(content.size.x, child.minimumSize.x);
        }
    }

    const float childMain = horizontal ? size.x : size.y;
    const float main = -availableMain * 0.5f + precedingMainExtent + childMain * 0.5f;
    const float crossAvailable = horizontal ? content.size.y : content.size.x;
    const float crossSize = horizontal ? size.y : size.x;
    const float cross = AlignOffset(parent.alignment, crossAvailable, crossSize);
    return horizontal
             ? ContainerRect {{content.center.x + main, content.center.y - cross}, size}
             : ContainerRect {{content.center.x + cross, content.center.y - main}, size};
}

ContainerRect ResolveGridChild(const ContainerDefinition& parent,
                               const ContainerRect& content,
                               const ContainerDefinition& child,
                               const Vec2 preferredSize,
                               const ContainerChildPlacement& placement) noexcept
{
    const std::uint32_t rows = std::max(1U, parent.rows);
    const std::uint32_t columns = std::max(1U, parent.columns);
    const std::uint32_t row = std::min(placement.row, rows - 1U);
    const std::uint32_t column = std::min(placement.column, columns - 1U);
    const std::uint32_t rowSpan = std::min(std::max(1U, placement.rowSpan), rows - row);
    const std::uint32_t columnSpan = std::min(std::max(1U, placement.columnSpan), columns - column);
    const float cellWidth = std::max(
        0.0f,
        (content.size.x - parent.spacing * static_cast<float>(columns - 1U)) /
            static_cast<float>(columns));
    const float cellHeight = std::max(
        0.0f,
        (content.size.y - parent.spacing * static_cast<float>(rows - 1U)) /
            static_cast<float>(rows));
    const Vec2 cellSize {
        cellWidth * static_cast<float>(columnSpan) + parent.spacing * static_cast<float>(columnSpan - 1U),
        cellHeight * static_cast<float>(rowSpan) + parent.spacing * static_cast<float>(rowSpan - 1U)};
    Vec2 size = ClampSize(preferredSize, child.minimumSize);
    if (placement.horizontalAlignment == ContainerAlignment::Stretch)
    {
        size.x = std::max(cellSize.x, child.minimumSize.x);
    }
    if (placement.verticalAlignment == ContainerAlignment::Stretch)
    {
        size.y = std::max(cellSize.y, child.minimumSize.y);
    }
    const Vec2 cellCenter {
        content.center.x - content.size.x * 0.5f +
            static_cast<float>(column) * (cellWidth + parent.spacing) + cellSize.x * 0.5f,
        content.center.y + content.size.y * 0.5f -
            static_cast<float>(row) * (cellHeight + parent.spacing) - cellSize.y * 0.5f};
    return ContainerRect {
        {cellCenter.x + AlignOffset(placement.horizontalAlignment, cellSize.x, size.x),
         cellCenter.y - AlignOffset(placement.verticalAlignment, cellSize.y, size.y)},
        size};
}

ContainerRect ResolveChildBounds(const ContainerDefinition& parent,
                                 const ContainerRect& content,
                                 const ContainerDefinition& child,
                                 const Vec2 preferredSize,
                                 const ContainerChildPlacement& placement,
                                 const float precedingLinearMainExtent) noexcept
{
    switch (parent.type)
    {
    case ContainerType::Linear:
        return ResolveLinearChild(parent, content, child, preferredSize, precedingLinearMainExtent);
    case ContainerType::Grid:
        return ResolveGridChild(parent, content, child, preferredSize, placement);
    case ContainerType::Canvas:
    case ContainerType::Overlay:
        return ResolveCanvasChild(parent, content, child, preferredSize, placement);
    }
    return ResolveCanvasChild(parent, content, child, preferredSize, placement);
}

Vec2 ContentExtent(const ContainerRect& viewport,
                   const std::vector<ContainerRect>& childBounds) noexcept
{
    const float left = viewport.center.x - viewport.size.x * 0.5f;
    const float top = viewport.center.y + viewport.size.y * 0.5f;
    float right = left + viewport.size.x;
    float bottom = top - viewport.size.y;
    for (const ContainerRect& child : childBounds)
    {
        right = std::max(right, child.center.x + child.size.x * 0.5f);
        bottom = std::min(bottom, child.center.y - child.size.y * 0.5f);
    }
    return {right - left, top - bottom};
}

float ClampScrollOffset(const float requested, const float content, const float viewport) noexcept
{
    return std::clamp(requested, 0.0f, std::max(0.0f, content - viewport));
}

bool ScrollbarVisible(const ScrollMode mode, const float content, const float viewport) noexcept
{
    return mode == ScrollMode::AlwaysVisible ||
           (mode == ScrollMode::Automatic && content > viewport + std::numeric_limits<float>::epsilon());
}

ScrollbarGeometry ResolveScrollbar(const bool horizontal,
                                   const ContainerRect& viewport,
                                   const Vec2 contentSize,
                                   const Vec2 scrollOffset,
                                   const ContainerScrolling& scrolling) noexcept
{
    ScrollbarGeometry result;
    const ScrollMode mode = horizontal ? scrolling.horizontal : scrolling.vertical;
    const float content = horizontal ? contentSize.x : contentSize.y;
    const float available = horizontal ? viewport.size.x : viewport.size.y;
    result.visible = ScrollbarVisible(mode, content, available);
    if (!result.visible || available <= 0.0f)
    {
        return result;
    }

    const ScrollbarAppearance& appearance = scrolling.appearance;
    const float thickness = std::min(appearance.thickness, horizontal ? viewport.size.y : viewport.size.x);
    if (horizontal)
    {
        const float trackWidth = std::max(0.0f, viewport.size.x - appearance.margins.left - appearance.margins.right);
        result.track = {{viewport.center.x + (appearance.margins.left - appearance.margins.right) * 0.5f,
                         viewport.center.y - viewport.size.y * 0.5f + appearance.margins.bottom + thickness * 0.5f},
                        {trackWidth, thickness}};
        const float thumbWidth = std::clamp(
            content > 0.0f ? trackWidth * available / content : trackWidth,
            std::min(appearance.minimumThumbSize, trackWidth),
            trackWidth);
        const float travel = std::max(0.0f, trackWidth - thumbWidth);
        const float ratio = content > available ? scrollOffset.x / (content - available) : 0.0f;
        result.thumb = {{result.track.center.x - travel * 0.5f + travel * ratio, result.track.center.y},
                        {thumbWidth, thickness}};
    }
    else
    {
        const float trackHeight = std::max(0.0f, viewport.size.y - appearance.margins.top - appearance.margins.bottom);
        result.track = {{viewport.center.x + viewport.size.x * 0.5f - appearance.margins.right - thickness * 0.5f,
                         viewport.center.y + (appearance.margins.bottom - appearance.margins.top) * 0.5f},
                        {thickness, trackHeight}};
        const float thumbHeight = std::clamp(
            content > 0.0f ? trackHeight * available / content : trackHeight,
            std::min(appearance.minimumThumbSize, trackHeight),
            trackHeight);
        const float travel = std::max(0.0f, trackHeight - thumbHeight);
        const float ratio = content > available ? scrollOffset.y / (content - available) : 0.0f;
        result.thumb = {{result.track.center.x, result.track.center.y + travel * 0.5f - travel * ratio},
                        {thickness, thumbHeight}};
    }
    return result;
}

bool ValidateDefinition(const ContainerDefinition& container, std::string& error)
{
    if (PageNameIsBlank(container.id))
    {
        error = "Container id cannot be empty.";
        return false;
    }
    if (!HasKnownEnumValues(container))
    {
        error = "Container '" + container.id + "' contains an unsupported enum value.";
        return false;
    }
    if (!IsFinite(container.geometry.center) || !IsFinite(container.geometry.size) ||
        !IsFinite(container.minimumSize) || !IsFinite(container.layoutReferenceSize) ||
        container.geometry.size.x < 0.0f || container.geometry.size.y < 0.0f ||
        container.minimumSize.x < 0.0f || container.minimumSize.y < 0.0f ||
        container.layoutReferenceSize.x <= 0.0f || container.layoutReferenceSize.y <= 0.0f)
    {
        error = "Container '" + container.id + "' has invalid or non-finite geometry.";
        return false;
    }
    if (!IsValidInsets(container.padding) || !IsNonNegativeFinite(container.spacing) ||
        !IsNonNegativeFinite(container.appearance.borderThickness) ||
        !IsNonNegativeFinite(container.appearance.cornerRadius) ||
        !IsFinite(container.scrolling.offset) || container.scrolling.offset.x < 0.0f ||
        container.scrolling.offset.y < 0.0f ||
        !IsNonNegativeFinite(container.scrolling.appearance.thickness) ||
        !IsNonNegativeFinite(container.scrolling.appearance.cornerRadius) ||
        !IsNonNegativeFinite(container.scrolling.appearance.minimumThumbSize) ||
        !IsValidInsets(container.scrolling.appearance.margins))
    {
        error = "Container '" + container.id + "' has invalid layout or scrolling values.";
        return false;
    }
    if (container.rows == 0U || container.columns == 0U ||
        container.rows > kMaximumContainerCount || container.columns > kMaximumContainerCount)
    {
        error = "Container '" + container.id + "' grid dimensions must be between 1 and 4096.";
        return false;
    }
    return true;
}

enum class VisitState : std::uint8_t
{
    Unvisited,
    Visiting,
    Visited
};

class HierarchyValidator final
{
public:
    HierarchyValidator(const std::vector<ContainerDefinition>& containers,
                       const ContainerIndex& index,
                       std::size_t maximumDepth,
                       std::string& error)
        : containers_(containers), index_(index), error_(error), maximumDepth_(maximumDepth), states_(containers.size(), VisitState::Unvisited),
          subtreeDepths_(containers.size(), 1U)
    {
    }

    bool ValidateAll()
    {
        for (std::size_t position = 0U; position < containers_.size(); ++position)
        {
            if (states_[position] == VisitState::Unvisited && !Visit(position, 1U))
            {
                return false;
            }
        }
        return true;
    }

private:
    bool Visit(const std::size_t position, const std::size_t depth)
    {
        if (depth > maximumDepth_)
        {
            error_ = "Container hierarchy exceeds the supported depth of " +
                     std::to_string(maximumDepth_) + ".";
            return false;
        }
        if (states_[position] == VisitState::Visiting)
        {
            error_ = "Container hierarchy contains a cycle at '" + containers_[position].id + "'.";
            return false;
        }
        if (states_[position] == VisitState::Visited)
        {
            return true;
        }

        states_[position] = VisitState::Visiting;
        for (const ContainerChildPlacement& child : containers_[position].children)
        {
            const std::size_t childIndex = index_.at(child.childId);
            if (!Visit(childIndex, depth + 1U))
            {
                return false;
            }
            subtreeDepths_[position] = std::max(subtreeDepths_[position], subtreeDepths_[childIndex] + 1U);
            if (subtreeDepths_[position] > maximumDepth_)
            {
                error_ = "Container hierarchy exceeds the supported depth.";
                return false;
            }
        }
        states_[position] = VisitState::Visited;
        return true;
    }

    const std::vector<ContainerDefinition>& containers_;
    const ContainerIndex& index_;
    std::string& error_;
    std::size_t maximumDepth_;
    std::vector<VisitState> states_;
    std::vector<std::size_t> subtreeDepths_;
};

/** @brief Resolves bottom-up preferred sizes before the top-down placement pass. */
class IntrinsicSizeResolver final
{
public:
    IntrinsicSizeResolver(const std::vector<ContainerDefinition>& containers,
                          const ContainerIndex& index)
        : containers_(containers), index_(index), sizes_(containers.size()), resolved_(containers.size(), false)
    {
    }

    const std::vector<Vec2>& ResolveAll()
    {
        for (std::size_t position = 0U; position < containers_.size(); ++position)
        {
            Resolve(position);
        }
        return sizes_;
    }

private:
    Vec2 Resolve(const std::size_t position)
    {
        if (resolved_[position])
        {
            return sizes_[position];
        }

        const ContainerDefinition& container = containers_[position];
        std::vector<Vec2> childSizes;
        childSizes.reserve(container.children.size());
        for (const ContainerChildPlacement& placement : container.children)
        {
            childSizes.push_back(Resolve(index_.at(placement.childId)));
        }

        const Vec2 contentSize = ResolveNaturalContentSize(container, childSizes);
        const Vec2 fittedSize {
            contentSize.x + container.padding.left + container.padding.right,
            contentSize.y + container.padding.top + container.padding.bottom};
        Vec2 preferredSize = ClampSize(container.geometry.size, container.minimumSize);
        switch (container.sizeMode)
        {
        case ContainerSizeMode::FitContent:
            preferredSize = ClampSize(fittedSize, container.minimumSize);
            break;
        case ContainerSizeMode::FitHorizontal:
            preferredSize.x = std::max(fittedSize.x, container.minimumSize.x);
            break;
        case ContainerSizeMode::FitVertical:
            preferredSize.y = std::max(fittedSize.y, container.minimumSize.y);
            break;
        case ContainerSizeMode::Fixed:
            break;
        }

        sizes_[position] = preferredSize;
        resolved_[position] = true;
        return preferredSize;
    }

    Vec2 ResolveNaturalContentSize(const ContainerDefinition& container,
                                   const std::vector<Vec2>& childSizes) const
    {
        switch (container.type)
        {
        case ContainerType::Linear:
            return ResolveLinearContentSize(container, childSizes);
        case ContainerType::Grid:
            return ResolveGridContentSize(container, childSizes);
        case ContainerType::Canvas:
        case ContainerType::Overlay:
            return ResolveCanvasContentSize(container, childSizes);
        }
        return {};
    }

    Vec2 ResolveCanvasContentSize(const ContainerDefinition& container,
                                  const std::vector<Vec2>& childSizes) const noexcept
    {
        Vec2 result {};
        for (std::size_t position = 0U; position < container.children.size(); ++position)
        {
            const ContainerDefinition& child = containers_[index_.at(container.children[position].childId)];
            result.x = std::max(result.x, 2.0f * std::abs(child.geometry.center.x) + childSizes[position].x);
            result.y = std::max(result.y, 2.0f * std::abs(child.geometry.center.y) + childSizes[position].y);
        }
        return result;
    }

    Vec2 ResolveLinearContentSize(const ContainerDefinition& container,
                                  const std::vector<Vec2>& childSizes) const noexcept
    {
        Vec2 result {};
        const bool horizontal = container.orientation == LinearOrientation::Horizontal;
        for (const Vec2 childSize : childSizes)
        {
            if (horizontal)
            {
                result.x += childSize.x;
                result.y = std::max(result.y, childSize.y);
            }
            else
            {
                result.x = std::max(result.x, childSize.x);
                result.y += childSize.y;
            }
        }
        if (childSizes.size() > 1U)
        {
            const float totalSpacing = container.spacing * static_cast<float>(childSizes.size() - 1U);
            if (horizontal) result.x += totalSpacing;
            else result.y += totalSpacing;
        }
        return result;
    }

    Vec2 ResolveGridContentSize(const ContainerDefinition& container,
                                const std::vector<Vec2>& childSizes) const
    {
        const std::uint32_t rowCount = std::max(1U, container.rows);
        const std::uint32_t columnCount = std::max(1U, container.columns);
        float rowHeight = 0.0f;
        float columnWidth = 0.0f;
        for (std::size_t position = 0U; position < container.children.size(); ++position)
        {
            const ContainerChildPlacement& placement = container.children[position];
            const std::uint32_t row = std::min(placement.row, rowCount - 1U);
            const std::uint32_t column = std::min(placement.column, columnCount - 1U);
            const std::uint32_t rowSpan = std::min(std::max(1U, placement.rowSpan), rowCount - row);
            const std::uint32_t columnSpan =
                std::min(std::max(1U, placement.columnSpan), columnCount - column);
            const float widthPerColumn = std::max(
                0.0f,
                (childSizes[position].x - container.spacing * static_cast<float>(columnSpan - 1U)) /
                    static_cast<float>(columnSpan));
            const float heightPerRow = std::max(
                0.0f,
                (childSizes[position].y - container.spacing * static_cast<float>(rowSpan - 1U)) /
                    static_cast<float>(rowSpan));
            columnWidth = std::max(columnWidth, widthPerColumn);
            rowHeight = std::max(rowHeight, heightPerRow);
        }

        Vec2 result {columnWidth * static_cast<float>(columnCount), rowHeight * static_cast<float>(rowCount)};
        result.x += container.spacing * static_cast<float>(columnCount - 1U);
        result.y += container.spacing * static_cast<float>(rowCount - 1U);
        return result;
    }

    const std::vector<ContainerDefinition>& containers_;
    const ContainerIndex& index_;
    std::vector<Vec2> sizes_;
    std::vector<std::uint8_t> resolved_;
};

class ChildRenderOrderLess final
{
public:
    explicit ChildRenderOrderLess(const std::vector<ContainerChildPlacement>& children) noexcept
        : children_(children)
    {
    }

    bool operator()(const std::size_t lhs, const std::size_t rhs) const noexcept
    {
        return children_[lhs].renderOrder < children_[rhs].renderOrder;
    }

private:
    const std::vector<ContainerChildPlacement>& children_;
};

class LayoutEvaluator final
{
public:
    LayoutEvaluator(const std::vector<ContainerDefinition>& containers,
                    const ContainerIndex& index,
                    const std::vector<Vec2>& preferredSizes,
                    std::vector<ContainerLayoutNode>& output)
        : containers_(containers), index_(index), preferredSizes_(preferredSizes), output_(output)
    {
    }

    void EvaluateRoot(const ContainerDefinition& container)
    {
        EvaluateNode(container,
                     ContainerRect {container.geometry.center, preferredSizes_[index_.at(container.id)]},
                     {},
                     true,
                     true,
                     0U,
                     0);
    }

private:
    void EvaluateNode(const ContainerDefinition& container,
                      const ContainerRect& bounds,
                      const std::string_view parentId,
                      const bool ancestorVisible,
                      const bool ancestorEnabled,
                      const std::size_t depth,
                      const int renderOrder)
    {
        ContainerLayoutNode node;
        node.sourceIndex = index_.at(container.id);
        node.parentSourceIndex = parentId.empty()
                                     ? std::numeric_limits<std::size_t>::max()
                                     : index_.at(std::string(parentId));
        node.id = container.id;
        node.parentId = std::string(parentId);
        node.bounds = bounds;
        node.viewport = InsetRect(bounds, container.padding);
        node.effectiveVisible = ancestorVisible && container.visible;
        node.effectiveEnabled = ancestorEnabled && container.enabled;
        node.depth = depth;
        node.renderOrder = renderOrder;

        std::vector<ContainerRect> childBounds;
        childBounds.reserve(container.children.size());
        float precedingLinearMainExtent = 0.0f;
        for (std::size_t childPosition = 0U; childPosition < container.children.size(); ++childPosition)
        {
            const ContainerChildPlacement& placement = container.children[childPosition];
            const std::size_t childIndex = index_.at(placement.childId);
            const ContainerDefinition& child = containers_[childIndex];
            childBounds.push_back(ResolveChildBounds(
                container,
                node.viewport,
                child,
                preferredSizes_[childIndex],
                placement,
                precedingLinearMainExtent));
            if (container.type == ContainerType::Linear)
            {
                const Vec2 childSize = childBounds.back().size;
                precedingLinearMainExtent +=
                    (container.orientation == LinearOrientation::Horizontal ? childSize.x : childSize.y) +
                    container.spacing;
            }
        }
        node.contentSize = ContentExtent(node.viewport, childBounds);
        node.scrollOffset = {
            container.scrolling.horizontal == ScrollMode::Disabled
                ? 0.0f
                : ClampScrollOffset(container.scrolling.offset.x, node.contentSize.x, node.viewport.size.x),
            container.scrolling.vertical == ScrollMode::Disabled
                ? 0.0f
                : ClampScrollOffset(container.scrolling.offset.y, node.contentSize.y, node.viewport.size.y)};
        node.horizontalScrollbar = ResolveScrollbar(
            true, node.viewport, node.contentSize, node.scrollOffset, container.scrolling);
        node.verticalScrollbar = ResolveScrollbar(
            false, node.viewport, node.contentSize, node.scrollOffset, container.scrolling);
        output_.push_back(node);

        const Vec2 scrollTranslation {-node.scrollOffset.x, node.scrollOffset.y};
        std::vector<std::size_t> order(container.children.size());
        for (std::size_t position = 0U; position < order.size(); ++position)
        {
            order[position] = position;
        }
        if (container.type == ContainerType::Overlay)
        {
            std::stable_sort(order.begin(), order.end(), ChildRenderOrderLess(container.children));
        }
        for (const std::size_t childPosition : order)
        {
            const ContainerChildPlacement& placement = container.children[childPosition];
            const ContainerDefinition& child = containers_[index_.at(placement.childId)];
            ContainerRect translated = childBounds[childPosition];
            translated.center.x += scrollTranslation.x;
            translated.center.y += scrollTranslation.y;
            EvaluateNode(child,
                         translated,
                         container.id,
                         node.effectiveVisible,
                         node.effectiveEnabled,
                         depth + 1U,
                         placement.renderOrder);
        }
    }

    const std::vector<ContainerDefinition>& containers_;
    const ContainerIndex& index_;
    const std::vector<Vec2>& preferredSizes_;
    std::vector<ContainerLayoutNode>& output_;
};
} // namespace

const ContainerDefinition* FindContainer(const std::vector<ContainerDefinition>& containers,
                                         const std::string_view id) noexcept
{
    const auto iterator = std::find_if(
        containers.begin(), containers.end(), [id](const ContainerDefinition& value) { return value.id == id; });
    return iterator == containers.end() ? nullptr : &*iterator;
}

ContainerDefinition* FindContainer(std::vector<ContainerDefinition>& containers,
                                   const std::string_view id) noexcept
{
    const auto iterator = std::find_if(
        containers.begin(), containers.end(), [id](const ContainerDefinition& value) { return value.id == id; });
    return iterator == containers.end() ? nullptr : &*iterator;
}

bool detail::ValidateLayoutNodes(const std::vector<ContainerDefinition>& containers,
                                 const std::size_t maximumCount, const std::size_t maximumDepth,
                                 std::string& error)
{
    error.clear();
    if (containers.size() > maximumCount)
    {
        error = "Container count exceeds the supported limit of " + std::to_string(maximumCount) + ".";
        return false;
    }

    ContainerIndex index;
    std::unordered_set<std::string> normalizedIds;
    index.reserve(containers.size());
    normalizedIds.reserve(containers.size());
    for (std::size_t position = 0U; position < containers.size(); ++position)
    {
        const ContainerDefinition& container = containers[position];
        if (!ValidateDefinition(container, error))
        {
            return false;
        }
        if (!index.emplace(container.id, position).second)
        {
            error = "Duplicate container id '" + container.id + "'.";
            return false;
        }
        if (!normalizedIds.insert(NormalizePageName(container.id)).second)
        {
            error = "Duplicate normalized container id '" + container.id + "'.";
            return false;
        }
    }

    OwnerIndex owners;
    owners.reserve(containers.size());
    for (const ContainerDefinition& parent : containers)
    {
        std::unordered_set<std::string> localChildren;
        localChildren.reserve(parent.children.size());
        for (const ContainerChildPlacement& placement : parent.children)
        {
            if (!HasKnownEnumValues(placement))
            {
                error = "Container '" + parent.id + "' contains an unsupported child placement enum.";
                return false;
            }
            if (placement.childId.empty() || index.find(placement.childId) == index.end())
            {
                error = "Container '" + parent.id + "' references unknown child '" + placement.childId + "'.";
                return false;
            }
            if (placement.childId == parent.id)
            {
                error = "Container '" + parent.id + "' cannot own itself.";
                return false;
            }
            if (!localChildren.insert(placement.childId).second)
            {
                error = "Container '" + parent.id + "' owns child '" + placement.childId + "' more than once.";
                return false;
            }
            if (!owners.emplace(placement.childId, parent.id).second)
            {
                error = "Container '" + placement.childId + "' has more than one parent.";
                return false;
            }
            if (placement.rowSpan == 0U || placement.columnSpan == 0U)
            {
                error = "Container child placement spans must be positive for '" + placement.childId + "'.";
                return false;
            }
        }
    }

    HierarchyValidator validator(containers, index, maximumDepth, error);
    return validator.ValidateAll();
}

bool detail::EvaluateLayoutNodes(const std::vector<ContainerDefinition>& containers,
                                 const std::size_t maximumCount, const std::size_t maximumDepth,
                                 std::vector<ContainerLayoutNode>& output, std::string& error)
{
    output.clear();
    if (!ValidateLayoutNodes(containers, maximumCount, maximumDepth, error))
    {
        return false;
    }

    ContainerIndex index;
    OwnerIndex owners;
    index.reserve(containers.size());
    owners.reserve(containers.size());
    for (std::size_t position = 0U; position < containers.size(); ++position)
    {
        index.emplace(containers[position].id, position);
        for (const ContainerChildPlacement& child : containers[position].children)
        {
            owners.emplace(child.childId, containers[position].id);
        }
    }

    output.reserve(containers.size());
    IntrinsicSizeResolver sizeResolver(containers, index);
    const std::vector<Vec2>& preferredSizes = sizeResolver.ResolveAll();
    LayoutEvaluator evaluator(containers, index, preferredSizes, output);

    for (const ContainerDefinition& container : containers)
    {
        if (owners.find(container.id) == owners.end())
        {
            evaluator.EvaluateRoot(container);
        }
    }
    for (const ContainerLayoutNode& node : output)
    {
        if (!IsFinite(node.bounds.center) || !IsFinite(node.bounds.size) ||
            !IsFinite(node.viewport.center) || !IsFinite(node.viewport.size) ||
            !IsFinite(node.contentSize) || !IsFinite(node.scrollOffset) ||
            !IsFinite(node.horizontalScrollbar.track.center) || !IsFinite(node.horizontalScrollbar.track.size) ||
            !IsFinite(node.horizontalScrollbar.thumb.center) || !IsFinite(node.horizontalScrollbar.thumb.size) ||
            !IsFinite(node.verticalScrollbar.track.center) || !IsFinite(node.verticalScrollbar.track.size) ||
            !IsFinite(node.verticalScrollbar.thumb.center) || !IsFinite(node.verticalScrollbar.thumb.size))
        {
            error = "Container layout arithmetic produced non-finite geometry.";
            output.clear();
            return false;
        }
    }
    return true;
}
bool ContainerLayoutEngine::Validate(const std::vector<ContainerDefinition>& containers, std::string& error) const
{
    return detail::ValidateLayoutNodes(containers, kMaximumContainerCount, kMaximumContainerDepth, error);
}
bool ContainerLayoutEngine::Evaluate(const std::vector<ContainerDefinition>& containers,
                                    std::vector<ContainerLayoutNode>& output, std::string& error) const
{
    return detail::EvaluateLayoutNodes(containers, kMaximumContainerCount, kMaximumContainerDepth, output, error);
}
} // namespace mfd::api1

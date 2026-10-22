/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

#include "mfd/apilevel1/ContainerInputRouter.h"
#include "ContainerPointerQuery.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace mfd::api1
{
namespace
{
bool Contains(const ContainerRect& rectangle, const Vec2 point) noexcept
{
    return point.x >= rectangle.center.x - rectangle.size.x * 0.5f &&
           point.x <= rectangle.center.x + rectangle.size.x * 0.5f &&
           point.y >= rectangle.center.y - rectangle.size.y * 0.5f &&
           point.y <= rectangle.center.y + rectangle.size.y * 0.5f;
}

float MoveOffset(const float current,
                 const float wheelDelta,
                 const float logicalStep,
                 const float maximum) noexcept
{
    return std::clamp(current - wheelDelta * logicalStep, 0.0f, maximum);
}
} // namespace

detail::ContainerPointerQuery::ContainerPointerQuery(
    const std::vector<ContainerDefinition>& definitions,
    const std::vector<ContainerLayoutNode>& layouts, const Vec2 pointer) noexcept
    : definitions_(definitions), pointer_(pointer)
{
    if (definitions.size() > bySource_.size() || layouts.size() > bySource_.size() ||
        !std::isfinite(pointer.x) || !std::isfinite(pointer.y)) return;
    for (const ContainerLayoutNode& layout : layouts)
    {
        if (layout.sourceIndex >= definitions.size() || bySource_[layout.sourceIndex] != nullptr) return;
        bySource_[layout.sourceIndex] = &layout;
    }
    valid_ = true;
}

bool detail::ContainerPointerQuery::CanReceive(const ContainerLayoutNode& layout) const noexcept
{
    return layout.effectiveEnabled && VisibleAt(layout);
}
bool detail::ContainerPointerQuery::VisibleAt(const ContainerLayoutNode& layout, const bool testOwnBounds) const noexcept
{
    if (!valid_ || layout.sourceIndex >= definitions_.size() ||
        !layout.effectiveVisible || (testOwnBounds && !Contains(layout.viewport, pointer_))) return false;
    const ContainerLayoutNode* current = &layout;
    for (std::size_t depth = 0U; current != nullptr && depth <= kMaximumContainerDepth + 1U; ++depth)
    {
        if (current->sourceIndex >= definitions_.size()) return false;
        if (definitions_[current->sourceIndex].scrolling.clipContent &&
            !Contains(current->viewport, pointer_)) return false;
        if (current->parentSourceIndex == std::numeric_limits<std::size_t>::max()) return true;
        if (current->parentSourceIndex >= definitions_.size()) return false;
        current = bySource_[current->parentSourceIndex];
    }
    return false;
}

bool ContainerInputRouter::CanReceivePointer(const std::vector<ContainerDefinition>& definitions,
                                             const std::vector<ContainerLayoutNode>& layouts,
                                             const ContainerLayoutNode& layout,
                                             const Vec2 pointer) const noexcept
{
    return detail::ContainerPointerQuery(definitions, layouts, pointer).CanReceive(layout);
}

bool ContainerInputRouter::RouteWheel(const std::vector<ContainerDefinition>& definitions,
                                      const std::vector<ContainerLayoutNode>& layouts,
                                      const Vec2 pointer,
                                      const Vec2 wheelDelta,
                                      const float logicalStep,
                                      ContainerScrollRequest& request) const noexcept
{
    if (!std::isfinite(pointer.x) || !std::isfinite(pointer.y) ||
        !std::isfinite(wheelDelta.x) || !std::isfinite(wheelDelta.y) ||
        !std::isfinite(logicalStep) || logicalStep <= 0.0f)
    {
        return false;
    }

    const bool useVertical = std::abs(wheelDelta.y) >= std::abs(wheelDelta.x);
    const float selectedDelta = useVertical ? wheelDelta.y : wheelDelta.x;
    if (std::abs(selectedDelta) <= std::numeric_limits<float>::epsilon())
    {
        return false;
    }

    const detail::ContainerPointerQuery pointerQuery(definitions, layouts, pointer);
    for (auto iterator = layouts.rbegin(); iterator != layouts.rend(); ++iterator)
    {
        const ContainerLayoutNode& layout = *iterator;
        if (!pointerQuery.CanReceive(layout))
        {
            continue;
        }

        const ContainerDefinition& definition = definitions[layout.sourceIndex];
        const ScrollMode mode = useVertical ? definition.scrolling.vertical : definition.scrolling.horizontal;
        const float viewportExtent = useVertical ? layout.viewport.size.y : layout.viewport.size.x;
        const float contentExtent = useVertical ? layout.contentSize.y : layout.contentSize.x;
        const float maximum = std::max(0.0f, contentExtent - viewportExtent);
        if (mode == ScrollMode::Disabled || maximum <= 0.0f)
        {
            continue;
        }

        request.sourceIndex = layout.sourceIndex;
        request.offset = layout.scrollOffset;
        const float current = useVertical ? request.offset.y : request.offset.x;
        const float updated = MoveOffset(current, selectedDelta, logicalStep, maximum);
        if (updated == current)
        {
            continue;
        }
        if (useVertical) request.offset.y = updated;
        else request.offset.x = updated;
        return true;
    }
    return false;
}

float ContainerInputRouter::OffsetForThumb(const ContainerLayoutNode& layout,
                                           const bool horizontal,
                                           const float thumbCenterCoordinate) const noexcept
{
    if (!std::isfinite(thumbCenterCoordinate))
    {
        return horizontal ? layout.scrollOffset.x : layout.scrollOffset.y;
    }
    const ScrollbarGeometry& scrollbar = horizontal ? layout.horizontalScrollbar : layout.verticalScrollbar;
    const float trackExtent = horizontal ? scrollbar.track.size.x : scrollbar.track.size.y;
    const float thumbExtent = horizontal ? scrollbar.thumb.size.x : scrollbar.thumb.size.y;
    const float travel = std::max(0.0f, trackExtent - thumbExtent);
    const float maximum = std::max(
        0.0f,
        (horizontal ? layout.contentSize.x : layout.contentSize.y) -
            (horizontal ? layout.viewport.size.x : layout.viewport.size.y));
    if (!scrollbar.visible || travel <= 0.0f || maximum <= 0.0f)
    {
        return horizontal ? layout.scrollOffset.x : layout.scrollOffset.y;
    }

    const float trackCenter = horizontal ? scrollbar.track.center.x : scrollbar.track.center.y;
    const float start = horizontal ? trackCenter - travel * 0.5f : trackCenter + travel * 0.5f;
    const float directedDistance = horizontal ? thumbCenterCoordinate - start : start - thumbCenterCoordinate;
    return std::clamp(directedDistance / travel, 0.0f, 1.0f) * maximum;
}
} // namespace mfd::api1

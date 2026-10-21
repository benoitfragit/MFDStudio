/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

#include "mfd/apilevel1/ContainerInputRouter.h"

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

const ContainerLayoutNode* FindLayoutBySourceIndex(const std::vector<ContainerLayoutNode>& layouts,
                                                   const std::size_t sourceIndex) noexcept
{
    for (const ContainerLayoutNode& layout : layouts)
    {
        if (layout.sourceIndex == sourceIndex)
        {
            return &layout;
        }
    }
    return nullptr;
}

bool IsInsideEffectiveClip(const std::vector<ContainerDefinition>& definitions,
                           const std::vector<ContainerLayoutNode>& layouts,
                           const ContainerLayoutNode& layout,
                           const Vec2 point) noexcept
{
    const ContainerLayoutNode* current = &layout;
    std::size_t remainingDepth = kMaximumContainerDepth + 1U;
    while (current != nullptr && remainingDepth > 0U)
    {
        if (current->sourceIndex >= definitions.size())
        {
            return false;
        }
        if (definitions[current->sourceIndex].scrolling.clipContent && !Contains(current->viewport, point))
        {
            return false;
        }
        if (current->parentSourceIndex == std::numeric_limits<std::size_t>::max())
        {
            return true;
        }
        current = FindLayoutBySourceIndex(layouts, current->parentSourceIndex);
        --remainingDepth;
    }
    return false;
}

float MoveOffset(const float current,
                 const float wheelDelta,
                 const float logicalStep,
                 const float maximum) noexcept
{
    return std::clamp(current - wheelDelta * logicalStep, 0.0f, maximum);
}
} // namespace

bool ContainerInputRouter::CanReceivePointer(const std::vector<ContainerDefinition>& definitions,
                                             const std::vector<ContainerLayoutNode>& layouts,
                                             const ContainerLayoutNode& layout,
                                             const Vec2 pointer) const noexcept
{
    return std::isfinite(pointer.x) && std::isfinite(pointer.y) &&
           layout.sourceIndex < definitions.size() && layout.effectiveVisible && layout.effectiveEnabled &&
           Contains(layout.viewport, pointer) && IsInsideEffectiveClip(definitions, layouts, layout, pointer);
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

    for (auto iterator = layouts.rbegin(); iterator != layouts.rend(); ++iterator)
    {
        const ContainerLayoutNode& layout = *iterator;
        if (!CanReceivePointer(definitions, layouts, layout, pointer))
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

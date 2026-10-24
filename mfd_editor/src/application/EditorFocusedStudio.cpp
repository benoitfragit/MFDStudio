/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "internal/application/EditorFocusedStudio.h"

/**
 * @file
 * @brief Implementation of pure focused-studio framing and hierarchy helpers.
 */

#include <algorithm>
#include <cmath>
#include <limits>

namespace editor::app
{
mfd::PageViewState ComputeFocusedStudioView(const mfd::api1::ContainerRect& bounds,
                                             const float viewportWidth,
                                             const float viewportHeight,
                                             const float padding) noexcept
{
    mfd::PageViewState view {};
    if (!std::isfinite(viewportWidth) || !std::isfinite(viewportHeight) ||
        viewportWidth <= 0.0f || viewportHeight <= 0.0f ||
        !std::isfinite(bounds.center.x) || !std::isfinite(bounds.center.y) ||
        !std::isfinite(bounds.size.x) || !std::isfinite(bounds.size.y) ||
        bounds.size.x <= 0.0f || bounds.size.y <= 0.0f)
    {
        return view;
    }

    const float minimumViewportExtent = std::min(viewportWidth, viewportHeight);
    const float visibleHalfWidth = viewportWidth / minimumViewportExtent;
    const float visibleHalfHeight = viewportHeight / minimumViewportExtent;
    const float halfWidth = std::max(0.00001f, bounds.size.x * 0.5f);
    const float halfHeight = std::max(0.00001f, bounds.size.y * 0.5f);
    const float safePadding = std::isfinite(padding) ? std::clamp(padding, 0.1f, 1.0f) : 0.78f;

    view.center = bounds.center;
    view.zoom = std::clamp(
        std::min(visibleHalfWidth / halfWidth, visibleHalfHeight / halfHeight) * safePadding,
        0.1f,
        20.0f);
    return view;
}

const mfd::api1::ContainerLayoutNode* FindFocusedStudioLayout(
    const std::vector<mfd::api1::ContainerLayoutNode>& layouts,
    const std::size_t sourceIndex) noexcept
{
    const auto found = std::find_if(
        layouts.begin(),
        layouts.end(),
        [sourceIndex](const mfd::api1::ContainerLayoutNode& node)
        {
            return node.sourceIndex == sourceIndex;
        });
    return found == layouts.end() ? nullptr : &(*found);
}

std::vector<mfd::api1::ContainerLayoutNode> CollectFocusedContainerLayouts(
    const std::vector<mfd::api1::ContainerLayoutNode>& layouts,
    const std::size_t rootSourceIndex)
{
    constexpr std::size_t kMaximumFocusedNodeCount =
        mfd::api1::kMaximumContainerCount + mfd::api1::kMaximumWidgetCount;
    if (layouts.size() > kMaximumFocusedNodeCount)
    {
        return {};
    }

    const auto root = std::find_if(
        layouts.begin(),
        layouts.end(),
        [rootSourceIndex](const mfd::api1::ContainerLayoutNode& node)
        {
            return node.sourceIndex == rootSourceIndex;
        });
    if (root == layouts.end())
    {
        return {};
    }

    std::vector<mfd::api1::ContainerLayoutNode> focused;
    focused.reserve(static_cast<std::size_t>(layouts.end() - root));
    const std::size_t rootDepth = root->depth;
    for (auto current = root; current != layouts.end(); ++current)
    {
        if (current != root && current->depth <= rootDepth)
        {
            break;
        }
        focused.push_back(*current);
    }

    focused.front().parentSourceIndex = std::numeric_limits<std::size_t>::max();
    return focused;
}
} // namespace editor::app

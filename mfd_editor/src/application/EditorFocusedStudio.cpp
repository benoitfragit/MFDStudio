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

#include "mfd/apilevel1/widgets/WidgetModel.h"
#include "mfd/apilevel1/widgets/WidgetVisuals.h"

namespace editor::app
{
bool FocusedStudioLayoutCache::Resolve(const mfd::PageDefinition& page,
                                       const std::size_t sourceIndex,
                                       const bool includeSubtree)
{
    const bool keyMatches = resolved_ && pageIdentity_ == &page && sourceIndex_ == sourceIndex &&
                            includeSubtree_ == includeSubtree;
    if (keyMatches)
    {
        return valid_;
    }

    pageIdentity_ = &page;
    sourceIndex_ = sourceIndex;
    includeSubtree_ = includeSubtree;
    resolved_ = true;
    valid_ = false;
    projectedNodes_.clear();
    layouts_.clear();
    widgetVisuals_.clear();

    std::vector<mfd::api1::ContainerLayoutNode> pageLayouts;
    std::string layoutError;
    if (!mfd::api1::EvaluateWidgetLayout(page.containers, page.widgets, pageLayouts, layoutError))
    {
        return false;
    }

    const mfd::api1::ContainerLayoutNode* selected = FindFocusedStudioLayout(pageLayouts, sourceIndex);
    if (selected == nullptr)
    {
        return false;
    }

    if (!includeSubtree)
    {
        layouts_.push_back(*selected);
        valid_ = true;
        return true;
    }

    projectedNodes_ = mfd::api1::ProjectWidgetLayoutNodes(page.containers, page.widgets);
    layouts_ = CollectFocusedContainerLayouts(pageLayouts, sourceIndex);
    if (layouts_.empty() || projectedNodes_.empty())
    {
        return false;
    }

    widgetVisuals_.resize(page.widgets.size());
    for (const mfd::api1::ContainerLayoutNode& layout : layouts_)
    {
        if (layout.sourceIndex < page.containers.size())
        {
            continue;
        }
        const std::size_t widgetIndex = layout.sourceIndex - page.containers.size();
        if (widgetIndex >= page.widgets.size())
        {
            continue;
        }

        mfd::api1::WidgetState state = mfd::api1::InitialWidgetState(page.widgets[widgetIndex]);
        state.visible = layout.effectiveVisible;
        state.enabled = layout.effectiveEnabled;
        widgetVisuals_[widgetIndex] =
            mfd::api1::BuildWidgetVisual(page.widgets[widgetIndex], state, layout.bounds);
    }

    valid_ = true;
    return true;
}

void FocusedStudioLayoutCache::Invalidate() noexcept
{
    resolved_ = false;
    valid_ = false;
}

bool FocusedStudioLayoutCache::IsValid() const noexcept
{
    return valid_;
}

const std::vector<mfd::api1::ContainerLayoutNode>& FocusedStudioLayoutCache::Layouts() const noexcept
{
    return layouts_;
}

const std::vector<mfd::api1::ContainerDefinition>& FocusedStudioLayoutCache::ProjectedNodes() const noexcept
{
    return projectedNodes_;
}

const mfd::ReticleGroup* FocusedStudioLayoutCache::FindWidgetVisual(
    const std::size_t sourceIndex,
    const std::size_t containerCount) const noexcept
{
    if (sourceIndex < containerCount)
    {
        return nullptr;
    }
    const std::size_t widgetIndex = sourceIndex - containerCount;
    if (widgetIndex >= widgetVisuals_.size() || !widgetVisuals_[widgetIndex].has_value())
    {
        return nullptr;
    }
    return &(*widgetVisuals_[widgetIndex]);
}

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

bool ContainerGeometryChanged(const mfd::api1::ContainerRect& currentGeometry,
                              const mfd::api1::ContainerRect& nextGeometry) noexcept
{
    return currentGeometry.center.x != nextGeometry.center.x ||
           currentGeometry.center.y != nextGeometry.center.y ||
           currentGeometry.size.x != nextGeometry.size.x ||
           currentGeometry.size.y != nextGeometry.size.y;
}
} // namespace editor::app

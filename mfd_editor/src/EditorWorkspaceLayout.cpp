/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorWorkspaceLayout.h"

#include <algorithm>
#include <cmath>

namespace editor
{
namespace
{
/**
 * @brief Returns a finite, non-negative copy of @p value, or 0 when it is not finite.
 * @param value Raw input that may be NaN, infinite or negative.
 * @return A sanitized width guaranteed to be finite and non-negative.
 */
float SanitizeNonNegativeWidth(const float value) noexcept
{
    if (!std::isfinite(value))
    {
        return 0.0f;
    }
    return std::max(0.0f, value);
}
} // namespace

bool WorkspaceLayoutRegion::IsVisible() const noexcept
{
    return width > 0.0f && height > 0.0f;
}

WorkspaceLayoutResult ComputeWorkspaceLayout(const WorkspaceLayoutRequest& request) noexcept
{
    WorkspaceLayoutResult result;

    const float totalWidth = std::max(0.0f, request.width);
    const float totalHeight = std::max(0.0f, request.height);
    const float spacing = std::max(0.0f, request.spacing);
    const float minCenterWidth = std::max(0.0f, request.minCenterWidth);
    const float minCenterHeight = std::max(0.0f, request.minCenterHeight);
    const float minLeadingPanelWidth = std::max(0.0f, request.minLeadingPanelWidth);
    const float minBottomPanelHeight = std::max(0.0f, request.minBottomPanelHeight);

    float previewX = 0.0f;
    float previewWidth = totalWidth;

    if (request.showLeadingPanel)
    {
        const float maxLeadingWidth = std::max(0.0f, totalWidth - minCenterWidth - spacing);
        const float clampedLeadingWidth = std::clamp(request.leadingPanelWidth, 0.0f, maxLeadingWidth);
        if (clampedLeadingWidth >= minLeadingPanelWidth)
        {
            result.leadingPanel = {0.0f, 0.0f, clampedLeadingWidth, totalHeight};
            previewX = clampedLeadingWidth + spacing;
            previewWidth = std::max(0.0f, totalWidth - previewX);
        }
    }

    float previewHeight = totalHeight;
    if (request.showBottomPanel)
    {
        const float maxBottomHeight = std::max(0.0f, totalHeight - minCenterHeight - spacing);
        const float clampedBottomHeight = std::clamp(request.bottomPanelHeight, 0.0f, maxBottomHeight);
        if (clampedBottomHeight >= minBottomPanelHeight)
        {
            previewHeight = std::max(0.0f, totalHeight - clampedBottomHeight - spacing);
            result.bottomPanel = {previewX, previewHeight + spacing, previewWidth, clampedBottomHeight};
        }
    }

    result.previewPanel = {previewX, 0.0f, previewWidth, previewHeight};
    return result;
}

StudioSplitResult ComputeStudioSplitLayout(const StudioSplitRequest& request) noexcept
{
    const float totalWidth = SanitizeNonNegativeWidth(request.width);
    const float spacing = SanitizeNonNegativeWidth(request.spacing);
    const float minPrimaryWidth = SanitizeNonNegativeWidth(request.minPrimaryWidth);
    const float minSecondaryWidth = SanitizeNonNegativeWidth(request.minSecondaryWidth);

    StudioSplitResult result;
    result.primaryWidth = totalWidth;

    if (!request.showSecondary)
    {
        return result;
    }

    const float maxSecondaryWidth = totalWidth - minPrimaryWidth - spacing;
    if (maxSecondaryWidth < minSecondaryWidth)
    {
        // The page-context pane and the studio cannot both keep their minimum width:
        // hand the whole row to the studio and report the transient auto-collapse.
        result.secondaryAutoCollapsed = true;
        return result;
    }

    const float preferredSecondaryWidth = std::isfinite(request.secondaryPreferredWidth)
                                              ? request.secondaryPreferredWidth
                                              : minSecondaryWidth;
    const float secondaryWidth = std::clamp(preferredSecondaryWidth, minSecondaryWidth, maxSecondaryWidth);

    result.secondaryVisible = true;
    result.secondaryWidth = secondaryWidth;
    result.primaryWidth = std::max(0.0f, totalWidth - secondaryWidth - spacing);
    return result;
}
} // namespace editor

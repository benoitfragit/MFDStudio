/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorWorkspaceLayout.h"

#include <algorithm>

namespace editor
{
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
} // namespace editor

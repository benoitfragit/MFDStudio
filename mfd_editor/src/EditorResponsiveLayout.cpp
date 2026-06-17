/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorResponsiveLayout.h"

/**
 * @file
 * @brief Implementation of the responsive editor shell layout helper.
 */

#include <algorithm>
#include <cmath>

namespace editor
{
namespace
{
/**
 * @brief Returns a finite, non-negative copy of @p value, falling back to @p fallback.
 * @param value Raw input that may be NaN, infinite or negative.
 * @param fallback Replacement used when @p value is not finite.
 * @return A sanitized width guaranteed to be finite and non-negative.
 */
float SanitizeNonNegative(const float value, const float fallback) noexcept
{
    if (!std::isfinite(value))
    {
        return std::max(0.0f, fallback);
    }
    return std::max(0.0f, value);
}

/**
 * @brief Returns a sanitized preferred width that is never below the panel minimum.
 * @param preferred Raw preferred width which may be invalid.
 * @param minWidth Already-sanitized minimum width.
 * @return A finite width clamped to at least @p minWidth.
 */
float SanitizePreferredWidth(const float preferred, const float minWidth) noexcept
{
    if (!std::isfinite(preferred))
    {
        return minWidth;
    }
    return std::max(minWidth, preferred);
}

/**
 * @brief Cost in pixels of showing one auxiliary panel at its minimum width.
 * @param minWidth Sanitized panel minimum width.
 * @param splitterWidth Sanitized splitter width reserved next to the panel.
 * @return The width that must be reserved to keep the panel usable.
 */
float PanelMinimumFootprint(const float minWidth, const float splitterWidth) noexcept
{
    return minWidth + splitterWidth;
}
} // namespace

ShellLayoutResult ComputeShellLayout(const ShellLayoutRequest& request) noexcept
{
    const float totalWidth = SanitizeNonNegative(request.totalWidth, 0.0f);
    const float splitterWidth = SanitizeNonNegative(request.splitterWidth, 0.0f);
    const float minWorkspaceWidth = SanitizeNonNegative(request.minWorkspaceWidth, 0.0f);

    const float sidebarMinWidth = SanitizeNonNegative(request.sidebar.minWidth, 0.0f);
    const float inspectorMinWidth = SanitizeNonNegative(request.inspector.minWidth, 0.0f);
    const float sidebarPreferredWidth = SanitizePreferredWidth(request.sidebar.preferredWidth, sidebarMinWidth);
    const float inspectorPreferredWidth = SanitizePreferredWidth(request.inspector.preferredWidth, inspectorMinWidth);

    // Strict priority ladder: the inspector outranks the sidebar. We keep the largest
    // prefix of the wanted panels (highest priority first) that still leaves the
    // workspace at or above its floor. A lower-priority panel is never shown while a
    // wanted higher-priority panel is starved, so resizing walks a monotonic
    // Wide -> Compact -> Focus sequence instead of swapping which panel is visible.
    bool inspectorVisible = false;
    bool sidebarVisible = false;
    bool higherPriorityPanelStarved = false;
    float reservedAuxFootprint = 0.0f;

    if (request.inspector.wantVisible)
    {
        const float footprint = PanelMinimumFootprint(inspectorMinWidth, splitterWidth);
        if (totalWidth - reservedAuxFootprint - footprint >= minWorkspaceWidth)
        {
            inspectorVisible = true;
            reservedAuxFootprint += footprint;
        }
        else
        {
            higherPriorityPanelStarved = true;
        }
    }

    if (request.sidebar.wantVisible && !higherPriorityPanelStarved)
    {
        const float footprint = PanelMinimumFootprint(sidebarMinWidth, splitterWidth);
        if (totalWidth - reservedAuxFootprint - footprint >= minWorkspaceWidth)
        {
            sidebarVisible = true;
            reservedAuxFootprint += footprint;
        }
    }

    const float splitterTotal = splitterWidth * static_cast<float>((sidebarVisible ? 1 : 0) + (inspectorVisible ? 1 : 0));
    const float maxAuxTotal = std::max(0.0f, totalWidth - minWorkspaceWidth - splitterTotal);

    float sidebarWidth = sidebarVisible ? sidebarPreferredWidth : 0.0f;
    float inspectorWidth = inspectorVisible ? inspectorPreferredWidth : 0.0f;

    // Compress the preferred widths to keep the workspace at or above its floor. The
    // sidebar (lower priority) yields its slack before the inspector.
    float excess = (sidebarWidth + inspectorWidth) - maxAuxTotal;
    if (excess > 0.0f && sidebarVisible)
    {
        const float reduction = std::min(excess, std::max(0.0f, sidebarWidth - sidebarMinWidth));
        sidebarWidth -= reduction;
        excess -= reduction;
    }
    if (excess > 0.0f && inspectorVisible)
    {
        const float reduction = std::min(excess, std::max(0.0f, inspectorWidth - inspectorMinWidth));
        inspectorWidth -= reduction;
        excess -= reduction;
    }

    const float workspaceWidth = std::max(0.0f, totalWidth - sidebarWidth - inspectorWidth - splitterTotal);

    ShellLayoutResult result;
    result.sidebar.visible = sidebarVisible;
    result.sidebar.autoCollapsed = request.sidebar.wantVisible && !sidebarVisible;
    result.sidebar.width = sidebarWidth;
    result.inspector.visible = inspectorVisible;
    result.inspector.autoCollapsed = request.inspector.wantVisible && !inspectorVisible;
    result.inspector.width = inspectorWidth;
    result.workspaceWidth = workspaceWidth;

    const int visibleAuxCount = (sidebarVisible ? 1 : 0) + (inspectorVisible ? 1 : 0);
    if (visibleAuxCount >= 2)
    {
        result.mode = ShellLayoutMode::Wide;
    }
    else if (visibleAuxCount == 1)
    {
        result.mode = ShellLayoutMode::Compact;
    }
    else
    {
        result.mode = ShellLayoutMode::Focus;
    }

    return result;
}
} // namespace editor

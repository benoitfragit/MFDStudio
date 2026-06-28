/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "internal/application/EditorSidebarLayout.h"

/**
 * @file
 * @brief Implementation of the internal sidebar section layout helper.
 */

#include <algorithm>
#include <cmath>

namespace editor::app
{
namespace
{
/**
 * @brief Returns a finite, non-negative copy of @p value, or `0` when invalid.
 * @param value Raw dimension that may be negative, NaN or infinite.
 * @return One safe non-negative dimension.
 */
float SanitizeNonNegativeDimension(const float value) noexcept
{
    if (!std::isfinite(value))
    {
        return 0.0f;
    }

    return std::max(0.0f, value);
}
} // namespace

SidebarSectionLayoutResult ComputeSidebarSectionLayout(const SidebarSectionLayoutRequest& request) noexcept
{
    SidebarSectionLayoutResult result;

    const float totalHeight = SanitizeNonNegativeDimension(request.height);
    const float preferredSpacing = SanitizeNonNegativeDimension(request.sectionSpacing);
    const float pagesPreferredHeight = SanitizeNonNegativeDimension(request.pagesPreferredHeight);
    const float pagesMinHeight = SanitizeNonNegativeDimension(request.pagesMinHeight);
    const float reticlesMinHeight = SanitizeNonNegativeDimension(request.reticlesMinHeight);
    const float totalMinHeight = pagesMinHeight + reticlesMinHeight;

    if (preferredSpacing > 0.0f && totalHeight > totalMinHeight)
    {
        result.sectionSpacing = std::min(preferredSpacing, totalHeight - totalMinHeight);
    }

    const float distributableHeight = std::max(0.0f, totalHeight - result.sectionSpacing);
    if (distributableHeight <= 0.0f)
    {
        return result;
    }

    float pagesHeight = pagesPreferredHeight;
    if (distributableHeight >= totalMinHeight)
    {
        const float maxPagesHeight = std::max(0.0f, distributableHeight - reticlesMinHeight);
        pagesHeight = std::clamp(pagesHeight, pagesMinHeight, maxPagesHeight);
    }
    else if (totalMinHeight > 0.0f)
    {
        pagesHeight = distributableHeight * (pagesMinHeight / totalMinHeight);
    }
    else
    {
        pagesHeight = distributableHeight * 0.5f;
    }

    result.pagesHeight = std::clamp(pagesHeight, 0.0f, distributableHeight);
    result.reticlesHeight = std::max(0.0f, distributableHeight - result.pagesHeight);
    return result;
}
} // namespace editor::app

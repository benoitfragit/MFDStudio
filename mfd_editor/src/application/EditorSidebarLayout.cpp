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

    if (!request.pagesExpanded || !request.reticlesExpanded)
    {
        if (totalHeight < totalMinHeight && totalMinHeight > 0.0f)
        {
            result.pagesHeight = totalHeight * (pagesMinHeight / totalMinHeight);
            result.reticlesHeight = std::max(0.0f, totalHeight - result.pagesHeight);
            return result;
        }

        if (!request.pagesExpanded && !request.reticlesExpanded)
        {
            result.pagesHeight = pagesMinHeight;
            result.reticlesHeight = reticlesMinHeight;
            return result;
        }

        if (!request.pagesExpanded)
        {
            result.pagesHeight = pagesMinHeight;
            result.reticlesHeight = std::max(0.0f, totalHeight - result.pagesHeight);
            return result;
        }

        result.reticlesHeight = reticlesMinHeight;
        result.pagesHeight = std::max(0.0f, totalHeight - result.reticlesHeight);
        return result;
    }

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

float ResolveContentAwareSidebarPreferredHeight(const float distributableHeight,
                                                 const float preferredFirstFraction,
                                                 const bool firstHasContent,
                                                 const bool secondHasContent,
                                                 const float firstMinimumHeight,
                                                 const float secondMinimumHeight) noexcept
{
    const float safeHeight = SanitizeNonNegativeDimension(distributableHeight);
    const float safeFirstMinimum = SanitizeNonNegativeDimension(firstMinimumHeight);
    const float safeSecondMinimum = SanitizeNonNegativeDimension(secondMinimumHeight);
    if (firstHasContent && !secondHasContent)
    {
        return std::max(0.0f, safeHeight - safeSecondMinimum);
    }
    if (!firstHasContent && secondHasContent)
    {
        return std::min(safeFirstMinimum, safeHeight);
    }

    const float safeFraction = std::isfinite(preferredFirstFraction)
                                   ? std::clamp(preferredFirstFraction, 0.0f, 1.0f)
                                   : 0.5f;
    return safeHeight * safeFraction;
}

float ResolveSidebarPagesFraction(
    const float currentPagesHeight, const float dragDeltaY, const float distributableHeight) noexcept
{
    const float safeDistributable = SanitizeNonNegativeDimension(distributableHeight);
    if (safeDistributable <= 0.0f)
    {
        return 0.5f;
    }

    const float safePagesHeight = SanitizeNonNegativeDimension(currentPagesHeight);
    const float safeDelta = std::isfinite(dragDeltaY) ? dragDeltaY : 0.0f;
    const float draggedHeight = safePagesHeight + safeDelta;
    return std::clamp(draggedHeight / safeDistributable, 0.0f, 1.0f);
}
} // namespace editor::app

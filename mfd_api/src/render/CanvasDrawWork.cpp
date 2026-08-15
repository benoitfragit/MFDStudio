/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of internal Canvas2D logical-work estimates.
 */

#include "CanvasDrawWork.h"

#include <algorithm>
#include <limits>

namespace mfd::detail
{
std::size_t EstimateTextDrawWorkUnits(const std::string_view text) noexcept
{
    return std::max<std::size_t>(1U, text.size());
}

std::size_t EstimateConvexFillWorkUnits(const std::size_t pointCount) noexcept
{
    return pointCount >= 3U ? pointCount - 2U : 0U;
}

std::size_t EstimateClosedStrokeWorkUnits(const std::size_t pointCount) noexcept
{
    return pointCount >= 2U ? pointCount : 0U;
}

std::size_t EstimateRingFillWorkUnits(const std::size_t segmentCount) noexcept
{
    constexpr std::size_t kTrianglesPerSegment = 2U;
    if (segmentCount > std::numeric_limits<std::size_t>::max() / kTrianglesPerSegment)
    {
        return std::numeric_limits<std::size_t>::max();
    }

    return segmentCount * kTrianglesPerSegment;
}

std::size_t EstimateDirectDrawWorkUnits() noexcept
{
    return 1U;
}
} // namespace mfd::detail

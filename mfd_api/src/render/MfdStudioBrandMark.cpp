/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Implementation of the private MFDStudio brand-mark geometry.
 */

#include "MfdStudioBrandMark.h"

#include <algorithm>
#include <cstddef>
#include <cmath>

namespace mfd::internal
{
namespace
{
constexpr float kFrameInsetFraction = 0.09375f;
constexpr float kFrameRoundness = 0.46f;
constexpr float kMonogramStrokeFraction = 0.140625f;
constexpr std::array<Vector2, 5> kNormalizedMonogramPoints {{
    {0.2930f, 0.7129f},
    {0.2930f, 0.2930f},
    {0.5000f, 0.4590f},
    {0.7070f, 0.2930f},
    {0.7070f, 0.7129f}}};

bool IsFiniteRectangle(const Rectangle bounds) noexcept
{
    return std::isfinite(bounds.x) && std::isfinite(bounds.y) &&
           std::isfinite(bounds.width) && std::isfinite(bounds.height);
}
} // namespace

MfdStudioBrandMarkGeometry BuildMfdStudioBrandMarkGeometry(const Rectangle bounds) noexcept
{
    MfdStudioBrandMarkGeometry geometry;
    if (!IsFiniteRectangle(bounds) || bounds.width <= 0.0f || bounds.height <= 0.0f)
    {
        return geometry;
    }

    const float side = std::min(bounds.width, bounds.height);
    const float originX = bounds.x + (bounds.width - side) * 0.5f;
    const float originY = bounds.y + (bounds.height - side) * 0.5f;
    const float frameInset = side * kFrameInsetFraction;

    geometry.frame = {
        originX + frameInset,
        originY + frameInset,
        side - frameInset * 2.0f,
        side - frameInset * 2.0f};
    geometry.frameRoundness = kFrameRoundness;
    geometry.monogramStrokeWidth = side * kMonogramStrokeFraction;

    for (std::size_t index = 0; index < geometry.monogramPoints.size(); ++index)
    {
        geometry.monogramPoints[index] = {
            originX + kNormalizedMonogramPoints[index].x * side,
            originY + kNormalizedMonogramPoints[index].y * side};
    }

    return geometry;
}

void DrawMfdStudioBrandMark(const Rectangle bounds, const Color surfaceColor, const Color monogramColor) noexcept
{
    const MfdStudioBrandMarkGeometry geometry = BuildMfdStudioBrandMarkGeometry(bounds);
    if (geometry.frame.width <= 0.0f || geometry.frame.height <= 0.0f || geometry.monogramStrokeWidth <= 0.0f)
    {
        return;
    }

    DrawRectangleRounded(geometry.frame, geometry.frameRoundness, 16, surfaceColor);
    for (std::size_t index = 1; index < geometry.monogramPoints.size(); ++index)
    {
        DrawLineEx(
            geometry.monogramPoints[index - 1],
            geometry.monogramPoints[index],
            geometry.monogramStrokeWidth,
            monogramColor);
    }
}
} // namespace mfd::internal

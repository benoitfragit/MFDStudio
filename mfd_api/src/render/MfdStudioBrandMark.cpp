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

/**
 * @brief Builds half-stroke offsets with the editor's miter joins and flat end caps.
 * @return Offsets for the fixed, non-degenerate normalized monogram path.
 */
std::array<Vector2, 5> BuildMonogramJoinOffsets() noexcept
{
    std::array<Vector2, 4> segmentNormals {};
    for (std::size_t index = 0; index < segmentNormals.size(); ++index)
    {
        const Vector2 start = kNormalizedMonogramPoints[index];
        const Vector2 end = kNormalizedMonogramPoints[index + 1];
        const float deltaX = end.x - start.x;
        const float deltaY = end.y - start.y;
        const float length = std::hypot(deltaX, deltaY);
        segmentNormals[index] = {-deltaY / length, deltaX / length};
    }

    std::array<Vector2, 5> offsets {};
    offsets.front() = segmentNormals.front();
    offsets.back() = segmentNormals.back();
    for (std::size_t index = 1; index + 1 < offsets.size(); ++index)
    {
        const Vector2 incoming = segmentNormals[index - 1];
        const Vector2 outgoing = segmentNormals[index];
        // The fixed brand path has no reversed segments, so the denominator is positive.
        const float denominator = 1.0f + incoming.x * outgoing.x + incoming.y * outgoing.y;
        offsets[index] = {
            (incoming.x + outgoing.x) / denominator,
            (incoming.y + outgoing.y) / denominator};
    }

    return offsets;
}

/**
 * @brief Draws a continuous monogram strip whose adjacent segments share their join vertices.
 * @param geometry Resolved brand-mark geometry with a positive stroke width.
 * @param color Monogram fill color.
 */
void DrawMonogramStroke(const MfdStudioBrandMarkGeometry& geometry, const Color color) noexcept
{
    static const std::array<Vector2, 5> joinOffsets = BuildMonogramJoinOffsets();
    const float halfStroke = geometry.monogramStrokeWidth * 0.5f;
    std::array<Vector2, 10> vertices {};
    for (std::size_t index = 0; index < geometry.monogramPoints.size(); ++index)
    {
        const Vector2 point = geometry.monogramPoints[index];
        const Vector2 offset {joinOffsets[index].x * halfStroke, joinOffsets[index].y * halfStroke};
        vertices[index * 2] = {point.x - offset.x, point.y - offset.y};
        vertices[index * 2 + 1] = {point.x + offset.x, point.y + offset.y};
    }

    DrawTriangleStrip(vertices.data(), static_cast<int>(vertices.size()), color);
}

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
    DrawMonogramStroke(geometry, monogramColor);
}
} // namespace mfd::internal

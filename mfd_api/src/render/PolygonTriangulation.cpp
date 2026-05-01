/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Internal polygon triangulation helpers used by Canvas2D.
 */

#include "PolygonTriangulation.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace mfd::detail
{
namespace
{
constexpr float kGeometryEpsilon = 1.0e-5f;

float SignedTwiceArea(const ArrayView<const Vector2> points) noexcept
{
    float area = 0.0f;
    for (std::size_t index = 0; index < points.size(); ++index)
    {
        const Vector2& current = points[index];
        const Vector2& next = points[(index + 1U) % points.size()];
        area += current.x * next.y - next.x * current.y;
    }

    return area;
}

float Cross(const Vector2& lhs, const Vector2& middle, const Vector2& rhs) noexcept
{
    const float abx = middle.x - lhs.x;
    const float aby = middle.y - lhs.y;
    const float acx = rhs.x - lhs.x;
    const float acy = rhs.y - lhs.y;
    return abx * acy - aby * acx;
}

bool PointInsideOrOnTriangle(const Vector2& point,
                             const Vector2& a,
                             const Vector2& b,
                             const Vector2& c,
                             const float orientationSign) noexcept
{
    const float ab = Cross(a, b, point) * orientationSign;
    const float bc = Cross(b, c, point) * orientationSign;
    const float ca = Cross(c, a, point) * orientationSign;
    return ab >= -kGeometryEpsilon &&
           bc >= -kGeometryEpsilon &&
           ca >= -kGeometryEpsilon;
}

bool IsEar(const ArrayView<const Vector2> points,
           const std::vector<std::size_t>& remainingIndices,
           const std::size_t candidateIndex,
           const float orientationSign) noexcept
{
    const std::size_t previousIndex =
        remainingIndices[(candidateIndex + remainingIndices.size() - 1U) % remainingIndices.size()];
    const std::size_t currentIndex = remainingIndices[candidateIndex];
    const std::size_t nextIndex = remainingIndices[(candidateIndex + 1U) % remainingIndices.size()];

    const Vector2& previous = points[previousIndex];
    const Vector2& current = points[currentIndex];
    const Vector2& next = points[nextIndex];
    if (Cross(previous, current, next) * orientationSign <= kGeometryEpsilon)
    {
        return false;
    }

    for (const std::size_t pointIndex : remainingIndices)
    {
        if (pointIndex == previousIndex || pointIndex == currentIndex || pointIndex == nextIndex)
        {
            continue;
        }

        if (PointInsideOrOnTriangle(points[pointIndex], previous, current, next, orientationSign))
        {
            return false;
        }
    }

    return true;
}
} // namespace

bool PolygonIsConvex(const ArrayView<const Vector2> points) noexcept
{
    if (points.size() < 3)
    {
        return false;
    }

    float expectedSign = 0.0f;
    for (std::size_t index = 0; index < points.size(); ++index)
    {
        const Vector2& previous = points[(index + points.size() - 1U) % points.size()];
        const Vector2& current = points[index];
        const Vector2& next = points[(index + 1U) % points.size()];
        const float cross = Cross(previous, current, next);
        if (std::abs(cross) <= kGeometryEpsilon)
        {
            continue;
        }

        const float sign = cross > 0.0f ? 1.0f : -1.0f;
        if (expectedSign == 0.0f)
        {
            expectedSign = sign;
            continue;
        }

        if (sign != expectedSign)
        {
            return false;
        }
    }

    return expectedSign != 0.0f;
}

bool TriangulateSimplePolygon(const ArrayView<const Vector2> points, std::vector<std::size_t>& triangleIndices)
{
    triangleIndices.clear();
    if (points.size() < 3)
    {
        return false;
    }

    const float signedArea = SignedTwiceArea(points);
    if (std::abs(signedArea) <= kGeometryEpsilon)
    {
        return false;
    }

    std::vector<std::size_t> remainingIndices(points.size());
    std::iota(remainingIndices.begin(), remainingIndices.end(), 0U);

    const float orientationSign = signedArea > 0.0f ? 1.0f : -1.0f;
    triangleIndices.reserve((points.size() - 2U) * 3U);

    while (remainingIndices.size() > 3U)
    {
        bool clippedEar = false;
        for (std::size_t candidateIndex = 0; candidateIndex < remainingIndices.size(); ++candidateIndex)
        {
            if (!IsEar(points, remainingIndices, candidateIndex, orientationSign))
            {
                continue;
            }

            const std::size_t previousIndex =
                remainingIndices[(candidateIndex + remainingIndices.size() - 1U) % remainingIndices.size()];
            const std::size_t currentIndex = remainingIndices[candidateIndex];
            const std::size_t nextIndex = remainingIndices[(candidateIndex + 1U) % remainingIndices.size()];
            triangleIndices.push_back(previousIndex);
            triangleIndices.push_back(currentIndex);
            triangleIndices.push_back(nextIndex);
            remainingIndices.erase(remainingIndices.begin() + static_cast<std::ptrdiff_t>(candidateIndex));
            clippedEar = true;
            break;
        }

        if (!clippedEar)
        {
            triangleIndices.clear();
            return false;
        }
    }

    if (remainingIndices.size() != 3U)
    {
        triangleIndices.clear();
        return false;
    }

    const Vector2& previous = points[remainingIndices[0]];
    const Vector2& current = points[remainingIndices[1]];
    const Vector2& next = points[remainingIndices[2]];
    if (Cross(previous, current, next) * orientationSign <= kGeometryEpsilon)
    {
        triangleIndices.clear();
        return false;
    }

    triangleIndices.push_back(remainingIndices[0]);
    triangleIndices.push_back(remainingIndices[1]);
    triangleIndices.push_back(remainingIndices[2]);
    return true;
}
} // namespace mfd::detail

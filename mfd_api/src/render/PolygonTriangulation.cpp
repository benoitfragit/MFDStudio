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

#include "RenderWorkBudget.h"

namespace mfd::detail
{
namespace
{
constexpr float kGeometryEpsilon = 1.0e-5f;
constexpr std::size_t kMaxFilledPolygonPoints = 512U;

bool IsFinitePoint(const Vector2& point) noexcept
{
    return std::isfinite(point.x) && std::isfinite(point.y);
}

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

// A simple polygon cannot contain crossings, overlapping edges or repeated non-adjacent vertices.
// The 512-point input cap bounds this allocation-free pairwise check to fewer than 131072 pairs.
double Orient(const Vector2& a, const Vector2& b, const Vector2& c) noexcept
{
    return (static_cast<double>(b.x) - a.x) * (static_cast<double>(c.y) - a.y) -
           (static_cast<double>(b.y) - a.y) * (static_cast<double>(c.x) - a.x);
}

bool PointOnSegment(const Vector2& a, const Vector2& b, const Vector2& point) noexcept
{
    return Orient(a, b, point) == 0.0 &&
           point.x >= std::min(a.x, b.x) && point.x <= std::max(a.x, b.x) &&
           point.y >= std::min(a.y, b.y) && point.y <= std::max(a.y, b.y);
}

bool SegmentsIntersect(const Vector2& a, const Vector2& b,
                       const Vector2& c, const Vector2& d) noexcept
{
    const double abC = Orient(a, b, c);
    const double abD = Orient(a, b, d);
    const double cdA = Orient(c, d, a);
    const double cdB = Orient(c, d, b);
    if ((abC > 0.0 && abD < 0.0 || abC < 0.0 && abD > 0.0) &&
        (cdA > 0.0 && cdB < 0.0 || cdA < 0.0 && cdB > 0.0))
    {
        return true;
    }
    return (abC == 0.0 && PointOnSegment(a, b, c)) ||
           (abD == 0.0 && PointOnSegment(a, b, d)) ||
           (cdA == 0.0 && PointOnSegment(c, d, a)) ||
           (cdB == 0.0 && PointOnSegment(c, d, b));
}

bool PolygonIsSimple(const ArrayView<const Vector2> points) noexcept
{
    if (points.size() < 3U || points.size() > kMaxFilledPolygonPoints)
    {
        return false;
    }

    for (std::size_t index = 0U; index < points.size(); ++index)
    {
        const Vector2& current = points[index];
        const Vector2& next = points[(index + 1U) % points.size()];
        if (!IsFinitePoint(current) || !IsFinitePoint(next) ||
            (current.x == next.x && current.y == next.y))
        {
            return false;
        }
    }

    for (std::size_t first = 0U; first < points.size(); ++first)
    {
        const std::size_t firstNext = (first + 1U) % points.size();
        for (std::size_t second = first + 1U; second < points.size(); ++second)
        {
            const std::size_t secondNext = (second + 1U) % points.size();
            if (firstNext == second || secondNext == first)
            {
                continue; // Adjacent edges share their endpoint by definition.
            }
            if (SegmentsIntersect(points[first], points[firstNext],
                                  points[second], points[secondNext]))
            {
                return false;
            }
        }
    }
    return true;
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

enum class EarTestResult
{
    NotEar,
    Ear,
    BudgetExhausted
};

EarTestResult TestEar(const ArrayView<const Vector2> points,
                      const std::vector<std::size_t>& remainingIndices,
                      const std::size_t candidateIndex,
                      const float orientationSign,
                      RenderWorkBudget& workBudget) noexcept
{
    if (!workBudget.TryConsume())
    {
        return EarTestResult::BudgetExhausted;
    }

    const std::size_t previousIndex =
        remainingIndices[(candidateIndex + remainingIndices.size() - 1U) % remainingIndices.size()];
    const std::size_t currentIndex = remainingIndices[candidateIndex];
    const std::size_t nextIndex = remainingIndices[(candidateIndex + 1U) % remainingIndices.size()];

    const Vector2& previous = points[previousIndex];
    const Vector2& current = points[currentIndex];
    const Vector2& next = points[nextIndex];
    if (Cross(previous, current, next) * orientationSign <= kGeometryEpsilon)
    {
        return EarTestResult::NotEar;
    }

    for (const std::size_t pointIndex : remainingIndices)
    {
        if (!workBudget.TryConsume())
        {
            return EarTestResult::BudgetExhausted;
        }

        if (pointIndex == previousIndex || pointIndex == currentIndex || pointIndex == nextIndex)
        {
            continue;
        }

        if (PointInsideOrOnTriangle(points[pointIndex], previous, current, next, orientationSign))
        {
            return EarTestResult::NotEar;
        }
    }

    return EarTestResult::Ear;
}
} // namespace

bool PolygonIsConvex(const ArrayView<const Vector2> points) noexcept
{
    if (points.size() < 3 || points.size() > kMaxFilledPolygonPoints)
    {
        return false;
    }

    if (!PolygonIsSimple(points))
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
    return TriangulateSimplePolygonWithBudget(points, triangleIndices, kMaxTriangulationTests);
}

bool TriangulateSimplePolygonWithBudget(const ArrayView<const Vector2> points,
                                        std::vector<std::size_t>& triangleIndices,
                                        const std::size_t maximumTests)
{
    triangleIndices.clear();
    if (points.size() < 3 || points.size() > kMaxFilledPolygonPoints)
    {
        return false;
    }

    for (const Vector2& point : points)
    {
        if (!IsFinitePoint(point))
        {
            return false;
        }
    }

    // Reject non-simple contours before ear clipping, including star polygons.
    if (!PolygonIsSimple(points))
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
    RenderWorkBudget workBudget(maximumTests);

    while (remainingIndices.size() > 3U)
    {
        bool clippedEar = false;
        for (std::size_t candidateIndex = 0; candidateIndex < remainingIndices.size(); ++candidateIndex)
        {
            const EarTestResult earResult =
                TestEar(points, remainingIndices, candidateIndex, orientationSign, workBudget);
            if (earResult == EarTestResult::BudgetExhausted)
            {
                triangleIndices.clear();
                return false;
            }

            if (earResult == EarTestResult::NotEar)
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

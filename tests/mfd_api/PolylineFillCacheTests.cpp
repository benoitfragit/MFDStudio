/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Unit tests covering the internal closed-polyline fill cache.
 */

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include "PolylineFillCache.h"

namespace
{
std::vector<Vector2> MakeConvexPolygon(const std::size_t pointCount,
                                       const float radius = 100.0f)
{
    constexpr float kPi = 3.14159265358979323846f;
    std::vector<Vector2> points;
    points.reserve(pointCount);
    for (std::size_t index = 0U; index < pointCount; ++index)
    {
        const float angle = 2.0f * kPi * static_cast<float>(index) /
                            static_cast<float>(pointCount);
        points.push_back(Vector2 {radius * std::cos(angle), radius * std::sin(angle)});
    }
    return points;
}
} // namespace

TEST(PolylineFillCacheTests, ReusesConvexTopologyForIdenticalPointContent)
{
    const std::vector<Vector2> firstPoints = MakeConvexPolygon(8U);
    const std::vector<Vector2> copiedPoints = firstPoints;
    mfd::PolylineFillCache cache;

    EXPECT_EQ(cache.Resolve(firstPoints).kind, mfd::PolylineFillCache::FillKind::Convex);
    EXPECT_EQ(cache.Resolve(copiedPoints).kind, mfd::PolylineFillCache::FillKind::Convex);

    EXPECT_EQ(cache.EntryCount(), 1U);
    EXPECT_EQ(cache.CacheStats().misses, 1U);
    EXPECT_EQ(cache.CacheStats().hits, 1U);
}

TEST(PolylineFillCacheTests, TriangulatesConcaveSimplePolygonOnce)
{
    const std::vector<Vector2> points {
        Vector2 {-3.0f, 0.0f},
        Vector2 {-1.0f, 2.0f},
        Vector2 {0.0f, 0.5f},
        Vector2 {1.0f, 2.0f},
        Vector2 {3.0f, 0.0f},
        Vector2 {0.0f, -2.0f}};
    mfd::PolylineFillCache cache;

    const mfd::PolylineFillCache::Result& result = cache.Resolve(points);

    EXPECT_EQ(result.kind, mfd::PolylineFillCache::FillKind::Triangulated);
    EXPECT_EQ(result.triangleIndices.size(), (points.size() - 2U) * 3U);
    EXPECT_EQ(cache.Resolve(points).kind, mfd::PolylineFillCache::FillKind::Triangulated);
    EXPECT_EQ(cache.CacheStats().misses, 1U);
    EXPECT_EQ(cache.CacheStats().hits, 1U);
}

TEST(PolylineFillCacheTests, RejectsSelfIntersectingAndNonFinitePolygons)
{
    const std::vector<Vector2> selfIntersecting {
        Vector2 {-1.0f, -1.0f},
        Vector2 {1.0f, 1.0f},
        Vector2 {-1.0f, 1.0f},
        Vector2 {1.0f, -1.0f}};
    const std::vector<Vector2> nonFinite {
        Vector2 {0.0f, 0.0f},
        Vector2 {1.0f, 0.0f},
        Vector2 {std::numeric_limits<float>::infinity(), 1.0f}};
    mfd::PolylineFillCache cache;

    EXPECT_EQ(cache.Resolve(selfIntersecting).kind, mfd::PolylineFillCache::FillKind::Invalid);
    EXPECT_EQ(cache.Resolve(nonFinite).kind, mfd::PolylineFillCache::FillKind::Invalid);
    EXPECT_EQ(cache.CacheStats().misses, 2U);
}

TEST(PolylineFillCacheTests, GeometryMutationCreatesANewTopologyEntry)
{
    std::vector<Vector2> points = MakeConvexPolygon(6U);
    mfd::PolylineFillCache cache;
    ASSERT_EQ(cache.Resolve(points).kind, mfd::PolylineFillCache::FillKind::Convex);

    points[2].x += 0.25f;

    EXPECT_NE(cache.Resolve(points).kind, mfd::PolylineFillCache::FillKind::Invalid);
    EXPECT_EQ(cache.EntryCount(), 2U);
    EXPECT_EQ(cache.CacheStats().misses, 2U);
    EXPECT_EQ(cache.CacheStats().hits, 0U);
}

TEST(PolylineFillCacheTests, RepeatedMaximumSizePolygonPaysValidationOnlyOnce)
{
    const std::vector<Vector2> points = MakeConvexPolygon(512U);
    mfd::PolylineFillCache cache;

    ASSERT_EQ(cache.Resolve(points).kind, mfd::PolylineFillCache::FillKind::Convex);
    for (std::size_t frame = 0U; frame < 100U; ++frame)
    {
        EXPECT_EQ(cache.Resolve(points).kind, mfd::PolylineFillCache::FillKind::Convex);
    }

    EXPECT_EQ(cache.CacheStats().misses, 1U);
    EXPECT_EQ(cache.CacheStats().hits, 100U);
}

TEST(PolylineFillCacheTests, RetainedEntriesStayBounded)
{
    mfd::PolylineFillCache cache;
    for (std::size_t index = 0U; index < 300U; ++index)
    {
        const float offset = static_cast<float>(index);
        const std::vector<Vector2> triangle {
            Vector2 {offset, 0.0f},
            Vector2 {offset + 1.0f, 0.0f},
            Vector2 {offset, 1.0f}};
        EXPECT_EQ(cache.Resolve(triangle).kind, mfd::PolylineFillCache::FillKind::Convex);
    }

    EXPECT_EQ(cache.EntryCount(), 256U);
    EXPECT_EQ(cache.CacheStats().misses, 300U);
}

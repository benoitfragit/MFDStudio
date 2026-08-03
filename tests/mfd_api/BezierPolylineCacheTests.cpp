/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Unit tests covering the internal Bézier polyline cache.
 */

#include <gtest/gtest.h>

#include <cstddef>
#include <vector>

#include "BezierPolylineCache.h"

namespace
{
mfd::BezierGeometry MakeBezier(const int segments = 16)
{
    mfd::BezierGeometry geometry;
    geometry.controlPoints = {
        {-0.32f, -0.18f},
        {-0.12f, 0.24f},
        {0.14f, 0.22f},
        {0.30f, -0.10f}};
    geometry.segments = segments;
    return geometry;
}
} // namespace

TEST(BezierPolylineCacheTests, BuildPolylineKeepsBezierEndpointsAndSegmentCount)
{
    const mfd::BezierGeometry geometry = MakeBezier(20);

    std::vector<mfd::Vec2> points;
    mfd::BezierPolylineCache::BuildPolyline(geometry, points);

    ASSERT_EQ(points.size(), 21U);
    EXPECT_FLOAT_EQ(points.front().x, geometry.controlPoints.front().x);
    EXPECT_FLOAT_EQ(points.front().y, geometry.controlPoints.front().y);
    EXPECT_FLOAT_EQ(points.back().x, geometry.controlPoints.back().x);
    EXPECT_FLOAT_EQ(points.back().y, geometry.controlPoints.back().y);
}

TEST(BezierPolylineCacheTests, ResolveReusesCachedPolylineWhenGeometryDoesNotChange)
{
    const mfd::BezierGeometry geometry = MakeBezier(12);
    mfd::BezierPolylineCache cache;

    const auto& first = cache.Resolve(geometry);
    ASSERT_EQ(first.size(), 13U);
    EXPECT_EQ(cache.EntryCount(), 1U);
    EXPECT_EQ(cache.CacheStats().hits, 0U);
    EXPECT_EQ(cache.CacheStats().misses, 1U);

    const auto& second = cache.Resolve(geometry);
    EXPECT_EQ(second.size(), first.size());
    EXPECT_EQ(second.data(), first.data());
    EXPECT_EQ(cache.EntryCount(), 1U);
    EXPECT_EQ(cache.CacheStats().hits, 1U);
    EXPECT_EQ(cache.CacheStats().misses, 1U);
}

TEST(BezierPolylineCacheTests, ResolveInvalidatesCachedPolylineAfterGeometryMutation)
{
    mfd::BezierGeometry geometry = MakeBezier(10);
    mfd::BezierPolylineCache cache;

    const std::size_t firstSize = cache.Resolve(geometry).size();
    ASSERT_EQ(firstSize, 11U);
    ASSERT_EQ(cache.CacheStats().misses, 1U);

    geometry.segments = 24;
    geometry.controlPoints[1].y = 0.30f;

    const auto& updated = cache.Resolve(geometry);
    EXPECT_EQ(updated.size(), 25U);
    EXPECT_EQ(cache.EntryCount(), 1U);
    EXPECT_EQ(cache.CacheStats().hits, 0U);
    EXPECT_EQ(cache.CacheStats().misses, 2U);
}

TEST(BezierPolylineCacheTests, BuildPolylineBoundsHighDegreeEvaluationWork)
{
    mfd::BezierGeometry geometry;
    geometry.controlPoints.reserve(64U);
    for (std::size_t index = 0U; index < 64U; ++index)
    {
        geometry.controlPoints.push_back(
            {static_cast<float>(index), static_cast<float>(index % 7U)});
    }
    geometry.segments = 1024;

    std::vector<mfd::Vec2> points;
    mfd::BezierPolylineCache::BuildPolyline(geometry, points);

    const std::size_t lerpsPerSample = 64U * 63U / 2U;
    ASSERT_GE(points.size(), 2U);
    EXPECT_LE(points.size() * lerpsPerSample, mfd::kMaxBezierLerpOperations);
    EXPECT_FLOAT_EQ(points.front().x, geometry.controlPoints.front().x);
    EXPECT_FLOAT_EQ(points.back().x, geometry.controlPoints.back().x);
}

TEST(BezierPolylineCacheTests, BuildPolylineAvoidsWin32CostOverflow)
{
    mfd::BezierGeometry geometry;
    geometry.controlPoints.resize(70000U, mfd::Vec2 {1.0f, 2.0f});
    geometry.controlPoints.front() = {-1.0f, -2.0f};
    geometry.controlPoints.back() = {3.0f, 4.0f};
    geometry.segments = 1024;

    std::vector<mfd::Vec2> points;
    mfd::BezierPolylineCache::BuildPolyline(geometry, points);

    ASSERT_EQ(points.size(), 2U);
    EXPECT_FLOAT_EQ(points.front().x, -1.0f);
    EXPECT_FLOAT_EQ(points.back().x, 3.0f);
}

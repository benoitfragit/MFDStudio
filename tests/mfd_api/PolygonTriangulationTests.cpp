/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for private polygon triangulation helpers used by Canvas2D.
 */

#include <gtest/gtest.h>

#include <cstddef>
#include <vector>

#include <raylib.h>

#include "PolygonTriangulation.h"

TEST(PolygonTriangulationTests, PolygonIsConvexRejectsConcavePolygons)
{
    const std::vector<Vector2> polygon {
        Vector2 {-3.0f, 0.0f},
        Vector2 {-1.0f, 2.0f},
        Vector2 {0.0f, 0.5f},
        Vector2 {1.0f, 2.0f},
        Vector2 {3.0f, 0.0f},
        Vector2 {0.0f, -2.0f}};

    EXPECT_FALSE(mfd::detail::PolygonIsConvex(polygon));
}

TEST(PolygonTriangulationTests, PolygonIsConvexAcceptsConvexPolygon)
{
    const std::vector<Vector2> polygon {
        Vector2 {-2.0f, -1.0f},
        Vector2 {2.0f, -1.0f},
        Vector2 {2.0f, 1.0f},
        Vector2 {-2.0f, 1.0f}};

    EXPECT_TRUE(mfd::detail::PolygonIsConvex(polygon));
}

TEST(PolygonTriangulationTests, TriangulateSimplePolygonHandlesConcaveSimplePolygon)
{
    const std::vector<Vector2> polygon {
        Vector2 {-3.0f, 0.0f},
        Vector2 {-1.0f, 2.0f},
        Vector2 {0.0f, 0.5f},
        Vector2 {1.0f, 2.0f},
        Vector2 {3.0f, 0.0f},
        Vector2 {0.0f, -2.0f}};
    std::vector<std::size_t> triangleIndices;

    ASSERT_TRUE(mfd::detail::TriangulateSimplePolygon(polygon, triangleIndices));
    ASSERT_EQ(triangleIndices.size(), (polygon.size() - 2U) * 3U);

    for (const std::size_t index : triangleIndices)
    {
        EXPECT_LT(index, polygon.size());
    }
}

TEST(PolygonTriangulationTests, TriangulateSimplePolygonRejectsSelfIntersectingPolygon)
{
    const std::vector<Vector2> polygon {
        Vector2 {-1.0f, -1.0f},
        Vector2 {1.0f, 1.0f},
        Vector2 {-1.0f, 1.0f},
        Vector2 {1.0f, -1.0f}};
    std::vector<std::size_t> triangleIndices;

    EXPECT_FALSE(mfd::detail::TriangulateSimplePolygon(polygon, triangleIndices));
    EXPECT_TRUE(triangleIndices.empty());
}

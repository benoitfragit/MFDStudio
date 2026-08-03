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

#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include <raylib.h>

#include "PolygonTriangulation.h"
#include "RenderWorkBudget.h"

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

TEST(PolygonTriangulationTests, PolygonIsConvexRejectsNonFinitePoints)
{
    const std::vector<Vector2> polygon {
        Vector2 {0.0f, 0.0f},
        Vector2 {1.0f, 0.0f},
        Vector2 {std::numeric_limits<float>::quiet_NaN(), 1.0f},
        Vector2 {0.0f, 1.0f}};

    EXPECT_FALSE(mfd::detail::PolygonIsConvex(polygon));
}

TEST(PolygonTriangulationTests, TriangulateSimplePolygonRejectsOversizedPolygons)
{
    std::vector<Vector2> polygon;
    polygon.reserve(513U);
    for (std::size_t index = 0; index < 513U; ++index)
    {
        const float angle = static_cast<float>(index) * 0.0125f;
        polygon.push_back(Vector2 {std::cos(angle), std::sin(angle)});
    }

    std::vector<std::size_t> triangleIndices;
    EXPECT_FALSE(mfd::detail::TriangulateSimplePolygon(polygon, triangleIndices));
    EXPECT_TRUE(triangleIndices.empty());
}

TEST(PolygonTriangulationTests, TriangulationStopsWhenItsWorkBudgetIsExhausted)
{
    const std::vector<Vector2> polygon {
        Vector2 {-3.0f, 0.0f},
        Vector2 {-1.0f, 2.0f},
        Vector2 {0.0f, 0.5f},
        Vector2 {1.0f, 2.0f},
        Vector2 {3.0f, 0.0f},
        Vector2 {0.0f, -2.0f}};
    std::vector<std::size_t> triangleIndices;

    EXPECT_FALSE(mfd::detail::TriangulateSimplePolygonWithBudget(polygon, triangleIndices, 1U));
    EXPECT_TRUE(triangleIndices.empty());
}

TEST(RenderWorkBudgetTests, NestedConsumersCannotExceedTheSharedLimit)
{
    constexpr std::size_t kLimit = 7U;
    mfd::detail::RenderWorkBudget budget(kLimit);

    std::size_t acceptedWork = 0U;
    for (std::size_t segment = 0U; segment < 8U; ++segment)
    {
        for (std::size_t fragment = 0U; fragment < 8U; ++fragment)
        {
            if (!budget.TryConsume())
            {
                break;
            }
            ++acceptedWork;
        }
    }

    EXPECT_EQ(acceptedWork, kLimit);
    EXPECT_EQ(budget.RemainingUnits(), 0U);
    EXPECT_FALSE(budget.TryConsume());
}

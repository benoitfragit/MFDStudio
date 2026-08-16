/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Non-rendering tests for the private MFDStudio brand-mark geometry.
 */

#include <limits>

#include <gtest/gtest.h>

#include "MfdStudioBrandMark.h"

namespace
{
TEST(MfdStudioBrandMarkTests, InvalidBoundsProduceEmptyGeometry)
{
    const auto zeroWidth = mfd::internal::BuildMfdStudioBrandMarkGeometry(Rectangle {4.0f, 8.0f, 0.0f, 80.0f});
    const auto nonFinite = mfd::internal::BuildMfdStudioBrandMarkGeometry(
        Rectangle {4.0f, 8.0f, std::numeric_limits<float>::infinity(), 80.0f});

    EXPECT_FLOAT_EQ(zeroWidth.frame.width, 0.0f);
    EXPECT_FLOAT_EQ(zeroWidth.monogramStrokeWidth, 0.0f);
    EXPECT_FLOAT_EQ(nonFinite.frame.width, 0.0f);
    EXPECT_FLOAT_EQ(nonFinite.monogramStrokeWidth, 0.0f);
}

TEST(MfdStudioBrandMarkTests, GeometryUsesTheShortAxisAndStaysCentered)
{
    const auto geometry =
        mfd::internal::BuildMfdStudioBrandMarkGeometry(Rectangle {10.0f, 20.0f, 200.0f, 100.0f});

    EXPECT_NEAR(geometry.frame.x, 69.375f, 0.0001f);
    EXPECT_NEAR(geometry.frame.y, 29.375f, 0.0001f);
    EXPECT_NEAR(geometry.frame.width, 81.25f, 0.0001f);
    EXPECT_NEAR(geometry.frame.height, 81.25f, 0.0001f);
    EXPECT_NEAR(geometry.monogramStrokeWidth, 14.0625f, 0.0001f);

    EXPECT_NEAR(geometry.monogramPoints.front().x, 89.3f, 0.0001f);
    EXPECT_NEAR(geometry.monogramPoints.front().y, 91.29f, 0.0001f);
    EXPECT_NEAR(geometry.monogramPoints[2].x, 110.0f, 0.0001f);
    EXPECT_NEAR(geometry.monogramPoints[2].y, 65.9f, 0.0001f);
    EXPECT_NEAR(geometry.monogramPoints.back().x, 130.7f, 0.0001f);
    EXPECT_NEAR(geometry.monogramPoints.back().y, 91.29f, 0.0001f);
}
} // namespace

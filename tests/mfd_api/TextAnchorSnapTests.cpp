/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Unit tests validating centered text pixel snapping helpers.
 */

#include <limits>

#include <gtest/gtest.h>

#include "TextAnchorSnap.h"

TEST(TextAnchorSnapTests, SnapCenteredTextAnchorRoundsToNearestPixel)
{
    const Vector2 anchor {18.49f, 37.51f};
    const Vector2 snapped = mfd::detail::SnapCenteredTextAnchor(anchor);

    EXPECT_FLOAT_EQ(snapped.x, 18.0f);
    EXPECT_FLOAT_EQ(snapped.y, 38.0f);
}

TEST(TextAnchorSnapTests, SnapCenteredTextAnchorPreservesNonFiniteInputs)
{
    const Vector2 anchor {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()};
    const Vector2 snapped = mfd::detail::SnapCenteredTextAnchor(anchor);

    EXPECT_TRUE(std::isnan(snapped.x));
    EXPECT_TRUE(std::isinf(snapped.y));
}

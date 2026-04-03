/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <numbers>

#include "mfd/control/UserSpaceProjector.h"

TEST(UserSpaceProjectorTests, ProjectsPositionWithAnchorScaleAndOriginRotation)
{
    mfd::UserSpaceFrame frame;
    frame.userOrigin = {10.0f, 20.0f};
    frame.pageAnchor = {1.0f, -1.0f};
    frame.originRotationRadians = std::numbers::pi_v<float> * 0.5f;
    frame.pageUnitsPerUserUnit = 0.5f;

    mfd::UserSpaceProjector projector(frame);

    const mfd::Vec2 pagePosition = projector.ToPagePosition({10.0f, 22.0f});
    EXPECT_FLOAT_EQ(pagePosition.x, 2.0f);
    EXPECT_FLOAT_EQ(pagePosition.y, -1.0f);

    const float pageRotation = projector.ToPageRotationDegrees(std::numbers::pi_v<float>);
    EXPECT_NEAR(pageRotation, 90.0f, 1.0e-4f);
}

TEST(UserSpaceProjectorTests, UsesConfiguredBasisForOffsetsAndRotations)
{
    mfd::UserSpaceFrame frame;
    frame.userXAxisInPage = {0.0f, 1.0f};
    frame.userYAxisInPage = {1.0f, 0.0f};
    frame.pageUnitsPerUserUnit = 2.0f;

    mfd::UserSpaceProjector projector(frame);

    const mfd::Vec2 pageOffset = projector.ToPageOffset({2.0f, 3.0f});
    EXPECT_FLOAT_EQ(pageOffset.x, 6.0f);
    EXPECT_FLOAT_EQ(pageOffset.y, 4.0f);

    const float pageRotation = projector.ToPageRotationDegrees(0.0f);
    EXPECT_NEAR(pageRotation, 90.0f, 1.0e-4f);

    const mfd::Transform2D transform =
        projector.ToPageTransform({1.0f, 2.0f}, std::numbers::pi_v<float> * 0.5f, {0.25f, 0.75f});
    EXPECT_FLOAT_EQ(transform.position.x, 4.0f);
    EXPECT_FLOAT_EQ(transform.position.y, 2.0f);
    EXPECT_NEAR(transform.rotationDegrees, 0.0f, 1.0e-4f);
    EXPECT_FLOAT_EQ(transform.scale.x, 0.25f);
    EXPECT_FLOAT_EQ(transform.scale.y, 0.75f);
}

TEST(UserSpaceProjectorTests, SanitizesInvalidFrameValuesToSafeDefaults)
{
    mfd::UserSpaceFrame frame;
    frame.userOrigin = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()};
    frame.pageAnchor = {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()};
    frame.originRotationRadians = std::numeric_limits<float>::quiet_NaN();
    frame.pageUnitsPerUserUnit = -4.0f;
    frame.userXAxisInPage = {0.0f, 0.0f};
    frame.userYAxisInPage = {std::numeric_limits<float>::quiet_NaN(), 0.0f};

    mfd::UserSpaceProjector projector(frame);

    const mfd::Vec2 pageOffset = projector.ToPageOffset({2.0f, 3.0f});
    EXPECT_FLOAT_EQ(pageOffset.x, 2.0f);
    EXPECT_FLOAT_EQ(pageOffset.y, 3.0f);

    const mfd::Vec2 pagePosition = projector.ToPagePosition({2.0f, 3.0f});
    EXPECT_FLOAT_EQ(pagePosition.x, 2.0f);
    EXPECT_FLOAT_EQ(pagePosition.y, 3.0f);

    EXPECT_FLOAT_EQ(projector.ToPageRotationDegrees(std::numeric_limits<float>::quiet_NaN()), 0.0f);
}

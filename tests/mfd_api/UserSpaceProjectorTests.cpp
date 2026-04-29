/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for UserSpaceProjectorTests.
 */

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <random>

#include "mfd/control/UserSpaceProjector.h"

namespace
{
constexpr float kPi = 3.14159265358979323846f;

float MaybeNonFinite(std::mt19937& rng)
{
    std::uniform_int_distribution<int> selector(0, 9);
    std::uniform_real_distribution<float> finiteDist(-500.0f, 500.0f);

    switch (selector(rng))
    {
    case 0:
        return std::numeric_limits<float>::quiet_NaN();

    case 1:
        return std::numeric_limits<float>::infinity();

    case 2:
        return -std::numeric_limits<float>::infinity();

    default:
        return finiteDist(rng);
    }
}
}

TEST(UserSpaceProjectorTests, ProjectsPositionWithAnchorScaleAndOriginRotation)
{
    mfd::UserSpaceFrame frame;
    frame.userOrigin = {10.0f, 20.0f};
    frame.pageAnchor = {1.0f, -1.0f};
    frame.originRotationRadians = kPi * 0.5f;
    frame.pageUnitsPerUserUnit = 0.5f;

    mfd::UserSpaceProjector projector(frame);

    const mfd::Vec2 pagePosition = projector.ToPagePosition({10.0f, 22.0f});
    EXPECT_FLOAT_EQ(pagePosition.x, 2.0f);
    EXPECT_FLOAT_EQ(pagePosition.y, -1.0f);

    const float pageRotation = projector.ToPageRotationDegrees(kPi);
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
        projector.ToPageTransform({1.0f, 2.0f}, kPi * 0.5f, {0.25f, 0.75f});
    EXPECT_FLOAT_EQ(transform.position.x, 4.0f);
    EXPECT_FLOAT_EQ(transform.position.y, 2.0f);
    EXPECT_NEAR(transform.rotationDegrees, 0.0f, 1.0e-4f);
    EXPECT_FLOAT_EQ(transform.scale.x, 0.25f);
    EXPECT_FLOAT_EQ(transform.scale.y, 0.75f);
}

TEST(UserSpaceProjectorTests, ConvertsDocumentedNauticalMilesAndRadiansToPageUnitsAndDegrees)
{
    mfd::UserSpaceFrame frame;
    frame.pageUnitsPerUserUnit = 0.04f;

    mfd::UserSpaceProjector projector(frame);

    const mfd::Vec2 fiveNm = projector.ToPageOffset({5.0f, 0.0f});
    EXPECT_FLOAT_EQ(fiveNm.x, 0.20f);
    EXPECT_FLOAT_EQ(fiveNm.y, 0.0f);

    const mfd::Vec2 tenNm = projector.ToPageOffset({0.0f, 10.0f});
    EXPECT_FLOAT_EQ(tenNm.x, 0.0f);
    EXPECT_FLOAT_EQ(tenNm.y, 0.40f);

    EXPECT_NEAR(projector.ToPageRotationDegrees(0.0f), 0.0f, 1.0e-4f);
    EXPECT_NEAR(projector.ToPageRotationDegrees(kPi * 0.5f), 90.0f, 1.0e-4f);
    EXPECT_NEAR(projector.ToPageRotationDegrees(-kPi * 0.25f), -45.0f, 1.0e-4f);
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

TEST(UserSpaceProjectorTests, DeterministicPseudoFuzzKeepsFiniteOutputsAndConsistentTransforms)
{
    std::mt19937 rng(20260425U);
    std::uniform_real_distribution<float> finiteScale(0.05f, 4.0f);

    for (int iteration = 0; iteration < 256; ++iteration)
    {
        mfd::UserSpaceFrame frame;
        frame.userOrigin = {MaybeNonFinite(rng), MaybeNonFinite(rng)};
        frame.pageAnchor = {MaybeNonFinite(rng), MaybeNonFinite(rng)};
        frame.originRotationRadians = MaybeNonFinite(rng);
        frame.pageUnitsPerUserUnit = MaybeNonFinite(rng);
        frame.userXAxisInPage = {MaybeNonFinite(rng), MaybeNonFinite(rng)};
        frame.userYAxisInPage = {MaybeNonFinite(rng), MaybeNonFinite(rng)};

        const mfd::Vec2 userOffset {MaybeNonFinite(rng), MaybeNonFinite(rng)};
        const mfd::Vec2 userPosition {MaybeNonFinite(rng), MaybeNonFinite(rng)};
        const float userRotationRadians = MaybeNonFinite(rng);
        const mfd::Vec2 scale {finiteScale(rng), finiteScale(rng)};

        mfd::UserSpaceProjector projector(frame);

        const mfd::Vec2 pageOffset = projector.ToPageOffset(userOffset);
        const mfd::Vec2 pagePosition = projector.ToPagePosition(userPosition);
        const float pageRotation = projector.ToPageRotationDegrees(userRotationRadians);
        const mfd::Transform2D transform = projector.ToPageTransform(userPosition, userRotationRadians, scale);

        EXPECT_TRUE(std::isfinite(pageOffset.x)) << iteration;
        EXPECT_TRUE(std::isfinite(pageOffset.y)) << iteration;
        EXPECT_TRUE(std::isfinite(pagePosition.x)) << iteration;
        EXPECT_TRUE(std::isfinite(pagePosition.y)) << iteration;
        EXPECT_TRUE(std::isfinite(pageRotation)) << iteration;
        EXPECT_TRUE(std::isfinite(transform.position.x)) << iteration;
        EXPECT_TRUE(std::isfinite(transform.position.y)) << iteration;
        EXPECT_TRUE(std::isfinite(transform.rotationDegrees)) << iteration;
        EXPECT_FLOAT_EQ(transform.position.x, pagePosition.x);
        EXPECT_FLOAT_EQ(transform.position.y, pagePosition.y);
        EXPECT_FLOAT_EQ(transform.rotationDegrees, pageRotation);
        EXPECT_FLOAT_EQ(transform.scale.x, scale.x);
        EXPECT_FLOAT_EQ(transform.scale.y, scale.y);
    }
}

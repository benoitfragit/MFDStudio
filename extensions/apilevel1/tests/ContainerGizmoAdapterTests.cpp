/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

#include <limits>

#include <gtest/gtest.h>

#include "mfd/apilevel1/ContainerGizmoAdapter.h"

TEST(ContainerGizmoAdapterTests, ResizesFromCapturedGeometryAndHonorsMinimumSize)
{
    mfd::api1::ContainerGizmoAdapter adapter;
    const mfd::api1::ContainerRect initial {{0.2f, -0.3f}, {0.8f, 0.4f}};
    const auto enlarged = adapter.ResizeAroundCenter(initial, {0.1f, 0.1f}, 1.5f);
    EXPECT_FLOAT_EQ(enlarged.center.x, initial.center.x);
    EXPECT_FLOAT_EQ(enlarged.center.y, initial.center.y);
    EXPECT_FLOAT_EQ(enlarged.size.x, 1.2f);
    EXPECT_FLOAT_EQ(enlarged.size.y, 0.6f);

    const auto clamped = adapter.ResizeAroundCenter(initial, {0.3f, 0.25f}, 0.1f);
    EXPECT_FLOAT_EQ(clamped.size.x, 0.3f);
    EXPECT_FLOAT_EQ(clamped.size.y, 0.25f);
}

TEST(ContainerGizmoAdapterTests, PreservesContentFittedAxes)
{
    const mfd::api1::ContainerGizmoAdapter adapter;
    const mfd::api1::ContainerRect initial {{}, {0.8f, 0.4f}};
    const auto horizontal = adapter.ResizeAroundCenter(
        initial, {}, 2.0f, mfd::api1::ContainerSizeMode::FitHorizontal);
    EXPECT_FLOAT_EQ(horizontal.size.x, 0.8f);
    EXPECT_FLOAT_EQ(horizontal.size.y, 0.8f);
    const auto vertical = adapter.ResizeAroundCenter(
        initial, {}, 2.0f, mfd::api1::ContainerSizeMode::FitVertical);
    EXPECT_FLOAT_EQ(vertical.size.x, 1.6f);
    EXPECT_FLOAT_EQ(vertical.size.y, 0.4f);
}

TEST(ContainerGizmoAdapterTests, RejectsNonFiniteScaleWithoutMutatingGeometry)
{
    mfd::api1::ContainerGizmoAdapter adapter;
    const mfd::api1::ContainerRect initial {{0.2f, -0.3f}, {0.8f, 0.4f}};
    const auto unchanged = adapter.ResizeAroundCenter(
        initial, {0.1f, 0.1f}, std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(unchanged.center.x, initial.center.x);
    EXPECT_FLOAT_EQ(unchanged.size.x, initial.size.x);
    EXPECT_FLOAT_EQ(unchanged.size.y, initial.size.y);
}

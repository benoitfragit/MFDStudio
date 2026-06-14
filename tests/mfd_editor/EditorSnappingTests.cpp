/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorSnapping.h"

#include <gtest/gtest.h>

namespace
{
using editor::app::ArrowNudgeDelta;
using editor::app::SnapScalarToGrid;
using editor::app::SnapToGrid;

TEST(EditorSnappingTests, SnapScalarRoundsToNearestMultiple)
{
    EXPECT_FLOAT_EQ(SnapScalarToGrid(0.13f, 0.05f), 0.15f);
    EXPECT_FLOAT_EQ(SnapScalarToGrid(0.11f, 0.05f), 0.10f);
    EXPECT_FLOAT_EQ(SnapScalarToGrid(-0.13f, 0.05f), -0.15f);
}

TEST(EditorSnappingTests, SnapScalarLeavesValueUnchangedForInvalidStep)
{
    EXPECT_FLOAT_EQ(SnapScalarToGrid(0.137f, 0.0f), 0.137f);
    EXPECT_FLOAT_EQ(SnapScalarToGrid(0.137f, -0.05f), 0.137f);
}

TEST(EditorSnappingTests, SnapToGridSnapsBothAxes)
{
    const mfd::Vec2 snapped = SnapToGrid(mfd::Vec2 {0.13f, -0.24f}, 0.05f);
    EXPECT_FLOAT_EQ(snapped.x, 0.15f);
    EXPECT_FLOAT_EQ(snapped.y, -0.25f);
}

TEST(EditorSnappingTests, ArrowNudgeCombinesActiveDirections)
{
    const mfd::Vec2 delta = ArrowNudgeDelta(false, true, true, false, 0.01f);
    EXPECT_FLOAT_EQ(delta.x, 0.01f);
    EXPECT_FLOAT_EQ(delta.y, 0.01f);

    const mfd::Vec2 opposed = ArrowNudgeDelta(true, true, true, true, 0.01f);
    EXPECT_FLOAT_EQ(opposed.x, 0.0f);
    EXPECT_FLOAT_EQ(opposed.y, 0.0f);
}

TEST(EditorSnappingTests, ArrowNudgeIgnoresNonPositiveStep)
{
    const mfd::Vec2 delta = ArrowNudgeDelta(true, false, false, true, 0.0f);
    EXPECT_FLOAT_EQ(delta.x, 0.0f);
    EXPECT_FLOAT_EQ(delta.y, 0.0f);
}
} // namespace

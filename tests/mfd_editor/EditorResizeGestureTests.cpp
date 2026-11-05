/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "internal/application/EditorResizeGesture.h"

#include <gtest/gtest.h>

TEST(EditorResizeGestureTests, OffsetPressAndSubthresholdJitterStayPending)
{
    editor::app::EditorResizeGesture gesture;
    gesture.Begin({40.0f, 0.0f}, {});
    EXPECT_FALSE(gesture.ScaleAt({40.0f, 0.0f}, 6.0f));
    EXPECT_FALSE(gesture.ScaleAt({42.0f, 0.0f}, 6.0f));
}

TEST(EditorResizeGestureTests, DragScalesFromActualPressAndCanReturnToOriginalSize)
{
    editor::app::EditorResizeGesture gesture;
    gesture.Begin({140.0f, 100.0f}, {100.0f, 100.0f});
    EXPECT_EQ(gesture.ScaleAt({160.0f, 100.0f}, 6.0f), 1.5f);
    EXPECT_EQ(gesture.ScaleAt({140.0f, 100.0f}, 6.0f), 1.0f);
    gesture.Begin({140.0f, 100.0f}, {100.0f, 100.0f});
    EXPECT_FALSE(gesture.ScaleAt({141.0f, 100.0f}, 6.0f));
}

TEST(EditorResizeGestureTests, PressAtPivotRemainsFiniteAndScaleIsBounded)
{
    editor::app::EditorResizeGesture gesture;
    gesture.Begin({}, {});
    EXPECT_FALSE(gesture.ScaleAt({}, 0.0f));
    EXPECT_EQ(gesture.ScaleAt({1000.0f, 0.0f}, 6.0f), 10.0f);
    EXPECT_EQ(gesture.ScaleAt({}, 6.0f), 1.0f);
    gesture.Begin({1000.0f, 0.0f}, {});
    EXPECT_EQ(gesture.ScaleAt({}, 6.0f), 0.1f);
}

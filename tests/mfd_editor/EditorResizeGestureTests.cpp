/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "internal/application/EditorResizeGesture.h"

#include <gtest/gtest.h>

#include <initializer_list>

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

TEST(EditorResizeGestureTests, AllFourCornerPressesUseTheSamePixelThresholdAtDifferentZooms)
{
    const ImVec2 center {180.0f, 90.0f};
    for (const float halfWidth : {20.0f, 80.0f})
    {
        for (const float xSign : {-1.0f, 1.0f})
        {
            for (const float ySign : {-1.0f, 1.0f})
            {
                // Press ten pixels inside each corner, within the 16 px hit radius.
                const ImVec2 press {center.x + xSign * (halfWidth - 10.0f),
                                    center.y + ySign * halfWidth};
                editor::app::EditorResizeGesture gesture;
                gesture.Begin(press, center);
                EXPECT_FALSE(gesture.ScaleAt(press, 6.0f));
                EXPECT_FALSE(gesture.ScaleAt({press.x + 2.0f, press.y}, 6.0f));

                const ImVec2 doubled {2.0f * press.x - center.x, 2.0f * press.y - center.y};
                EXPECT_EQ(gesture.ScaleAt(doubled, 6.0f), 2.0f);
            }
        }
    }
}

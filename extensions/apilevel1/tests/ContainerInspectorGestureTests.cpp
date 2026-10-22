/* This file is part of MFDStudio. Project author: Benoit Fra. */
#include "ContainerInspectorGesture.h"
#include <gtest/gtest.h>

TEST(ContainerInspectorGestureTests, ActivationIdleDragAndReleaseShareOneIdentity)
{
    mfd::api1::detail::ContainerInspectorGesture gesture;
    EXPECT_EQ(gesture.Update(0, 0U, 0U, false), 0U);
    const auto first = gesture.Update(1, 17U, 0U, true);
    ASSERT_NE(first, 0U);
    EXPECT_EQ(gesture.Update(2, 17U, 17U, false), first);
    EXPECT_EQ(gesture.Update(3, 17U, 0U, false), first);
    EXPECT_EQ(gesture.Update(4, 17U, 17U, false), first);
    EXPECT_EQ(gesture.Update(5, 0U, 17U, false), first); // Keyboard commit on deactivation.
    EXPECT_EQ(gesture.Update(6, 0U, 0U, false), first);
    EXPECT_NE(gesture.Update(7, 17U, 17U, true), first);
}

TEST(ContainerInspectorGestureTests, ClicksSelectionChangesAndClosedInspectorSeparateEdits)
{
    mfd::api1::detail::ContainerInspectorGesture gesture;
    const auto first = gesture.Update(0, 0U, 1U, true);
    const auto second = gesture.Update(1, 0U, 1U, true);
    EXPECT_NE(first, second);
    const auto drag = gesture.Update(2, 2U, 2U, true);
    EXPECT_NE(drag, second);
    const auto other = gesture.Update(3, 3U, 3U, true);
    EXPECT_NE(other, drag);
    EXPECT_NE(gesture.Update(20, 3U, 3U, false), other); // Inspector not drawn in between.
}

TEST(ContainerInspectorGestureTests, CanceledActivationCannotReuseThePreviousGesture)
{
    mfd::api1::detail::ContainerInspectorGesture gesture;
    const auto first = gesture.Update(0, 5U, 5U, true);
    gesture.Update(1, 0U, 0U, false);
    const auto canceled = gesture.Update(2, 5U, 0U, true);
    gesture.Update(3, 0U, 0U, false);
    const auto next = gesture.Update(4, 5U, 5U, true);
    EXPECT_NE(canceled, first);
    EXPECT_NE(next, canceled);
}

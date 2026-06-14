/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorUndoHistory.h"

#include <gtest/gtest.h>

namespace
{
using editor::UndoHistory;

TEST(EditorUndoHistoryTests, EmptyHistoryHasNothingToStep)
{
    UndoHistory<int> history;
    EXPECT_FALSE(history.CanUndo());
    EXPECT_FALSE(history.CanRedo());
    EXPECT_FALSE(history.Undo(7).has_value());
    EXPECT_FALSE(history.Redo(7).has_value());
}

TEST(EditorUndoHistoryTests, UndoReturnsRecordedStateAndEnablesRedo)
{
    UndoHistory<int> history;
    history.Record(1); // pre-edit state captured before moving to 2

    const std::optional<int> previous = history.Undo(2);
    ASSERT_TRUE(previous.has_value());
    EXPECT_EQ(*previous, 1);
    EXPECT_FALSE(history.CanUndo());
    EXPECT_TRUE(history.CanRedo());
}

TEST(EditorUndoHistoryTests, RedoReturnsTheStateThatWasUndone)
{
    UndoHistory<int> history;
    history.Record(1);

    const std::optional<int> previous = history.Undo(2);
    ASSERT_TRUE(previous.has_value());

    const std::optional<int> next = history.Redo(1);
    ASSERT_TRUE(next.has_value());
    EXPECT_EQ(*next, 2);
    EXPECT_TRUE(history.CanUndo());
    EXPECT_FALSE(history.CanRedo());
}

TEST(EditorUndoHistoryTests, RecordingAfterUndoClearsRedo)
{
    UndoHistory<int> history;
    history.Record(1);
    EXPECT_TRUE(history.Undo(2).has_value());
    ASSERT_TRUE(history.CanRedo());

    history.Record(2); // a fresh edit invalidates the forward branch
    EXPECT_FALSE(history.CanRedo());
    EXPECT_TRUE(history.CanUndo());
}

TEST(EditorUndoHistoryTests, CapacityDropsTheOldestUndoState)
{
    UndoHistory<int> history(2U);
    history.Record(10);
    history.Record(20);
    history.Record(30); // exceeds capacity, oldest (10) is dropped

    EXPECT_EQ(history.UndoDepth(), 2U);

    EXPECT_EQ(history.Undo(40).value_or(-1), 30);
    EXPECT_EQ(history.Undo(30).value_or(-1), 20);
    EXPECT_FALSE(history.CanUndo());
}

TEST(EditorUndoHistoryTests, CapacityIsClampedToAtLeastOne)
{
    UndoHistory<int> history(0U);
    EXPECT_EQ(history.Capacity(), 1U);

    history.Record(1);
    history.Record(2);
    EXPECT_EQ(history.UndoDepth(), 1U);
    EXPECT_EQ(history.Undo(3).value_or(-1), 2);
}
} // namespace

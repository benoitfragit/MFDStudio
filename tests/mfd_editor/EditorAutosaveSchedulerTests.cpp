/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorAutosaveScheduler.h"

#include <gtest/gtest.h>

#include <limits>

namespace
{
using editor::EditorAutosaveScheduler;

TEST(EditorAutosaveSchedulerTests, NotDueWhileDocumentIsClean)
{
    EditorAutosaveScheduler scheduler;
    scheduler.Advance(100.0f);
    EXPECT_FALSE(scheduler.DueForAutosave(30.0f, false));
}

TEST(EditorAutosaveSchedulerTests, DueOnlyAfterTheIntervalElapsesWhileDirty)
{
    EditorAutosaveScheduler scheduler;
    scheduler.Advance(29.0f);
    EXPECT_FALSE(scheduler.DueForAutosave(30.0f, true));

    scheduler.Advance(2.0f);
    EXPECT_TRUE(scheduler.DueForAutosave(30.0f, true));
}

TEST(EditorAutosaveSchedulerTests, MarkAutosavedRestartsTheTimer)
{
    EditorAutosaveScheduler scheduler;
    scheduler.Advance(40.0f);
    ASSERT_TRUE(scheduler.DueForAutosave(30.0f, true));

    scheduler.MarkAutosaved();
    EXPECT_FALSE(scheduler.DueForAutosave(30.0f, true));
    EXPECT_FLOAT_EQ(scheduler.ElapsedSeconds(), 0.0f);
}

TEST(EditorAutosaveSchedulerTests, ResetClearsAccumulatedTime)
{
    EditorAutosaveScheduler scheduler;
    scheduler.Advance(40.0f);
    scheduler.Reset();
    EXPECT_FLOAT_EQ(scheduler.ElapsedSeconds(), 0.0f);
    EXPECT_FALSE(scheduler.DueForAutosave(30.0f, true));
}

TEST(EditorAutosaveSchedulerTests, NonFiniteAndNonPositiveDeltasAreIgnored)
{
    EditorAutosaveScheduler scheduler;
    scheduler.Advance(std::numeric_limits<float>::quiet_NaN());
    scheduler.Advance(std::numeric_limits<float>::infinity());
    scheduler.Advance(-5.0f);
    scheduler.Advance(0.0f);
    EXPECT_FLOAT_EQ(scheduler.ElapsedSeconds(), 0.0f);
}

TEST(EditorAutosaveSchedulerTests, IntervalIsClampedToAtLeastOneSecond)
{
    EditorAutosaveScheduler scheduler;
    scheduler.Advance(0.5f);
    EXPECT_FALSE(scheduler.DueForAutosave(0.0f, true));

    scheduler.Advance(0.6f);
    EXPECT_TRUE(scheduler.DueForAutosave(0.0f, true));
}
} // namespace

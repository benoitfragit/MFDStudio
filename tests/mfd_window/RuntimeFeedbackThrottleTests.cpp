/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for runtime feedback cadence throttling.
 */

#include <gtest/gtest.h>

#include "mfd/window/RuntimeFeedbackThrottle.hpp"

TEST(RuntimeFeedbackThrottleTests, SendsFirstChangedFeedbackImmediately)
{
    mfd::window::RuntimeFeedbackThrottle throttle;

    EXPECT_TRUE(throttle.ShouldSendChanged(0.020f));
    EXPECT_FALSE(throttle.ShouldSendHeartbeat(0.250f));
}

TEST(RuntimeFeedbackThrottleTests, ThrottlesChangedFeedbackUntilFastInterval)
{
    mfd::window::RuntimeFeedbackThrottle throttle;

    ASSERT_TRUE(throttle.ShouldSendChanged(0.020f));
    throttle.MarkChangedSent(0.020f);

    throttle.Advance(0.010f);
    EXPECT_FALSE(throttle.ShouldSendChanged(0.020f));

    throttle.Advance(0.010f);
    EXPECT_TRUE(throttle.ShouldSendChanged(0.020f));
}

TEST(RuntimeFeedbackThrottleTests, SendsUnchangedFeedbackOnlyOnHeartbeat)
{
    mfd::window::RuntimeFeedbackThrottle throttle;

    throttle.MarkChangedSent(0.020f);
    throttle.Advance(0.100f);
    EXPECT_FALSE(throttle.ShouldSendHeartbeat(0.250f));

    throttle.Advance(0.150f);
    EXPECT_TRUE(throttle.ShouldSendHeartbeat(0.250f));
    throttle.MarkHeartbeatSent(0.250f);
    EXPECT_FALSE(throttle.ShouldSendHeartbeat(0.250f));
}

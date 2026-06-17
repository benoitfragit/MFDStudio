/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for the client-side window liveness monitor.
 */

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "mfd/client/WindowLivenessMonitor.h"

namespace
{
using mfd::client::WindowConnectionState;
using mfd::client::WindowLivenessMonitor;

TEST(WindowLivenessMonitorTests, StartsWaitingForFirstContact)
{
    WindowLivenessMonitor monitor(2.0);

    EXPECT_EQ(monitor.State(), WindowConnectionState::WaitingForFirstContact);
    EXPECT_FALSE(monitor.ConsumeDisconnect());
}

TEST(WindowLivenessMonitorTests, FirstActivityConnects)
{
    WindowLivenessMonitor monitor(2.0);

    monitor.RecordActivity(0.0);

    EXPECT_EQ(monitor.State(), WindowConnectionState::Connected);
    EXPECT_FALSE(monitor.ConsumeDisconnect());
}

TEST(WindowLivenessMonitorTests, StaysConnectedWhileHeartbeatArrivesBeforeTimeout)
{
    WindowLivenessMonitor monitor(2.0);

    monitor.RecordActivity(0.0);
    EXPECT_EQ(monitor.Update(1.0), WindowConnectionState::Connected);
    monitor.RecordActivity(1.5);
    EXPECT_EQ(monitor.Update(3.0), WindowConnectionState::Connected);
    EXPECT_FALSE(monitor.ConsumeDisconnect());
}

TEST(WindowLivenessMonitorTests, ReportsLostAfterTimeout)
{
    WindowLivenessMonitor monitor(2.0);

    monitor.RecordActivity(0.0);
    EXPECT_EQ(monitor.Update(2.5), WindowConnectionState::Lost);
    EXPECT_TRUE(monitor.ConsumeDisconnect());
    EXPECT_FALSE(monitor.ConsumeDisconnect());
}

TEST(WindowLivenessMonitorTests, GracefulClosingReportsClosedImmediately)
{
    WindowLivenessMonitor monitor(60.0);

    monitor.RecordActivity(0.0);
    monitor.RecordWindowClosing(0.1);

    EXPECT_EQ(monitor.State(), WindowConnectionState::Closed);
    EXPECT_TRUE(monitor.ConsumeDisconnect());
    EXPECT_FALSE(monitor.ConsumeDisconnect());
}

TEST(WindowLivenessMonitorTests, ClosingIsStickyAgainstLaterActivity)
{
    WindowLivenessMonitor monitor(2.0);

    monitor.RecordActivity(0.0);
    monitor.RecordWindowClosing(0.1);
    monitor.RecordActivity(0.2);

    EXPECT_EQ(monitor.State(), WindowConnectionState::Closed);
}

TEST(WindowLivenessMonitorTests, ResetReArmsForReconnection)
{
    WindowLivenessMonitor monitor(2.0);

    monitor.RecordActivity(0.0);
    EXPECT_EQ(monitor.Update(5.0), WindowConnectionState::Lost);
    ASSERT_TRUE(monitor.ConsumeDisconnect());

    monitor.Reset();
    EXPECT_EQ(monitor.State(), WindowConnectionState::WaitingForFirstContact);
    EXPECT_FALSE(monitor.ConsumeDisconnect());

    monitor.RecordActivity(6.0);
    EXPECT_EQ(monitor.State(), WindowConnectionState::Connected);
}

TEST(WindowLivenessMonitorTests, ActivityAfterTimeoutReconnects)
{
    WindowLivenessMonitor monitor(2.0);

    monitor.RecordActivity(0.0);
    EXPECT_EQ(monitor.Update(5.0), WindowConnectionState::Lost);
    EXPECT_TRUE(monitor.ConsumeDisconnect());

    monitor.RecordActivity(6.0);
    EXPECT_EQ(monitor.State(), WindowConnectionState::Connected);
}

TEST(WindowLivenessMonitorTests, ObserveTreatsCounterIncreaseAsActivity)
{
    WindowLivenessMonitor monitor(2.0);

    EXPECT_EQ(monitor.Observe(0U, false, 0.0), WindowConnectionState::WaitingForFirstContact);
    EXPECT_EQ(monitor.Observe(3U, false, 0.5), WindowConnectionState::Connected);
    EXPECT_EQ(monitor.Observe(3U, false, 1.0), WindowConnectionState::Connected);
    EXPECT_EQ(monitor.Observe(3U, false, 3.0), WindowConnectionState::Lost);
    EXPECT_TRUE(monitor.ConsumeDisconnect());
}

TEST(WindowLivenessMonitorTests, ObserveHonorsClosingFlag)
{
    WindowLivenessMonitor monitor(60.0);

    EXPECT_EQ(monitor.Observe(1U, false, 0.0), WindowConnectionState::Connected);
    EXPECT_EQ(monitor.Observe(1U, true, 0.2), WindowConnectionState::Closed);
    EXPECT_TRUE(monitor.ConsumeDisconnect());
}

TEST(WindowLivenessMonitorTests, IgnoresNonFiniteTimestamps)
{
    WindowLivenessMonitor monitor(2.0);

    monitor.RecordActivity(std::numeric_limits<double>::quiet_NaN());
    EXPECT_EQ(monitor.State(), WindowConnectionState::WaitingForFirstContact);

    monitor.RecordActivity(0.0);
    EXPECT_EQ(monitor.Update(std::numeric_limits<double>::infinity()), WindowConnectionState::Connected);
    EXPECT_FALSE(monitor.ConsumeDisconnect());
}

TEST(WindowLivenessMonitorTests, ClampsNonFiniteTimeout)
{
    WindowLivenessMonitor monitor(std::numeric_limits<double>::quiet_NaN());

    EXPECT_TRUE(std::isfinite(monitor.TimeoutSeconds()));
    EXPECT_GT(monitor.TimeoutSeconds(), 0.0);
}
} // namespace

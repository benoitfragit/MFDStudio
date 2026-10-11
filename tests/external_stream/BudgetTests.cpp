#include "StreamBudget.h"
#include <gtest/gtest.h>
#include <limits>

namespace
{
using mfd::stream::detail::BudgetUnavailable;
using mfd::stream::detail::ConfigurationRetry;
using mfd::stream::detail::MemoryReservation;
using mfd::stream::detail::ReservedStreamBytes;
using mfd::stream::detail::kStreamMemoryBudget;

TEST(ExternalStreamBudget, RejectionPreservesExistingReservationAndRecoversAfterRelease)
{
    const auto initial = ReservedStreamBytes();
    {
        MemoryReservation existing(1024);
        {
            MemoryReservation pressure(kStreamMemoryBudget - initial - 1024);
            EXPECT_THROW(MemoryReservation next(1), BudgetUnavailable);
            EXPECT_EQ(ReservedStreamBytes(), kStreamMemoryBudget);
        }
        EXPECT_EQ(ReservedStreamBytes(), initial + 1024);
        MemoryReservation replacement(2048);
        EXPECT_EQ(ReservedStreamBytes(), initial + 3072);
    }
    EXPECT_EQ(ReservedStreamBytes(), initial);
}
TEST(ExternalStreamBudget, PartialGenerationFailureReleasesItsReservations)
{
    const auto initial = ReservedStreamBytes();
    try
    {
        MemoryReservation sharedTextures(4096);
        MemoryReservation readback(kStreamMemoryBudget);
        FAIL() << "Oversubscribed readback was accepted";
    }
    catch (const BudgetUnavailable&)
    {
        EXPECT_EQ(ReservedStreamBytes(), initial);
    }
    EXPECT_THROW(MemoryReservation invalid(std::numeric_limits<std::uint64_t>::max()), BudgetUnavailable);
    EXPECT_EQ(ReservedStreamBytes(), initial);
}
TEST(ExternalStreamBudget, TightTicksCannotRetryAllocationsMoreThanFourTimesPerSecond)
{
    ConfigurationRetry retry;
    unsigned int attempts = 0;
    for (std::uint64_t now = 1000; now < 2000; ++now)
    {
        if (retry.Ready(now)) { ++attempts; retry.Defer(now); }
    }
    EXPECT_EQ(attempts, 4U);
    EXPECT_TRUE(retry.Deferred());
    EXPECT_FALSE(retry.Ready(1999));
    EXPECT_TRUE(retry.Ready(2000));
    retry.Reset();
    EXPECT_FALSE(retry.Deferred());
    EXPECT_TRUE(retry.Ready(2000));
}
} // namespace

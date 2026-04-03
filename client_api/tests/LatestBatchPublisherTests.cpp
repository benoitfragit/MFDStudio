/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "mfd/client/LatestBatchPublisher.h"

namespace
{
mfd::CommandBatch MakeBatch(const std::uint32_t sequence)
{
    mfd::CommandBatch batch;
    batch.sequence = sequence;
    return batch;
}
} // namespace

TEST(LatestBatchPublisherTests, FlushDeliversAcceptedBatch)
{
    std::mutex mutex;
    std::condition_variable condition;
    std::vector<std::uint32_t> deliveredSequences;

    mfd::client::LatestBatchPublisher publisher(
        [&mutex, &condition, &deliveredSequences](const mfd::CommandBatch& batch)
        {
            {
                std::lock_guard lock(mutex);
                deliveredSequences.push_back(batch.sequence);
            }
            condition.notify_all();
            return true;
        });

    ASSERT_TRUE(publisher.IsReady());
    EXPECT_TRUE(publisher.SubmitLatest(MakeBatch(42)));
    publisher.Flush();

    std::lock_guard lock(mutex);
    ASSERT_EQ(deliveredSequences.size(), 1U);
    EXPECT_EQ(deliveredSequences.front(), 42U);
    EXPECT_TRUE(publisher.LastError().empty());
}

TEST(LatestBatchPublisherTests, KeepsOnlyNewestPendingBatchWhileBusy)
{
    using namespace std::chrono_literals;

    std::mutex mutex;
    std::condition_variable condition;
    bool releaseFirstSend = false;
    std::size_t enteredSendCount = 0;
    std::vector<std::uint32_t> deliveredSequences;

    mfd::client::LatestBatchPublisher publisher(
        [&mutex, &condition, &releaseFirstSend, &enteredSendCount, &deliveredSequences](const mfd::CommandBatch& batch)
        {
            std::unique_lock lock(mutex);
            ++enteredSendCount;
            deliveredSequences.push_back(batch.sequence);
            condition.notify_all();

            if (enteredSendCount == 1U)
            {
                condition.wait(
                    lock,
                    [&releaseFirstSend]()
                    {
                        return releaseFirstSend;
                    });
            }

            return true;
        });

    ASSERT_TRUE(publisher.IsReady());
    ASSERT_TRUE(publisher.SubmitLatest(MakeBatch(1)));

    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(condition.wait_for(
            lock,
            1s,
            [&enteredSendCount]()
            {
                return enteredSendCount >= 1U;
            }));
    }

    EXPECT_TRUE(publisher.SubmitLatest(MakeBatch(2)));
    EXPECT_TRUE(publisher.SubmitLatest(MakeBatch(3)));

    {
        std::lock_guard lock(mutex);
        releaseFirstSend = true;
    }
    condition.notify_all();

    publisher.Flush();

    std::lock_guard lock(mutex);
    ASSERT_EQ(deliveredSequences.size(), 2U);
    EXPECT_EQ(deliveredSequences[0], 1U);
    EXPECT_EQ(deliveredSequences[1], 3U);
}

TEST(LatestBatchPublisherTests, ReportsAndClearsTransportErrors)
{
    std::mutex mutex;
    bool failNextSend = true;

    mfd::client::LatestBatchPublisher publisher(
        [&mutex, &failNextSend](const mfd::CommandBatch&)
        {
            std::lock_guard lock(mutex);
            const bool shouldFail = failNextSend;
            failNextSend = false;
            return !shouldFail;
        },
        []()
        {
            return std::string {"transport failure"};
        });

    ASSERT_TRUE(publisher.SubmitLatest(MakeBatch(7)));
    publisher.Flush();
    EXPECT_EQ(publisher.LastError(), "transport failure");

    ASSERT_TRUE(publisher.SubmitLatest(MakeBatch(8)));
    publisher.Flush();
    EXPECT_TRUE(publisher.LastError().empty());
}

TEST(LatestBatchPublisherTests, RejectsSubmissionsAfterStop)
{
    mfd::client::LatestBatchPublisher publisher(
        [](const mfd::CommandBatch&)
        {
            return true;
        });

    publisher.Stop();
    EXPECT_FALSE(publisher.SubmitLatest(MakeBatch(9)));
    EXPECT_EQ(publisher.LastError(), "Latest batch publisher has been stopped");
}

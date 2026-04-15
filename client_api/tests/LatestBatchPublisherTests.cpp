/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for LatestBatchPublisherTests.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
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

template <typename CommandType>
bool ContainsCommandType(const mfd::CommandBatch& batch)
{
    for (const mfd::UserCommand& command : batch.commands)
    {
        const bool found = std::visit(
            [](const auto& value) noexcept
            {
                return std::is_same_v<std::decay_t<decltype(value)>, CommandType>;
            },
            command);

        if (found)
        {
            return true;
        }
    }

    return false;
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

TEST(LatestBatchPublisherTests, PreservesPendingDynamicReticleLifecycleCommands)
{
    using namespace std::chrono_literals;

    std::mutex mutex;
    std::condition_variable condition;
    bool releaseFirstSend = false;
    std::size_t enteredSendCount = 0;
    std::vector<mfd::CommandBatch> deliveredBatches;

    mfd::client::LatestBatchPublisher publisher(
        [&mutex, &condition, &releaseFirstSend, &enteredSendCount, &deliveredBatches](const mfd::CommandBatch& batch)
        {
            std::unique_lock lock(mutex);
            ++enteredSendCount;
            deliveredBatches.push_back(batch);
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

    mfd::CommandBatch firstBatch;
    firstBatch.sequence = 1U;
    firstBatch.commands.push_back(mfd::SetPageViewCommand {"tactical", mfd::PageViewState {}});
    ASSERT_TRUE(publisher.SubmitLatest(std::move(firstBatch)));

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

    mfd::CommandBatch secondBatch;
    secondBatch.sequence = 2U;
    secondBatch.commands.push_back(mfd::RemoveDynamicReticleCommand {mfd::ReticleHandle {"radar", "trk_01"}});
    ASSERT_TRUE(publisher.SubmitLatest(std::move(secondBatch)));

    mfd::CommandBatch thirdBatch;
    thirdBatch.sequence = 3U;
    thirdBatch.commands.push_back(mfd::ActivatePageCommand {"radar"});
    ASSERT_TRUE(publisher.SubmitLatest(std::move(thirdBatch)));

    {
        std::lock_guard lock(mutex);
        releaseFirstSend = true;
    }
    condition.notify_all();

    publisher.Flush();

    std::lock_guard lock(mutex);
    ASSERT_EQ(deliveredBatches.size(), 2U);
    EXPECT_EQ(deliveredBatches[1].sequence, 3U);
    EXPECT_TRUE(ContainsCommandType<mfd::RemoveDynamicReticleCommand>(deliveredBatches[1]));
    EXPECT_TRUE(ContainsCommandType<mfd::ActivatePageCommand>(deliveredBatches[1]));
    EXPECT_FALSE(ContainsCommandType<mfd::SetPageViewCommand>(deliveredBatches[1]));
}

TEST(LatestBatchPublisherTests, NewDynamicReticleLifecycleStateOverridesPendingState)
{
    using namespace std::chrono_literals;

    std::mutex mutex;
    std::condition_variable condition;
    bool releaseFirstSend = false;
    std::size_t enteredSendCount = 0;
    std::vector<mfd::CommandBatch> deliveredBatches;

    mfd::client::LatestBatchPublisher publisher(
        [&mutex, &condition, &releaseFirstSend, &enteredSendCount, &deliveredBatches](const mfd::CommandBatch& batch)
        {
            std::unique_lock lock(mutex);
            ++enteredSendCount;
            deliveredBatches.push_back(batch);
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

    ASSERT_TRUE(publisher.SubmitLatest(MakeBatch(1U)));

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

    mfd::CommandBatch secondBatch;
    secondBatch.sequence = 2U;
    secondBatch.commands.push_back(mfd::RemoveDynamicReticleCommand {mfd::ReticleHandle {"radar", "trk_02"}});
    ASSERT_TRUE(publisher.SubmitLatest(std::move(secondBatch)));

    mfd::ReticlePatch patch;
    patch.visible = true;
    mfd::CommandBatch thirdBatch;
    thirdBatch.sequence = 3U;
    thirdBatch.commands.push_back(
        mfd::UpsertDynamicReticleCommand {mfd::ReticleHandle {"radar", "trk_02"}, "radar_track", patch});
    ASSERT_TRUE(publisher.SubmitLatest(std::move(thirdBatch)));

    {
        std::lock_guard lock(mutex);
        releaseFirstSend = true;
    }
    condition.notify_all();

    publisher.Flush();

    std::lock_guard lock(mutex);
    ASSERT_EQ(deliveredBatches.size(), 2U);
    EXPECT_EQ(deliveredBatches[1].commands.size(), 1U);
    EXPECT_TRUE(ContainsCommandType<mfd::UpsertDynamicReticleCommand>(deliveredBatches[1]));
    EXPECT_FALSE(ContainsCommandType<mfd::RemoveDynamicReticleCommand>(deliveredBatches[1]));
}


/**
 * @brief Verifies the vector overload forwards both sequence and command payload unchanged.
 */
TEST(LatestBatchPublisherTests, SubmitLatestVectorOverloadPreservesSequenceAndCommands)
{
    std::mutex mutex;
    std::condition_variable condition;
    mfd::CommandBatch delivered;
    bool deliveredReady = false;

    mfd::client::LatestBatchPublisher publisher(
        [&mutex, &condition, &delivered, &deliveredReady](const mfd::CommandBatch& batch)
        {
            {
                std::lock_guard lock(mutex);
                delivered = batch;
                deliveredReady = true;
            }
            condition.notify_all();
            return true;
        });

    std::vector<mfd::UserCommand> commands;
    commands.push_back(mfd::ActivatePageCommand {"radar"});

    ASSERT_TRUE(publisher.SubmitLatest(std::move(commands), 77U));
    publisher.Flush();

    std::lock_guard lock(mutex);
    ASSERT_TRUE(deliveredReady);
    EXPECT_EQ(delivered.sequence, 77U);
    ASSERT_EQ(delivered.commands.size(), 1U);
    const auto* activate = std::get_if<mfd::ActivatePageCommand>(&delivered.commands.front());
    ASSERT_NE(activate, nullptr);
    EXPECT_EQ(activate->pageName, "radar");
}

/**
 * @brief Ensures construction without a send callback is rejected with a stable readiness error.
 */
TEST(LatestBatchPublisherTests, ReportsNotReadyWhenSendCallbackIsMissing)
{
    mfd::client::LatestBatchPublisher publisher({}, {});

    EXPECT_FALSE(publisher.IsReady());
    EXPECT_EQ(publisher.LastError(), "Latest batch publisher requires a valid send callback");

    EXPECT_FALSE(publisher.SubmitLatest(MakeBatch(1U)));
    EXPECT_EQ(publisher.LastError(), "Latest batch publisher requires a valid send callback");
}

/**
 * @brief Falls back to the default send-failure error when no transport-specific error callback exists.
 */
TEST(LatestBatchPublisherTests, TurnsSendFailureIntoDefaultErrorWhenNoErrorCallbackIsProvided)
{
    mfd::client::LatestBatchPublisher publisher(
        [](const mfd::CommandBatch&)
        {
            return false;
        });

    ASSERT_TRUE(publisher.SubmitLatest(MakeBatch(13U)));
    publisher.Flush();

    EXPECT_EQ(publisher.LastError(), "Unable to send the latest command batch");
}

/**
 * @brief Propagates std::exception messages thrown by the send callback into LastError().
 */
TEST(LatestBatchPublisherTests, CapturesThrownExceptionsFromSendCallback)
{
    mfd::client::LatestBatchPublisher publisher(
        [](const mfd::CommandBatch&)
        {
            throw std::runtime_error("simulated send crash");
            return true;
        });

    ASSERT_TRUE(publisher.SubmitLatest(MakeBatch(55U)));
    publisher.Flush();

    EXPECT_EQ(publisher.LastError(), "simulated send crash");
}

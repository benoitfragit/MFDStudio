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
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
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
        if (std::holds_alternative<CommandType>(command))
        {
            return true;
        }
    }

    return false;
}

const mfd::UpdateReticleCommand* FindStaticReticleUpdate(const mfd::CommandBatch& batch,
                                                         const std::string& page,
                                                         const std::string& reticle)
{
    for (const mfd::UserCommand& command : batch.commands)
    {
        const auto* update = std::get_if<mfd::UpdateReticleCommand>(&command);
        if (update != nullptr && update->target.page == page && update->target.reticle == reticle)
        {
            return update;
        }
    }

    return nullptr;
}

std::size_t CountStaticReticleUpdates(const mfd::CommandBatch& batch,
                                      const std::string& page,
                                      const std::string& reticle)
{
    std::size_t count = 0;
    for (const mfd::UserCommand& command : batch.commands)
    {
        const auto* update = std::get_if<mfd::UpdateReticleCommand>(&command);
        if (update != nullptr && update->target.page == page && update->target.reticle == reticle)
        {
            ++count;
        }
    }

    return count;
}

constexpr auto kShutdownSyncTimeout = std::chrono::seconds {1};

// New shutdown tests run in a child process: even a broken join cannot hang
// the suite. CTest's existing per-test timeout remains the outer guard.
class PublisherShutdownDeadline
{
public:
    PublisherShutdownDeadline()
        : watchdog_([this]()
          {
              std::unique_lock lock(mutex_);
              if (!condition_.wait_for(lock, std::chrono::seconds {10},
                                       [this]() { return completed_; }))
              {
                  std::fputs("Publisher shutdown timed out\n", stderr);
                  std::_Exit(EXIT_FAILURE);
              }
          })
    {
    }

    ~PublisherShutdownDeadline()
    {
        {
            std::lock_guard lock(mutex_);
            completed_ = true;
        }
        condition_.notify_all();
        watchdog_.join();
    }

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    bool completed_ = false;
    std::thread watchdog_;
};

enum class ShutdownSendResult
{
    Success,
    Failure,
    Exception,
    UnknownException
};

void CheckStopDuringSend(const ShutdownSendResult result, const bool flushConcurrently)
{
    PublisherShutdownDeadline deadline;
    std::mutex mutex;
    std::condition_variable condition;
    bool enteredSend = false;
    bool releaseSend = false;
    bool flushStarted = false;
    bool flushCompleted = false;
    bool stopCompleted = false;
    std::size_t sendCount = 0U;

    mfd::client::LatestBatchPublisher publisher(
        [&mutex, &condition, &enteredSend, &releaseSend, &sendCount, result](const mfd::CommandBatch&)
        {
            {
                std::unique_lock lock(mutex);
                ++sendCount;
                enteredSend = true;
                condition.notify_all();
                EXPECT_TRUE(condition.wait_for(lock, std::chrono::seconds {5},
                                               [&releaseSend]() { return releaseSend; }));
            }
            if (result == ShutdownSendResult::Exception)
            {
                throw std::runtime_error("shutdown send failure");
            }
            if (result == ShutdownSendResult::UnknownException)
            {
                throw 42;
            }
            return result == ShutdownSendResult::Success;
        },
        []() { return std::string {"shutdown transport failure"}; });

    EXPECT_TRUE(publisher.SubmitLatest(MakeBatch(1U)));
    {
        std::unique_lock lock(mutex);
        EXPECT_TRUE(condition.wait_for(lock, kShutdownSyncTimeout,
                                       [&enteredSend]() { return enteredSend; }));
    }
    // Leave a second batch pending to verify Stop abandons unsent work too.
    EXPECT_TRUE(publisher.SubmitLatest(MakeBatch(2U)));

    std::thread flusher;
    if (flushConcurrently)
    {
        flusher = std::thread([&publisher, &mutex, &condition, &flushStarted, &flushCompleted]()
        {
            {
                std::lock_guard lock(mutex);
                flushStarted = true;
            }
            condition.notify_all();
            publisher.Flush();
            {
                std::lock_guard lock(mutex);
                flushCompleted = true;
            }
            condition.notify_all();
        });
        std::unique_lock lock(mutex);
        EXPECT_TRUE(condition.wait_for(lock, kShutdownSyncTimeout,
                                       [&flushStarted]() { return flushStarted; }));
        EXPECT_FALSE(flushCompleted);
    }

    std::thread stopper([&publisher, &mutex, &condition, &stopCompleted]()
    {
        condition.notify_all();
        publisher.Stop();
        {
            std::lock_guard lock(mutex);
            stopCompleted = true;
        }
        condition.notify_all();
    });
    {
        std::unique_lock lock(mutex);
        // IsReady observes stopRequested under the publisher mutex. The early
        // notification is only a wakeup hint; elapsed time never proves Stop.
        EXPECT_TRUE(condition.wait_for(lock, kShutdownSyncTimeout,
                                       [&publisher]() { return !publisher.IsReady(); }));
        EXPECT_FALSE(stopCompleted);
        if (flushConcurrently)
        {
            EXPECT_FALSE(flushCompleted);
        }
        releaseSend = true;
    }
    condition.notify_all();

    {
        std::unique_lock lock(mutex);
        EXPECT_TRUE(condition.wait_for(lock, kShutdownSyncTimeout,
                                       [&stopCompleted]() { return stopCompleted; }));
        if (flushConcurrently)
        {
            EXPECT_TRUE(condition.wait_for(lock, kShutdownSyncTimeout,
                                           [&flushCompleted]() { return flushCompleted; }));
        }
    }
    // No fatal assertions can bypass callback release or thread cleanup.
    stopper.join();
    if (flusher.joinable())
    {
        flusher.join();
    }

    EXPECT_EQ(sendCount, 1U);
    EXPECT_FALSE(publisher.IsReady());
    if (result == ShutdownSendResult::Success)
    {
        EXPECT_TRUE(publisher.LastError().empty());
    }
    else if (result == ShutdownSendResult::Failure)
    {
        EXPECT_EQ(publisher.LastError(), "shutdown transport failure");
    }
    else if (result == ShutdownSendResult::Exception)
    {
        EXPECT_EQ(publisher.LastError(), "shutdown send failure");
    }
    else
    {
        EXPECT_EQ(publisher.LastError(), "Unknown exception while sending the latest command batch");
    }
    EXPECT_FALSE(publisher.SubmitLatest(MakeBatch(3U)));
    publisher.Flush();
    EXPECT_EQ(sendCount, 1U);
}

void CheckStopWithBlockedRetry()
{
    PublisherShutdownDeadline deadline;
    std::size_t sendCount = 0U;
    mfd::client::LatestBatchPublisher publisher(
        [&sendCount](const mfd::CommandBatch&)
        {
            ++sendCount;
            return false;
        });

    EXPECT_TRUE(publisher.SubmitLatest(MakeBatch(1U)));
    publisher.Flush(); // A retained failed batch is now blocked until a submission.
    EXPECT_EQ(sendCount, 1U);
    EXPECT_EQ(publisher.LastError(), "Unable to send the latest command batch");
    publisher.Stop();
    publisher.Flush();
    EXPECT_FALSE(publisher.IsReady());
    EXPECT_FALSE(publisher.SubmitLatest(MakeBatch(2U)));
    EXPECT_EQ(sendCount, 1U);
}

void CheckStopIsIdempotent()
{
    PublisherShutdownDeadline deadline;
    std::size_t sendCount = 0U;
    mfd::client::LatestBatchPublisher publisher(
        [&sendCount](const mfd::CommandBatch&)
        {
            ++sendCount;
            return true;
        });

    EXPECT_TRUE(publisher.SubmitLatest(MakeBatch(1U)));
    publisher.Flush();
    publisher.Stop();
    publisher.Stop();
    publisher.Flush();
    EXPECT_FALSE(publisher.IsReady());
    EXPECT_FALSE(publisher.SubmitLatest(MakeBatch(2U)));
    EXPECT_EQ(sendCount, 1U);
}

void CheckReentrantAndConcurrentStop(const bool throwAfterStop)
{
    PublisherShutdownDeadline deadline;
    std::mutex mutex;
    std::condition_variable condition;
    bool callbackEntered = false;
    bool allowSelfStop = false;
    std::atomic<unsigned int> sends {0};
    mfd::client::LatestBatchPublisher* callbackPublisher = nullptr;
    mfd::client::LatestBatchPublisher publisher(
        [&mutex, &condition, &callbackEntered, &allowSelfStop, &callbackPublisher, &sends, throwAfterStop]
        (const mfd::CommandBatch&)
        {
            ++sends;
            {
                std::unique_lock<std::mutex> lock(mutex);
                callbackEntered = true;
                condition.notify_all();
                condition.wait(lock, [&allowSelfStop]() { return allowSelfStop; });
            }
            callbackPublisher->Stop();
            callbackPublisher->Stop();
            if (throwAfterStop)
            {
                throw std::runtime_error("callback failed after self-stop");
            }
            return false;
        });
    callbackPublisher = &publisher;
    EXPECT_TRUE(publisher.SubmitLatest(MakeBatch(1U)));
    {
        std::unique_lock<std::mutex> lock(mutex);
        EXPECT_TRUE(condition.wait_for(lock, kShutdownSyncTimeout, [&callbackEntered]() { return callbackEntered; }));
    }
    std::thread firstStopper([&publisher]() { publisher.Stop(); });
    std::thread secondStopper([&publisher]() { publisher.Stop(); });
    // Wait until an external stopper has requested stop, before releasing the callback.
    const auto deadlineAt = std::chrono::steady_clock::now() + kShutdownSyncTimeout;
    while (publisher.IsReady() && std::chrono::steady_clock::now() < deadlineAt)
    {
        std::this_thread::yield();
    }
    EXPECT_FALSE(publisher.IsReady());
    {
        std::lock_guard<std::mutex> lock(mutex);
        allowSelfStop = true;
    }
    condition.notify_all();
    firstStopper.join();
    secondStopper.join();
    publisher.Stop();
    publisher.Flush();
    EXPECT_EQ(sends.load(), 1U);
}

void CheckSelfStopThenDestruction()
{
    PublisherShutdownDeadline deadline;
    std::atomic<bool> callbackStopped {false};
    mfd::client::LatestBatchPublisher* callbackPublisher = nullptr;
    {
        mfd::client::LatestBatchPublisher publisher(
            [&callbackPublisher, &callbackStopped](const mfd::CommandBatch&)
            {
                callbackPublisher->Stop();
                callbackStopped = true;
                return true;
            });
        callbackPublisher = &publisher;
        EXPECT_TRUE(publisher.SubmitLatest(MakeBatch(1U)));
        publisher.Flush();
        EXPECT_TRUE(callbackStopped.load());
        EXPECT_FALSE(publisher.IsReady());
    }
}

void ExitWithShutdownTestResult()
{
    std::_Exit(testing::Test::HasFailure() ? EXIT_FAILURE : EXIT_SUCCESS);
}
} // namespace

TEST(LatestBatchPublisherShutdownDeathTest, CallbackDestructionEnforcesLifetimeContract)
{
    EXPECT_DEATH(
        {
            std::unique_ptr<mfd::client::LatestBatchPublisher> publisher;
            publisher = std::make_unique<mfd::client::LatestBatchPublisher>(
                [&publisher](const mfd::CommandBatch&)
                {
                    publisher.reset();
                    return true;
                });
            publisher->SubmitLatest(MakeBatch(1U));
            std::this_thread::sleep_for(std::chrono::seconds {2});
            // The DLL may use a separate CRT terminate handler. Require a
            // failing process exit, without assuming the executable's handler.
            std::_Exit(EXIT_SUCCESS);
        },
        "");
}

TEST(LatestBatchPublisherShutdownDeathTest, StopDuringFailedSendDoesNotRetry)
{
    EXPECT_EXIT(
        {
            CheckStopDuringSend(ShutdownSendResult::Failure, false);
            ExitWithShutdownTestResult();
        },
        testing::ExitedWithCode(EXIT_SUCCESS), "");
}

TEST(LatestBatchPublisherShutdownDeathTest, StopDuringSuccessfulSendAbandonsPendingBatch)
{
    EXPECT_EXIT(
        {
            CheckStopDuringSend(ShutdownSendResult::Success, false);
            ExitWithShutdownTestResult();
        },
        testing::ExitedWithCode(EXIT_SUCCESS), "");
}

TEST(LatestBatchPublisherShutdownDeathTest, StopDuringThrowingSendDoesNotRetry)
{
    EXPECT_EXIT(
        {
            CheckStopDuringSend(ShutdownSendResult::Exception, false);
            ExitWithShutdownTestResult();
        },
        testing::ExitedWithCode(EXIT_SUCCESS), "");
}

TEST(LatestBatchPublisherShutdownDeathTest, StopDuringUnknownSendExceptionDoesNotRetry)
{
    EXPECT_EXIT(
        {
            CheckStopDuringSend(ShutdownSendResult::UnknownException, false);
            ExitWithShutdownTestResult();
        },
        testing::ExitedWithCode(EXIT_SUCCESS), "");
}

TEST(LatestBatchPublisherShutdownDeathTest, FlushIsReleasedWhenStopFinishesFailedSend)
{
    EXPECT_EXIT(
        {
            CheckStopDuringSend(ShutdownSendResult::Failure, true);
            ExitWithShutdownTestResult();
        },
        testing::ExitedWithCode(EXIT_SUCCESS), "");
}

TEST(LatestBatchPublisherShutdownDeathTest, StopAbandonsBlockedRetry)
{
    EXPECT_EXIT(
        {
            CheckStopWithBlockedRetry();
            ExitWithShutdownTestResult();
        },
        testing::ExitedWithCode(EXIT_SUCCESS), "");
}

TEST(LatestBatchPublisherShutdownDeathTest, StopIsIdempotent)
{
    EXPECT_EXIT(
        {
            CheckStopIsIdempotent();
            ExitWithShutdownTestResult();
        },
        testing::ExitedWithCode(EXIT_SUCCESS), "");
}

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

TEST(LatestBatchPublisherTests, BatchBuilderCanQueryReadiness)
{
    std::uint32_t delivered = 0U;
    mfd::client::LatestBatchPublisher publisher(
        [&delivered](const mfd::CommandBatch& batch)
        {
            delivered = batch.sequence;
            return true;
        });

    EXPECT_TRUE(publisher.SubmitLatest([&publisher]()
    {
        EXPECT_TRUE(publisher.IsReady());
        return MakeBatch(41U);
    }));
    publisher.Flush();
    EXPECT_EQ(delivered, 41U);
}

TEST(LatestBatchPublisherTests, BatchBuilderCanReadTransportError)
{
    bool failNextSend = true;
    mfd::client::LatestBatchPublisher publisher(
        [&failNextSend](const mfd::CommandBatch&)
        {
            const bool failed = failNextSend;
            failNextSend = false;
            return !failed;
        },
        []() { return std::string {"transport failure"}; });
    ASSERT_TRUE(publisher.SubmitLatest(MakeBatch(1U)));
    publisher.Flush();

    EXPECT_TRUE(publisher.SubmitLatest([&publisher]()
    {
        EXPECT_EQ(publisher.LastError(), "transport failure");
        return MakeBatch(2U);
    }));
    publisher.Flush();
    EXPECT_TRUE(publisher.LastError().empty());
}

TEST(LatestBatchPublisherTests, StopDuringBatchConstructionRejectsPublication)
{
    std::atomic<std::size_t> sent {0U};
    mfd::client::LatestBatchPublisher publisher(
        [&sent](const mfd::CommandBatch&)
        {
            ++sent;
            return true;
        });
    std::mutex mutex;
    std::condition_variable condition;
    bool building = false;
    bool releaseBuilder = false;
    bool accepted = true;
    std::thread submission([&publisher, &mutex, &condition, &building, &releaseBuilder, &accepted]()
    {
        accepted = publisher.SubmitLatest([&mutex, &condition, &building, &releaseBuilder]()
        {
            std::unique_lock lock(mutex);
            building = true;
            condition.notify_all();
            condition.wait(lock, [&releaseBuilder]() { return releaseBuilder; });
            return MakeBatch(1U);
        });
    });
    {
        std::unique_lock lock(mutex);
        condition.wait(lock, [&building]() { return building; });
    }
    publisher.Stop();
    {
        std::lock_guard lock(mutex);
        releaseBuilder = true;
    }
    condition.notify_all();
    submission.join();

    EXPECT_FALSE(accepted);
    EXPECT_FALSE(publisher.IsReady());
    EXPECT_EQ(sent.load(), 0U);
    EXPECT_EQ(publisher.LastError(), "Latest batch publisher has been stopped");
}

TEST(LatestBatchPublisherTests, ConcurrentBuildersPreserveUndeliveredDeltas)
{
    // Block the sender so both submissions merge into the same pending batch.
    std::mutex mutex;
    std::condition_variable condition;
    bool sending = false;
    bool releaseSend = false;
    std::vector<mfd::CommandBatch> delivered;
    mfd::client::LatestBatchPublisher publisher(
        [&mutex, &condition, &sending, &releaseSend, &delivered](const mfd::CommandBatch& batch)
        {
            std::unique_lock lock(mutex);
            delivered.push_back(batch);
            sending = true;
            condition.notify_all();
            condition.wait(lock, [&releaseSend]() { return releaseSend; });
            return true;
        });
    ASSERT_TRUE(publisher.SubmitLatest(MakeBatch(0U)));
    {
        std::unique_lock lock(mutex);
        condition.wait(lock, [&sending]() { return sending; });
    }

    std::atomic<std::size_t> built {0U};
    std::atomic<std::size_t> accepted {0U};
    std::vector<std::thread> submissions;
    for (std::uint32_t index = 1U; index <= 2U; ++index)
    {
        submissions.emplace_back([&publisher, &built, &accepted, index]()
        {
            if (publisher.SubmitLatest([&built, index]()
            {
                ++built;
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
                while (built.load() < 2U && std::chrono::steady_clock::now() < deadline)
                {
                    std::this_thread::yield();
                }
                EXPECT_EQ(built.load(), 2U);
                mfd::CommandBatch batch = MakeBatch(index);
                batch.commands.emplace_back(mfd::RemoveDynamicReticleCommand {
                    mfd::DynamicReticleHandle {"radar", "track_" + std::to_string(index)}});
                return batch;
            })) ++accepted;
        });
    }
    for (std::thread& submission : submissions) submission.join();
    {
        std::lock_guard lock(mutex);
        releaseSend = true;
    }
    condition.notify_all();
    publisher.Flush();
    EXPECT_EQ(accepted.load(), 2U);
    ASSERT_EQ(delivered.size(), 2U);
    ASSERT_EQ(delivered.back().commands.size(), 2U);
    EXPECT_NE(std::get<mfd::RemoveDynamicReticleCommand>(delivered.back().commands[0]).target.reticleId,
              std::get<mfd::RemoveDynamicReticleCommand>(delivered.back().commands[1]).target.reticleId);
}

TEST(LatestBatchPublisherTests, BatchBuilderExceptionsPropagateWithoutBlockingPublisher)
{
    mfd::client::LatestBatchPublisher publisher([](const mfd::CommandBatch&) { return true; });
    EXPECT_THROW(publisher.SubmitLatest([]() -> mfd::CommandBatch
    {
        throw std::runtime_error("builder failure");
    }), std::runtime_error);
    EXPECT_TRUE(publisher.IsReady());
    EXPECT_TRUE(publisher.SubmitLatest([]() { return MakeBatch(1U); }));
    publisher.Flush();
}

TEST(LatestBatchPublisherTests, KeepsOnlyNewestPendingBatchWhileBusy)
{
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
            std::chrono::seconds {1},
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
            std::chrono::seconds {1},
            [&enteredSendCount]()
            {
                return enteredSendCount >= 1U;
            }));
    }

    mfd::CommandBatch secondBatch;
    secondBatch.sequence = 2U;
    secondBatch.commands.push_back(
        mfd::RemoveDynamicReticleCommand {mfd::DynamicReticleHandle {"radar", "trk_01"}});
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

TEST(LatestBatchPublisherTests, NumericAndTextDynamicIdentifiersDoNotCollideDuringMerge)
{
    std::mutex mutex;
    std::condition_variable condition;
    bool releaseFirstSend = false;
    std::size_t enteredSendCount = 0U;
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

    ASSERT_TRUE(publisher.SubmitLatest(MakeBatch(1U)));
    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(condition.wait_for(
            lock,
            std::chrono::seconds {1},
            [&enteredSendCount]()
            {
                return enteredSendCount >= 1U;
            }));
    }

    mfd::DynamicReticleHandle numericHandle;
    numericHandle.page = "radar";
    numericHandle.runtimeReticleId = 1U;
    mfd::CommandBatch numericBatch;
    numericBatch.sequence = 2U;
    numericBatch.mappingHash = "hash-alpha";
    numericBatch.commands.push_back(mfd::RemoveDynamicReticleCommand {numericHandle});
    ASSERT_TRUE(publisher.SubmitLatest(std::move(numericBatch)));

    mfd::DynamicReticleHandle textualHandle;
    textualHandle.page = "radar";
    textualHandle.reticleId = "1";
    mfd::CommandBatch textualBatch;
    textualBatch.sequence = 3U;
    textualBatch.mappingHash = "hash-alpha";
    textualBatch.commands.push_back(mfd::RemoveDynamicReticleCommand {textualHandle});
    ASSERT_TRUE(publisher.SubmitLatest(std::move(textualBatch)));

    {
        std::lock_guard lock(mutex);
        releaseFirstSend = true;
    }
    condition.notify_all();
    publisher.Flush();

    std::lock_guard lock(mutex);
    ASSERT_EQ(deliveredBatches.size(), 2U);
    std::size_t numericRemovalCount = 0U;
    std::size_t textualRemovalCount = 0U;
    for (const mfd::UserCommand& command : deliveredBatches[1].commands)
    {
        const auto* removal = std::get_if<mfd::RemoveDynamicReticleCommand>(&command);
        if (removal == nullptr)
        {
            continue;
        }

        if (removal->target.runtimeReticleId == 1U)
        {
            ++numericRemovalCount;
        }
        if (removal->target.reticleId == "1")
        {
            ++textualRemovalCount;
        }
    }

    EXPECT_EQ(numericRemovalCount, 1U);
    EXPECT_EQ(textualRemovalCount, 1U);
}

TEST(LatestBatchPublisherTests, PreservesPendingResetWindowCommandWhenMergingNewerSameHashBatch)
{
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
            std::chrono::seconds {1},
            [&enteredSendCount]()
            {
                return enteredSendCount >= 1U;
            }));
    }

    mfd::CommandBatch secondBatch;
    secondBatch.sequence = 2U;
    secondBatch.mappingHash = "hash-alpha";
    secondBatch.commands.push_back(mfd::ResetWindowCommand {});
    ASSERT_TRUE(publisher.SubmitLatest(std::move(secondBatch)));

    mfd::CommandBatch thirdBatch;
    thirdBatch.sequence = 3U;
    thirdBatch.mappingHash = "hash-alpha";
    thirdBatch.commands.push_back(mfd::ActivatePageCommand {"nav"});
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
    ASSERT_EQ(deliveredBatches[1].commands.size(), 2U);
    ASSERT_NE(std::get_if<mfd::ResetWindowCommand>(&deliveredBatches[1].commands[0]), nullptr);
    ASSERT_NE(std::get_if<mfd::ActivatePageCommand>(&deliveredBatches[1].commands[1]), nullptr);
}

TEST(LatestBatchPublisherTests, NewestResetWindowCommandDropsOlderPendingDynamicLifecycleState)
{
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
            std::chrono::seconds {1},
            [&enteredSendCount]()
            {
                return enteredSendCount >= 1U;
            }));
    }

    mfd::CommandBatch secondBatch;
    secondBatch.sequence = 2U;
    secondBatch.mappingHash = "hash-alpha";
    secondBatch.commands.push_back(
        mfd::RemoveDynamicReticleCommand {mfd::DynamicReticleHandle {"radar", "trk_91", 11U, 9001U}});
    ASSERT_TRUE(publisher.SubmitLatest(std::move(secondBatch)));

    mfd::CommandBatch thirdBatch;
    thirdBatch.sequence = 3U;
    thirdBatch.mappingHash = "hash-alpha";
    thirdBatch.commands.push_back(mfd::ResetWindowCommand {});
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
    EXPECT_TRUE(ContainsCommandType<mfd::ResetWindowCommand>(deliveredBatches[1]));
    EXPECT_TRUE(ContainsCommandType<mfd::ActivatePageCommand>(deliveredBatches[1]));
    EXPECT_FALSE(ContainsCommandType<mfd::RemoveDynamicReticleCommand>(deliveredBatches[1]));
}

TEST(LatestBatchPublisherTests, NewDynamicReticleLifecycleStateOverridesPendingState)
{
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
            std::chrono::seconds {1},
            [&enteredSendCount]()
            {
                return enteredSendCount >= 1U;
            }));
    }

    mfd::CommandBatch secondBatch;
    secondBatch.sequence = 2U;
    secondBatch.commands.push_back(
        mfd::RemoveDynamicReticleCommand {mfd::DynamicReticleHandle {"radar", "trk_02"}});
    ASSERT_TRUE(publisher.SubmitLatest(std::move(secondBatch)));

    mfd::ReticlePatch patch;
    patch.visible = true;
    mfd::CommandBatch thirdBatch;
    thirdBatch.sequence = 3U;
    mfd::UpsertDynamicReticleCommand command;
    command.target = mfd::DynamicReticleHandle {"radar", "trk_02"};
    command.templateId = "radar_track";
    command.patch = patch;
    thirdBatch.commands.push_back(std::move(command));
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

TEST(LatestBatchPublisherTests, DoesNotCarryPendingDynamicLifecycleAcrossDifferentMappingHashes)
{
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
    firstBatch.mappingHash = "hash-alpha";
    ASSERT_TRUE(publisher.SubmitLatest(std::move(firstBatch)));

    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(condition.wait_for(
            lock,
            std::chrono::seconds {1},
            [&enteredSendCount]()
            {
                return enteredSendCount >= 1U;
            }));
    }

    mfd::CommandBatch secondBatch;
    secondBatch.sequence = 2U;
    secondBatch.mappingHash = "hash-alpha";
    secondBatch.commands.push_back(
        mfd::RemoveDynamicReticleCommand {mfd::DynamicReticleHandle {"radar", "trk_42", 11U, 9001U}});
    ASSERT_TRUE(publisher.SubmitLatest(std::move(secondBatch)));

    mfd::CommandBatch thirdBatch;
    thirdBatch.sequence = 3U;
    thirdBatch.mappingHash = "hash-beta";
    thirdBatch.commands.push_back(mfd::ActivatePageCommand {"nav"});
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
    EXPECT_EQ(deliveredBatches[1].mappingHash, "hash-beta");
    EXPECT_TRUE(ContainsCommandType<mfd::ActivatePageCommand>(deliveredBatches[1]));
    EXPECT_FALSE(ContainsCommandType<mfd::RemoveDynamicReticleCommand>(deliveredBatches[1]));
}

TEST(LatestBatchPublisherTests, PreservesGeneratedIdentifiersWhenFlatteningBulkDynamicUpdates)
{
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
            std::chrono::seconds {1},
            [&enteredSendCount]()
            {
                return enteredSendCount >= 1U;
            }));
    }

    mfd::ReticlePatch patch;
    patch.visible = true;

    mfd::DynamicReticleState state;
    state.reticleId = "trk_01";
    state.runtimeReticleId = 9001U;
    state.patch = patch;

    mfd::CommandBatch secondBatch;
    secondBatch.sequence = 2U;
    mfd::UpsertDynamicReticlesCommand bulkUpsert;
    bulkUpsert.page = "radar";
    bulkUpsert.pageId = 11U;
    bulkUpsert.templateId = "radar_track";
    bulkUpsert.templateTransportId = 77U;
    bulkUpsert.reticles.push_back(std::move(state));
    secondBatch.commands.push_back(std::move(bulkUpsert));
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
    ASSERT_EQ(deliveredBatches[1].commands.size(), 2U);

    const auto* upsert = std::get_if<mfd::UpsertDynamicReticleCommand>(&deliveredBatches[1].commands[0]);
    ASSERT_NE(upsert, nullptr);
    EXPECT_EQ(upsert->target.page, "radar");
    EXPECT_EQ(upsert->target.pageId, 11U);
    EXPECT_EQ(upsert->target.runtimeReticleId, 9001U);
    EXPECT_EQ(upsert->templateId, "radar_track");
    EXPECT_EQ(upsert->templateTransportId, 77U);
}

TEST(LatestBatchPublisherTests, PreservesPendingStaticReticleVisibilityDeltaWhenNewestBatchDoesNotRewriteIt)
{
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
            std::chrono::seconds {1},
            [&enteredSendCount]()
            {
                return enteredSendCount >= 1U;
            }));
    }

    mfd::ReticlePatch hiddenPatch;
    hiddenPatch.visible = false;
    mfd::CommandBatch secondBatch;
    secondBatch.sequence = 2U;
    secondBatch.mappingHash = "hash-alpha";
    secondBatch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"HUD", "fpmLimitX"}, hiddenPatch});
    ASSERT_TRUE(publisher.SubmitLatest(std::move(secondBatch)));

    mfd::CommandBatch thirdBatch;
    thirdBatch.sequence = 3U;
    thirdBatch.mappingHash = "hash-alpha";
    thirdBatch.commands.push_back(mfd::ActivatePageCommand {"HUD"});
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
    EXPECT_TRUE(ContainsCommandType<mfd::ActivatePageCommand>(deliveredBatches[1]));

    const mfd::UpdateReticleCommand* carried = FindStaticReticleUpdate(deliveredBatches[1], "HUD", "fpmLimitX");
    ASSERT_NE(carried, nullptr);
    ASSERT_TRUE(carried->patch.visible.has_value());
    EXPECT_FALSE(carried->patch.visible.value());
}

TEST(LatestBatchPublisherTests, NewestStaticReticleVisibilityOverridesPendingStaticVisibility)
{
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
            std::chrono::seconds {1},
            [&enteredSendCount]()
            {
                return enteredSendCount >= 1U;
            }));
    }

    mfd::ReticlePatch shownPatch;
    shownPatch.visible = true;
    mfd::CommandBatch secondBatch;
    secondBatch.sequence = 2U;
    secondBatch.mappingHash = "hash-alpha";
    secondBatch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"HUD", "fpmLimitX"}, shownPatch});
    ASSERT_TRUE(publisher.SubmitLatest(std::move(secondBatch)));

    mfd::ReticlePatch hiddenPatch;
    hiddenPatch.visible = false;
    mfd::CommandBatch thirdBatch;
    thirdBatch.sequence = 3U;
    thirdBatch.mappingHash = "hash-alpha";
    thirdBatch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"HUD", "fpmLimitX"}, hiddenPatch});
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
    EXPECT_EQ(CountStaticReticleUpdates(deliveredBatches[1], "HUD", "fpmLimitX"), 1U);

    const mfd::UpdateReticleCommand* update = FindStaticReticleUpdate(deliveredBatches[1], "HUD", "fpmLimitX");
    ASSERT_NE(update, nullptr);
    ASSERT_TRUE(update->patch.visible.has_value());
    EXPECT_FALSE(update->patch.visible.value());
}

TEST(LatestBatchPublisherTests, MergesPendingStaticReticlePatchFieldsWithoutOverridingNewestFields)
{
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
            std::chrono::seconds {1},
            [&enteredSendCount]()
            {
                return enteredSendCount >= 1U;
            }));
    }

    mfd::ReticlePatch pendingPatch;
    pendingPatch.visible = false;
    pendingPatch.position = mfd::Vec2 {0.1f, 0.2f};
    pendingPatch.primitivePatchesById[31].streamPlayback = mfd::StreamPlayback::Stop;
    pendingPatch.primitivePatchesById[32].streamPlayback = mfd::StreamPlayback::Pause;
    mfd::CommandBatch secondBatch;
    secondBatch.sequence = 2U;
    secondBatch.mappingHash = "hash-alpha";
    secondBatch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"HUD", "fpmLimitX"}, pendingPatch});
    ASSERT_TRUE(publisher.SubmitLatest(std::move(secondBatch)));

    mfd::ReticlePatch newestPatch;
    newestPatch.position = mfd::Vec2 {0.3f, 0.4f};
    newestPatch.primitivePatchesById[31].streamPlayback = mfd::StreamPlayback::Play;
    newestPatch.primitivePatchesById[32].visible = false;
    mfd::CommandBatch thirdBatch;
    thirdBatch.sequence = 3U;
    thirdBatch.mappingHash = "hash-alpha";
    thirdBatch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"HUD", "fpmLimitX"}, newestPatch});
    ASSERT_TRUE(publisher.SubmitLatest(std::move(thirdBatch)));

    {
        std::lock_guard lock(mutex);
        releaseFirstSend = true;
    }
    condition.notify_all();

    publisher.Flush();

    std::lock_guard lock(mutex);
    ASSERT_EQ(deliveredBatches.size(), 2U);
    EXPECT_EQ(CountStaticReticleUpdates(deliveredBatches[1], "HUD", "fpmLimitX"), 1U);

    const mfd::UpdateReticleCommand* update = FindStaticReticleUpdate(deliveredBatches[1], "HUD", "fpmLimitX");
    ASSERT_NE(update, nullptr);
    ASSERT_TRUE(update->patch.visible.has_value());
    EXPECT_FALSE(update->patch.visible.value());
    ASSERT_TRUE(update->patch.position.has_value());
    EXPECT_FLOAT_EQ(update->patch.position->x, 0.3f);
    EXPECT_FLOAT_EQ(update->patch.position->y, 0.4f);
    const auto& resumed = update->patch.primitivePatchesById.at(31);
    EXPECT_EQ(resumed.streamPlayback, mfd::StreamPlayback::Play);
    EXPECT_TRUE(resumed.streamStopToken.has_value());
    EXPECT_EQ(update->patch.primitivePatchesById.at(32).streamPlayback, mfd::StreamPlayback::Pause);
}

TEST(LatestBatchPublisherTests, DoesNotCarryPendingStaticReticleUpdatesAcrossDifferentMappingHashes)
{
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
    firstBatch.mappingHash = "hash-alpha";
    ASSERT_TRUE(publisher.SubmitLatest(std::move(firstBatch)));

    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(condition.wait_for(
            lock,
            std::chrono::seconds {1},
            [&enteredSendCount]()
            {
                return enteredSendCount >= 1U;
            }));
    }

    mfd::ReticlePatch hiddenPatch;
    hiddenPatch.visible = false;
    mfd::CommandBatch secondBatch;
    secondBatch.sequence = 2U;
    secondBatch.mappingHash = "hash-alpha";
    secondBatch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"HUD", "fpmLimitX"}, hiddenPatch});
    ASSERT_TRUE(publisher.SubmitLatest(std::move(secondBatch)));

    mfd::CommandBatch thirdBatch;
    thirdBatch.sequence = 3U;
    thirdBatch.mappingHash = "hash-beta";
    thirdBatch.commands.push_back(mfd::ActivatePageCommand {"HUD"});
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
    EXPECT_EQ(deliveredBatches[1].mappingHash, "hash-beta");
    EXPECT_TRUE(ContainsCommandType<mfd::ActivatePageCommand>(deliveredBatches[1]));
    EXPECT_EQ(FindStaticReticleUpdate(deliveredBatches[1], "HUD", "fpmLimitX"), nullptr);
}

TEST(LatestBatchPublisherTests, NewestResetWindowCommandDropsOlderPendingStaticReticleUpdate)
{
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
            std::chrono::seconds {1},
            [&enteredSendCount]()
            {
                return enteredSendCount >= 1U;
            }));
    }

    mfd::ReticlePatch hiddenPatch;
    hiddenPatch.visible = false;
    mfd::CommandBatch secondBatch;
    secondBatch.sequence = 2U;
    secondBatch.mappingHash = "hash-alpha";
    secondBatch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"HUD", "fpmLimitX"}, hiddenPatch});
    ASSERT_TRUE(publisher.SubmitLatest(std::move(secondBatch)));

    mfd::CommandBatch thirdBatch;
    thirdBatch.sequence = 3U;
    thirdBatch.mappingHash = "hash-alpha";
    thirdBatch.commands.push_back(mfd::ResetWindowCommand {});
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
    EXPECT_TRUE(ContainsCommandType<mfd::ResetWindowCommand>(deliveredBatches[1]));
    EXPECT_EQ(FindStaticReticleUpdate(deliveredBatches[1], "HUD", "fpmLimitX"), nullptr);
}

TEST(LatestBatchPublisherTests, NewestGeneratedPrimitivePatchOverridesPendingNamedPrimitivePatch)
{
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
            std::chrono::seconds {1},
            [&enteredSendCount]()
            {
                return enteredSendCount >= 1U;
            }));
    }

    mfd::PrimitivePatch namedPrimitive;
    namedPrimitive.visible = false;
    mfd::ReticlePatch pendingPatch;
    pendingPatch.primitivePatches.emplace("limitLine", namedPrimitive);
    mfd::CommandBatch secondBatch;
    secondBatch.sequence = 2U;
    secondBatch.mappingHash = "hash-alpha";
    secondBatch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"HUD", "fpmLimitX"}, pendingPatch});
    ASSERT_TRUE(publisher.SubmitLatest(std::move(secondBatch)));

    mfd::PrimitivePatch generatedPrimitive;
    generatedPrimitive.visible = true;
    mfd::ReticlePatch newestPatch;
    newestPatch.primitivePatchesById.emplace(42U, generatedPrimitive);
    mfd::CommandBatch thirdBatch;
    thirdBatch.sequence = 3U;
    thirdBatch.mappingHash = "hash-alpha";
    thirdBatch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"HUD", "fpmLimitX"}, newestPatch});
    ASSERT_TRUE(publisher.SubmitLatest(std::move(thirdBatch)));

    {
        std::lock_guard lock(mutex);
        releaseFirstSend = true;
    }
    condition.notify_all();

    publisher.Flush();

    std::lock_guard lock(mutex);
    ASSERT_EQ(deliveredBatches.size(), 2U);
    const mfd::CommandBatch& finalBatch = deliveredBatches[1];
    EXPECT_EQ(finalBatch.sequence, 3U);

    std::optional<std::size_t> namedIndex;
    std::optional<std::size_t> generatedIndex;
    for (std::size_t index = 0; index < finalBatch.commands.size(); ++index)
    {
        const auto* update = std::get_if<mfd::UpdateReticleCommand>(&finalBatch.commands[index]);
        if (update == nullptr || update->target.page != "HUD" || update->target.reticle != "fpmLimitX")
        {
            continue;
        }

        if (!update->patch.primitivePatches.empty())
        {
            namedIndex = index;
        }
        if (!update->patch.primitivePatchesById.empty())
        {
            generatedIndex = index;
        }
    }

    ASSERT_TRUE(namedIndex.has_value());
    ASSERT_TRUE(generatedIndex.has_value());
    // The pending named delta must never share a command with the newest generated-id
    // delta, and it must be applied strictly before it so "latest wins" survives
    // transport normalization.
    EXPECT_NE(*namedIndex, *generatedIndex);
    EXPECT_LT(*namedIndex, *generatedIndex);

    const auto& generatedCommand = std::get<mfd::UpdateReticleCommand>(finalBatch.commands[*generatedIndex]);
    EXPECT_TRUE(generatedCommand.patch.primitivePatches.empty());
    const auto generatedEntry = generatedCommand.patch.primitivePatchesById.find(42U);
    ASSERT_NE(generatedEntry, generatedCommand.patch.primitivePatchesById.end());
    ASSERT_TRUE(generatedEntry->second.visible.has_value());
    EXPECT_TRUE(generatedEntry->second.visible.value());

    const auto& namedCommand = std::get<mfd::UpdateReticleCommand>(finalBatch.commands[*namedIndex]);
    EXPECT_TRUE(namedCommand.patch.primitivePatchesById.empty());
    const auto namedEntry = namedCommand.patch.primitivePatches.find("limitLine");
    ASSERT_NE(namedEntry, namedCommand.patch.primitivePatches.end());
    ASSERT_TRUE(namedEntry->second.visible.has_value());
    EXPECT_FALSE(namedEntry->second.visible.value());
}

TEST(LatestBatchPublisherTests, NewestNamedPrimitivePatchOverridesPendingGeneratedPrimitivePatch)
{
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
            std::chrono::seconds {1},
            [&enteredSendCount]()
            {
                return enteredSendCount >= 1U;
            }));
    }

    mfd::PrimitivePatch generatedPrimitive;
    generatedPrimitive.visible = false;
    mfd::ReticlePatch pendingPatch;
    pendingPatch.primitivePatchesById.emplace(42U, generatedPrimitive);
    mfd::CommandBatch secondBatch;
    secondBatch.sequence = 2U;
    secondBatch.mappingHash = "hash-alpha";
    secondBatch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"HUD", "fpmLimitX"}, pendingPatch});
    ASSERT_TRUE(publisher.SubmitLatest(std::move(secondBatch)));

    mfd::PrimitivePatch namedPrimitive;
    namedPrimitive.visible = true;
    mfd::ReticlePatch newestPatch;
    newestPatch.primitivePatches.emplace("limitLine", namedPrimitive);
    mfd::CommandBatch thirdBatch;
    thirdBatch.sequence = 3U;
    thirdBatch.mappingHash = "hash-alpha";
    thirdBatch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"HUD", "fpmLimitX"}, newestPatch});
    ASSERT_TRUE(publisher.SubmitLatest(std::move(thirdBatch)));

    {
        std::lock_guard lock(mutex);
        releaseFirstSend = true;
    }
    condition.notify_all();

    publisher.Flush();

    std::lock_guard lock(mutex);
    ASSERT_EQ(deliveredBatches.size(), 2U);
    const mfd::CommandBatch& finalBatch = deliveredBatches[1];
    EXPECT_EQ(finalBatch.sequence, 3U);

    std::optional<std::size_t> namedIndex;
    std::optional<std::size_t> generatedIndex;
    for (std::size_t index = 0; index < finalBatch.commands.size(); ++index)
    {
        const auto* update = std::get_if<mfd::UpdateReticleCommand>(&finalBatch.commands[index]);
        if (update == nullptr || update->target.page != "HUD" || update->target.reticle != "fpmLimitX")
        {
            continue;
        }

        if (!update->patch.primitivePatches.empty())
        {
            namedIndex = index;
        }
        if (!update->patch.primitivePatchesById.empty())
        {
            generatedIndex = index;
        }
    }

    ASSERT_TRUE(namedIndex.has_value());
    ASSERT_TRUE(generatedIndex.has_value());
    // Symmetric case: the newest named delta must not be overridden by the pending
    // generated-id delta, so the pending generated-id command is applied before it.
    EXPECT_NE(*namedIndex, *generatedIndex);
    EXPECT_LT(*generatedIndex, *namedIndex);

    const auto& namedCommand = std::get<mfd::UpdateReticleCommand>(finalBatch.commands[*namedIndex]);
    EXPECT_TRUE(namedCommand.patch.primitivePatchesById.empty());
    const auto namedEntry = namedCommand.patch.primitivePatches.find("limitLine");
    ASSERT_NE(namedEntry, namedCommand.patch.primitivePatches.end());
    ASSERT_TRUE(namedEntry->second.visible.has_value());
    EXPECT_TRUE(namedEntry->second.visible.value());

    const auto& generatedCommand = std::get<mfd::UpdateReticleCommand>(finalBatch.commands[*generatedIndex]);
    EXPECT_TRUE(generatedCommand.patch.primitivePatches.empty());
    const auto generatedEntry = generatedCommand.patch.primitivePatchesById.find(42U);
    ASSERT_NE(generatedEntry, generatedCommand.patch.primitivePatchesById.end());
    ASSERT_TRUE(generatedEntry->second.visible.has_value());
    EXPECT_FALSE(generatedEntry->second.visible.value());
}

TEST(LatestBatchPublisherTests, PreservesPendingTrueHorizonVisibleDeltaWhenNewestBatchDoesNotRewriteIt)
{
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
            std::chrono::seconds {1},
            [&enteredSendCount]()
            {
                return enteredSendCount >= 1U;
            }));
    }

    mfd::ReticlePatch shownPatch;
    shownPatch.visible = true;
    mfd::CommandBatch secondBatch;
    secondBatch.sequence = 2U;
    secondBatch.mappingHash = "hash-alpha";
    secondBatch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"HUD", "trueHorizon"}, shownPatch});
    ASSERT_TRUE(publisher.SubmitLatest(std::move(secondBatch)));

    mfd::CommandBatch thirdBatch;
    thirdBatch.sequence = 3U;
    thirdBatch.mappingHash = "hash-alpha";
    thirdBatch.commands.push_back(mfd::ActivatePageCommand {"HUD"});
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
    EXPECT_TRUE(ContainsCommandType<mfd::ActivatePageCommand>(deliveredBatches[1]));

    const mfd::UpdateReticleCommand* carried = FindStaticReticleUpdate(deliveredBatches[1], "HUD", "trueHorizon");
    ASSERT_NE(carried, nullptr);
    ASSERT_TRUE(carried->patch.visible.has_value());
    EXPECT_TRUE(carried->patch.visible.value());
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
    EXPECT_EQ(activate->page, "radar");
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
 * @brief Ensures a rejected submission cannot consume generator dirty state.
 */
TEST(LatestBatchPublisherTests, DoesNotInvokeBatchBuilderAfterStop)
{
    mfd::client::LatestBatchPublisher publisher(
        [](const mfd::CommandBatch&)
        {
            return true;
        });
    publisher.Stop();

    bool builderInvoked = false;
    EXPECT_FALSE(publisher.SubmitLatest(
        [&builderInvoked]()
        {
            builderInvoked = true;
            return MakeBatch(91U);
        }));

    EXPECT_FALSE(builderInvoked);
    EXPECT_EQ(publisher.LastError(), "Latest batch publisher has been stopped");
}

TEST(LatestBatchPublisherTests, DynamicStreamControlsSurviveBulkDeltasAndRepeatedRetries)
{
    mfd::CommandBatch attempted;
    mfd::client::LatestBatchPublisher publisher([&attempted](const mfd::CommandBatch& batch)
    {
        attempted = batch;
        return false;
    });
    mfd::UpsertDynamicReticlesCommand bulk;
    bulk.page = "HUD";
    bulk.templateId = "camera";
    mfd::DynamicReticleState state;
    state.runtimeReticleId = 123;
    state.patch.position = mfd::Vec2 {0.1f, 0.2f};
    state.patch.primitivePatchesById[31].streamPlayback = mfd::StreamPlayback::Stop;
    state.patch.primitivePatchesById[32].streamPlayback = mfd::StreamPlayback::Pause;
    bulk.reticles.push_back(state);
    auto batch = MakeBatch(1);
    batch.commands.push_back(bulk);
    ASSERT_TRUE(publisher.SubmitLatest(std::move(batch)));
    publisher.Flush();

    bulk.reticles[0].patch = {};
    bulk.reticles[0].patch.primitivePatchesById[31].streamPlayback = mfd::StreamPlayback::Play;
    batch = MakeBatch(2);
    batch.commands.push_back(bulk);
    ASSERT_TRUE(publisher.SubmitLatest(std::move(batch)));
    publisher.Flush();

    bulk.reticles[0].patch = {};
    bulk.reticles[0].patch.position = mfd::Vec2 {0.8f, 0.9f};
    for (std::uint32_t sequence = 3; sequence < 10; ++sequence)
    {
        batch = MakeBatch(sequence);
        batch.commands.push_back(bulk);
        ASSERT_TRUE(publisher.SubmitLatest(std::move(batch)));
        publisher.Flush();
    }
    ASSERT_EQ(attempted.commands.size(), 2U);
    const auto* controls = std::get_if<mfd::UpsertDynamicReticleCommand>(&attempted.commands.front());
    ASSERT_NE(controls, nullptr);
    EXPECT_FALSE(controls->patch.position.has_value());
    EXPECT_EQ(controls->patch.primitivePatchesById.at(31).streamPlayback, mfd::StreamPlayback::Play);
    EXPECT_TRUE(controls->patch.primitivePatchesById.at(31).streamStopToken.has_value());
    EXPECT_EQ(controls->patch.primitivePatchesById.at(32).streamPlayback, mfd::StreamPlayback::Pause);
    const auto* delta = std::get_if<mfd::UpsertDynamicReticlesCommand>(&attempted.commands.back());
    ASSERT_NE(delta, nullptr);
    ASSERT_TRUE(delta->reticles[0].patch.position.has_value());
    EXPECT_FLOAT_EQ(delta->reticles[0].patch.position->x, 0.8f);
}

TEST(LatestBatchPublisherTests, DynamicStreamNameAndIdControlsKeepChronologicalOrder)
{
    mfd::CommandBatch attempted;
    mfd::client::LatestBatchPublisher publisher([&attempted](const mfd::CommandBatch& batch)
    {
        attempted = batch;
        return false;
    });
    mfd::UpsertDynamicReticleCommand update;
    update.target = {"HUD", "camera1"};
    update.templateId = "camera";
    update.patch.primitivePatches["screen"].streamPlayback = mfd::StreamPlayback::Stop;
    auto batch = MakeBatch(1);
    batch.commands.push_back(update);
    update.patch = {};
    update.patch.primitivePatchesById[31].streamPlayback = mfd::StreamPlayback::Play;
    batch.commands.push_back(update);
    ASSERT_TRUE(publisher.SubmitLatest(std::move(batch)));
    publisher.Flush();
    update.patch = {};
    update.patch.visible = false;
    batch = MakeBatch(2);
    batch.commands.push_back(update);
    ASSERT_TRUE(publisher.SubmitLatest(std::move(batch)));
    publisher.Flush();
    ASSERT_EQ(attempted.commands.size(), 3U);
    const auto& stopped = std::get<mfd::UpsertDynamicReticleCommand>(attempted.commands[0]);
    const auto& resumed = std::get<mfd::UpsertDynamicReticleCommand>(attempted.commands[1]);
    EXPECT_EQ(stopped.patch.primitivePatches.at("screen").streamPlayback, mfd::StreamPlayback::Stop);
    EXPECT_EQ(resumed.patch.primitivePatchesById.at(31).streamPlayback, mfd::StreamPlayback::Play);
    EXPECT_EQ(std::get<mfd::UpsertDynamicReticleCommand>(attempted.commands[2]).patch.visible, false);
}

TEST(LatestBatchPublisherTests, DynamicStreamControlsDoNotCrossRemoveRecreateOrTemplateChange)
{
    for (const bool removeBeforeRecreate : {false, true})
    {
        mfd::CommandBatch attempted;
        mfd::client::LatestBatchPublisher publisher([&attempted](const mfd::CommandBatch& batch)
        {
            attempted = batch;
            return false;
        });
        mfd::UpsertDynamicReticleCommand update;
        update.target = {"HUD", "camera1"};
        update.templateId = "camera";
        update.patch.primitivePatchesById[31].streamPlayback = mfd::StreamPlayback::Stop;
        auto batch = MakeBatch(1);
        batch.commands.push_back(update);
        ASSERT_TRUE(publisher.SubmitLatest(std::move(batch)));
        publisher.Flush();
        batch = MakeBatch(2);
        if (removeBeforeRecreate)
            batch.commands.push_back(mfd::RemoveDynamicReticleCommand {update.target});
        else
            update.templateId = "otherCamera";
        update.patch = {};
        update.patch.visible = true;
        batch.commands.push_back(update);
        ASSERT_TRUE(publisher.SubmitLatest(std::move(batch)));
        publisher.Flush();
        ASSERT_EQ(attempted.commands.size(), removeBeforeRecreate ? 2U : 1U);
        const auto& recreated = std::get<mfd::UpsertDynamicReticleCommand>(attempted.commands.back());
        EXPECT_TRUE(recreated.patch.primitivePatchesById.empty());
    }
}

/**
 * @brief Ensures an asynchronously failed batch is retained until a later submission retries it.
 */
TEST(LatestBatchPublisherTests, RetriesFailedBatchWithNextSubmission)
{
    std::mutex mutex;
    std::vector<mfd::CommandBatch> deliveredBatches;
    std::size_t sendCount = 0U;
    mfd::client::LatestBatchPublisher publisher(
        [&mutex, &deliveredBatches, &sendCount](const mfd::CommandBatch& batch)
        {
            std::lock_guard lock(mutex);
            ++sendCount;
            deliveredBatches.push_back(batch);
            return sendCount > 1U;
        });

    mfd::CommandBatch failedBatch = MakeBatch(17U);
    mfd::ReticlePatch failedPatch;
    failedPatch.visible = true;
    failedBatch.commands.emplace_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"HUD", "retry"}, failedPatch});
    ASSERT_TRUE(publisher.SubmitLatest(std::move(failedBatch)));
    publisher.Flush();
    EXPECT_EQ(publisher.LastError(), "Unable to send the latest command batch");
    {
        std::lock_guard lock(mutex);
        ASSERT_EQ(deliveredBatches.size(), 1U);
    }

    ASSERT_TRUE(publisher.SubmitLatest(MakeBatch(18U)));
    publisher.Flush();

    std::lock_guard lock(mutex);
    ASSERT_EQ(deliveredBatches.size(), 2U);
    EXPECT_EQ(deliveredBatches[0].sequence, 17U);
    EXPECT_EQ(deliveredBatches[1].sequence, 18U);
    const mfd::UpdateReticleCommand* retryUpdate =
        FindStaticReticleUpdate(deliveredBatches[1], "HUD", "retry");
    ASSERT_NE(retryUpdate, nullptr);
    EXPECT_EQ(retryUpdate->patch.visible, std::optional<bool> {true});
    EXPECT_TRUE(publisher.LastError().empty());
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

TEST(LatestBatchPublisherTests, CoalescesPresetCustomTransitionsWithoutStaleColorFields)
{
    for (const bool olderPreset : {false, true})
    {
        for (const bool newerPreset : {false, true})
        {
            std::mutex mutex;
            std::condition_variable condition;
            bool entered = false;
            bool release = false;
            std::vector<mfd::CommandBatch> delivered;
            mfd::client::LatestBatchPublisher publisher(
                [&mutex, &condition, &entered, &release, &delivered](const mfd::CommandBatch& batch)
                {
                    std::unique_lock lock(mutex);
                    delivered.push_back(batch);
                    if (batch.sequence == 1)
                    {
                        entered = true;
                        condition.notify_all();
                        return condition.wait_for(lock, std::chrono::seconds(5), [&release]() { return release; });
                    }
                    return true;
                });
            EXPECT_TRUE(publisher.SubmitLatest(MakeBatch(1)));
            {
                std::unique_lock lock(mutex);
                EXPECT_TRUE(condition.wait_for(lock, std::chrono::seconds(2), [&entered]() { return entered; }));
            }
            mfd::UpdateReticleCommand older;
            older.target = {"Radar", "track"};
            if (olderPreset) older.patch.colorPreset = mfd::ColorPreset::Green;
            else older.patch.color = mfd::ColorRgba {10, 20, 30, 200};
            older.patch.primitivePatches["area"].color = mfd::ColorRgba {1, 2, 3, 4};
            older.patch.primitivePatches["area"].fillColorPreset = mfd::ColorPreset::Yellow;
            auto pending = MakeBatch(2);
            pending.commands.push_back(older);
            EXPECT_TRUE(publisher.SubmitLatest(std::move(pending)));
            mfd::UpdateReticleCommand newer;
            newer.target = older.target;
            if (newerPreset) newer.patch.colorPreset = mfd::ColorPreset::Blue;
            else
            {
                newer.patch.colorPreset = mfd::ColorPreset::Custom;
                newer.patch.color = mfd::ColorRgba {120, 50, 180, 150};
            }
            newer.patch.primitivePatches["area"].colorPreset = mfd::ColorPreset::Red;
            auto latest = MakeBatch(3);
            latest.commands.push_back(newer);
            EXPECT_TRUE(publisher.SubmitLatest(std::move(latest)));
            {
                std::lock_guard lock(mutex);
                release = true;
            }
            condition.notify_all();
            publisher.Flush();
            ASSERT_EQ(delivered.size(), 2U);
            const auto* command = FindStaticReticleUpdate(delivered.back(), "Radar", "track");
            ASSERT_NE(command, nullptr);
            EXPECT_EQ(command->patch.colorPreset, newer.patch.colorPreset);
            EXPECT_EQ(command->patch.color.has_value(), !newerPreset);
            if (command->patch.color) EXPECT_EQ(command->patch.color->a, 150);
            const auto& area = command->patch.primitivePatches.at("area");
            EXPECT_EQ(area.colorPreset, mfd::ColorPreset::Red);
            EXPECT_FALSE(area.color);
            EXPECT_EQ(area.fillColorPreset, mfd::ColorPreset::Yellow);
        }
    }
}

TEST(LatestBatchPublisherShutdownDeathTest, CallbackStopAndConcurrentJoinsComplete)
{
    EXPECT_EXIT({ CheckReentrantAndConcurrentStop(false); ExitWithShutdownTestResult(); },
                testing::ExitedWithCode(EXIT_SUCCESS), "");
}

TEST(LatestBatchPublisherShutdownDeathTest, ThrowAfterCallbackStopAndConcurrentJoinsComplete)
{
    EXPECT_EXIT({ CheckReentrantAndConcurrentStop(true); ExitWithShutdownTestResult(); },
                testing::ExitedWithCode(EXIT_SUCCESS), "");
}

TEST(LatestBatchPublisherShutdownDeathTest, DestructorJoinsAfterSelfStop)
{
    EXPECT_EXIT({ CheckSelfStopThenDestruction(); ExitWithShutdownTestResult(); },
                testing::ExitedWithCode(EXIT_SUCCESS), "");
}

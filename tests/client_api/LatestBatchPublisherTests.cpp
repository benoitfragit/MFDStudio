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
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <stdexcept>
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
    mfd::CommandBatch secondBatch;
    secondBatch.sequence = 2U;
    secondBatch.mappingHash = "hash-alpha";
    secondBatch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"HUD", "fpmLimitX"}, pendingPatch});
    ASSERT_TRUE(publisher.SubmitLatest(std::move(secondBatch)));

    mfd::ReticlePatch newestPatch;
    newestPatch.position = mfd::Vec2 {0.3f, 0.4f};
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

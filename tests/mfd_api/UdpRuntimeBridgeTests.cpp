/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for UdpRuntimeBridge threading and transport behavior.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "mfd/control/CommandTypes.h"
#include "mfd/control/StrobeFeedback.h"
#include "mfd/control/UdpRuntimeBridge.h"
#include "mfd/ipc/ExchangeChannel.h"

namespace
{
/**
 * @brief In-memory state shared by fake exchange channels.
 */
struct FakeChannelState
{
    mutable std::mutex mutex;
    std::deque<std::vector<std::byte>> inboundPayloads;
    std::deque<std::vector<std::byte>> sentPayloads;
    std::string lastError {};
    bool ready = true;

    /**
     * @brief Pushes one payload to be consumed by `TryReceive`.
     */
    void PushInbound(std::vector<std::byte> payload)
    {
        std::lock_guard lock(mutex);
        inboundPayloads.push_back(std::move(payload));
    }

    /**
     * @brief Pushes the same payload multiple times to the inbound queue.
     */
    void PushInboundRepeated(const std::vector<std::byte>& payload, const std::size_t count)
    {
        std::lock_guard lock(mutex);
        for (std::size_t index = 0; index < count; ++index)
        {
            inboundPayloads.push_back(payload);
        }
    }
};

/**
 * @brief Fake exchange channel used to validate UdpRuntimeBridge thread behavior.
 */
class FakeExchangeChannel final : public mfd::IExchangeChannel
{
public:
    /**
     * @brief Role of one fake channel instance.
     */
    enum class Role
    {
        Receiver,
        Sender
    };

    FakeExchangeChannel(std::shared_ptr<FakeChannelState> state, const Role role)
        : state_(std::move(state))
        , role_(role)
    {
    }

    bool IsReady() const noexcept override
    {
        std::lock_guard lock(state_->mutex);
        return state_->ready;
    }

    bool Send(const mfd::ByteView buffer) override
    {
        std::lock_guard lock(state_->mutex);
        if (!state_->ready || role_ != Role::Sender)
        {
            state_->lastError = "Fake sender channel is not ready";
            return false;
        }

        state_->sentPayloads.emplace_back(buffer.begin(), buffer.end());
        state_->lastError.clear();
        return true;
    }

    std::optional<std::vector<std::byte>> TryReceive() override
    {
        std::lock_guard lock(state_->mutex);
        if (!state_->ready || role_ != Role::Receiver || state_->inboundPayloads.empty())
        {
            return std::nullopt;
        }

        std::vector<std::byte> payload = std::move(state_->inboundPayloads.front());
        state_->inboundPayloads.pop_front();
        return payload;
    }

    std::string LastError() const override
    {
        std::lock_guard lock(state_->mutex);
        return state_->lastError;
    }

private:
    std::shared_ptr<FakeChannelState> state_;
    Role role_ = Role::Receiver;
};

/**
 * @brief Waits until a condition becomes true or a timeout is reached.
 */
bool WaitUntil(const std::chrono::milliseconds timeout, const std::function<bool()>& predicate)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (predicate())
        {
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    return predicate();
}

/**
 * @brief Converts a string payload into a byte vector.
 */
std::vector<std::byte> ToBytes(const std::string& payload)
{
    std::vector<std::byte> bytes(payload.size());
    std::memcpy(bytes.data(), payload.data(), payload.size());
    return bytes;
}

/**
 * @brief Builds a command batch containing repeated reset commands.
 */
mfd::CommandBatch MakeResetBatch(const std::uint32_t sequence, const std::size_t commandCount)
{
    mfd::CommandBatch batch;
    batch.sequence = sequence;
    batch.mappingHash = "map_hash";
    batch.commands.reserve(commandCount);
    for (std::size_t index = 0; index < commandCount; ++index)
    {
        batch.commands.push_back(mfd::ResetWindowCommand {});
    }

    return batch;
}

/**
 * @brief Builds one reticle position update command for coalescing tests.
 */
mfd::UserCommand MakeReticlePositionCommand(const float x)
{
    mfd::UpdateReticleCommand command;
    command.target.pageId = 11U;
    command.target.reticleId = 23U;
    command.patch.position = mfd::Vec2 {x, 0.0f};
    return command;
}

/**
 * @brief Builds one window brightness update command for coalescing tests.
 */
mfd::UserCommand MakeWindowBrightnessCommand(const float brightness)
{
    mfd::UpdateWindowDisplayCommand command;
    command.patch.brightness = brightness;
    return command;
}

/**
 * @brief Builds one window disabled-state update command for coalescing tests.
 */
mfd::UserCommand MakeWindowDisabledCommand(const bool disabled)
{
    mfd::UpdateWindowDisplayCommand command;
    command.patch.disabled = disabled;
    return command;
}
} // namespace

TEST(UdpRuntimeBridgeTests, DrainsReceivedBatchesFromWorkerQueue)
{
    auto receiverState = std::make_shared<FakeChannelState>();
    auto senderState = std::make_shared<FakeChannelState>();

    mfd::CommandBatch batch;
    batch.sequence = 17U;
    batch.mappingHash = "map_hash";
    batch.commands.push_back(mfd::ResetWindowCommand {});
    receiverState->PushInbound(ToBytes(mfd::SerializeCommandBatch(batch)));

    mfd::UdpRuntimeBridge bridge(
        [receiverState]()
        {
            return std::make_unique<FakeExchangeChannel>(receiverState, FakeExchangeChannel::Role::Receiver);
        },
        [senderState]()
        {
            return std::make_unique<FakeExchangeChannel>(senderState, FakeExchangeChannel::Role::Sender);
        });

    ASSERT_TRUE(bridge.Start());
    ASSERT_TRUE(bridge.IsRunning());

    std::vector<mfd::CommandBatch> drained;
    ASSERT_TRUE(WaitUntil(
        std::chrono::milliseconds(300),
        [&bridge, &drained]()
        {
            return bridge.DrainReceivedBatches(drained, 4) > 0;
        }));

    EXPECT_EQ(drained.size(), 1U);
    EXPECT_EQ(drained.front().sequence, 17U);
    EXPECT_EQ(drained.front().mappingHash, "map_hash");
    ASSERT_EQ(drained.front().commands.size(), 1U);
    EXPECT_NE(std::get_if<mfd::ResetWindowCommand>(&drained.front().commands.front()), nullptr);
    const mfd::UdpRuntimeBridgeMetrics metrics = bridge.MetricsSnapshot();
    EXPECT_EQ(metrics.receivedPackets, 1U);
    EXPECT_GT(metrics.receivedBytes, 0U);
    EXPECT_EQ(metrics.decodedBatches, 1U);
    EXPECT_EQ(metrics.decodeErrors, 0U);
    EXPECT_EQ(metrics.queuedBatches, 1U);
    EXPECT_EQ(metrics.drainedBatches, 1U);
    EXPECT_EQ(metrics.drainedCommands, 1U);
    EXPECT_EQ(metrics.inboundQueueDepth, 0U);
    bridge.Stop();
}

TEST(UdpRuntimeBridgeTests, MetricsSnapshotCountsDecodeErrorsAndQueueDepth)
{
    auto receiverState = std::make_shared<FakeChannelState>();
    auto senderState = std::make_shared<FakeChannelState>();

    receiverState->PushInbound(ToBytes("not a command envelope"));

    mfd::UdpRuntimeBridge bridge(
        [receiverState]()
        {
            return std::make_unique<FakeExchangeChannel>(receiverState, FakeExchangeChannel::Role::Receiver);
        },
        [senderState]()
        {
            return std::make_unique<FakeExchangeChannel>(senderState, FakeExchangeChannel::Role::Sender);
        });

    ASSERT_TRUE(bridge.Start());

    ASSERT_TRUE(WaitUntil(
        std::chrono::milliseconds(300),
        [&bridge]()
        {
            return bridge.MetricsSnapshot().decodeErrors == 1U;
        }));

    const mfd::UdpRuntimeBridgeMetrics metrics = bridge.MetricsSnapshot();
    EXPECT_EQ(metrics.receivedPackets, 1U);
    EXPECT_GT(metrics.receivedBytes, 0U);
    EXPECT_EQ(metrics.decodedBatches, 0U);
    EXPECT_EQ(metrics.queuedBatches, 0U);
    EXPECT_EQ(metrics.inboundQueueDepth, 0U);
    bridge.Stop();
}

TEST(UdpRuntimeBridgeTests, DrainReceivedBatchesForCommandBudgetLeavesExcessQueued)
{
    auto receiverState = std::make_shared<FakeChannelState>();
    auto senderState = std::make_shared<FakeChannelState>();

    receiverState->PushInbound(ToBytes(mfd::SerializeCommandBatch(MakeResetBatch(1U, 2U))));
    receiverState->PushInbound(ToBytes(mfd::SerializeCommandBatch(MakeResetBatch(2U, 2U))));
    receiverState->PushInbound(ToBytes(mfd::SerializeCommandBatch(MakeResetBatch(3U, 1U))));

    mfd::UdpRuntimeBridge bridge(
        [receiverState]()
        {
            return std::make_unique<FakeExchangeChannel>(receiverState, FakeExchangeChannel::Role::Receiver);
        },
        [senderState]()
        {
            return std::make_unique<FakeExchangeChannel>(senderState, FakeExchangeChannel::Role::Sender);
        });

    ASSERT_TRUE(bridge.Start());
    ASSERT_TRUE(WaitUntil(
        std::chrono::milliseconds(300),
        [&bridge]()
        {
            return bridge.MetricsSnapshot().inboundQueueDepth == 3U;
        }));

    std::vector<mfd::CommandBatch> drained;
    EXPECT_EQ(bridge.DrainReceivedBatchesForCommandBudget(drained, 3U), 1U);
    ASSERT_EQ(drained.size(), 1U);
    EXPECT_EQ(drained.front().sequence, 1U);
    EXPECT_EQ(drained.front().commands.size(), 2U);

    mfd::UdpRuntimeBridgeMetrics metrics = bridge.MetricsSnapshot();
    EXPECT_EQ(metrics.drainedBatches, 1U);
    EXPECT_EQ(metrics.drainedCommands, 2U);
    EXPECT_EQ(metrics.inboundQueueDepth, 2U);

    drained.clear();
    EXPECT_EQ(bridge.DrainReceivedBatchesForCommandBudget(drained, 3U), 2U);
    ASSERT_EQ(drained.size(), 2U);
    EXPECT_EQ(drained[0].sequence, 2U);
    EXPECT_EQ(drained[1].sequence, 3U);

    metrics = bridge.MetricsSnapshot();
    EXPECT_EQ(metrics.drainedBatches, 3U);
    EXPECT_EQ(metrics.drainedCommands, 5U);
    EXPECT_EQ(metrics.inboundQueueDepth, 0U);
    bridge.Stop();
}

TEST(UdpRuntimeBridgeTests, DrainReceivedBatchesForCommandBudgetStillDrainsFrontOversizedBatch)
{
    auto receiverState = std::make_shared<FakeChannelState>();
    auto senderState = std::make_shared<FakeChannelState>();

    receiverState->PushInbound(ToBytes(mfd::SerializeCommandBatch(MakeResetBatch(9U, 2U))));

    mfd::UdpRuntimeBridge bridge(
        [receiverState]()
        {
            return std::make_unique<FakeExchangeChannel>(receiverState, FakeExchangeChannel::Role::Receiver);
        },
        [senderState]()
        {
            return std::make_unique<FakeExchangeChannel>(senderState, FakeExchangeChannel::Role::Sender);
        });

    ASSERT_TRUE(bridge.Start());
    ASSERT_TRUE(WaitUntil(
        std::chrono::milliseconds(300),
        [&bridge]()
        {
            return bridge.MetricsSnapshot().inboundQueueDepth == 1U;
        }));

    std::vector<mfd::CommandBatch> drained;
    EXPECT_EQ(bridge.DrainReceivedBatchesForCommandBudget(drained, 1U), 1U);
    ASSERT_EQ(drained.size(), 1U);
    EXPECT_EQ(drained.front().sequence, 9U);
    EXPECT_EQ(drained.front().commands.size(), 2U);

    mfd::UdpRuntimeBridgeMetrics metrics = bridge.MetricsSnapshot();
    EXPECT_EQ(metrics.drainedBatches, 1U);
    EXPECT_EQ(metrics.drainedCommands, 2U);
    EXPECT_EQ(metrics.inboundQueueDepth, 0U);
    bridge.Stop();
}

TEST(UdpRuntimeBridgeTests, CoalescesRepeatedStateLikeCommandsInQueuedBatch)
{
    auto receiverState = std::make_shared<FakeChannelState>();
    auto senderState = std::make_shared<FakeChannelState>();

    mfd::CommandBatch batch;
    batch.sequence = 31U;
    batch.commands.push_back(MakeReticlePositionCommand(0.1f));
    batch.commands.push_back(MakeReticlePositionCommand(0.2f));
    batch.commands.push_back(MakeReticlePositionCommand(0.3f));
    receiverState->PushInbound(ToBytes(mfd::SerializeCommandBatch(batch)));

    mfd::UdpRuntimeBridge bridge(
        [receiverState]()
        {
            return std::make_unique<FakeExchangeChannel>(receiverState, FakeExchangeChannel::Role::Receiver);
        },
        [senderState]()
        {
            return std::make_unique<FakeExchangeChannel>(senderState, FakeExchangeChannel::Role::Sender);
        });

    ASSERT_TRUE(bridge.Start());
    ASSERT_TRUE(WaitUntil(
        std::chrono::milliseconds(300),
        [&bridge]()
        {
            return bridge.MetricsSnapshot().inboundQueueDepth == 1U;
        }));

    std::vector<mfd::CommandBatch> drained;
    ASSERT_EQ(bridge.DrainReceivedBatchesForCommandBudget(drained, 4U), 1U);
    ASSERT_EQ(drained.size(), 1U);
    ASSERT_EQ(drained.front().commands.size(), 1U);

    const auto* command = std::get_if<mfd::UpdateReticleCommand>(&drained.front().commands.front());
    ASSERT_NE(command, nullptr);
    ASSERT_TRUE(command->patch.position.has_value());
    EXPECT_FLOAT_EQ(command->patch.position->x, 0.3f);

    const mfd::UdpRuntimeBridgeMetrics metrics = bridge.MetricsSnapshot();
    EXPECT_EQ(metrics.coalescedCommands, 2U);
    EXPECT_EQ(metrics.drainedCommands, 1U);
    bridge.Stop();
}

TEST(UdpRuntimeBridgeTests, CoalescingPreservesEventLikeCommandBarriers)
{
    auto receiverState = std::make_shared<FakeChannelState>();
    auto senderState = std::make_shared<FakeChannelState>();

    mfd::CommandBatch batch;
    batch.sequence = 32U;
    batch.commands.push_back(MakeReticlePositionCommand(0.1f));
    batch.commands.push_back(mfd::ResetWindowCommand {});
    batch.commands.push_back(MakeReticlePositionCommand(0.2f));
    receiverState->PushInbound(ToBytes(mfd::SerializeCommandBatch(batch)));

    mfd::UdpRuntimeBridge bridge(
        [receiverState]()
        {
            return std::make_unique<FakeExchangeChannel>(receiverState, FakeExchangeChannel::Role::Receiver);
        },
        [senderState]()
        {
            return std::make_unique<FakeExchangeChannel>(senderState, FakeExchangeChannel::Role::Sender);
        });

    ASSERT_TRUE(bridge.Start());
    ASSERT_TRUE(WaitUntil(
        std::chrono::milliseconds(300),
        [&bridge]()
        {
            return bridge.MetricsSnapshot().inboundQueueDepth == 1U;
        }));

    std::vector<mfd::CommandBatch> drained;
    ASSERT_EQ(bridge.DrainReceivedBatchesForCommandBudget(drained, 4U), 1U);
    ASSERT_EQ(drained.size(), 1U);
    ASSERT_EQ(drained.front().commands.size(), 3U);
    EXPECT_NE(std::get_if<mfd::UpdateReticleCommand>(&drained.front().commands[0]), nullptr);
    EXPECT_NE(std::get_if<mfd::ResetWindowCommand>(&drained.front().commands[1]), nullptr);
    EXPECT_NE(std::get_if<mfd::UpdateReticleCommand>(&drained.front().commands[2]), nullptr);
    EXPECT_EQ(bridge.MetricsSnapshot().coalescedCommands, 0U);
    bridge.Stop();
}

TEST(UdpRuntimeBridgeTests, CoalescingMergesStatePatchFields)
{
    auto receiverState = std::make_shared<FakeChannelState>();
    auto senderState = std::make_shared<FakeChannelState>();

    mfd::CommandBatch batch;
    batch.sequence = 33U;
    batch.commands.push_back(MakeWindowBrightnessCommand(0.35f));
    batch.commands.push_back(MakeWindowDisabledCommand(true));
    receiverState->PushInbound(ToBytes(mfd::SerializeCommandBatch(batch)));

    mfd::UdpRuntimeBridge bridge(
        [receiverState]()
        {
            return std::make_unique<FakeExchangeChannel>(receiverState, FakeExchangeChannel::Role::Receiver);
        },
        [senderState]()
        {
            return std::make_unique<FakeExchangeChannel>(senderState, FakeExchangeChannel::Role::Sender);
        });

    ASSERT_TRUE(bridge.Start());
    ASSERT_TRUE(WaitUntil(
        std::chrono::milliseconds(300),
        [&bridge]()
        {
            return bridge.MetricsSnapshot().inboundQueueDepth == 1U;
        }));

    std::vector<mfd::CommandBatch> drained;
    ASSERT_EQ(bridge.DrainReceivedBatchesForCommandBudget(drained, 4U), 1U);
    ASSERT_EQ(drained.size(), 1U);
    ASSERT_EQ(drained.front().commands.size(), 1U);

    const auto* command = std::get_if<mfd::UpdateWindowDisplayCommand>(&drained.front().commands.front());
    ASSERT_NE(command, nullptr);
    ASSERT_TRUE(command->patch.brightness.has_value());
    ASSERT_TRUE(command->patch.disabled.has_value());
    EXPECT_FLOAT_EQ(*command->patch.brightness, 0.35f);
    EXPECT_TRUE(*command->patch.disabled);
    EXPECT_EQ(bridge.MetricsSnapshot().coalescedCommands, 1U);
    bridge.Stop();
}

TEST(UdpRuntimeBridgeTests, SendsQueuedStrobeFeedbackFromWorkerThread)
{
    auto receiverState = std::make_shared<FakeChannelState>();
    auto senderState = std::make_shared<FakeChannelState>();

    mfd::UdpRuntimeBridge bridge(
        [receiverState]()
        {
            return std::make_unique<FakeExchangeChannel>(receiverState, FakeExchangeChannel::Role::Receiver);
        },
        [senderState]()
        {
            return std::make_unique<FakeExchangeChannel>(senderState, FakeExchangeChannel::Role::Sender);
        });

    ASSERT_TRUE(bridge.Start());
    mfd::StrobeStatusFeedback feedback;
    feedback.sequence = 7;
    feedback.pageName = "hud";
    feedback.strobeId = "cursor";
    bridge.EnqueueStrobeFeedback(std::move(feedback));

    ASSERT_TRUE(WaitUntil(
        std::chrono::milliseconds(300),
        [senderState]()
        {
            std::lock_guard lock(senderState->mutex);
            return !senderState->sentPayloads.empty();
        }));

    bridge.Stop();
    EXPECT_FALSE(bridge.IsRunning());
}

TEST(UdpRuntimeBridgeTests, SendsQueuedActivePageFeedbackFromWorkerThread)
{
    auto receiverState = std::make_shared<FakeChannelState>();
    auto senderState = std::make_shared<FakeChannelState>();

    mfd::UdpRuntimeBridge bridge(
        [receiverState]()
        {
            return std::make_unique<FakeExchangeChannel>(receiverState, FakeExchangeChannel::Role::Receiver);
        },
        [senderState]()
        {
            return std::make_unique<FakeExchangeChannel>(senderState, FakeExchangeChannel::Role::Sender);
        });

    ASSERT_TRUE(bridge.Start());
    mfd::ActivePageFeedback feedback;
    feedback.sequence = 11U;
    feedback.pageName = "Page1";
    bridge.EnqueueActivePageFeedback(std::move(feedback));

    ASSERT_TRUE(WaitUntil(
        std::chrono::milliseconds(300),
        [senderState]()
        {
            std::lock_guard lock(senderState->mutex);
            return !senderState->sentPayloads.empty();
        }));

    bridge.Stop();
    const mfd::UdpRuntimeBridgeMetrics metrics = bridge.MetricsSnapshot();
    EXPECT_EQ(metrics.feedbackQueued, 1U);
    EXPECT_EQ(metrics.feedbackSent, 1U);
    EXPECT_EQ(metrics.feedbackDropped, 0U);
    EXPECT_FALSE(bridge.IsRunning());
}

TEST(UdpRuntimeBridgeTests, CountsFeedbackQueueOverflowDrops)
{
    auto receiverState = std::make_shared<FakeChannelState>();
    auto senderState = std::make_shared<FakeChannelState>();
    receiverState->ready = false;
    senderState->ready = false;

    mfd::UdpRuntimeBridge bridge(
        [receiverState]()
        {
            return std::make_unique<FakeExchangeChannel>(receiverState, FakeExchangeChannel::Role::Receiver);
        },
        [senderState]()
        {
            return std::make_unique<FakeExchangeChannel>(senderState, FakeExchangeChannel::Role::Sender);
        });

    EXPECT_FALSE(bridge.Start());
    ASSERT_TRUE(bridge.HasFeedbackSender());

    for (std::size_t index = 0; index < 260U; ++index)
    {
        mfd::ActivePageFeedback feedback;
        feedback.sequence = static_cast<std::uint32_t>(index + 1U);
        feedback.pageName = "Page1";
        bridge.EnqueueActivePageFeedback(std::move(feedback));
    }

    const mfd::UdpRuntimeBridgeMetrics metrics = bridge.MetricsSnapshot();
    EXPECT_EQ(metrics.feedbackQueued, 260U);
    EXPECT_EQ(metrics.feedbackDropped, 4U);
    EXPECT_EQ(metrics.outboundQueueDepth, 256U);
    bridge.Stop();
}

TEST(UdpRuntimeBridgeTests, ReportsInboundBatchOverflowUsingBatchTerminology)
{
    auto receiverState = std::make_shared<FakeChannelState>();
    auto senderState = std::make_shared<FakeChannelState>();

    mfd::CommandBatch batch;
    batch.sequence = 1U;
    batch.mappingHash = "map_hash";
    batch.commands.push_back(mfd::ResetWindowCommand {});

    const std::vector<std::byte> payload = ToBytes(mfd::SerializeCommandBatch(batch));
    receiverState->PushInboundRepeated(payload, 8200U);

    mfd::UdpRuntimeBridge bridge(
        [receiverState]()
        {
            return std::make_unique<FakeExchangeChannel>(receiverState, FakeExchangeChannel::Role::Receiver);
        },
        [senderState]()
        {
            return std::make_unique<FakeExchangeChannel>(senderState, FakeExchangeChannel::Role::Sender);
        });

    ASSERT_TRUE(bridge.Start());

    ASSERT_TRUE(WaitUntil(
        std::chrono::seconds(2),
        [receiverState]()
        {
            std::lock_guard lock(receiverState->mutex);
            return receiverState->inboundPayloads.empty();
        }));

    ASSERT_TRUE(WaitUntil(
        std::chrono::milliseconds(300),
        [&bridge]()
        {
            return bridge.LastCommandStatus().find("oldest batches") != std::string::npos;
        }));

    std::vector<mfd::CommandBatch> drained;
    EXPECT_EQ(bridge.DrainReceivedBatches(drained, 9000U), 8192U);
    EXPECT_EQ(bridge.LastCommandStatus(), "UDP command batch queue overflow, dropping oldest batches");
    bridge.Stop();
}

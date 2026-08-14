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
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "mfd/control/CommandTypes.h"
#include "mfd/control/StrobeFeedback.h"
#include "mfd/control/UdpRuntimeBridge.h"
#include "mfd/control/WindowFeedback.h"
#include "mfd/ipc/ExchangeChannel.h"
#include "UdpRuntimeBridgeInternal.hpp"

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
    std::optional<std::size_t> maxSendBytes {};
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

        if (state_->maxSendBytes.has_value() && buffer.size() > *state_->maxSendBytes)
        {
            state_->lastError = "UDP payload exceeds configured maxPacketSize";
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
 * @brief Builds one bulk dynamic-reticle batch with the requested weighted workload.
 */
mfd::CommandBatch MakeBulkDynamicReticleBatch(const std::uint32_t sequence,
                                              const std::size_t reticleCount)
{
    mfd::UpsertDynamicReticlesCommand command;
    command.pageId = 11U;
    command.templateTransportId = 301U;
    command.reticles.reserve(reticleCount);
    for (std::size_t index = 0U; index < reticleCount; ++index)
    {
        mfd::DynamicReticleState state;
        state.runtimeReticleId = static_cast<mfd::RuntimeDynamicId>(index + 1U);
        command.reticles.push_back(std::move(state));
    }

    mfd::CommandBatch batch;
    batch.sequence = sequence;
    batch.mappingHash = "map_hash";
    batch.commands.emplace_back(std::move(command));
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
 * @brief Builds one string-addressed reticle position update command for fallback coalescing tests.
 */
mfd::UserCommand MakeNamedReticlePositionCommand(const float x)
{
    mfd::UpdateReticleCommand command;
    command.target.page = "Radar";
    command.target.reticle = "HeadingBox";
    command.patch.position = mfd::Vec2 {x, 0.0f};
    return command;
}

/**
 * @brief Builds one reticle time-value update command for primitive-level coalescing tests.
 */
mfd::UserCommand MakeReticleTimeValueCommand()
{
    mfd::UpdateReticleCommand command;
    command.target.pageId = 11U;
    command.target.reticleId = 23U;
    command.patch.primitivePatchesById[101U].timeValue = mfd::TimeValue {2026, 6, 13, 14, 30, 45};
    return command;
}

/**
 * @brief Builds one reticle UTC toggle update command for primitive-level coalescing tests.
 */
mfd::UserCommand MakeReticleTimeUtcCommand(const bool utc)
{
    mfd::UpdateReticleCommand command;
    command.target.pageId = 11U;
    command.target.reticleId = 23U;
    command.patch.primitivePatchesById[101U].timeUtc = utc;
    return command;
}

/**
 * @brief Builds one reticle time-field visibility update command for primitive-level coalescing tests.
 */
mfd::UserCommand MakeReticleTimeFieldVisibilityCommand()
{
    mfd::UpdateReticleCommand command;
    command.target.pageId = 11U;
    command.target.reticleId = 23U;
    command.patch.primitivePatchesById[101U].timeFields =
        mfd::TimeFieldVisibility {true, true, false, true, true, false};
    return command;
}

/**
 * @brief Builds one reticle clear-time override command for primitive-level coalescing tests.
 */
mfd::UserCommand MakeReticleClearTimeValueCommand()
{
    mfd::UpdateReticleCommand command;
    command.target.pageId = 11U;
    command.target.reticleId = 23U;
    command.patch.primitivePatchesById[101U].clearTimeValue = true;
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

/**
 * @brief Builds one generated strobe update command for numeric-id coalescing tests.
 */
mfd::UserCommand MakeGeneratedStrobeCommand(const std::optional<bool> active, const std::optional<mfd::Vec2> position)
{
    mfd::UpdateStrobeCommand command;
    command.pageId = 11U;
    command.strobeId = 101U;
    command.active = active;
    command.position = position;
    return command;
}

/**
 * @brief Builds one generated dynamic-reticle upsert command for numeric-id coalescing tests.
 */
mfd::UserCommand MakeGeneratedDynamicUpsertCommand(const std::optional<mfd::Vec2> position,
                                                   const std::optional<bool> visible)
{
    mfd::UpsertDynamicReticleCommand command;
    command.target.pageId = 11U;
    command.target.runtimeReticleId = 501U;
    command.templateTransportId = 301U;
    if (position.has_value())
    {
        command.patch.position = *position;
    }
    if (visible.has_value())
    {
        command.patch.visible = *visible;
    }

    return command;
}

/**
 * @brief Builds one generated dynamic-set visibility command for numeric-id coalescing tests.
 */
mfd::UserCommand MakeGeneratedDynamicSetVisibilityCommand(const bool visible)
{
    mfd::SetDynamicReticleSetVisibilityCommand command;
    command.pageId = 11U;
    command.templateTransportId = 301U;
    command.visible = visible;
    return command;
}

/**
 * @brief Builds one generated dynamic-set strobe-magnet command for numeric-id coalescing tests.
 */
mfd::UserCommand MakeGeneratedDynamicSetStrobeMagnetCommand(const bool enabled)
{
    mfd::SetDynamicReticleSetStrobeMagnetEnabledCommand command;
    command.pageId = 11U;
    command.templateTransportId = 301U;
    command.enabled = enabled;
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
    batch.fragment = mfd::CommandBatchFragment {101U, 201U, 301U, 0U, 1U};
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
    ASSERT_TRUE(drained.front().fragment.has_value());
    EXPECT_EQ(drained.front().fragment->clientId, 101U);
    EXPECT_EQ(drained.front().fragment->sessionEpoch, 201U);
    EXPECT_EQ(drained.front().fragment->batchId, 301U);
    EXPECT_EQ(drained.front().fragment->chunkIndex, 0U);
    EXPECT_EQ(drained.front().fragment->chunkCount, 1U);
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

TEST(UdpRuntimeBridgeTests, WeightedBudgetCountsEveryBulkDynamicReticle)
{
    auto receiverState = std::make_shared<FakeChannelState>();
    auto senderState = std::make_shared<FakeChannelState>();
    receiverState->PushInbound(ToBytes(mfd::SerializeCommandBatch(MakeBulkDynamicReticleBatch(1U, 300U))));
    receiverState->PushInbound(ToBytes(mfd::SerializeCommandBatch(MakeBulkDynamicReticleBatch(2U, 300U))));
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
    EXPECT_EQ(bridge.DrainReceivedBatchesForCommandBudget(drained, 512U, 4U), 1U);
    ASSERT_EQ(drained.size(), 1U);
    EXPECT_EQ(drained.front().sequence, 1U);

    mfd::UdpRuntimeBridgeMetrics metrics = bridge.MetricsSnapshot();
    EXPECT_EQ(metrics.drainedBatches, 1U);
    EXPECT_EQ(metrics.drainedCommands, 1U);
    EXPECT_EQ(metrics.inboundQueueDepth, 2U);

    drained.clear();
    EXPECT_EQ(bridge.DrainReceivedBatchesForCommandBudget(drained, 512U, 4U), 2U);
    ASSERT_EQ(drained.size(), 2U);
    EXPECT_EQ(drained[0].sequence, 2U);
    EXPECT_EQ(drained[1].sequence, 3U);

    metrics = bridge.MetricsSnapshot();
    EXPECT_EQ(metrics.drainedBatches, 3U);
    EXPECT_EQ(metrics.drainedCommands, 3U);
    EXPECT_EQ(metrics.inboundQueueDepth, 0U);
    bridge.Stop();
}

TEST(UdpRuntimeBridgeTests, WeightedBudgetStillDrainsFrontOversizedBulkBatch)
{
    auto receiverState = std::make_shared<FakeChannelState>();
    auto senderState = std::make_shared<FakeChannelState>();
    receiverState->PushInbound(ToBytes(mfd::SerializeCommandBatch(MakeBulkDynamicReticleBatch(9U, 600U))));

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
    EXPECT_EQ(bridge.DrainReceivedBatchesForCommandBudget(drained, 512U, 4U), 1U);
    ASSERT_EQ(drained.size(), 1U);
    EXPECT_EQ(drained.front().sequence, 9U);
    EXPECT_EQ(bridge.MetricsSnapshot().drainedCommands, 1U);
    bridge.Stop();
}

TEST(UdpRuntimeBridgeTests, WeightedBudgetHonorsExplicitBatchCap)
{
    auto receiverState = std::make_shared<FakeChannelState>();
    auto senderState = std::make_shared<FakeChannelState>();
    for (std::uint32_t sequence = 1U; sequence <= 10U; ++sequence)
    {
        receiverState->PushInbound(ToBytes(mfd::SerializeCommandBatch(MakeResetBatch(sequence, 1U))));
    }

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
            return bridge.MetricsSnapshot().inboundQueueDepth == 10U;
        }));

    std::vector<mfd::CommandBatch> drained;
    EXPECT_EQ(bridge.DrainReceivedBatchesForCommandBudget(drained, 512U, 4U), 4U);
    ASSERT_EQ(drained.size(), 4U);
    EXPECT_EQ(drained.front().sequence, 1U);
    EXPECT_EQ(drained.back().sequence, 4U);

    const mfd::UdpRuntimeBridgeMetrics metrics = bridge.MetricsSnapshot();
    EXPECT_EQ(metrics.drainedBatches, 4U);
    EXPECT_EQ(metrics.drainedCommands, 4U);
    EXPECT_EQ(metrics.inboundQueueDepth, 6U);
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

TEST(UdpRuntimeBridgeTests, CoalescingFallsBackToStringAddressingWhenIdsAreAbsent)
{
    mfd::CommandBatch batch;
    batch.sequence = 31U;
    batch.commands.push_back(MakeNamedReticlePositionCommand(0.1f));
    batch.commands.push_back(MakeNamedReticlePositionCommand(0.2f));

    EXPECT_EQ(mfd::detail::CoalesceCommandBatch(batch), 1U);
    ASSERT_EQ(batch.commands.size(), 1U);

    const auto* command = std::get_if<mfd::UpdateReticleCommand>(&batch.commands.front());
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->target.page, "Radar");
    EXPECT_EQ(command->target.reticle, "HeadingBox");
    ASSERT_TRUE(command->patch.position.has_value());
    EXPECT_FLOAT_EQ(command->patch.position->x, 0.2f);
}

TEST(UdpRuntimeBridgeTests, CoalescingPreservesPageActivationOrder)
{
    mfd::CommandBatch batch;
    batch.sequence = 32U;
    batch.commands.push_back(mfd::ActivatePageCommand {"Radar"});
    batch.commands.push_back(MakeGeneratedStrobeCommand(true, std::nullopt));
    batch.commands.push_back(mfd::ActivatePageCommand {"Navigation"});

    EXPECT_EQ(mfd::detail::CoalesceCommandBatch(batch), 0U);
    ASSERT_EQ(batch.commands.size(), 3U);
    const auto* firstActivation = std::get_if<mfd::ActivatePageCommand>(&batch.commands[0]);
    const auto* strobeUpdate = std::get_if<mfd::UpdateStrobeCommand>(&batch.commands[1]);
    const auto* secondActivation = std::get_if<mfd::ActivatePageCommand>(&batch.commands[2]);
    ASSERT_NE(firstActivation, nullptr);
    ASSERT_NE(strobeUpdate, nullptr);
    ASSERT_NE(secondActivation, nullptr);
    EXPECT_EQ(firstActivation->page, "Radar");
    EXPECT_EQ(secondActivation->page, "Navigation");
}

TEST(UdpRuntimeBridgeTests, CoalescingDoesNotCrossUnrelatedStateCommands)
{
    mfd::CommandBatch batch;
    batch.sequence = 32U;
    batch.commands.push_back(MakeReticlePositionCommand(0.1f));
    batch.commands.push_back(MakeWindowBrightnessCommand(0.5f));
    batch.commands.push_back(MakeReticlePositionCommand(0.2f));

    EXPECT_EQ(mfd::detail::CoalesceCommandBatch(batch), 0U);
    ASSERT_EQ(batch.commands.size(), 3U);
    EXPECT_NE(std::get_if<mfd::UpdateReticleCommand>(&batch.commands[0]), nullptr);
    EXPECT_NE(std::get_if<mfd::UpdateWindowDisplayCommand>(&batch.commands[1]), nullptr);
    EXPECT_NE(std::get_if<mfd::UpdateReticleCommand>(&batch.commands[2]), nullptr);
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

TEST(UdpRuntimeBridgeTests, CoalescingMergesPrimitiveTimePatchFields)
{
    auto receiverState = std::make_shared<FakeChannelState>();
    auto senderState = std::make_shared<FakeChannelState>();

    mfd::CommandBatch batch;
    batch.sequence = 34U;
    batch.commands.push_back(MakeReticleTimeValueCommand());
    batch.commands.push_back(MakeReticleTimeUtcCommand(true));
    batch.commands.push_back(MakeReticleTimeFieldVisibilityCommand());
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
    ASSERT_EQ(command->patch.primitivePatchesById.size(), 1U);

    const mfd::PrimitivePatch& primitivePatch = command->patch.primitivePatchesById.at(101U);
    ASSERT_TRUE(primitivePatch.timeValue.has_value());
    EXPECT_EQ(primitivePatch.timeValue->year, 2026);
    EXPECT_EQ(primitivePatch.timeValue->hour, 14);
    ASSERT_TRUE(primitivePatch.timeUtc.has_value());
    EXPECT_TRUE(*primitivePatch.timeUtc);
    ASSERT_TRUE(primitivePatch.timeFields.has_value());
    EXPECT_TRUE(primitivePatch.timeFields->year);
    EXPECT_TRUE(primitivePatch.timeFields->minute);
    EXPECT_FALSE(primitivePatch.timeFields->day);
    EXPECT_FALSE(primitivePatch.clearTimeValue);
    EXPECT_EQ(bridge.MetricsSnapshot().coalescedCommands, 2U);
    bridge.Stop();
}

TEST(UdpRuntimeBridgeTests, CoalescingLetsClearTimeOverrideReplaceEarlierTimeValue)
{
    auto receiverState = std::make_shared<FakeChannelState>();
    auto senderState = std::make_shared<FakeChannelState>();

    mfd::CommandBatch batch;
    batch.sequence = 35U;
    batch.commands.push_back(MakeReticleTimeValueCommand());
    batch.commands.push_back(MakeReticleTimeUtcCommand(true));
    batch.commands.push_back(MakeReticleClearTimeValueCommand());
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
    ASSERT_EQ(command->patch.primitivePatchesById.size(), 1U);

    const mfd::PrimitivePatch& primitivePatch = command->patch.primitivePatchesById.at(101U);
    EXPECT_FALSE(primitivePatch.timeValue.has_value());
    EXPECT_TRUE(primitivePatch.clearTimeValue);
    ASSERT_TRUE(primitivePatch.timeUtc.has_value());
    EXPECT_TRUE(*primitivePatch.timeUtc);
    EXPECT_EQ(bridge.MetricsSnapshot().coalescedCommands, 2U);
    bridge.Stop();
}

TEST(UdpRuntimeBridgeTests, CoalescingUsesGeneratedStrobeIdsWhenAvailable)
{
    auto receiverState = std::make_shared<FakeChannelState>();
    auto senderState = std::make_shared<FakeChannelState>();

    mfd::CommandBatch batch;
    batch.sequence = 36U;
    batch.mappingHash = "map_hash";
    batch.commands.push_back(MakeGeneratedStrobeCommand(true, std::nullopt));
    batch.commands.push_back(MakeGeneratedStrobeCommand(std::nullopt, mfd::Vec2 {0.45f, -0.15f}));
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

    const auto* command = std::get_if<mfd::UpdateStrobeCommand>(&drained.front().commands.front());
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->pageId, 11U);
    EXPECT_EQ(command->strobeId, 101U);
    ASSERT_TRUE(command->active.has_value());
    EXPECT_TRUE(*command->active);
    ASSERT_TRUE(command->position.has_value());
    EXPECT_FLOAT_EQ(command->position->x, 0.45f);
    EXPECT_FLOAT_EQ(command->position->y, -0.15f);
    EXPECT_EQ(bridge.MetricsSnapshot().coalescedCommands, 1U);
    bridge.Stop();
}

TEST(UdpRuntimeBridgeTests, CoalescingMergesGeneratedDynamicReticleUpserts)
{
    auto receiverState = std::make_shared<FakeChannelState>();
    auto senderState = std::make_shared<FakeChannelState>();

    mfd::CommandBatch batch;
    batch.sequence = 37U;
    batch.mappingHash = "map_hash";
    batch.commands.push_back(MakeGeneratedDynamicUpsertCommand(mfd::Vec2 {0.25f, -0.10f}, std::nullopt));
    batch.commands.push_back(MakeGeneratedDynamicUpsertCommand(std::nullopt, false));
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

    const auto* command = std::get_if<mfd::UpsertDynamicReticleCommand>(&drained.front().commands.front());
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->target.pageId, 11U);
    EXPECT_EQ(command->target.runtimeReticleId, 501U);
    EXPECT_EQ(command->templateTransportId, 301U);
    ASSERT_TRUE(command->patch.position.has_value());
    EXPECT_FLOAT_EQ(command->patch.position->x, 0.25f);
    EXPECT_FLOAT_EQ(command->patch.position->y, -0.10f);
    ASSERT_TRUE(command->patch.visible.has_value());
    EXPECT_FALSE(*command->patch.visible);
    EXPECT_EQ(bridge.MetricsSnapshot().coalescedCommands, 1U);
    bridge.Stop();
}

TEST(UdpRuntimeBridgeTests, CoalescingUsesGeneratedDynamicSetVisibilityIdsWhenAvailable)
{
    auto receiverState = std::make_shared<FakeChannelState>();
    auto senderState = std::make_shared<FakeChannelState>();

    mfd::CommandBatch batch;
    batch.sequence = 38U;
    batch.mappingHash = "map_hash";
    batch.commands.push_back(MakeGeneratedDynamicSetVisibilityCommand(false));
    batch.commands.push_back(MakeGeneratedDynamicSetVisibilityCommand(true));
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

    const auto* command =
        std::get_if<mfd::SetDynamicReticleSetVisibilityCommand>(&drained.front().commands.front());
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->pageId, 11U);
    EXPECT_EQ(command->templateTransportId, 301U);
    EXPECT_TRUE(command->visible);
    EXPECT_EQ(bridge.MetricsSnapshot().coalescedCommands, 1U);
    bridge.Stop();
}

TEST(UdpRuntimeBridgeTests, CoalescingUsesGeneratedDynamicSetStrobeMagnetIdsWhenAvailable)
{
    auto receiverState = std::make_shared<FakeChannelState>();
    auto senderState = std::make_shared<FakeChannelState>();

    mfd::CommandBatch batch;
    batch.sequence = 39U;
    batch.mappingHash = "map_hash";
    batch.commands.push_back(MakeGeneratedDynamicSetStrobeMagnetCommand(false));
    batch.commands.push_back(MakeGeneratedDynamicSetStrobeMagnetCommand(true));
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

    const auto* command =
        std::get_if<mfd::SetDynamicReticleSetStrobeMagnetEnabledCommand>(&drained.front().commands.front());
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->pageId, 11U);
    EXPECT_EQ(command->templateTransportId, 301U);
    EXPECT_TRUE(command->enabled);
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
    feedback.pageId = 11U;
    feedback.strobeId = 12U;
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

TEST(UdpRuntimeBridgeTests, SendsQueuedWindowLifecycleFeedbackFromWorkerThread)
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
    mfd::WindowLifecycleFeedback feedback;
    feedback.sequence = 5U;
    feedback.state = mfd::WindowLifecycleState::Closing;
    bridge.EnqueueWindowLifecycleFeedback(std::move(feedback));

    ASSERT_TRUE(WaitUntil(
        std::chrono::milliseconds(300),
        [senderState]()
        {
            std::lock_guard lock(senderState->mutex);
            return !senderState->sentPayloads.empty();
        }));

    bridge.Stop();

    std::vector<std::byte> sentPayload;
    {
        std::lock_guard lock(senderState->mutex);
        ASSERT_FALSE(senderState->sentPayloads.empty());
        sentPayload = senderState->sentPayloads.front();
    }

    // The fake sender stores raw bytes; decode them through the public feedback decoder.
    const std::string_view payloadView(reinterpret_cast<const char*>(sentPayload.data()), sentPayload.size());
    const auto decoded = mfd::DeserializeWindowLifecycleFeedback(payloadView);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->sequence, 5U);
    EXPECT_EQ(decoded->state, mfd::WindowLifecycleState::Closing);
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
    feedback.pageId = 21U;
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

TEST(UdpRuntimeBridgeTests, CountsOversizedCaptureFeedbackAsDropped)
{
    auto receiverState = std::make_shared<FakeChannelState>();
    auto senderState = std::make_shared<FakeChannelState>();
    senderState->maxSendBytes = 4096U;

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
    feedback.sequence = 12U;
    feedback.pageId = 21U;
    feedback.strobeId = 22U;
    mfd::StrobeFeedbackCapture capture;
    capture.runtimeReticleId = 7001U;
    capture.sourceTemplateTransportId = 31U;
    capture.metadata.emplace("oversized", std::string(5000U, 'x'));
    feedback.captureResult = std::move(capture);
    bridge.EnqueueStrobeFeedback(std::move(feedback));

    ASSERT_TRUE(WaitUntil(
        std::chrono::milliseconds(300),
        [&bridge]()
        {
            return bridge.MetricsSnapshot().feedbackDropped == 1U;
        }));

    bridge.Stop();
    const mfd::UdpRuntimeBridgeMetrics metrics = bridge.MetricsSnapshot();
    EXPECT_EQ(metrics.feedbackQueued, 1U);
    EXPECT_EQ(metrics.feedbackSent, 0U);
    EXPECT_EQ(metrics.feedbackDropped, 1U);
    EXPECT_EQ(bridge.LastFeedbackStatus(), "UDP payload exceeds configured maxPacketSize");

    std::lock_guard lock(senderState->mutex);
    EXPECT_TRUE(senderState->sentPayloads.empty());
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
        feedback.pageId = 21U;
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
    EXPECT_EQ(bridge.LastCommandStatus(), "UDP command queue budget exceeded, dropping oldest batches");
    bridge.Stop();
}

TEST(UdpRuntimeBridgeTests, BoundsCumulativeInboundCommandAndMemoryPressure)
{
    constexpr std::size_t batchCount = 70U;
    constexpr std::size_t commandsPerBatch = 1024U;

    auto receiverState = std::make_shared<FakeChannelState>();
    auto senderState = std::make_shared<FakeChannelState>();
    const std::vector<std::byte> payload =
        ToBytes(mfd::SerializeCommandBatch(MakeResetBatch(1U, commandsPerBatch)));
    receiverState->PushInboundRepeated(payload, batchCount);

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
        [&bridge, batchCount]()
        {
            return bridge.MetricsSnapshot().decodedBatches == batchCount;
        }));

    const mfd::UdpRuntimeBridgeMetrics metrics = bridge.MetricsSnapshot();
    EXPECT_GT(metrics.droppedBatches, 0U);
    EXPECT_LE(metrics.inboundQueueDepth, 64U);
    EXPECT_NE(bridge.LastCommandStatus().find("queue budget exceeded"), std::string::npos);

    std::vector<mfd::CommandBatch> retainedBatches;
    bridge.DrainReceivedBatches(retainedBatches, batchCount);
    std::size_t retainedCommandCount = 0U;
    for (const mfd::CommandBatch& retainedBatch : retainedBatches)
    {
        retainedCommandCount += retainedBatch.commands.size();
    }
    EXPECT_LE(retainedCommandCount, 65536U);
    bridge.Stop();
}

TEST(UdpRuntimeBridgeTests, BoundsCumulativeInboundBulkDynamicReticleWork)
{
    constexpr std::size_t batchCount = 220U;
    constexpr std::size_t reticlesPerBatch = 300U;

    auto receiverState = std::make_shared<FakeChannelState>();
    auto senderState = std::make_shared<FakeChannelState>();
    const std::vector<std::byte> payload =
        ToBytes(mfd::SerializeCommandBatch(MakeBulkDynamicReticleBatch(1U, reticlesPerBatch)));
    receiverState->PushInboundRepeated(payload, batchCount);

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
        std::chrono::seconds(10),
        [&bridge, batchCount]()
        {
            return bridge.MetricsSnapshot().decodedBatches == batchCount;
        }));

    const mfd::UdpRuntimeBridgeMetrics metrics = bridge.MetricsSnapshot();
    EXPECT_GT(metrics.droppedBatches, 0U);
    EXPECT_LT(metrics.inboundQueueDepth, batchCount);
    EXPECT_NE(bridge.LastCommandStatus().find("queue budget exceeded"), std::string::npos);

    std::vector<mfd::CommandBatch> retainedBatches;
    bridge.DrainReceivedBatches(retainedBatches, batchCount);
    std::size_t retainedReticleCount = 0U;
    for (const mfd::CommandBatch& retainedBatch : retainedBatches)
    {
        ASSERT_EQ(retainedBatch.commands.size(), 1U);
        const auto& command = std::get<mfd::UpsertDynamicReticlesCommand>(retainedBatch.commands.front());
        retainedReticleCount += command.reticles.size();
    }
    EXPECT_LE(retainedReticleCount, 65536U);
    bridge.Stop();
}

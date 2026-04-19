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
} // namespace

TEST(UdpRuntimeBridgeTests, DrainsReceivedCommandsFromWorkerQueue)
{
    auto receiverState = std::make_shared<FakeChannelState>();
    auto senderState = std::make_shared<FakeChannelState>();

    mfd::CommandBatch batch;
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

    std::vector<mfd::UserCommand> drained;
    ASSERT_TRUE(WaitUntil(
        std::chrono::milliseconds(300),
        [&bridge, &drained]()
        {
            return bridge.DrainReceivedCommands(drained, 4) > 0;
        }));

    EXPECT_EQ(drained.size(), 1U);
    EXPECT_NE(std::get_if<mfd::ResetWindowCommand>(&drained.front()), nullptr);
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

TEST(UdpRuntimeBridgeTests, DoesNotReplaceReadyStatusWithStaleReceiverErrorAfterReceivingCommands)
{
    auto receiverState = std::make_shared<FakeChannelState>();
    auto senderState = std::make_shared<FakeChannelState>();

    mfd::CommandBatch batch;
    batch.commands.push_back(mfd::ResetWindowCommand {});
    receiverState->PushInbound(ToBytes(mfd::SerializeCommandBatch(batch)));
    receiverState->lastError = "stale receiver error";

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

    std::vector<mfd::UserCommand> drained;
    ASSERT_TRUE(WaitUntil(
        std::chrono::milliseconds(300),
        [&bridge, &drained]()
        {
            return bridge.DrainReceivedCommands(drained, 4) > 0;
        }));

    EXPECT_EQ(bridge.LastCommandStatus(), "UDP command receiver thread ready");
    bridge.Stop();
}

/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for UdpRuntimeBridge.
 */

#include "mfd/control/UdpRuntimeBridge.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>

#include "mfd/control/CommandTransport.h"
#include "mfd/control/CommandTypes.h"
#include "mfd/control/FeedbackTransport.h"
#include "mfd/control/StrobeFeedback.h"
#include "mfd/ipc/ExchangeChannel.h"

namespace mfd
{
namespace
{
constexpr std::size_t kMaxPacketsPerPump = 64;
constexpr std::size_t kMaxQueuedBatches = 8192;
constexpr std::size_t kMaxQueuedFeedback = 256;
constexpr auto kIdleWait = std::chrono::milliseconds(2);

std::string DescribeFeedbackTarget(const FeedbackPayload& feedback)
{
    return std::visit(
        [](const auto& value) -> std::string
        {
            using FeedbackType = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<FeedbackType, StrobeStatusFeedback>)
            {
                return "page '" + value.pageName + "' strobe";
            }
            else if constexpr (std::is_same_v<FeedbackType, ActivePageFeedback>)
            {
                return "active page '" + value.pageName + "'";
            }
        },
        feedback);
}

} // namespace

struct UdpRuntimeBridge::Impl
{
    WindowCommandTransportConfig commandConfig {};
    WindowFeedbackTransportConfig feedbackConfig {};
    ChannelFactory commandReceiverFactory {};
    ChannelFactory feedbackSenderFactory {};

    std::unique_ptr<IExchangeChannel> commandReceiver {};
    std::unique_ptr<IExchangeChannel> feedbackSender {};

    std::deque<CommandBatch> inboundBatches;
    std::deque<FeedbackPayload> outboundFeedback;

    mutable std::mutex stateMutex;
    mutable std::mutex inboundMutex;
    mutable std::mutex outboundMutex;
    std::condition_variable waitCondition;

    std::thread worker;
    bool stopRequested = false;
    bool running = false;
    bool hasCommandReceiver = false;
    bool hasFeedbackSender = false;
    bool commandReady = false;
    bool feedbackReady = false;

    std::string lastCommandStatus;
    std::string lastFeedbackStatus;

    void SetCommandStatus(std::string status)
    {
        std::lock_guard lock(stateMutex);
        lastCommandStatus = std::move(status);
    }

    void SetFeedbackStatus(std::string status)
    {
        std::lock_guard lock(stateMutex);
        lastFeedbackStatus = std::move(status);
    }

    bool ShouldStop() const
    {
        std::lock_guard lock(stateMutex);
        return stopRequested;
    }

    bool IsCommandReady() const
    {
        std::lock_guard lock(stateMutex);
        return commandReady;
    }

    bool IsFeedbackReady() const
    {
        std::lock_guard lock(stateMutex);
        return feedbackReady;
    }

    void PushQueuedBatch(CommandBatch&& batch)
    {
        if (batch.commands.empty())
        {
            return;
        }

        std::lock_guard lock(inboundMutex);

        const std::size_t overflow =
            inboundBatches.size() + 1U > kMaxQueuedBatches
                ? inboundBatches.size() + 1U - kMaxQueuedBatches
                : 0U;

        for (std::size_t index = 0; index < overflow && !inboundBatches.empty(); ++index)
        {
            inboundBatches.pop_front();
        }

        if (overflow > 0U)
        {
            SetCommandStatus("UDP command batch queue overflow, dropping oldest batches");
        }

        inboundBatches.push_back(std::move(batch));
    }

    bool FlushQueuedFeedback()
    {
        if (!IsFeedbackReady() || feedbackSender == nullptr)
        {
            return false;
        }

        std::deque<FeedbackPayload> localQueue;
        {
            std::lock_guard lock(outboundMutex);
            if (outboundFeedback.empty())
            {
                return false;
            }

            localQueue.swap(outboundFeedback);
        }

        bool sentAny = false;
        while (!localQueue.empty())
        {
            FeedbackPayload feedback = std::move(localQueue.front());
            localQueue.pop_front();

            try
            {
                const std::string payload = SerializeFeedbackPayload(feedback);
                const auto* payloadBytes = reinterpret_cast<const std::byte*>(payload.data());
                if (!feedbackSender->Send(ByteView(payloadBytes, payload.size())))
                {
                    SetFeedbackStatus(feedbackSender->LastError());
                    continue;
                }

                SetFeedbackStatus("UDP runtime feedback sent for " + DescribeFeedbackTarget(feedback));
                sentAny = true;
            }
            catch (const std::exception& exception)
            {
                SetFeedbackStatus(exception.what());
            }
            catch (...)
            {
                SetFeedbackStatus("Unknown exception while sending runtime feedback");
            }
        }

        return sentAny;
    }

    bool PumpQueuedCommands()
    {
        if (!IsCommandReady() || commandReceiver == nullptr)
        {
            return false;
        }

        bool receivedAny = false;
        for (std::size_t packetIndex = 0; packetIndex < kMaxPacketsPerPump; ++packetIndex)
        {
            try
            {
                const auto payload = commandReceiver->TryReceive();
                if (!payload.has_value())
                {
                    break;
                }

                receivedAny = true;

                const auto* raw = reinterpret_cast<const char*>(payload->data());
                std::string error;
                auto batch = DeserializeCommandBatch(std::string_view(raw, payload->size()), &error);
                if (!batch.has_value())
                {
                    SetCommandStatus(error);
                    continue;
                }

                PushQueuedBatch(std::move(*batch));
            }
            catch (const std::exception& exception)
            {
                SetCommandStatus(exception.what());
                break;
            }
            catch (...)
            {
                SetCommandStatus("Unknown exception while receiving UDP commands");
                break;
            }
        }

        if (commandReceiver != nullptr && !commandReceiver->LastError().empty())
        {
            SetCommandStatus(commandReceiver->LastError());
        }

        return receivedAny;
    }
};

UdpRuntimeBridge::UdpRuntimeBridge(WindowCommandTransportConfig commandConfig,
                                   WindowFeedbackTransportConfig feedbackConfig)
    : impl_(std::make_unique<Impl>())
{
    impl_->commandConfig = std::move(commandConfig);
    impl_->feedbackConfig = std::move(feedbackConfig);
    impl_->commandReceiverFactory = [config = impl_->commandConfig]()
    {
        return CreateCommandReceiverChannel(config);
    };
    impl_->feedbackSenderFactory = [config = impl_->feedbackConfig]()
    {
        return CreateFeedbackSenderChannel(config);
    };
}

UdpRuntimeBridge::UdpRuntimeBridge(ChannelFactory commandReceiverFactory, ChannelFactory feedbackSenderFactory)
    : impl_(std::make_unique<Impl>())
{
    impl_->commandReceiverFactory = std::move(commandReceiverFactory);
    impl_->feedbackSenderFactory = std::move(feedbackSenderFactory);
}

UdpRuntimeBridge::~UdpRuntimeBridge()
{
    Stop();
}

bool UdpRuntimeBridge::Start()
{
    if (impl_ == nullptr)
    {
        return false;
    }

    Stop();

    impl_->commandReceiver = impl_->commandReceiverFactory ? impl_->commandReceiverFactory() : nullptr;
    impl_->feedbackSender = impl_->feedbackSenderFactory ? impl_->feedbackSenderFactory() : nullptr;

    const bool hasCommandReceiver = impl_->commandReceiver != nullptr;
    const bool hasFeedbackSender = impl_->feedbackSender != nullptr;
    const bool commandReady = hasCommandReceiver && impl_->commandReceiver->IsReady();
    const bool feedbackReady = hasFeedbackSender && impl_->feedbackSender->IsReady();
    {
        std::lock_guard lock(impl_->stateMutex);
        impl_->hasCommandReceiver = hasCommandReceiver;
        impl_->hasFeedbackSender = hasFeedbackSender;
        impl_->commandReady = commandReady;
        impl_->feedbackReady = feedbackReady;
    }

    if (!hasCommandReceiver)
    {
        impl_->SetCommandStatus("UDP command bridge disabled in the window JSON");
    }
    else if (commandReady)
    {
        impl_->SetCommandStatus("UDP command receiver thread ready");
    }
    else
    {
        impl_->SetCommandStatus(impl_->commandReceiver->LastError());
    }

    if (!hasFeedbackSender)
    {
        impl_->SetFeedbackStatus("UDP runtime feedback sender disabled in the window JSON");
    }
    else if (feedbackReady)
    {
        impl_->SetFeedbackStatus("UDP runtime feedback sender thread ready");
    }
    else
    {
        impl_->SetFeedbackStatus(impl_->feedbackSender->LastError());
    }

    if (!commandReady && !feedbackReady)
    {
        return !hasCommandReceiver && !hasFeedbackSender;
    }

    {
        std::lock_guard lock(impl_->stateMutex);
        impl_->stopRequested = false;
        impl_->running = true;
    }
    impl_->worker = std::thread(
        [impl = impl_.get()]()
        {
            while (true)
            {
                {
                    if (impl->ShouldStop())
                    {
                        break;
                    }
                }

                const bool receivedCommands = impl->PumpQueuedCommands();
                const bool sentFeedback = impl->FlushQueuedFeedback();

                if (!receivedCommands && !sentFeedback)
                {
                    std::unique_lock lock(impl->stateMutex);
                    impl->waitCondition.wait_for(
                        lock,
                        kIdleWait,
                        [impl]()
                        {
                            return impl->stopRequested;
                        });
                }
            }

            impl->FlushQueuedFeedback();
            {
                std::lock_guard lock(impl->stateMutex);
                impl->running = false;
            }
        });

    return true;
}

void UdpRuntimeBridge::Stop() noexcept
{
    if (impl_ == nullptr)
    {
        return;
    }

    {
        std::lock_guard lock(impl_->stateMutex);
        impl_->stopRequested = true;
    }
    impl_->waitCondition.notify_all();

    if (impl_->worker.joinable())
    {
        impl_->worker.join();
    }

    {
        std::lock_guard lock(impl_->stateMutex);
        impl_->running = false;
        impl_->commandReady = false;
        impl_->feedbackReady = false;
        impl_->hasCommandReceiver = false;
        impl_->hasFeedbackSender = false;
    }
    impl_->commandReceiver.reset();
    impl_->feedbackSender.reset();

    {
        std::lock_guard lock(impl_->inboundMutex);
        impl_->inboundBatches.clear();
    }

    {
        std::lock_guard lock(impl_->outboundMutex);
        impl_->outboundFeedback.clear();
    }
}

bool UdpRuntimeBridge::IsRunning() const noexcept
{
    if (impl_ == nullptr)
    {
        return false;
    }

    std::lock_guard lock(impl_->stateMutex);
    return impl_->running;
}

bool UdpRuntimeBridge::HasCommandReceiver() const noexcept
{
    if (impl_ == nullptr)
    {
        return false;
    }

    std::lock_guard lock(impl_->stateMutex);
    return impl_->hasCommandReceiver;
}

bool UdpRuntimeBridge::HasFeedbackSender() const noexcept
{
    if (impl_ == nullptr)
    {
        return false;
    }

    std::lock_guard lock(impl_->stateMutex);
    return impl_->hasFeedbackSender;
}

bool UdpRuntimeBridge::CommandTransportReady() const noexcept
{
    if (impl_ == nullptr)
    {
        return false;
    }

    std::lock_guard lock(impl_->stateMutex);
    return impl_->commandReady;
}

bool UdpRuntimeBridge::FeedbackTransportReady() const noexcept
{
    if (impl_ == nullptr)
    {
        return false;
    }

    std::lock_guard lock(impl_->stateMutex);
    return impl_->feedbackReady;
}

std::size_t UdpRuntimeBridge::DrainReceivedBatches(std::vector<CommandBatch>& destination,
                                                   const std::size_t maxBatches)
{
    if (impl_ == nullptr || maxBatches == 0)
    {
        return 0;
    }

    std::lock_guard lock(impl_->inboundMutex);
    const std::size_t batchCount = std::min(maxBatches, impl_->inboundBatches.size());
    destination.reserve(destination.size() + batchCount);

    for (std::size_t index = 0; index < batchCount; ++index)
    {
        destination.push_back(std::move(impl_->inboundBatches.front()));
        impl_->inboundBatches.pop_front();
    }

    return batchCount;
}

std::size_t UdpRuntimeBridge::DrainReceivedCommands(std::vector<UserCommand>& destination,
                                                    const std::size_t maxCommands)
{
    if (impl_ == nullptr || maxCommands == 0)
    {
        return 0;
    }

    std::lock_guard lock(impl_->inboundMutex);
    std::size_t commandCount = 0;

    while (!impl_->inboundBatches.empty() && commandCount < maxCommands)
    {
        CommandBatch batch = std::move(impl_->inboundBatches.front());
        impl_->inboundBatches.pop_front();
        std::size_t consumedInBatch = 0;

        for (UserCommand& command : batch.commands)
        {
            if (commandCount >= maxCommands)
            {
                break;
            }

            destination.push_back(std::move(command));
            ++commandCount;
            ++consumedInBatch;
        }

        if (consumedInBatch < batch.commands.size())
        {
            batch.commands.erase(batch.commands.begin(),
                                 batch.commands.begin() + static_cast<std::ptrdiff_t>(consumedInBatch));
            impl_->inboundBatches.push_front(std::move(batch));
            break;
        }
    }

    return commandCount;
}

void UdpRuntimeBridge::EnqueueStrobeFeedback(StrobeStatusFeedback feedback)
{
    if (impl_ == nullptr || !HasFeedbackSender())
    {
        return;
    }

    {
        std::lock_guard lock(impl_->outboundMutex);
        if (impl_->outboundFeedback.size() >= kMaxQueuedFeedback)
        {
            impl_->outboundFeedback.pop_front();
            impl_->SetFeedbackStatus("UDP feedback queue overflow, dropping oldest runtime feedback");
        }

        impl_->outboundFeedback.emplace_back(std::move(feedback));
    }

    impl_->waitCondition.notify_all();
}

void UdpRuntimeBridge::EnqueueActivePageFeedback(ActivePageFeedback feedback)
{
    if (impl_ == nullptr || !HasFeedbackSender())
    {
        return;
    }

    {
        std::lock_guard lock(impl_->outboundMutex);
        if (impl_->outboundFeedback.size() >= kMaxQueuedFeedback)
        {
            impl_->outboundFeedback.pop_front();
            impl_->SetFeedbackStatus("UDP feedback queue overflow, dropping oldest runtime feedback");
        }

        impl_->outboundFeedback.emplace_back(std::move(feedback));
    }

    impl_->waitCondition.notify_all();
}

std::string UdpRuntimeBridge::LastCommandStatus() const
{
    if (impl_ == nullptr)
    {
        return {};
    }

    std::lock_guard lock(impl_->stateMutex);
    return impl_->lastCommandStatus;
}

std::string UdpRuntimeBridge::LastFeedbackStatus() const
{
    if (impl_ == nullptr)
    {
        return {};
    }

    std::lock_guard lock(impl_->stateMutex);
    return impl_->lastFeedbackStatus;
}
} // namespace mfd

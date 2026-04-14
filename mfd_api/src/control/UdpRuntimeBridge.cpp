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
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <mutex>
#include <thread>
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
constexpr std::size_t kMaxQueuedCommands = 8192;
constexpr std::size_t kMaxQueuedFeedback = 256;
constexpr auto kIdleWait = std::chrono::milliseconds(2);

} // namespace

struct UdpRuntimeBridge::Impl
{
    WindowCommandTransportConfig commandConfig {};
    WindowFeedbackTransportConfig feedbackConfig {};

    std::unique_ptr<IExchangeChannel> commandReceiver {};
    std::unique_ptr<IExchangeChannel> feedbackSender {};

    std::deque<UserCommand> inboundCommands;
    std::deque<StrobeStatusFeedback> outboundFeedback;

    mutable std::mutex inboundMutex;
    mutable std::mutex outboundMutex;
    mutable std::mutex statusMutex;
    mutable std::mutex waitMutex;
    std::condition_variable waitCondition;

    std::thread worker;
    std::atomic<bool> stopRequested {false};
    std::atomic<bool> running {false};
    std::atomic<bool> hasCommandReceiver {false};
    std::atomic<bool> hasFeedbackSender {false};
    std::atomic<bool> commandReady {false};
    std::atomic<bool> feedbackReady {false};

    std::string lastCommandStatus;
    std::string lastFeedbackStatus;

    void SetCommandStatus(std::string status)
    {
        std::lock_guard lock(statusMutex);
        lastCommandStatus = std::move(status);
    }

    void SetFeedbackStatus(std::string status)
    {
        std::lock_guard lock(statusMutex);
        lastFeedbackStatus = std::move(status);
    }
};

UdpRuntimeBridge::UdpRuntimeBridge(WindowCommandTransportConfig commandConfig,
                                   WindowFeedbackTransportConfig feedbackConfig)
    : impl_(std::make_unique<Impl>())
{
    impl_->commandConfig = std::move(commandConfig);
    impl_->feedbackConfig = std::move(feedbackConfig);
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

    impl_->commandReceiver = CreateCommandReceiverChannel(impl_->commandConfig);
    impl_->feedbackSender = CreateFeedbackSenderChannel(impl_->feedbackConfig);

    impl_->hasCommandReceiver.store(impl_->commandReceiver != nullptr);
    impl_->hasFeedbackSender.store(impl_->feedbackSender != nullptr);
    impl_->commandReady.store(impl_->commandReceiver != nullptr && impl_->commandReceiver->IsReady());
    impl_->feedbackReady.store(impl_->feedbackSender != nullptr && impl_->feedbackSender->IsReady());

    if (!impl_->hasCommandReceiver.load())
    {
        impl_->SetCommandStatus("UDP command bridge disabled in the window JSON");
    }
    else if (impl_->commandReady.load())
    {
        impl_->SetCommandStatus("UDP command receiver thread ready");
    }
    else
    {
        impl_->SetCommandStatus(impl_->commandReceiver->LastError());
    }

    if (!impl_->hasFeedbackSender.load())
    {
        impl_->SetFeedbackStatus("UDP strobe feedback sender disabled in the window JSON");
    }
    else if (impl_->feedbackReady.load())
    {
        impl_->SetFeedbackStatus("UDP strobe feedback sender thread ready");
    }
    else
    {
        impl_->SetFeedbackStatus(impl_->feedbackSender->LastError());
    }

    if (!impl_->commandReady.load() && !impl_->feedbackReady.load())
    {
        return !impl_->hasCommandReceiver.load() && !impl_->hasFeedbackSender.load();
    }

    impl_->stopRequested.store(false);
    impl_->running.store(true);
    impl_->worker = std::thread(
        [impl = impl_.get()]()
        {
            auto pushCommands = [impl](std::vector<UserCommand>&& commands)
            {
                if (commands.empty())
                {
                    return;
                }

                std::lock_guard lock(impl->inboundMutex);

                const std::size_t overflow =
                    impl->inboundCommands.size() + commands.size() > kMaxQueuedCommands
                        ? impl->inboundCommands.size() + commands.size() - kMaxQueuedCommands
                        : 0;

                for (std::size_t index = 0; index < overflow && !impl->inboundCommands.empty(); ++index)
                {
                    impl->inboundCommands.pop_front();
                }

                if (overflow > 0)
                {
                    impl->SetCommandStatus("UDP command queue overflow, dropping oldest commands");
                }

                for (auto& command : commands)
                {
                    impl->inboundCommands.push_back(std::move(command));
                }
            };

            auto flushFeedback = [impl]() -> bool
            {
                if (!impl->feedbackReady.load() || impl->feedbackSender == nullptr)
                {
                    return false;
                }

                std::deque<StrobeStatusFeedback> localQueue;
                {
                    std::lock_guard lock(impl->outboundMutex);
                    if (impl->outboundFeedback.empty())
                    {
                        return false;
                    }

                    localQueue.swap(impl->outboundFeedback);
                }

                bool sentAny = false;
                while (!localQueue.empty())
                {
                    StrobeStatusFeedback feedback = std::move(localQueue.front());
                    localQueue.pop_front();

                    try
                    {
                        const std::string payload = SerializeStrobeStatusFeedback(feedback);
                        const auto* payloadBytes = reinterpret_cast<const std::byte*>(payload.data());
                        if (!impl->feedbackSender->Send(std::span<const std::byte>(payloadBytes, payload.size())))
                        {
                            impl->SetFeedbackStatus(impl->feedbackSender->LastError());
                            continue;
                        }

                        impl->SetFeedbackStatus(
                            "UDP strobe feedback sent for page '" + feedback.pageName + "'");
                        sentAny = true;
                    }
                    catch (const std::exception& exception)
                    {
                        impl->SetFeedbackStatus(exception.what());
                    }
                    catch (...)
                    {
                        impl->SetFeedbackStatus("Unknown exception while sending strobe feedback");
                    }
                }

                return sentAny;
            };

            auto pumpCommands = [impl, &pushCommands]() -> bool
            {
                if (!impl->commandReady.load() || impl->commandReceiver == nullptr)
                {
                    return false;
                }

                bool receivedAny = false;
                for (std::size_t packetIndex = 0; packetIndex < kMaxPacketsPerPump; ++packetIndex)
                {
                    try
                    {
                        const auto payload = impl->commandReceiver->TryReceive();
                        if (!payload.has_value())
                        {
                            break;
                        }

                        receivedAny = true;

                        const auto* raw = reinterpret_cast<const char*>(payload->data());
                        std::string error;
                        auto commands =
                            DeserializeUserCommands(std::string_view(raw, payload->size()), &error);
                        if (!commands.has_value())
                        {
                            impl->SetCommandStatus(error);
                            continue;
                        }

                        pushCommands(std::move(*commands));
                    }
                    catch (const std::exception& exception)
                    {
                        impl->SetCommandStatus(exception.what());
                        break;
                    }
                    catch (...)
                    {
                        impl->SetCommandStatus("Unknown exception while receiving UDP commands");
                        break;
                    }
                }

                if (impl->commandReceiver != nullptr && !impl->commandReceiver->LastError().empty())
                {
                    impl->SetCommandStatus(impl->commandReceiver->LastError());
                }

                return receivedAny;
            };

            while (true)
            {
                {
                    std::lock_guard lock(impl->waitMutex);
                    if (impl->stopRequested.load())
                    {
                        break;
                    }
                }

                const bool receivedCommands = pumpCommands();
                const bool sentFeedback = flushFeedback();

                if (!receivedCommands && !sentFeedback)
                {
                    std::unique_lock lock(impl->waitMutex);
                    impl->waitCondition.wait_for(
                        lock,
                        kIdleWait,
                        [impl]()
                        {
                            return impl->stopRequested.load();
                        });
                }
            }

            (void)flushFeedback();
            impl->running.store(false);
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
        std::lock_guard lock(impl_->waitMutex);
        impl_->stopRequested.store(true);
    }
    impl_->waitCondition.notify_all();

    if (impl_->worker.joinable())
    {
        impl_->worker.join();
    }

    impl_->running.store(false);
    impl_->commandReady.store(false);
    impl_->feedbackReady.store(false);
    impl_->hasCommandReceiver.store(false);
    impl_->hasFeedbackSender.store(false);
    impl_->commandReceiver.reset();
    impl_->feedbackSender.reset();

    {
        std::lock_guard lock(impl_->inboundMutex);
        impl_->inboundCommands.clear();
    }

    {
        std::lock_guard lock(impl_->outboundMutex);
        impl_->outboundFeedback.clear();
    }
}

bool UdpRuntimeBridge::IsRunning() const noexcept
{
    return impl_ != nullptr && impl_->running.load();
}

bool UdpRuntimeBridge::HasCommandReceiver() const noexcept
{
    return impl_ != nullptr && impl_->hasCommandReceiver.load();
}

bool UdpRuntimeBridge::HasFeedbackSender() const noexcept
{
    return impl_ != nullptr && impl_->hasFeedbackSender.load();
}

bool UdpRuntimeBridge::CommandTransportReady() const noexcept
{
    return impl_ != nullptr && impl_->commandReady.load();
}

bool UdpRuntimeBridge::FeedbackTransportReady() const noexcept
{
    return impl_ != nullptr && impl_->feedbackReady.load();
}

std::size_t UdpRuntimeBridge::DrainReceivedCommands(std::vector<UserCommand>& destination,
                                                    const std::size_t maxCommands)
{
    if (impl_ == nullptr || maxCommands == 0)
    {
        return 0;
    }

    std::lock_guard lock(impl_->inboundMutex);
    const std::size_t commandCount = std::min(maxCommands, impl_->inboundCommands.size());
    destination.reserve(destination.size() + commandCount);

    for (std::size_t index = 0; index < commandCount; ++index)
    {
        destination.push_back(std::move(impl_->inboundCommands.front()));
        impl_->inboundCommands.pop_front();
    }

    return commandCount;
}

void UdpRuntimeBridge::EnqueueStrobeFeedback(StrobeStatusFeedback feedback)
{
    if (impl_ == nullptr || !impl_->hasFeedbackSender.load())
    {
        return;
    }

    {
        std::lock_guard lock(impl_->outboundMutex);
        if (impl_->outboundFeedback.size() >= kMaxQueuedFeedback)
        {
            impl_->outboundFeedback.pop_front();
            impl_->SetFeedbackStatus("UDP feedback queue overflow, dropping oldest strobe feedback");
        }

        impl_->outboundFeedback.push_back(std::move(feedback));
    }

    impl_->waitCondition.notify_all();
}

std::string UdpRuntimeBridge::LastCommandStatus() const
{
    if (impl_ == nullptr)
    {
        return {};
    }

    std::lock_guard lock(impl_->statusMutex);
    return impl_->lastCommandStatus;
}

std::string UdpRuntimeBridge::LastFeedbackStatus() const
{
    if (impl_ == nullptr)
    {
        return {};
    }

    std::lock_guard lock(impl_->statusMutex);
    return impl_->lastFeedbackStatus;
}
} // namespace mfd

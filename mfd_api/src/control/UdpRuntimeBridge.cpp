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
#include <utility>

#include "mfd/control/CommandTransport.h"
#include "mfd/control/CommandTypes.h"
#include "mfd/control/FeedbackTransport.h"
#include "mfd/control/StrobeFeedback.h"
#include "mfd/ipc/ExchangeChannel.h"
#include "mfd/plugin/MfdPluginLoader.h"

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
    ChannelFactory commandReceiverFactory {};
    ChannelFactory feedbackSenderFactory {};

    std::unique_ptr<IExchangeChannel> commandReceiver {};
    std::unique_ptr<IExchangeChannel> feedbackSender {};
    std::unique_ptr<MfdPluginLoader> shmPluginLoader {};

    std::deque<UserCommand> inboundCommands;
    std::deque<StrobeStatusFeedback> outboundFeedback;

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

    if (impl_->commandConfig.shm.has_value() && !impl_->commandConfig.shm->pluginPath.empty())
    {
        impl_->shmPluginLoader = std::make_unique<MfdPluginLoader>();
        if (!impl_->shmPluginLoader->Load(impl_->commandConfig.shm->pluginPath,
                                          impl_->commandConfig.shm->factorySymbol.empty()
                                              ? std::string{"CreateMfdShmAdapterPlugin"}
                                              : impl_->commandConfig.shm->factorySymbol))
        {
            impl_->SetCommandStatus(impl_->shmPluginLoader->LastError());
        }
        else if (impl_->shmPluginLoader->Plugin() != nullptr)
        {
            if (!impl_->shmPluginLoader->Plugin()->Initialize(*impl_->commandConfig.shm))
            {
                impl_->SetCommandStatus(impl_->shmPluginLoader->Plugin()->LastError());
            }
            else
            {
                impl_->SetCommandStatus("SHM plugin adapter initialized");
            }
        }
    }

    const bool hasCommandReceiver = impl_->commandReceiver != nullptr || impl_->shmPluginLoader != nullptr;
    const bool hasFeedbackSender = impl_->feedbackSender != nullptr;
    const bool commandReady = (impl_->commandReceiver != nullptr && impl_->commandReceiver->IsReady()) ||
                              (impl_->shmPluginLoader != nullptr && impl_->shmPluginLoader->Plugin() != nullptr);
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
        if (impl_->commandReceiver != nullptr)
        {
            impl_->SetCommandStatus(impl_->commandReceiver->LastError());
        }
        else if (impl_->shmPluginLoader != nullptr)
        {
            impl_->SetCommandStatus(impl_->shmPluginLoader->LastError());
        }
    }

    if (!hasFeedbackSender)
    {
        impl_->SetFeedbackStatus("UDP strobe feedback sender disabled in the window JSON");
    }
    else if (feedbackReady)
    {
        impl_->SetFeedbackStatus("UDP strobe feedback sender thread ready");
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
                if (!impl->IsFeedbackReady() || impl->feedbackSender == nullptr)
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
                        if (!impl->feedbackSender->Send(ByteView(payloadBytes, payload.size())))
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
                if (!impl->IsCommandReady())
                {
                    return false;
                }

                bool receivedAny = false;

                if (impl->shmPluginLoader != nullptr && impl->shmPluginLoader->Plugin() != nullptr)
                {
                    std::vector<UserCommand> pluginCommands;
                    if (!impl->shmPluginLoader->Plugin()->Poll(pluginCommands))
                    {
                        impl->SetCommandStatus(impl->shmPluginLoader->Plugin()->LastError());
                    }
                    else if (!pluginCommands.empty())
                    {
                        pushCommands(std::move(pluginCommands));
                        receivedAny = true;
                    }
                }

                if (impl->commandReceiver != nullptr)
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
                    if (impl->ShouldStop())
                    {
                        break;
                    }
                }

                const bool receivedCommands = pumpCommands();
                const bool sentFeedback = flushFeedback();

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

            (void)flushFeedback();
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
        impl_->inboundCommands.clear();
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
    if (impl_ == nullptr || !HasFeedbackSender())
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

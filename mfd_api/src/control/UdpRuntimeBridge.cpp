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

/**
 * @brief Bounded FIFO queue dropping oldest entries when capacity is reached.
 */
template<typename T>
class BoundedQueue
{
public:
    /**
     * @brief Creates a bounded queue with a fixed maximum element count.
     * @param capacity Maximum number of retained elements.
     */
    explicit BoundedQueue(const std::size_t capacity)
        : capacity_(capacity)
    {
    }

    /**
     * @brief Pushes one element, dropping oldest entries on overflow.
     * @param value Element to enqueue.
     * @return Number of dropped oldest elements.
     */
    std::size_t Push(T value)
    {
        std::lock_guard lock(mutex_);
        std::size_t dropped = 0;
        while (capacity_ > 0 && queue_.size() >= capacity_)
        {
            queue_.pop_front();
            ++dropped;
        }

        queue_.push_back(std::move(value));
        return dropped;
    }

    /**
     * @brief Pops up to `count` elements into a destination vector.
     * @param destination Vector receiving popped elements.
     * @param count Maximum number of elements to pop.
     * @return Number of popped elements.
     */
    std::size_t PopMany(std::vector<T>& destination, const std::size_t count)
    {
        std::lock_guard lock(mutex_);
        const std::size_t poppedCount = std::min(count, queue_.size());
        destination.reserve(destination.size() + poppedCount);
        for (std::size_t index = 0; index < poppedCount; ++index)
        {
            destination.push_back(std::move(queue_.front()));
            queue_.pop_front();
        }

        return poppedCount;
    }

    /**
     * @brief Swaps all currently queued entries with a caller-provided deque.
     * @param destination Deque receiving all queued entries.
     * @return `true` when at least one element was queued.
     */
    bool DrainTo(std::deque<T>& destination)
    {
        std::lock_guard lock(mutex_);
        if (queue_.empty())
        {
            return false;
        }

        destination.swap(queue_);
        return true;
    }

    /**
     * @brief Clears every queued entry.
     */
    void Clear()
    {
        std::lock_guard lock(mutex_);
        queue_.clear();
    }

private:
    std::size_t capacity_ = 0;
    mutable std::mutex mutex_ {};
    std::deque<T> queue_ {};
};

/**
 * @brief Small worker-loop helper managing one thread start/stop lifecycle.
 */
class WorkerLoop
{
public:
    template<typename Fn>
    void Start(Fn&& fn)
    {
        Stop();
        stopRequested_ = false;
        worker_ = std::thread([this, fn = std::forward<Fn>(fn)]() mutable { fn(*this); });
    }

    void Stop()
    {
        {
            std::lock_guard lock(waitMutex_);
            stopRequested_ = true;
        }
        waitCondition_.notify_all();
        if (worker_.joinable())
        {
            worker_.join();
        }
    }

    bool StopRequested() const
    {
        std::lock_guard lock(waitMutex_);
        return stopRequested_;
    }

    void Notify()
    {
        waitCondition_.notify_all();
    }

    void WaitForIdle()
    {
        std::unique_lock lock(waitMutex_);
        waitCondition_.wait_for(lock, kIdleWait, [this]() { return stopRequested_; });
    }

private:
    mutable std::mutex waitMutex_ {};
    std::condition_variable waitCondition_ {};
    std::thread worker_ {};
    bool stopRequested_ = false;
};

} // namespace

struct UdpRuntimeBridge::Impl
{
    WindowCommandTransportConfig commandConfig {};
    WindowFeedbackTransportConfig feedbackConfig {};

    std::unique_ptr<IExchangeChannel> commandReceiver {};
    std::unique_ptr<IExchangeChannel> feedbackSender {};

    BoundedQueue<UserCommand> inboundCommands {kMaxQueuedCommands};
    BoundedQueue<StrobeStatusFeedback> outboundFeedback {kMaxQueuedFeedback};
    mutable std::mutex statusMutex;

    WorkerLoop worker;
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

    impl_->running.store(true);
    impl_->worker.Start(
        [impl = impl_.get()](WorkerLoop& loop)
        {
            auto pushCommands = [impl](std::vector<UserCommand>&& commands)
            {
                if (commands.empty())
                {
                    return;
                }

                for (auto& command : commands)
                {
                    const std::size_t dropped = impl->inboundCommands.Push(std::move(command));
                    if (dropped > 0)
                    {
                        impl->SetCommandStatus("UDP command queue overflow, dropping oldest commands");
                    }
                }
            };

            auto flushFeedback = [impl]() -> bool
            {
                if (!impl->feedbackReady.load() || impl->feedbackSender == nullptr)
                {
                    return false;
                }

                std::deque<StrobeStatusFeedback> localQueue;
                if (!impl->outboundFeedback.DrainTo(localQueue))
                {
                    return false;
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
                if (loop.StopRequested())
                {
                    break;
                }

                const bool receivedCommands = pumpCommands();
                const bool sentFeedback = flushFeedback();

                if (!receivedCommands && !sentFeedback)
                {
                    loop.WaitForIdle();
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

    impl_->worker.Stop();

    impl_->running.store(false);
    impl_->commandReady.store(false);
    impl_->feedbackReady.store(false);
    impl_->hasCommandReceiver.store(false);
    impl_->hasFeedbackSender.store(false);
    impl_->commandReceiver.reset();
    impl_->feedbackSender.reset();

    impl_->inboundCommands.Clear();
    impl_->outboundFeedback.Clear();
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

    const std::size_t previousSize = destination.size();
    (void)impl_->inboundCommands.PopMany(destination, maxCommands);
    return destination.size() - previousSize;
}

void UdpRuntimeBridge::EnqueueStrobeFeedback(StrobeStatusFeedback feedback)
{
    if (impl_ == nullptr || !impl_->hasFeedbackSender.load())
    {
        return;
    }

    const std::size_t dropped = impl_->outboundFeedback.Push(std::move(feedback));
    if (dropped > 0)
    {
        impl_->SetFeedbackStatus("UDP feedback queue overflow, dropping oldest strobe feedback");
    }

    impl_->worker.Notify();
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

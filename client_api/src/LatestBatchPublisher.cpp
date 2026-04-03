/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "mfd/client/LatestBatchPublisher.h"

#include <condition_variable>
#include <exception>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>

#include "mfd/control/CommandClient.h"

namespace mfd::client
{
struct LatestBatchPublisher::Impl
{
    std::unique_ptr<mfd::CommandClient> ownedClient {};
    SendFunction sendFunction {};
    ErrorFunction errorFunction {};
    std::thread worker {};
    mutable std::mutex mutex {};
    std::condition_variable wakeCondition {};
    std::condition_variable idleCondition {};
    std::optional<mfd::CommandBatch> pendingBatch {};
    std::string lastError {};
    bool ready = false;
    bool stopRequested = false;
    bool sending = false;
};

namespace
{
void StartWorker(auto& impl)
{
    if (!impl.ready || !impl.sendFunction)
    {
        return;
    }

    impl.worker = std::thread(
        [&impl]()
        {
            for (;;)
            {
                mfd::CommandBatch batch;

                {
                    std::unique_lock lock(impl.mutex);
                    impl.wakeCondition.wait(
                        lock,
                        [&impl]()
                        {
                            return impl.stopRequested || impl.pendingBatch.has_value();
                        });

                    if (impl.stopRequested && !impl.pendingBatch.has_value())
                    {
                        break;
                    }

                    batch = std::move(*impl.pendingBatch);
                    impl.pendingBatch.reset();
                    impl.sending = true;
                }

                bool success = false;
                std::string error;

                try
                {
                    success = impl.sendFunction(batch);
                    if (!success)
                    {
                        error = impl.errorFunction ? impl.errorFunction() : "Unable to send the latest command batch";
                        if (error.empty())
                        {
                            error = "Unable to send the latest command batch";
                        }
                    }
                }
                catch (const std::exception& exception)
                {
                    error = exception.what();
                }
                catch (...)
                {
                    error = "Unknown exception while sending the latest command batch";
                }

                {
                    std::lock_guard lock(impl.mutex);
                    impl.sending = false;
                    impl.lastError = success ? std::string {} : std::move(error);
                }

                impl.idleCondition.notify_all();
            }

            impl.idleCondition.notify_all();
        });
}

std::unique_ptr<mfd::CommandClient> MakeOwnedClient(const mfd::WindowCommandTransportConfig& config)
{
    return std::make_unique<mfd::CommandClient>(config);
}

std::unique_ptr<mfd::CommandClient> MakeOwnedClient(const mfd::WindowUdpCommandTransport& config)
{
    return std::make_unique<mfd::CommandClient>(config);
}
} // namespace

LatestBatchPublisher::LatestBatchPublisher(const mfd::WindowCommandTransportConfig& config)
    : impl_(std::make_unique<Impl>())
{
    impl_->ownedClient = MakeOwnedClient(config);
    impl_->ready = impl_->ownedClient != nullptr && impl_->ownedClient->IsReady();
    impl_->lastError =
        impl_->ready ? std::string {} :
                       (impl_->ownedClient == nullptr ? "Unable to create the realtime command client"
                                                      : impl_->ownedClient->LastError());

    if (impl_->ready)
    {
        mfd::CommandClient* client = impl_->ownedClient.get();
        impl_->sendFunction = [client](const mfd::CommandBatch& batch)
        {
            return client->SendBatch(batch);
        };
        impl_->errorFunction = [client]()
        {
            return client->LastError();
        };
    }

    StartWorker(*impl_);
}

LatestBatchPublisher::LatestBatchPublisher(const mfd::WindowUdpCommandTransport& config)
    : impl_(std::make_unique<Impl>())
{
    impl_->ownedClient = MakeOwnedClient(config);
    impl_->ready = impl_->ownedClient != nullptr && impl_->ownedClient->IsReady();
    impl_->lastError =
        impl_->ready ? std::string {} :
                       (impl_->ownedClient == nullptr ? "Unable to create the realtime UDP command client"
                                                      : impl_->ownedClient->LastError());

    if (impl_->ready)
    {
        mfd::CommandClient* client = impl_->ownedClient.get();
        impl_->sendFunction = [client](const mfd::CommandBatch& batch)
        {
            return client->SendBatch(batch);
        };
        impl_->errorFunction = [client]()
        {
            return client->LastError();
        };
    }

    StartWorker(*impl_);
}

LatestBatchPublisher::LatestBatchPublisher(SendFunction sendFunction, ErrorFunction errorFunction)
    : impl_(std::make_unique<Impl>())
{
    impl_->sendFunction = std::move(sendFunction);
    impl_->errorFunction = std::move(errorFunction);
    impl_->ready = static_cast<bool>(impl_->sendFunction);
    if (!impl_->ready)
    {
        impl_->lastError = "Latest batch publisher requires a valid send callback";
    }

    StartWorker(*impl_);
}

LatestBatchPublisher::~LatestBatchPublisher()
{
    Stop();
}

bool LatestBatchPublisher::IsReady() const noexcept
{
    return impl_ != nullptr && impl_->ready;
}

bool LatestBatchPublisher::SubmitLatest(mfd::CommandBatch batch)
{
    if (!IsReady())
    {
        if (impl_ != nullptr)
        {
            std::lock_guard lock(impl_->mutex);
            if (impl_->lastError.empty())
            {
                impl_->lastError = "Latest batch publisher is not ready";
            }
        }
        return false;
    }

    {
        std::lock_guard lock(impl_->mutex);
        if (impl_->stopRequested)
        {
            impl_->lastError = "Latest batch publisher has been stopped";
            return false;
        }

        impl_->pendingBatch = std::move(batch);
    }

    impl_->wakeCondition.notify_one();
    return true;
}

bool LatestBatchPublisher::SubmitLatest(std::vector<mfd::UserCommand> commands, const std::uint32_t sequence)
{
    mfd::CommandBatch batch;
    batch.sequence = sequence;
    batch.commands = std::move(commands);
    return SubmitLatest(std::move(batch));
}

void LatestBatchPublisher::Flush()
{
    if (impl_ == nullptr || !impl_->worker.joinable())
    {
        return;
    }

    std::unique_lock lock(impl_->mutex);
    impl_->idleCondition.wait(
        lock,
        [this]()
        {
            return !impl_->pendingBatch.has_value() && !impl_->sending;
        });
}

void LatestBatchPublisher::Stop()
{
    if (impl_ == nullptr)
    {
        return;
    }

    {
        std::lock_guard lock(impl_->mutex);
        impl_->stopRequested = true;
        impl_->pendingBatch.reset();
    }

    impl_->wakeCondition.notify_all();
    impl_->idleCondition.notify_all();

    if (impl_->worker.joinable())
    {
        impl_->worker.join();
    }
}

std::string LatestBatchPublisher::LastError() const
{
    if (impl_ == nullptr)
    {
        return {};
    }

    std::lock_guard lock(impl_->mutex);
    return impl_->lastError;
}
} // namespace mfd::client

/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for LatestBatchPublisher.
 */

#include "mfd/client/LatestBatchPublisher.h"

#include <condition_variable>
#include <exception>
#include <cstddef>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

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
using DynamicOperationMap = std::unordered_map<std::string, std::size_t>;

std::string MakeDynamicReticleKey(const std::string& page, const std::string& reticle)
{
    return page + '\x1F' + reticle;
}

std::string MakeDynamicTemplateKey(const std::string& page, const std::string& templateId)
{
    return page + '\x1E' + templateId;
}

void PutDynamicLifecycleCommand(std::vector<mfd::UserCommand>& operations,
                                DynamicOperationMap& operationIndexes,
                                mfd::UserCommand command)
{
    auto key = std::visit(
        [](const auto& value) -> std::string
        {
            using Command = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Command, mfd::UpsertDynamicReticleCommand>)
            {
                return MakeDynamicReticleKey(value.target.page, value.target.reticle);
            }
            else if constexpr (std::is_same_v<Command, mfd::RemoveDynamicReticleCommand>)
            {
                return MakeDynamicReticleKey(value.target.page, value.target.reticle);
            }
            else if constexpr (std::is_same_v<Command, mfd::SetDynamicReticleSetVisibilityCommand>)
            {
                return MakeDynamicTemplateKey(value.page, value.templateId);
            }
            else
            {
                return {};
            }
        },
        command);

    if (key.empty())
    {
        return;
    }

    const auto indexIt = operationIndexes.find(key);
    if (indexIt == operationIndexes.end())
    {
        operationIndexes.emplace(std::move(key), operations.size());
        operations.push_back(std::move(command));
        return;
    }

    operations[indexIt->second] = std::move(command);
}

void CollectDynamicLifecycleCommands(const std::vector<mfd::UserCommand>& source,
                                     std::vector<mfd::UserCommand>& operations,
                                     DynamicOperationMap& operationIndexes)
{
    for (const mfd::UserCommand& command : source)
    {
        std::visit(
            [&operations, &operationIndexes](const auto& value)
            {
                using Command = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Command, mfd::UpsertDynamicReticleCommand>)
                {
                    PutDynamicLifecycleCommand(operations, operationIndexes, value);
                }
                else if constexpr (std::is_same_v<Command, mfd::RemoveDynamicReticleCommand>)
                {
                    PutDynamicLifecycleCommand(operations, operationIndexes, value);
                }
                else if constexpr (std::is_same_v<Command, mfd::UpsertDynamicReticlesCommand>)
                {
                    for (const mfd::DynamicReticleState& state : value.reticles)
                    {
                        PutDynamicLifecycleCommand(
                            operations,
                            operationIndexes,
                            mfd::UpsertDynamicReticleCommand {
                                mfd::ReticleHandle {value.page, state.reticleId},
                                value.templateId,
                                state.patch});
                    }
                }
                else if constexpr (std::is_same_v<Command, mfd::SetDynamicReticleSetVisibilityCommand>)
                {
                    PutDynamicLifecycleCommand(operations, operationIndexes, value);
                }
            },
            command);
    }
}

void MergePendingBatchKeepingDynamicReticleLifecycle(std::optional<mfd::CommandBatch>& pendingBatch,
                                                     mfd::CommandBatch&& newestBatch)
{
    if (!pendingBatch.has_value())
    {
        pendingBatch = std::move(newestBatch);
        return;
    }

    std::vector<mfd::UserCommand> pendingDynamicOperations;
    DynamicOperationMap pendingIndexes;
    CollectDynamicLifecycleCommands(pendingBatch->commands, pendingDynamicOperations, pendingIndexes);

    std::vector<mfd::UserCommand> newestDynamicOperations;
    DynamicOperationMap newestIndexes;
    CollectDynamicLifecycleCommands(newestBatch.commands, newestDynamicOperations, newestIndexes);

    std::vector<mfd::UserCommand> carriedOperations;
    carriedOperations.reserve(pendingDynamicOperations.size());

    for (const mfd::UserCommand& command : pendingDynamicOperations)
    {
        const auto key = std::visit(
            [](const auto& value) -> std::string
            {
                using Command = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Command, mfd::UpsertDynamicReticleCommand>)
                {
                    return MakeDynamicReticleKey(value.target.page, value.target.reticle);
                }
                else if constexpr (std::is_same_v<Command, mfd::RemoveDynamicReticleCommand>)
                {
                    return MakeDynamicReticleKey(value.target.page, value.target.reticle);
                }
                else if constexpr (std::is_same_v<Command, mfd::SetDynamicReticleSetVisibilityCommand>)
                {
                    return MakeDynamicTemplateKey(value.page, value.templateId);
                }
                else
                {
                    return {};
                }
            },
            command);

        if (!key.empty() && newestIndexes.find(key) == newestIndexes.end())
        {
            carriedOperations.push_back(command);
        }
    }

    if (carriedOperations.empty())
    {
        pendingBatch = std::move(newestBatch);
        return;
    }

    mfd::CommandBatch mergedBatch = std::move(newestBatch);
    mergedBatch.commands.insert(mergedBatch.commands.begin(), carriedOperations.begin(), carriedOperations.end());
    pendingBatch = std::move(mergedBatch);
}

template<typename ImplType>
void StartWorker(ImplType& impl)
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
                    std::unique_lock<std::mutex> lock(impl.mutex);
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
                    std::lock_guard<std::mutex> lock(impl.mutex);
                    impl.sending = false;
                    impl.lastError = success ? std::string {} : std::move(error);
                }

                impl.idleCondition.notify_all();
            }

            impl.idleCondition.notify_all();
        });
}

} // namespace

std::unique_ptr<mfd::CommandClient> MakeOwnedClient(const mfd::WindowCommandTransportConfig& config)
{
    return std::make_unique<mfd::CommandClient>(config);
}

std::unique_ptr<mfd::CommandClient> MakeOwnedClient(const mfd::WindowUdpCommandTransport& config)
{
    return std::make_unique<mfd::CommandClient>(config);
}

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
            std::lock_guard<std::mutex> lock(impl_->mutex);
            if (impl_->lastError.empty())
            {
                impl_->lastError = "Latest batch publisher is not ready";
            }
        }
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (impl_->stopRequested)
        {
            impl_->lastError = "Latest batch publisher has been stopped";
            return false;
        }

        MergePendingBatchKeepingDynamicReticleLifecycle(impl_->pendingBatch, std::move(batch));
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

    std::unique_lock<std::mutex> lock(impl_->mutex);
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
        std::lock_guard<std::mutex> lock(impl_->mutex);
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

    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->lastError;
}
} // namespace mfd::client

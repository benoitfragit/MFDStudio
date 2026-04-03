/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Generic realtime publisher keeping only the latest pending command batch.
 */

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "mfd/client/ClientExport.h"
#include "mfd/control/CommandTransport.h"
#include "mfd/control/CommandTypes.h"

namespace mfd::client
{
/**
 * @brief Asynchronously publishes realtime command batches while coalescing stale pending states.
 *
 * @note This helper is intended for state-streaming workloads such as radar or
 * cockpit mockups where only the latest state matters. Do not use it for
 * one-shot actions that must never be dropped.
 */
class MFD_CLIENT_API LatestBatchPublisher
{
public:
    /** @brief Callback used by tests or custom integrations to send one batch. */
    using SendFunction = std::function<bool(const mfd::CommandBatch&)>;
    /** @brief Callback returning the last transport-specific error string. */
    using ErrorFunction = std::function<std::string()>;

    /**
     * @brief Creates a publisher owning its own command client from a window transport configuration.
     * @param config Command transport configuration copied from the target window JSON.
     */
    explicit LatestBatchPublisher(const mfd::WindowCommandTransportConfig& config);

    /**
     * @brief Creates a publisher owning its own UDP command client.
     * @param config UDP transport configuration copied from the target window JSON.
     */
    explicit LatestBatchPublisher(const mfd::WindowUdpCommandTransport& config);

    /**
     * @brief Creates a publisher from custom send and error callbacks.
     * @param sendFunction Callback sending one batch.
     * @param errorFunction Optional callback exposing the last send error.
     */
    LatestBatchPublisher(SendFunction sendFunction, ErrorFunction errorFunction = {});

    ~LatestBatchPublisher();

    LatestBatchPublisher(const LatestBatchPublisher&) = delete;
    LatestBatchPublisher& operator=(const LatestBatchPublisher&) = delete;
    LatestBatchPublisher(LatestBatchPublisher&&) = delete;
    LatestBatchPublisher& operator=(LatestBatchPublisher&&) = delete;

    /**
     * @brief Indicates whether the publisher is ready to accept realtime batches.
     * @return `true` when the worker thread and transport are ready.
     */
    bool IsReady() const noexcept;

    /**
     * @brief Queues the latest realtime batch and replaces any older pending unsent batch.
     * @param batch Batch representing the newest known external state.
     * @return `true` when the batch was accepted by the publisher.
     */
    bool SubmitLatest(mfd::CommandBatch batch);

    /**
     * @brief Convenience overload building the command batch in place.
     * @param commands Commands representing the newest known external state.
     * @param sequence Optional caller-managed sequence identifier.
     * @return `true` when the batch was accepted by the publisher.
     */
    bool SubmitLatest(std::vector<mfd::UserCommand> commands, std::uint32_t sequence = 0);

    /**
     * @brief Blocks until the current send finishes and no pending batch remains.
     */
    void Flush();

    /**
     * @brief Stops the worker thread and drops any unsent pending batch.
     */
    void Stop();

    /**
     * @brief Returns the last initialization or send error observed by the publisher.
     * @return Human-readable error string, or an empty string when healthy.
     */
    std::string LastError() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_ {};
};
} // namespace mfd::client

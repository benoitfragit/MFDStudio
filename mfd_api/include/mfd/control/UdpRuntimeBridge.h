/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Background UDP I/O bridge decoupling command reception from rendering.
 */

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "mfd/MfdExport.h"
#include "mfd/control/CommandTransport.h"
#include "mfd/control/CommandTypes.h"
#include "mfd/control/FeedbackTransport.h"
#include "mfd/control/StrobeFeedback.h"
#include "mfd/control/WindowFeedback.h"

namespace mfd
{
/**
 * @brief Thread-safe value snapshot of UDP bridge runtime pressure counters.
 *
 * @note Counters are monotonic for the lifetime of one bridge instance. Queue
 * depths describe the moment when `UdpRuntimeBridge::MetricsSnapshot` was
 * called.
 */
struct UdpRuntimeBridgeMetrics
{
    /** @brief UDP datagrams received by the command worker. */
    std::uint64_t receivedPackets = 0;
    /** @brief Total command datagram payload bytes received by the worker. */
    std::uint64_t receivedBytes = 0;
    /** @brief Command batches decoded successfully. */
    std::uint64_t decodedBatches = 0;
    /** @brief Command datagrams rejected during decoding or validation. */
    std::uint64_t decodeErrors = 0;
    /** @brief Command batches accepted into the inbound queue. */
    std::uint64_t queuedBatches = 0;
    /** @brief Command batches intentionally dropped before application. */
    std::uint64_t droppedBatches = 0;
    /** @brief Command batches drained by the render thread. */
    std::uint64_t drainedBatches = 0;
    /** @brief Commands drained by the render thread. */
    std::uint64_t drainedCommands = 0;
    /** @brief Commands reported as applied by the render thread. */
    std::uint64_t appliedCommands = 0;
    /** @brief Commands reported as skipped or rejected by the render thread. */
    std::uint64_t skippedCommands = 0;
    /** @brief State-like commands collapsed by latest-wins coalescing. */
    std::uint64_t coalescedCommands = 0;
    /** @brief Batches intentionally truncated by a frame processing budget. */
    std::uint64_t truncatedBatches = 0;
    /** @brief Feedback payloads accepted into the outbound queue. */
    std::uint64_t feedbackQueued = 0;
    /** @brief Feedback payloads sent successfully. */
    std::uint64_t feedbackSent = 0;
    /** @brief Feedback payloads intentionally dropped or lost after send failure. */
    std::uint64_t feedbackDropped = 0;
    /** @brief Command batches currently queued for the render thread. */
    std::size_t inboundQueueDepth = 0;
    /** @brief Feedback payloads currently queued for the UDP worker. */
    std::size_t outboundQueueDepth = 0;
};

/**
 * @brief Background UDP I/O bridge used by a window application.
 *
 * @note The bridge owns a dedicated worker thread for UDP receive/send work.
 * The render thread remains responsible for applying commands to the scene and
 * for producing runtime feedback snapshots.
 */
class MFD_API UdpRuntimeBridge
{
public:
    /**
     * @brief Factory creating one exchange channel instance.
     */
    using ChannelFactory = std::function<std::unique_ptr<IExchangeChannel>()>;

    /**
     * @brief Creates a bridge from window command and feedback transport settings.
     * @param commandConfig UDP command transport configuration.
     * @param feedbackConfig UDP feedback transport configuration.
     */
    explicit UdpRuntimeBridge(WindowCommandTransportConfig commandConfig = {},
                              WindowFeedbackTransportConfig feedbackConfig = {});

    /**
     * @brief Creates a bridge from channel factories (useful for custom transports and tests).
     * @param commandReceiverFactory Factory creating the command receiver channel.
     * @param feedbackSenderFactory Factory creating the feedback sender channel.
     */
    UdpRuntimeBridge(ChannelFactory commandReceiverFactory, ChannelFactory feedbackSenderFactory);

    ~UdpRuntimeBridge();

    UdpRuntimeBridge(const UdpRuntimeBridge&) = delete;
    UdpRuntimeBridge& operator=(const UdpRuntimeBridge&) = delete;

    /**
     * @brief Starts the background worker thread when at least one UDP channel is available.
     * @return `true` when the bridge is running or when no UDP transport is configured.
     */
    bool Start();

    /**
     * @brief Stops the background worker thread and releases the UDP channels.
     */
    void Stop() noexcept;

    /**
     * @brief Indicates whether the background worker thread is currently running.
     * @return `true` when the worker thread is alive.
     */
    bool IsRunning() const noexcept;

    /**
     * @brief Indicates whether a command receiver transport is configured.
     * @return `true` when a command receiver channel exists.
     */
    bool HasCommandReceiver() const noexcept;

    /**
     * @brief Indicates whether a feedback sender transport is configured.
     * @return `true` when a feedback sender channel exists.
     */
    bool HasFeedbackSender() const noexcept;

    /**
     * @brief Indicates whether the command receiver channel is ready.
     * @return `true` when the command receiver can receive UDP packets.
     */
    bool CommandTransportReady() const noexcept;

    /**
     * @brief Indicates whether the feedback sender channel is ready.
     * @return `true` when the feedback sender can send UDP packets.
     */
    bool FeedbackTransportReady() const noexcept;

    /**
     * @brief Returns a thread-safe snapshot of UDP runtime pressure counters.
     * @return Value-type metrics snapshot.
     */
    [[nodiscard]] UdpRuntimeBridgeMetrics MetricsSnapshot() const;

    /**
     * @brief Drains received command batches into a caller-owned vector.
     * @param destination Vector receiving the drained batches. New batches are appended.
     * @param maxBatches Maximum number of batches drained during this call.
     * @return Number of batches appended to the destination vector.
     *
     * @note The internal inbound queue is bounded in command batches. When the
     * queue overflows, the worker drops the oldest batches first.
     */
    std::size_t DrainReceivedBatches(std::vector<CommandBatch>& destination,
                                     std::size_t maxBatches = 256);

    /**
     * @brief Drains whole command batches without exceeding a command budget.
     * @param destination Vector receiving the drained batches. New batches are appended.
     * @param maxCommands Maximum number of commands the caller can process.
     * @param maxBatches Maximum number of batches drained during this call.
     * @return Number of batches appended to the destination vector.
     *
     * @note Batches are never split by this method. If the next queued batch
     * does not fit the remaining command budget, it stays queued with its
     * sequence and mapping hash intact.
     */
    std::size_t DrainReceivedBatchesForCommandBudget(std::vector<CommandBatch>& destination,
                                                     std::size_t maxCommands,
                                                     std::size_t maxBatches = 256);

    /**
     * @brief Drains received commands into a caller-owned vector.
     * @param destination Vector receiving the drained commands. New commands are appended.
     * @param maxCommands Maximum number of commands drained during this call.
     * @return Number of commands appended to the destination vector.
     *
     * @note This compatibility helper flattens received command batches and
     * therefore drops `sequence` and `mappingHash`. Prefer `DrainReceivedBatches`
     * when the caller needs the full protocol envelope.
     */
    std::size_t DrainReceivedCommands(std::vector<UserCommand>& destination,
                                      std::size_t maxCommands = 256);

    /**
     * @brief Records render-thread command processing outcomes in bridge metrics.
     * @param appliedCommands Commands successfully applied to the runtime scene.
     * @param skippedCommands Commands rejected, skipped or left unapplied after draining.
     * @param truncatedBatches Batches intentionally truncated by the caller.
     */
    void RecordCommandProcessingResult(std::size_t appliedCommands,
                                       std::size_t skippedCommands,
                                       std::size_t truncatedBatches);

    /**
     * @brief Queues a strobe feedback payload to be sent by the worker thread.
     * @param feedback Feedback snapshot to enqueue.
     */
    void EnqueueStrobeFeedback(StrobeStatusFeedback feedback);

    /**
     * @brief Queues an active-page feedback payload to be sent by the worker thread.
     * @param feedback Feedback snapshot to enqueue.
     */
    void EnqueueActivePageFeedback(ActivePageFeedback feedback);

    /**
     * @brief Queues a window lifecycle heartbeat payload to be sent by the worker thread.
     * @param feedback Lifecycle snapshot to enqueue.
     *
     * @note The window emits periodic `Alive` payloads and one final `Closing`
     * payload during a graceful shutdown so clients can detect window
     * termination and reset their transports.
     */
    void EnqueueWindowLifecycleFeedback(WindowLifecycleFeedback feedback);

    /**
     * @brief Returns the last command-side status or error string.
     * @return Command-side status string.
     */
    std::string LastCommandStatus() const;

    /**
     * @brief Returns the last feedback-side status or error string.
     * @return Feedback-side status string.
     */
    std::string LastFeedbackStatus() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace mfd

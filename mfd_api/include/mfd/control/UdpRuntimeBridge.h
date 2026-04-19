/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Background command/feedback I/O bridge decoupling command reception from rendering.
 */

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "mfd/MfdExport.h"
#include "mfd/control/CommandTransport.h"
#include "mfd/control/CommandTypes.h"
#include "mfd/control/FeedbackTransport.h"
#include "mfd/control/StrobeFeedback.h"

namespace mfd
{
/**
 * @brief Background command/feedback I/O bridge used by a window application.
 *
 * @note The bridge owns a dedicated worker thread for configured transport receive/send work.
 * The render thread remains responsible for applying commands to the scene and
 * for producing strobe feedback snapshots.
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
    UdpRuntimeBridge(WindowCommandTransportConfig commandConfig = {},
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
     * @brief Starts the background worker thread when at least one command or feedback transport is available.
     * @return `true` when the bridge is running or when no transport is configured.
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
     * @brief Drains received commands into a caller-owned vector.
     * @param destination Vector receiving the drained commands. New commands are appended.
     * @param maxCommands Maximum number of commands drained during this call.
     * @return Number of commands appended to the destination vector.
     */
    std::size_t DrainReceivedCommands(std::vector<UserCommand>& destination,
                                      std::size_t maxCommands = 256);

    /**
     * @brief Queues a strobe feedback payload to be sent by the worker thread.
     * @param feedback Feedback snapshot to enqueue.
     */
    void EnqueueStrobeFeedback(StrobeStatusFeedback feedback);

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

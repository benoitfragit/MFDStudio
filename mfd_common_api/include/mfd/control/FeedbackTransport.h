/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief UDP transport configuration and factories used by window feedback streams.
 */

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "mfd/MfdExport.h"
#include "mfd/ipc/UdpLimits.h"

namespace mfd
{
class IExchangeChannel;

/**
 * @brief UDP transport settings attached to a window feedback stream.
 */
struct WindowUdpFeedbackTransport
{
    /** @brief Enables or disables the UDP transport. */
    bool enabled = false;
    /** @brief Numeric IPv4 address bound or targeted by the receiver/sender side. Prefer `127.0.0.1` for trusted local feedback streams. */
    std::string address = "127.0.0.1";
    /** @brief UDP port used by the transport. */
    std::uint16_t port = 47221;
    /** @brief Maximum Protocol Buffers UDP payload size accepted by the channel in the supported `[64, 65507]` range. */
    std::size_t maxPacketSize = kUdpDefaultPayloadBytes;
};

/**
 * @brief Optional feedback transports exposed by a window.
 */
struct WindowFeedbackTransportConfig
{
    /** @brief Optional UDP feedback transport settings. */
    std::optional<WindowUdpFeedbackTransport> udp;
};

/**
 * @brief Creates a UDP feedback receiver channel.
 * @param config UDP configuration.
 * @return Receiver-side exchange channel.
 */
MFD_API std::unique_ptr<IExchangeChannel> CreateFeedbackReceiverChannel(const WindowUdpFeedbackTransport& config);

/**
 * @brief Creates a UDP feedback sender channel.
 * @param config UDP configuration.
 * @return Sender-side exchange channel.
 */
MFD_API std::unique_ptr<IExchangeChannel> CreateFeedbackSenderChannel(const WindowUdpFeedbackTransport& config);

/**
 * @brief Creates a feedback receiver channel from a generic window configuration.
 * @param config Window feedback transport configuration.
 * @return Receiver-side exchange channel, or `nullptr` if UDP is unavailable.
 */
MFD_API std::unique_ptr<IExchangeChannel> CreateFeedbackReceiverChannel(const WindowFeedbackTransportConfig& config);

/**
 * @brief Creates a feedback sender channel from a generic window configuration.
 * @param config Window feedback transport configuration.
 * @return Sender-side exchange channel, or `nullptr` if UDP is unavailable.
 */
MFD_API std::unique_ptr<IExchangeChannel> CreateFeedbackSenderChannel(const WindowFeedbackTransportConfig& config);
} // namespace mfd

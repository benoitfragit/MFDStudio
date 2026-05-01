/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief UDP transport configuration and factories used by the remote command API.
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
 * @brief UDP transport settings attached to a window configuration.
 */
struct WindowUdpCommandTransport
{
    /** @brief Enables or disables the UDP transport. */
    bool enabled = false;
    /** @brief Numeric IPv4 address bound or targeted by the receiver/client side. Prefer `127.0.0.1` for trusted local control. */
    std::string address = "127.0.0.1";
    /** @brief UDP port used by the transport. */
    std::uint16_t port = 47220;
    /** @brief Maximum Protocol Buffers UDP payload size accepted by the channel in the supported `[64, 65507]` range. */
    std::size_t maxPacketSize = kUdpDefaultPayloadBytes;
};

/**
 * @brief Optional command transports exposed by a window.
 */
struct WindowCommandTransportConfig
{
    /** @brief Optional UDP command transport settings. */
    std::optional<WindowUdpCommandTransport> udp;
};

/**
 * @brief Creates a UDP command receiver channel.
 * @param config UDP configuration.
 * @return Receiver-side exchange channel.
 */
MFD_API std::unique_ptr<IExchangeChannel> CreateCommandReceiverChannel(const WindowUdpCommandTransport& config);

/**
 * @brief Creates a UDP command client channel.
 * @param config UDP configuration.
 * @return Client-side exchange channel.
 */
MFD_API std::unique_ptr<IExchangeChannel> CreateCommandClientChannel(const WindowUdpCommandTransport& config);

/**
 * @brief Creates a receiver channel from a generic window configuration.
 * @param config Window transport configuration.
 * @return Receiver-side exchange channel, or `nullptr` if UDP is unavailable.
 */
MFD_API std::unique_ptr<IExchangeChannel> CreateCommandReceiverChannel(const WindowCommandTransportConfig& config);

/**
 * @brief Creates a client channel from a generic window configuration.
 * @param config Window transport configuration.
 * @return Client-side exchange channel, or `nullptr` if UDP is unavailable.
 */
MFD_API std::unique_ptr<IExchangeChannel> CreateCommandClientChannel(const WindowCommandTransportConfig& config);
} // namespace mfd

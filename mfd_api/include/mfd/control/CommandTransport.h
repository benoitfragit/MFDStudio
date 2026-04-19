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
    /** @brief Address bound or targeted by the receiver/client side. */
    std::string address = "127.0.0.1";
    /** @brief UDP port used by the transport. */
    std::uint16_t port = 47220;
    /** @brief Maximum Protocol Buffers UDP payload size accepted by the channel. */
    std::size_t maxPacketSize = 4096;
};

/**
 * @brief Optional command transports exposed by a window.
 */
struct WindowShmCommandTransport
{
    /** @brief Enables or disables SHM transport. */
    bool enabled = false;
    /** @brief Communication mode token, expected value: "shm". */
    std::string type = "shm";
    /** @brief Shared memory name used for Client -> MFD packet flow. */
    std::string inMemoryName;
    /** @brief CanWrite event name used for Client -> MFD packet flow. */
    std::string inCanWriteEventName;
    /** @brief HasData event name used for Client -> MFD packet flow. */
    std::string inHasDataEventName;
    /** @brief Shared memory name used for MFD -> Client packet flow. */
    std::string outMemoryName;
    /** @brief CanWrite event name used for MFD -> Client packet flow. */
    std::string outCanWriteEventName;
    /** @brief HasData event name used for MFD -> Client packet flow. */
    std::string outHasDataEventName;
    /** @brief Timeout in milliseconds used by WaitForSingleObject operations. */
    std::uint32_t timeoutMs = 5U;
    /** @brief Shared-library path used to load the MFD-side SHM adapter plugin. */
    std::string pluginPath;
    /** @brief Symbol exported by the plugin to create the adapter instance. */
    std::string factorySymbol;
};

struct WindowCommandTransportConfig
{
    /** @brief Optional UDP command transport settings. */
    std::optional<WindowUdpCommandTransport> udp;
    /** @brief Optional SHM command transport settings. */
    std::optional<WindowShmCommandTransport> shm;
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
/**
 * @brief Creates a receiver channel from SHM mono-slot settings.
 */
MFD_API std::unique_ptr<IExchangeChannel> CreateCommandReceiverChannel(const WindowShmCommandTransport& config);

/**
 * @brief Creates a client channel from SHM mono-slot settings.
 */
MFD_API std::unique_ptr<IExchangeChannel> CreateCommandClientChannel(const WindowShmCommandTransport& config);
} // namespace mfd

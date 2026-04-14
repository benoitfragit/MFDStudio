/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief UDP transport implementation used by the public command API.
 */

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "mfd/MfdExport.h"
#include "mfd/ipc/ExchangeChannel.h"

namespace mfd
{
/**
 * @brief Concrete UDP channel configuration.
 */
struct UdpChannelConfig
{
    /** @brief Local bind address used by the socket. */
    std::string bindAddress = "127.0.0.1";
    /** @brief Local bind port used by the socket. */
    std::uint16_t bindPort = 0;
    /** @brief Default remote target address used by `Send()`. */
    std::string remoteAddress = "127.0.0.1";
    /** @brief Default remote target port used by `Send()`. */
    std::uint16_t remotePort = 0;
    /** @brief Maximum receive packet size in bytes. */
    std::size_t maxPacketSize = 4096;
};

/**
 * @brief UDP implementation of the generic exchange channel.
 */
class MFD_API UdpChannel final : public IExchangeChannel
{
public:
    /**
     * @brief Opens a UDP channel.
     * @param config UDP channel configuration.
     */
    explicit UdpChannel(const UdpChannelConfig& config);
    ~UdpChannel() override;

    UdpChannel(UdpChannel&&) noexcept;
    UdpChannel& operator=(UdpChannel&&) noexcept;

    UdpChannel(const UdpChannel&) = delete;
    UdpChannel& operator=(const UdpChannel&) = delete;

    /** @copydoc IExchangeChannel::IsReady */
    bool IsReady() const noexcept override;
    /** @copydoc IExchangeChannel::Send */
    bool Send(ByteView buffer) override;
    /** @copydoc IExchangeChannel::TryReceive */
    std::optional<std::vector<std::byte>> TryReceive() override;
    /** @copydoc IExchangeChannel::LastError */
    std::string LastError() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace mfd

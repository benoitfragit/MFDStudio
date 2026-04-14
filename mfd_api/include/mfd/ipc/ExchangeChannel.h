/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Abstract byte transport used by UDP and shared-memory command channels.
 */

#include <cstddef>
#include <optional>
#include "mfd/core/ArrayView.h"
#include <string>
#include <vector>

#include "mfd/MfdExport.h"

namespace mfd
{
/**
 * @brief Abstract byte-oriented exchange channel used by transports.
 */
class MFD_API IExchangeChannel
{
public:
    virtual ~IExchangeChannel() = default;

    /**
     * @brief Indicates whether the channel is ready for send/receive operations.
     * @return `true` when the channel is usable.
     */
    virtual bool IsReady() const noexcept = 0;

    /**
     * @brief Sends a raw payload.
     * @param buffer Payload bytes to send.
     * @return `true` if the payload was accepted by the channel.
     */
    virtual bool Send(ByteView buffer) = 0;

    /**
     * @brief Attempts to receive one payload without blocking indefinitely.
     * @return Received payload, or `std::nullopt` when no data is available.
     */
    virtual std::optional<std::vector<std::byte>> TryReceive() = 0;

    /**
     * @brief Returns the last channel-specific error.
     * @return Error string, or an empty string if the channel is healthy.
     */
    virtual std::string LastError() const = 0;
};
} // namespace mfd

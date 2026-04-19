/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for CommandTransport.
 */

#include "mfd/control/CommandTransport.h"

#include <algorithm>
#include <cstring>

#include "mfd/core/Validation.h"
#include "mfd/ipc/ExchangeChannel.h"
#include "mfd/ipc/ShmPacket.h"
#include "mfd/ipc/UdpChannel.h"
#include "mfd/ipc/windows/NamedShmMonoSlot.h"

namespace mfd
{
namespace
{
class NamedShmExchangeChannel final : public IExchangeChannel
{
public:
    NamedShmExchangeChannel(const ipc::windows::NamedShmMonoSlotConfig& config, const bool writer)
        : writer_(writer)
    {
        const auto role = writer ? ipc::windows::MonoSlotRole::Writer : ipc::windows::MonoSlotRole::Reader;
        ready_ = slot_.Open(config, role);
        if (!ready_)
        {
            lastError_ = slot_.LastError();
        }
    }

    bool IsReady() const noexcept override
    {
        return ready_;
    }

    bool Send(const ByteView buffer) override
    {
        if (!writer_)
        {
            lastError_ = "SHM receiver channel cannot send";
            return false;
        }

        ShmPacket packet;
        packet.payloadSize = static_cast<std::uint32_t>(std::min<std::size_t>(buffer.size(), kShmPayloadBytes));
        std::memcpy(packet.payload.data(), buffer.data(), packet.payloadSize);

        if (!slot_.Write(packet))
        {
            lastError_ = slot_.LastError();
            return false;
        }

        lastError_.clear();
        return true;
    }

    std::optional<std::vector<std::byte>> TryReceive() override
    {
        if (writer_)
        {
            return std::nullopt;
        }

        ShmPacket packet;
        if (!slot_.Read(packet))
        {
            lastError_ = slot_.LastError();
            return std::nullopt;
        }

        std::string validationError;
        if (!ValidateShmPacket(packet, &validationError))
        {
            lastError_ = validationError;
            return std::nullopt;
        }

        std::vector<std::byte> payload(packet.payloadSize);
        std::memcpy(payload.data(), packet.payload.data(), packet.payloadSize);
        lastError_.clear();
        return payload;
    }

    std::string LastError() const override
    {
        return lastError_;
    }

private:
    ipc::windows::NamedShmMonoSlot slot_ {};
    bool writer_ = false;
    bool ready_ = false;
    std::string lastError_ {};
};

ipc::windows::NamedShmMonoSlotConfig BuildReaderConfig(const WindowShmCommandTransport& config)
{
    ipc::windows::NamedShmMonoSlotConfig slotConfig;
    slotConfig.sharedMemoryName = config.inMemoryName;
    slotConfig.canWriteEventName = config.inCanWriteEventName;
    slotConfig.hasDataEventName = config.inHasDataEventName;
    slotConfig.timeoutMs = config.timeoutMs;
    return slotConfig;
}

} // namespace

std::unique_ptr<IExchangeChannel> CreateCommandReceiverChannel(const WindowUdpCommandTransport& config)
{
    if (!config.enabled || config.port == 0)
    {
        return nullptr;
    }

    UdpChannelConfig udpConfig;
    udpConfig.bindAddress = config.address;
    udpConfig.bindPort = config.port;
    udpConfig.maxPacketSize = config.maxPacketSize;
    return std::make_unique<UdpChannel>(udpConfig);
}

std::unique_ptr<IExchangeChannel> CreateCommandClientChannel(const WindowUdpCommandTransport& config)
{
    if (!config.enabled || config.port == 0)
    {
        return nullptr;
    }

    UdpChannelConfig udpConfig;
    udpConfig.remoteAddress = config.address;
    udpConfig.remotePort = config.port;
    udpConfig.maxPacketSize = config.maxPacketSize;
    return std::make_unique<UdpChannel>(udpConfig);
}

std::unique_ptr<IExchangeChannel> CreateCommandReceiverChannel(const WindowShmCommandTransport& config)
{
    if (!config.enabled)
    {
        return nullptr;
    }

    return std::make_unique<NamedShmExchangeChannel>(BuildReaderConfig(config), false);
}

std::unique_ptr<IExchangeChannel> CreateCommandClientChannel(const WindowShmCommandTransport& config)
{
    if (!config.enabled)
    {
        return nullptr;
    }

    return std::make_unique<NamedShmExchangeChannel>(BuildReaderConfig(config), true);
}

std::unique_ptr<IExchangeChannel> CreateCommandReceiverChannel(const WindowCommandTransportConfig& config)
{
    if (config.shm.has_value() && config.shm->enabled)
    {
        return CreateCommandReceiverChannel(*config.shm);
    }

    if (!config.udp.has_value())
    {
        return nullptr;
    }

    return CreateCommandReceiverChannel(*config.udp);
}

std::unique_ptr<IExchangeChannel> CreateCommandClientChannel(const WindowCommandTransportConfig& config)
{
    if (config.shm.has_value() && config.shm->enabled)
    {
        return CreateCommandClientChannel(*config.shm);
    }

    if (!config.udp.has_value())
    {
        return nullptr;
    }

    return CreateCommandClientChannel(*config.udp);
}
} // namespace mfd

/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "mfd/control/CommandTransport.h"

#include "mfd/ipc/UdpChannel.h"

namespace mfd
{
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

std::unique_ptr<IExchangeChannel> CreateCommandReceiverChannel(const WindowCommandTransportConfig& config)
{
    if (!config.udp.has_value())
    {
        return nullptr;
    }

    return CreateCommandReceiverChannel(*config.udp);
}

std::unique_ptr<IExchangeChannel> CreateCommandClientChannel(const WindowCommandTransportConfig& config)
{
    if (!config.udp.has_value())
    {
        return nullptr;
    }

    return CreateCommandClientChannel(*config.udp);
}
} // namespace mfd

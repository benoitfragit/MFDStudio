/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "mfd/control/FeedbackTransport.h"

#include "mfd/ipc/UdpChannel.h"

namespace mfd
{
namespace
{
std::unique_ptr<IExchangeChannel> CreateReceiver(const std::string& address,
                                                 const std::uint16_t port,
                                                 const std::size_t maxPacketSize)
{
    if (port == 0)
    {
        return nullptr;
    }

    UdpChannelConfig udpConfig;
    udpConfig.bindAddress = address;
    udpConfig.bindPort = port;
    udpConfig.maxPacketSize = maxPacketSize;
    return std::make_unique<UdpChannel>(udpConfig);
}

std::unique_ptr<IExchangeChannel> CreateSender(const std::string& address,
                                               const std::uint16_t port,
                                               const std::size_t maxPacketSize)
{
    if (port == 0)
    {
        return nullptr;
    }

    UdpChannelConfig udpConfig;
    udpConfig.remoteAddress = address;
    udpConfig.remotePort = port;
    udpConfig.maxPacketSize = maxPacketSize;
    return std::make_unique<UdpChannel>(udpConfig);
}
} // namespace

std::unique_ptr<IExchangeChannel> CreateFeedbackReceiverChannel(const WindowUdpFeedbackTransport& config)
{
    if (!config.enabled)
    {
        return nullptr;
    }

    return CreateReceiver(config.address, config.port, config.maxPacketSize);
}

std::unique_ptr<IExchangeChannel> CreateFeedbackSenderChannel(const WindowUdpFeedbackTransport& config)
{
    if (!config.enabled)
    {
        return nullptr;
    }

    return CreateSender(config.address, config.port, config.maxPacketSize);
}

std::unique_ptr<IExchangeChannel> CreateFeedbackReceiverChannel(const WindowFeedbackTransportConfig& config)
{
    if (!config.udp.has_value())
    {
        return nullptr;
    }

    return CreateFeedbackReceiverChannel(*config.udp);
}

std::unique_ptr<IExchangeChannel> CreateFeedbackSenderChannel(const WindowFeedbackTransportConfig& config)
{
    if (!config.udp.has_value())
    {
        return nullptr;
    }

    return CreateFeedbackSenderChannel(*config.udp);
}
} // namespace mfd

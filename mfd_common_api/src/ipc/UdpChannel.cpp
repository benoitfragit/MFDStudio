/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for UdpChannel.
 */

#include "mfd/ipc/UdpChannel.h"

#include <algorithm>
#include <limits>
#include <utility>

#ifdef _WIN32
#include <WinSock2.h>
#include <Ws2tcpip.h>

#include <mutex>
#endif

namespace mfd
{
struct UdpChannel::Impl
{
    bool ready = false;
    std::string lastError;
    std::size_t maxPacketSize = kUdpDefaultPayloadBytes;
    std::vector<std::byte> receiveBuffer {};

#ifdef _WIN32
    SOCKET socketHandle = INVALID_SOCKET;
    sockaddr_in remoteAddress {};
    bool hasRemoteAddress = false;

    ~Impl()
    {
        if (socketHandle != INVALID_SOCKET)
        {
            closesocket(socketHandle);
        }
    }
#endif
};

#ifdef _WIN32
namespace
{
bool EnsureWinsock(std::string& error)
{
    static std::once_flag once;
    static int startupStatus = 0;

    std::call_once(once,
                   []
                   {
                       WSADATA data {};
                       startupStatus = WSAStartup(MAKEWORD(2, 2), &data);
                   });

    if (startupStatus != 0)
    {
        error = "WSAStartup failed with code " + std::to_string(startupStatus);
        return false;
    }

    return true;
}

std::string SocketError(const char* prefix)
{
    return std::string(prefix) + " (WSA " + std::to_string(WSAGetLastError()) + ")";
}

bool ResolveAddress(const std::string& host,
                    const std::uint16_t port,
                    const bool allowAny,
                    sockaddr_in& address,
                    std::string& error)
{
    address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    if (allowAny && (host.empty() || host == "*" || host == "0.0.0.0"))
    {
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        return true;
    }

    if (InetPtonA(AF_INET, host.c_str(), &address.sin_addr) != 1)
    {
        error = "Invalid IPv4 address: " + host;
        return false;
    }

    return true;
}
} // namespace
#endif

UdpChannel::UdpChannel(const UdpChannelConfig& config)
    : impl_(std::make_unique<Impl>())
{
    impl_->maxPacketSize = std::clamp(config.maxPacketSize, kUdpMinPayloadBytes, kUdpMaxPayloadBytes);
    impl_->receiveBuffer.resize(impl_->maxPacketSize);

#ifdef _WIN32
    if (!EnsureWinsock(impl_->lastError))
    {
        return;
    }

    impl_->socketHandle = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (impl_->socketHandle == INVALID_SOCKET)
    {
        impl_->lastError = SocketError("Unable to create UDP socket");
        return;
    }

    u_long nonBlocking = 1;
    if (ioctlsocket(impl_->socketHandle, FIONBIO, &nonBlocking) != NO_ERROR)
    {
        impl_->lastError = SocketError("Unable to switch UDP socket to non-blocking mode");
        return;
    }

    sockaddr_in bindAddress {};
    if (!ResolveAddress(config.bindAddress, config.bindPort, true, bindAddress, impl_->lastError))
    {
        return;
    }

    if (::bind(impl_->socketHandle, reinterpret_cast<sockaddr*>(&bindAddress), sizeof(bindAddress)) == SOCKET_ERROR)
    {
        impl_->lastError = SocketError("Unable to bind UDP socket");
        return;
    }

    if (config.remotePort != 0)
    {
        if (!ResolveAddress(config.remoteAddress, config.remotePort, false, impl_->remoteAddress, impl_->lastError))
        {
            return;
        }

        impl_->hasRemoteAddress = true;
    }

    impl_->ready = true;
    impl_->lastError.clear();
#else
    impl_->lastError = "UDP channel is implemented for Windows targets in this project";
#endif
}

UdpChannel::~UdpChannel() = default;

UdpChannel::UdpChannel(UdpChannel&&) noexcept = default;
UdpChannel& UdpChannel::operator=(UdpChannel&&) noexcept = default;

bool UdpChannel::IsReady() const noexcept
{
    return impl_ != nullptr && impl_->ready;
}

bool UdpChannel::Send(const ByteView buffer)
{
    if (!IsReady())
    {
        return false;
    }

#ifdef _WIN32
    if (!impl_->hasRemoteAddress)
    {
        impl_->lastError = "UDP remote address is not configured";
        return false;
    }

    if (buffer.size() > kUdpMaxPayloadBytes)
    {
        impl_->lastError = "UDP payload exceeds maximum datagram size";
        return false;
    }

    if (buffer.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
    {
        impl_->lastError = "UDP payload exceeds the platform send limit";
        return false;
    }

    const int payloadSize = static_cast<int>(buffer.size());

    const int sentBytes = sendto(impl_->socketHandle,
                                 reinterpret_cast<const char*>(buffer.data()),
                                 payloadSize,
                                 0,
                                 reinterpret_cast<const sockaddr*>(&impl_->remoteAddress),
                                 sizeof(impl_->remoteAddress));

    if (sentBytes == SOCKET_ERROR)
    {
        impl_->lastError = SocketError("Unable to send UDP payload");
        return false;
    }

    if (sentBytes != payloadSize)
    {
        impl_->lastError = "UDP payload was partially sent";
        return false;
    }

    impl_->lastError.clear();
    return true;
#else
    return false;
#endif
}

std::optional<std::vector<std::byte>> UdpChannel::TryReceive()
{
    if (!IsReady())
    {
        return std::nullopt;
    }

#ifdef _WIN32
    if (impl_->receiveBuffer.size() != impl_->maxPacketSize)
    {
        impl_->receiveBuffer.resize(impl_->maxPacketSize);
    }

    sockaddr_in senderAddress {};
    int senderAddressSize = sizeof(senderAddress);

    const int receivedBytes = recvfrom(impl_->socketHandle,
                                       reinterpret_cast<char*>(impl_->receiveBuffer.data()),
                                       static_cast<int>(impl_->receiveBuffer.size()),
                                       0,
                                       reinterpret_cast<sockaddr*>(&senderAddress),
                                       &senderAddressSize);

    if (receivedBytes == SOCKET_ERROR)
    {
        const int socketError = WSAGetLastError();
        if (socketError == WSAEWOULDBLOCK)
        {
            impl_->lastError.clear();
            return std::nullopt;
        }

        if (socketError == WSAEMSGSIZE)
        {
            impl_->lastError = "UDP datagram exceeded receive buffer and was dropped";
            return std::nullopt;
        }

        impl_->lastError = SocketError("Unable to receive UDP payload");
        return std::nullopt;
    }

    impl_->lastError.clear();
    const auto payloadEnd = impl_->receiveBuffer.begin() + receivedBytes;
    return std::vector<std::byte>(impl_->receiveBuffer.begin(), payloadEnd);
#else
    return std::nullopt;
#endif
}

std::string UdpChannel::LastError() const
{
    return impl_ == nullptr ? std::string {} : impl_->lastError;
}
} // namespace mfd

/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for UDP command and feedback transport factories.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "mfd/control/CommandTransport.h"
#include "mfd/control/CommandTypes.h"
#include "mfd/control/FeedbackTransport.h"
#include "mfd/control/StrobeFeedback.h"
#include "mfd/ipc/ExchangeChannel.h"
#include "mfd/ipc/UdpChannel.h"
#include "mfd/ipc/UdpLimits.h"

namespace
{
#ifdef _WIN32
constexpr auto kLoopbackTimeout = std::chrono::milliseconds(500);
constexpr auto kLoopbackPollInterval = std::chrono::milliseconds(5);

struct CommandReceiverBinding
{
    mfd::WindowUdpCommandTransport config;
    std::unique_ptr<mfd::IExchangeChannel> receiver;
};

struct FeedbackReceiverBinding
{
    mfd::WindowUdpFeedbackTransport config;
    std::unique_ptr<mfd::IExchangeChannel> receiver;
};

std::uint16_t MakeCandidatePort(const std::uint64_t seed, const std::size_t attempt)
{
    return static_cast<std::uint16_t>(41000U + ((seed + attempt) % 20000U));
}

CommandReceiverBinding OpenCommandReceiver()
{
    CommandReceiverBinding binding;
    binding.config.enabled = true;
    binding.config.address = "127.0.0.1";
    binding.config.maxPacketSize = 2048U;

    const auto seed = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    for (std::size_t attempt = 0; attempt < 256U; ++attempt)
    {
        binding.config.port = MakeCandidatePort(seed, attempt);
        auto receiver = mfd::CreateCommandReceiverChannel(binding.config);
        if (receiver != nullptr && receiver->IsReady())
        {
            binding.receiver = std::move(receiver);
            return binding;
        }
    }

    binding.config.port = 0;
    return binding;
}

FeedbackReceiverBinding OpenFeedbackReceiver()
{
    FeedbackReceiverBinding binding;
    binding.config.enabled = true;
    binding.config.address = "127.0.0.1";
    binding.config.maxPacketSize = 2048U;

    const auto seed =
        static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()) + 7919U;
    for (std::size_t attempt = 0; attempt < 256U; ++attempt)
    {
        binding.config.port = MakeCandidatePort(seed, attempt);
        auto receiver = mfd::CreateFeedbackReceiverChannel(binding.config);
        if (receiver != nullptr && receiver->IsReady())
        {
            binding.receiver = std::move(receiver);
            return binding;
        }
    }

    binding.config.port = 0;
    return binding;
}

std::optional<std::vector<std::byte>> WaitForPayload(mfd::IExchangeChannel& receiver)
{
    const auto deadline = std::chrono::steady_clock::now() + kLoopbackTimeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        auto payload = receiver.TryReceive();
        if (payload.has_value())
        {
            return payload;
        }

        std::this_thread::sleep_for(kLoopbackPollInterval);
    }

    return receiver.TryReceive();
}

bool WaitUntil(const std::chrono::milliseconds timeout, const std::function<bool()>& predicate)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (predicate())
        {
            return true;
        }

        std::this_thread::sleep_for(kLoopbackPollInterval);
    }

    return predicate();
}

mfd::ByteView AsByteView(const std::string& payload)
{
    return mfd::ByteView(reinterpret_cast<const std::byte*>(payload.data()), payload.size());
}

std::string BytesToString(const std::vector<std::byte>& payload)
{
    return std::string(reinterpret_cast<const char*>(payload.data()), payload.size());
}
#endif
} // namespace

TEST(CommandTransportTests, FactoriesRejectDisabledOrIncompleteCommandConfiguration)
{
    mfd::WindowUdpCommandTransport disabled;
    disabled.enabled = false;
    disabled.port = 47220U;

    auto receiver = mfd::CreateCommandReceiverChannel(disabled);
    auto sender = mfd::CreateCommandClientChannel(disabled);
    EXPECT_EQ(receiver, nullptr);
    EXPECT_EQ(sender, nullptr);

    mfd::WindowUdpCommandTransport zeroPort;
    zeroPort.enabled = true;
    zeroPort.port = 0U;

    receiver = mfd::CreateCommandReceiverChannel(zeroPort);
    sender = mfd::CreateCommandClientChannel(zeroPort);
    EXPECT_EQ(receiver, nullptr);
    EXPECT_EQ(sender, nullptr);

    mfd::WindowCommandTransportConfig generic;
    EXPECT_EQ(mfd::CreateCommandReceiverChannel(generic), nullptr);
    EXPECT_EQ(mfd::CreateCommandClientChannel(generic), nullptr);
}

TEST(CommandTransportTests, FactoriesRejectDisabledOrIncompleteFeedbackConfiguration)
{
    mfd::WindowUdpFeedbackTransport disabled;
    disabled.enabled = false;
    disabled.port = 47221U;

    auto receiver = mfd::CreateFeedbackReceiverChannel(disabled);
    auto sender = mfd::CreateFeedbackSenderChannel(disabled);
    EXPECT_EQ(receiver, nullptr);
    EXPECT_EQ(sender, nullptr);

    mfd::WindowUdpFeedbackTransport zeroPort;
    zeroPort.enabled = true;
    zeroPort.port = 0U;

    receiver = mfd::CreateFeedbackReceiverChannel(zeroPort);
    sender = mfd::CreateFeedbackSenderChannel(zeroPort);
    EXPECT_EQ(receiver, nullptr);
    EXPECT_EQ(sender, nullptr);

    mfd::WindowFeedbackTransportConfig generic;
    EXPECT_EQ(mfd::CreateFeedbackReceiverChannel(generic), nullptr);
    EXPECT_EQ(mfd::CreateFeedbackSenderChannel(generic), nullptr);
}

TEST(CommandTransportTests, PublicUdpConfigsShareTheSameDefaultPayloadLimit)
{
    EXPECT_EQ(mfd::UdpChannelConfig {}.maxPacketSize, mfd::kUdpDefaultPayloadBytes);
    EXPECT_EQ(mfd::WindowUdpCommandTransport {}.maxPacketSize, mfd::kUdpDefaultPayloadBytes);
    EXPECT_EQ(mfd::WindowUdpFeedbackTransport {}.maxPacketSize, mfd::kUdpDefaultPayloadBytes);
}

TEST(CommandTransportTests, CommandTransportLoopbackDeliversSerializedBatches)
{
#ifndef _WIN32
    GTEST_SKIP() << "UDP loopback transport is implemented for Windows targets in this project";
#else
    CommandReceiverBinding binding = OpenCommandReceiver();
    ASSERT_NE(binding.receiver, nullptr);
    ASSERT_NE(binding.config.port, 0U);

    mfd::WindowCommandTransportConfig clientConfig;
    clientConfig.udp = binding.config;
    auto sender = mfd::CreateCommandClientChannel(clientConfig);
    ASSERT_NE(sender, nullptr);
    ASSERT_TRUE(sender->IsReady()) << sender->LastError();

    mfd::CommandBatch batch;
    batch.sequence = 18U;
    batch.mappingHash = "map_hash";
    batch.commands.push_back(mfd::ActivatePageCommand {"", 11U});

    const std::string payload = mfd::SerializeCommandBatch(batch);
    ASSERT_TRUE(sender->Send(AsByteView(payload))) << sender->LastError();

    const auto received = WaitForPayload(*binding.receiver);
    ASSERT_TRUE(received.has_value()) << binding.receiver->LastError();

    std::string error;
    const auto decoded = mfd::DeserializeCommandBatch(BytesToString(*received), &error);
    ASSERT_TRUE(decoded.has_value()) << error;
    EXPECT_EQ(decoded->sequence, 18U);
    EXPECT_EQ(decoded->mappingHash, "map_hash");
    ASSERT_EQ(decoded->commands.size(), 1U);

    const auto* activate = std::get_if<mfd::ActivatePageCommand>(&decoded->commands.front());
    ASSERT_NE(activate, nullptr);
    EXPECT_EQ(activate->pageId, 11U);
    EXPECT_TRUE(activate->page.empty());
#endif
}

TEST(CommandTransportTests, FeedbackTransportLoopbackDeliversSerializedFeedback)
{
#ifndef _WIN32
    GTEST_SKIP() << "UDP loopback transport is implemented for Windows targets in this project";
#else
    FeedbackReceiverBinding binding = OpenFeedbackReceiver();
    ASSERT_NE(binding.receiver, nullptr);
    ASSERT_NE(binding.config.port, 0U);

    mfd::WindowFeedbackTransportConfig senderConfig;
    senderConfig.udp = binding.config;
    auto sender = mfd::CreateFeedbackSenderChannel(senderConfig);
    ASSERT_NE(sender, nullptr);
    ASSERT_TRUE(sender->IsReady()) << sender->LastError();

    mfd::StrobeStatusFeedback feedback;
    feedback.sequence = 27U;
    feedback.pageName = "Radar";
    feedback.strobeId = "cursor";
    feedback.active = true;
    feedback.position = {0.14f, -0.18f};
    feedback.capture.shape = mfd::StrobeCaptureShape::Rectangle;
    feedback.capture.size = {0.22f, 0.10f};
    feedback.magnet.enabled = true;
    feedback.magnet.magnetized = true;
    feedback.magnet.reticleId = "track_9";

    const std::string payload = mfd::SerializeStrobeStatusFeedback(feedback);
    ASSERT_TRUE(sender->Send(AsByteView(payload))) << sender->LastError();

    const auto received = WaitForPayload(*binding.receiver);
    ASSERT_TRUE(received.has_value()) << binding.receiver->LastError();

    std::string error;
    const auto decoded = mfd::DeserializeStrobeStatusFeedback(BytesToString(*received), &error);
    ASSERT_TRUE(decoded.has_value()) << error;
    EXPECT_EQ(decoded->sequence, 27U);
    EXPECT_EQ(decoded->pageName, "Radar");
    EXPECT_EQ(decoded->strobeId, "cursor");
    EXPECT_TRUE(decoded->active);
    EXPECT_EQ(decoded->capture.shape, mfd::StrobeCaptureShape::Rectangle);
    EXPECT_FLOAT_EQ(decoded->capture.size.x, 0.22f);
    EXPECT_FLOAT_EQ(decoded->capture.size.y, 0.10f);
    EXPECT_TRUE(decoded->magnet.enabled);
    EXPECT_TRUE(decoded->magnet.magnetized);
    EXPECT_EQ(decoded->magnet.reticleId, "track_9");
#endif
}

TEST(CommandTransportTests, UdpChannelRequiresRemoteEndpointBeforeSending)
{
#ifndef _WIN32
    GTEST_SKIP() << "UDP loopback transport is implemented for Windows targets in this project";
#else
    mfd::UdpChannelConfig config;
    config.bindAddress = "127.0.0.1";
    config.bindPort = 0U;
    config.maxPacketSize = 256U;

    mfd::UdpChannel channel(config);
    ASSERT_TRUE(channel.IsReady()) << channel.LastError();
    EXPECT_FALSE(channel.TryReceive().has_value());

    const std::string payload = "abc";
    EXPECT_FALSE(channel.Send(AsByteView(payload)));
    EXPECT_EQ(channel.LastError(), "UDP remote address is not configured");
    EXPECT_FALSE(channel.TryReceive().has_value());
    EXPECT_TRUE(channel.LastError().empty());
#endif
}

TEST(CommandTransportTests, UdpChannelRejectsPayloadsLargerThanSupportedDatagramSize)
{
#ifndef _WIN32
    GTEST_SKIP() << "UDP loopback transport is implemented for Windows targets in this project";
#else
    mfd::UdpChannelConfig config;
    config.bindAddress = "127.0.0.1";
    config.bindPort = 0U;
    config.remoteAddress = "127.0.0.1";
    config.remotePort = 47220U;

    mfd::UdpChannel channel(config);
    ASSERT_TRUE(channel.IsReady()) << channel.LastError();

    std::vector<std::byte> payload(mfd::kUdpMaxPayloadBytes + 1U, std::byte {0x2A});
    EXPECT_FALSE(channel.Send(mfd::ByteView(payload.data(), payload.size())));
    EXPECT_EQ(channel.LastError(), "UDP payload exceeds maximum datagram size");
#endif
}

TEST(CommandTransportTests, UdpChannelDropsDatagramsThatExceedReceiveBuffer)
{
#ifndef _WIN32
    GTEST_SKIP() << "UDP loopback transport is implemented for Windows targets in this project";
#else
    std::unique_ptr<mfd::UdpChannel> receiver;
    mfd::UdpChannelConfig receiverConfig;
    receiverConfig.bindAddress = "127.0.0.1";
    receiverConfig.maxPacketSize = 256U;

    const auto seed = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()) + 1234U;
    for (std::size_t attempt = 0; attempt < 256U; ++attempt)
    {
        receiverConfig.bindPort = MakeCandidatePort(seed, attempt);
        auto candidate = std::make_unique<mfd::UdpChannel>(receiverConfig);
        if (candidate->IsReady())
        {
            receiver = std::move(candidate);
            break;
        }
    }

    ASSERT_NE(receiver, nullptr);
    ASSERT_NE(receiverConfig.bindPort, 0U);

    mfd::UdpChannelConfig senderConfig;
    senderConfig.bindAddress = "127.0.0.1";
    senderConfig.bindPort = 0U;
    senderConfig.remoteAddress = "127.0.0.1";
    senderConfig.remotePort = receiverConfig.bindPort;
    senderConfig.maxPacketSize = 2048U;

    mfd::UdpChannel sender(senderConfig);
    ASSERT_TRUE(sender.IsReady()) << sender.LastError();

    std::vector<std::byte> payload(1024U, std::byte {0x5A});
    ASSERT_TRUE(sender.Send(mfd::ByteView(payload.data(), payload.size()))) << sender.LastError();

    ASSERT_TRUE(WaitUntil(
        std::chrono::milliseconds(300),
        [&receiver]()
        {
            static_cast<void>(receiver->TryReceive());
            return receiver->LastError() == "UDP datagram exceeded receive buffer and was dropped";
        }));

    EXPECT_FALSE(receiver->TryReceive().has_value());
    EXPECT_TRUE(receiver->LastError().empty());
#endif
}

TEST(CommandTransportTests, UdpChannelRejectsInvalidBindAddress)
{
#ifndef _WIN32
    GTEST_SKIP() << "UDP loopback transport is implemented for Windows targets in this project";
#else
    mfd::UdpChannelConfig config;
    config.bindAddress = "not-an-ipv4-address";
    config.bindPort = 0U;

    mfd::UdpChannel channel(config);
    EXPECT_FALSE(channel.IsReady());
    EXPECT_NE(channel.LastError().find("Invalid IPv4 address"), std::string::npos);
#endif
}

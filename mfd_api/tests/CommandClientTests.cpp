/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for CommandClient helper methods.
 */

#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "mfd/control/CommandClient.h"
#include "mfd/control/CommandTypes.h"

namespace
{
/**
 * @brief In-memory channel used to capture payloads sent by CommandClient tests.
 */
class CapturingExchangeChannel final : public mfd::IExchangeChannel
{
public:
    bool IsReady() const noexcept override
    {
        return ready_;
    }

    bool Send(const mfd::ByteView buffer) override
    {
        if (!ready_)
        {
            lastError_ = "Channel not ready";
            return false;
        }

        sentPayloads_.emplace_back(buffer.begin(), buffer.end());
        lastError_.clear();
        return true;
    }

    std::optional<std::vector<std::byte>> TryReceive() override
    {
        return std::nullopt;
    }

    std::string LastError() const override
    {
        return lastError_;
    }

    /**
     * @brief Returns every payload sent through this fake channel.
     */
    const std::vector<std::vector<std::byte>>& SentPayloads() const noexcept
    {
        return sentPayloads_;
    }

private:
    bool ready_ = true;
    std::string lastError_ {};
    std::vector<std::vector<std::byte>> sentPayloads_ {};
};
} // namespace

TEST(CommandClientTests, ResetWindowHelperSendsResetWindowCommand)
{
    auto channel = std::make_unique<CapturingExchangeChannel>();
    CapturingExchangeChannel* const rawChannel = channel.get();

    mfd::CommandClient client(std::move(channel));
    ASSERT_TRUE(client.IsReady());

    ASSERT_TRUE(client.ResetWindow());
    ASSERT_EQ(rawChannel->SentPayloads().size(), 1U);

    const std::vector<std::byte>& payloadBytes = rawChannel->SentPayloads().front();
    const std::string payload(reinterpret_cast<const char*>(payloadBytes.data()), payloadBytes.size());
    const auto command = mfd::DeserializeUserCommand(payload);

    ASSERT_TRUE(command.has_value());
    EXPECT_NE(std::get_if<mfd::ResetWindowCommand>(&*command), nullptr);
}

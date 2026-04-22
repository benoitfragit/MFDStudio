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

TEST(CommandClientTests, SplitBulkDynamicReticlesPreservesGeneratedIdentifiers)
{
    auto channel = std::make_unique<CapturingExchangeChannel>();
    CapturingExchangeChannel* const rawChannel = channel.get();

    mfd::CommandClient client(std::move(channel));
    ASSERT_TRUE(client.IsReady());

    mfd::UpsertDynamicReticlesCommand command;
    command.page = "Radar";
    command.pageId = 11U;
    command.templateId = "radar_track";
    command.templateTransportId = 77U;

    for (std::size_t index = 0; index < 24U; ++index)
    {
        mfd::DynamicReticleState state;
        state.reticleId = "track_" + std::to_string(index);
        state.patch.text = std::string(256U, static_cast<char>('A' + (index % 26U)));
        command.reticles.push_back(std::move(state));
    }

    mfd::CommandBatch batch;
    batch.sequence = 42U;
    batch.mappingHash = "map_hash";
    batch.commands.push_back(command);

    ASSERT_TRUE(client.SendBatch(batch));
    ASSERT_GT(rawChannel->SentPayloads().size(), 1U);

    for (const std::vector<std::byte>& payloadBytes : rawChannel->SentPayloads())
    {
        const std::string payload(reinterpret_cast<const char*>(payloadBytes.data()), payloadBytes.size());
        const auto decodedBatch = mfd::DeserializeCommandBatch(payload);

        ASSERT_TRUE(decodedBatch.has_value());
        EXPECT_EQ(decodedBatch->sequence, 42U);
        EXPECT_EQ(decodedBatch->mappingHash, "map_hash");
        ASSERT_EQ(decodedBatch->commands.size(), 1U);

        const auto* splitCommand = std::get_if<mfd::UpsertDynamicReticlesCommand>(&decodedBatch->commands.front());
        ASSERT_NE(splitCommand, nullptr);
        EXPECT_EQ(splitCommand->page, "Radar");
        EXPECT_EQ(splitCommand->pageId, 11U);
        EXPECT_EQ(splitCommand->templateId, "radar_track");
        EXPECT_EQ(splitCommand->templateTransportId, 77U);
        EXPECT_FALSE(splitCommand->reticles.empty());
    }
}

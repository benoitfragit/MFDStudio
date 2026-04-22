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

mfd::GeneratedTransportMap MakeTransportMap()
{
    mfd::GeneratedTransportMap map;
    map.mappingHash = "map_hash";
    map.pages.push_back({11U, "Radar", "radar", true, false});
    map.reticles.push_back({22U, 11U, "heading_box", "heading_box", "static"});
    map.primitives.push_back(
        {33U, mfd::TransportPrimitiveOwnerKind::Reticle, 22U, "heading_value", "heading_value", "text", true});
    map.templates.push_back({55U, "radar_track", "radar_track"});
    map.primitives.push_back(
        {66U, mfd::TransportPrimitiveOwnerKind::Template, 55U, "track_label", "track_label", "text", true});
    map.blinkTypes.push_back({44U, 11U, "slow", "slow", 750U});
    return map;
}
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
        EXPECT_TRUE(splitCommand->page.empty());
        EXPECT_EQ(splitCommand->pageId, 11U);
        EXPECT_TRUE(splitCommand->templateId.empty());
        EXPECT_EQ(splitCommand->templateTransportId, 77U);
        EXPECT_FALSE(splitCommand->reticles.empty());
    }
}

TEST(CommandClientTests, NameBasedStaticHelpersResolveThroughConfiguredTransportMap)
{
    auto channel = std::make_unique<CapturingExchangeChannel>();
    CapturingExchangeChannel* const rawChannel = channel.get();

    mfd::CommandClient client(std::move(channel), MakeTransportMap());
    ASSERT_TRUE(client.IsReady());

    mfd::ReticlePatch patch;
    patch.blinkType = "slow";
    patch.texts.emplace("heading_value", "123");

    ASSERT_TRUE(client.UpdateReticle("Radar", "heading_box", patch)) << client.LastError();
    ASSERT_EQ(rawChannel->SentPayloads().size(), 1U);

    const std::vector<std::byte>& payloadBytes = rawChannel->SentPayloads().front();
    const std::string payload(reinterpret_cast<const char*>(payloadBytes.data()), payloadBytes.size());
    const auto batch = mfd::DeserializeCommandBatch(payload);

    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch->mappingHash, "map_hash");
    ASSERT_EQ(batch->commands.size(), 1U);

    const auto* command = std::get_if<mfd::UpdateReticleCommand>(&batch->commands.front());
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->target.pageId, 11U);
    EXPECT_EQ(command->target.reticleId, 22U);
    EXPECT_TRUE(command->target.page.empty());
    EXPECT_TRUE(command->target.reticle.empty());
    EXPECT_EQ(command->patch.blinkTypeId, 44U);
    EXPECT_TRUE(command->patch.texts.empty());
    EXPECT_EQ(command->patch.textsById.size(), 1U);
    EXPECT_EQ(command->patch.textsById.at(33U), "123");
}

TEST(CommandClientTests, DynamicHelpersDeriveStableHiddenRuntimeIdWhenTransportMapIsConfigured)
{
    auto channel = std::make_unique<CapturingExchangeChannel>();
    CapturingExchangeChannel* const rawChannel = channel.get();

    mfd::CommandClient client(std::move(channel), MakeTransportMap());
    ASSERT_TRUE(client.IsReady());

    mfd::ReticlePatch patch;
    patch.text = "T42";

    ASSERT_TRUE(client.UpsertDynamicReticle("Radar", "track_042", "radar_track", patch)) << client.LastError();
    ASSERT_TRUE(client.RemoveDynamicReticle("Radar", "track_042")) << client.LastError();
    ASSERT_EQ(rawChannel->SentPayloads().size(), 2U);

    auto decodeBatch = [](const std::vector<std::byte>& payloadBytes)
    {
        const std::string payload(reinterpret_cast<const char*>(payloadBytes.data()), payloadBytes.size());
        return mfd::DeserializeCommandBatch(payload);
    };

    const auto upsertBatch = decodeBatch(rawChannel->SentPayloads().front());
    ASSERT_TRUE(upsertBatch.has_value());
    EXPECT_EQ(upsertBatch->mappingHash, "map_hash");
    ASSERT_EQ(upsertBatch->commands.size(), 1U);

    const auto* upsertCommand = std::get_if<mfd::UpsertDynamicReticleCommand>(&upsertBatch->commands.front());
    ASSERT_NE(upsertCommand, nullptr);
    EXPECT_EQ(upsertCommand->target.pageId, 11U);
    EXPECT_EQ(upsertCommand->templateTransportId, 55U);
    EXPECT_NE(upsertCommand->target.runtimeReticleId, 0U);
    EXPECT_TRUE(upsertCommand->target.page.empty());
    EXPECT_TRUE(upsertCommand->target.reticleId.empty());

    const auto removeBatch = decodeBatch(rawChannel->SentPayloads().back());
    ASSERT_TRUE(removeBatch.has_value());
    EXPECT_EQ(removeBatch->mappingHash, "map_hash");
    ASSERT_EQ(removeBatch->commands.size(), 1U);

    const auto* removeCommand = std::get_if<mfd::RemoveDynamicReticleCommand>(&removeBatch->commands.front());
    ASSERT_NE(removeCommand, nullptr);
    EXPECT_EQ(removeCommand->target.pageId, 11U);
    EXPECT_EQ(removeCommand->target.runtimeReticleId, upsertCommand->target.runtimeReticleId);
    EXPECT_TRUE(removeCommand->target.page.empty());
    EXPECT_TRUE(removeCommand->target.reticleId.empty());
}

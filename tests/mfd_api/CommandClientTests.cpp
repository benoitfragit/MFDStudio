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
#include <string_view>
#include <utility>
#include <vector>

#include "mfd/control/CommandClient.h"
#include "mfd/control/CommandTypes.h"
#include "mfd/ipc/UdpLimits.h"

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

/**
 * @brief Minimal generated-page stand-in used to test CommandClient page overloads.
 */
struct GeneratedRadarPage
{
    using MfdGeneratedPageTag = mfd::CommandClient::GeneratedPageTag;

    static constexpr mfd::TransportId GeneratedId() noexcept
    {
        return 11U;
    }

    static constexpr std::string_view MappingHash() noexcept
    {
        return "map_hash";
    }
};

/**
 * @brief Generated-page stand-in carrying a stale mapping hash.
 */
struct StaleGeneratedRadarPage
{
    using MfdGeneratedPageTag = mfd::CommandClient::GeneratedPageTag;

    static constexpr mfd::TransportId GeneratedId() noexcept
    {
        return 11U;
    }

    static constexpr std::string_view MappingHash() noexcept
    {
        return "stale_map_hash";
    }
};

std::optional<mfd::CommandBatch> DecodeBatchPayload(const std::vector<std::byte>& payloadBytes)
{
    const std::string payload(reinterpret_cast<const char*>(payloadBytes.data()), payloadBytes.size());
    return mfd::DeserializeCommandBatch(payload);
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

    const auto upsertBatch = DecodeBatchPayload(rawChannel->SentPayloads().front());
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

    const auto removeBatch = DecodeBatchPayload(rawChannel->SentPayloads().back());
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

TEST(CommandClientTests, NameBasedBulkDynamicReticlesResolveThroughConfiguredTransportMap)
{
    auto channel = std::make_unique<CapturingExchangeChannel>();
    CapturingExchangeChannel* const rawChannel = channel.get();

    mfd::CommandClient client(std::move(channel), MakeTransportMap());
    ASSERT_TRUE(client.IsReady());

    mfd::ReticlePatch firstPatch;
    firstPatch.visible = true;
    firstPatch.blinkType = "slow";
    firstPatch.texts.emplace("track_label", "T001");
    firstPatch.letterSpacings.emplace("track_label", 0.008f);

    mfd::ReticlePatch secondPatch;
    secondPatch.visible = true;
    secondPatch.texts.emplace("track_label", "T002");
    secondPatch.letterSpacings.emplace("track_label", 0.010f);

    mfd::UpsertDynamicReticlesCommand command;
    command.page = "Radar";
    command.templateId = "radar_track";
    command.reticles.push_back(mfd::DynamicReticleState {"track_001", 0U, firstPatch});
    command.reticles.push_back(mfd::DynamicReticleState {"track_002", 0U, secondPatch});

    mfd::CommandBatch batch;
    batch.sequence = 77U;
    batch.commands.push_back(std::move(command));

    ASSERT_TRUE(client.SendBatch(batch)) << client.LastError();
    ASSERT_EQ(rawChannel->SentPayloads().size(), 1U);

    const std::vector<std::byte>& payloadBytes = rawChannel->SentPayloads().front();
    const std::string payload(reinterpret_cast<const char*>(payloadBytes.data()), payloadBytes.size());
    const auto decodedBatch = mfd::DeserializeCommandBatch(payload);

    ASSERT_TRUE(decodedBatch.has_value());
    EXPECT_EQ(decodedBatch->sequence, 77U);
    EXPECT_EQ(decodedBatch->mappingHash, "map_hash");
    ASSERT_EQ(decodedBatch->commands.size(), 1U);

    const auto* upsert = std::get_if<mfd::UpsertDynamicReticlesCommand>(&decodedBatch->commands.front());
    ASSERT_NE(upsert, nullptr);
    EXPECT_TRUE(upsert->page.empty());
    EXPECT_EQ(upsert->pageId, 11U);
    EXPECT_TRUE(upsert->templateId.empty());
    EXPECT_EQ(upsert->templateTransportId, 55U);
    ASSERT_EQ(upsert->reticles.size(), 2U);

    EXPECT_TRUE(upsert->reticles[0].reticleId.empty());
    EXPECT_NE(upsert->reticles[0].runtimeReticleId, 0U);
    EXPECT_EQ(upsert->reticles[0].patch.blinkTypeId, 44U);
    EXPECT_TRUE(upsert->reticles[0].patch.texts.empty());
    EXPECT_TRUE(upsert->reticles[0].patch.letterSpacings.empty());
    EXPECT_EQ(upsert->reticles[0].patch.textsById.at(66U), "T001");
    EXPECT_FLOAT_EQ(upsert->reticles[0].patch.letterSpacingsById.at(66U), 0.008f);

    EXPECT_TRUE(upsert->reticles[1].reticleId.empty());
    EXPECT_NE(upsert->reticles[1].runtimeReticleId, 0U);
    EXPECT_NE(upsert->reticles[1].runtimeReticleId, upsert->reticles[0].runtimeReticleId);
    EXPECT_TRUE(upsert->reticles[1].patch.texts.empty());
    EXPECT_TRUE(upsert->reticles[1].patch.letterSpacings.empty());
    EXPECT_EQ(upsert->reticles[1].patch.textsById.at(66U), "T002");
    EXPECT_FLOAT_EQ(upsert->reticles[1].patch.letterSpacingsById.at(66U), 0.010f);
}

TEST(CommandClientTests, PageStrobeAndDynamicSetHelpersResolveGeneratedIdsWhenTransportMapIsConfigured)
{
    auto channel = std::make_unique<CapturingExchangeChannel>();
    CapturingExchangeChannel* const rawChannel = channel.get();

    mfd::CommandClient client(std::move(channel), MakeTransportMap());
    ASSERT_TRUE(client.IsReady());

    const GeneratedRadarPage radarPage;
    ASSERT_TRUE(client.ActivatePage(radarPage)) << client.LastError();
    ASSERT_TRUE(client.SetPageView(radarPage, {0.25f, -0.5f}, 1.75f)) << client.LastError();
    ASSERT_TRUE(client.SetStrobeActive("Radar", true)) << client.LastError();
    ASSERT_TRUE(client.SetStrobePosition("Radar", {0.1f, -0.2f})) << client.LastError();
    ASSERT_TRUE(client.SetDynamicReticleSetVisible("Radar", "radar_track", false)) << client.LastError();
    ASSERT_EQ(rawChannel->SentPayloads().size(), 5U);

    const auto activateBatch = DecodeBatchPayload(rawChannel->SentPayloads()[0]);
    ASSERT_TRUE(activateBatch.has_value());
    EXPECT_EQ(activateBatch->mappingHash, "map_hash");
    const auto* activate = std::get_if<mfd::ActivatePageCommand>(&activateBatch->commands.front());
    ASSERT_NE(activate, nullptr);
    EXPECT_EQ(activate->pageId, 11U);
    EXPECT_TRUE(activate->page.empty());

    const auto pageViewBatch = DecodeBatchPayload(rawChannel->SentPayloads()[1]);
    ASSERT_TRUE(pageViewBatch.has_value());
    EXPECT_EQ(pageViewBatch->mappingHash, "map_hash");
    const auto* pageView = std::get_if<mfd::SetPageViewCommand>(&pageViewBatch->commands.front());
    ASSERT_NE(pageView, nullptr);
    EXPECT_EQ(pageView->pageId, 11U);
    EXPECT_TRUE(pageView->page.empty());
    EXPECT_FLOAT_EQ(pageView->view.center.x, 0.25f);
    EXPECT_FLOAT_EQ(pageView->view.center.y, -0.5f);
    EXPECT_FLOAT_EQ(pageView->view.zoom, 1.75f);

    const auto strobeActiveBatch = DecodeBatchPayload(rawChannel->SentPayloads()[2]);
    ASSERT_TRUE(strobeActiveBatch.has_value());
    EXPECT_EQ(strobeActiveBatch->mappingHash, "map_hash");
    const auto* strobeActive = std::get_if<mfd::UpdateStrobeCommand>(&strobeActiveBatch->commands.front());
    ASSERT_NE(strobeActive, nullptr);
    EXPECT_EQ(strobeActive->pageId, 11U);
    EXPECT_TRUE(strobeActive->page.empty());
    ASSERT_TRUE(strobeActive->active.has_value());
    EXPECT_TRUE(*strobeActive->active);
    EXPECT_FALSE(strobeActive->position.has_value());

    const auto strobePositionBatch = DecodeBatchPayload(rawChannel->SentPayloads()[3]);
    ASSERT_TRUE(strobePositionBatch.has_value());
    EXPECT_EQ(strobePositionBatch->mappingHash, "map_hash");
    const auto* strobePosition = std::get_if<mfd::UpdateStrobeCommand>(&strobePositionBatch->commands.front());
    ASSERT_NE(strobePosition, nullptr);
    EXPECT_EQ(strobePosition->pageId, 11U);
    EXPECT_TRUE(strobePosition->page.empty());
    EXPECT_FALSE(strobePosition->active.has_value());
    ASSERT_TRUE(strobePosition->position.has_value());
    EXPECT_FLOAT_EQ(strobePosition->position->x, 0.1f);
    EXPECT_FLOAT_EQ(strobePosition->position->y, -0.2f);

    const auto setVisibilityBatch = DecodeBatchPayload(rawChannel->SentPayloads()[4]);
    ASSERT_TRUE(setVisibilityBatch.has_value());
    EXPECT_EQ(setVisibilityBatch->mappingHash, "map_hash");
    const auto* setVisibility =
        std::get_if<mfd::SetDynamicReticleSetVisibilityCommand>(&setVisibilityBatch->commands.front());
    ASSERT_NE(setVisibility, nullptr);
    EXPECT_EQ(setVisibility->pageId, 11U);
    EXPECT_EQ(setVisibility->templateTransportId, 55U);
    EXPECT_TRUE(setVisibility->page.empty());
    EXPECT_TRUE(setVisibility->templateId.empty());
    EXPECT_FALSE(setVisibility->visible);
}

TEST(CommandClientTests, WindowDisplayHelpersSendWithoutTransportMap)
{
    auto channel = std::make_unique<CapturingExchangeChannel>();
    CapturingExchangeChannel* const rawChannel = channel.get();

    mfd::CommandClient client(std::move(channel));
    ASSERT_TRUE(client.IsReady());

    ASSERT_TRUE(client.SetWindowColorInverted(true));
    ASSERT_TRUE(client.SetWindowBrightness(0.55f));
    ASSERT_TRUE(client.SetWindowDisabled(true));
    ASSERT_EQ(rawChannel->SentPayloads().size(), 3U);

    const auto invertBatch = DecodeBatchPayload(rawChannel->SentPayloads()[0]);
    ASSERT_TRUE(invertBatch.has_value());
    EXPECT_TRUE(invertBatch->mappingHash.empty());
    const auto* invert = std::get_if<mfd::UpdateWindowDisplayCommand>(&invertBatch->commands.front());
    ASSERT_NE(invert, nullptr);
    ASSERT_TRUE(invert->patch.invertColors.has_value());
    EXPECT_TRUE(*invert->patch.invertColors);

    const auto brightnessBatch = DecodeBatchPayload(rawChannel->SentPayloads()[1]);
    ASSERT_TRUE(brightnessBatch.has_value());
    const auto* brightness = std::get_if<mfd::UpdateWindowDisplayCommand>(&brightnessBatch->commands.front());
    ASSERT_NE(brightness, nullptr);
    ASSERT_TRUE(brightness->patch.brightness.has_value());
    EXPECT_FLOAT_EQ(*brightness->patch.brightness, 0.55f);

    const auto disabledBatch = DecodeBatchPayload(rawChannel->SentPayloads()[2]);
    ASSERT_TRUE(disabledBatch.has_value());
    const auto* disabled = std::get_if<mfd::UpdateWindowDisplayCommand>(&disabledBatch->commands.front());
    ASSERT_NE(disabled, nullptr);
    ASSERT_TRUE(disabled->patch.disabled.has_value());
    EXPECT_TRUE(*disabled->patch.disabled);
}

TEST(CommandClientTests, GeneratedPageHelpersCarryMappingHashWithoutTransportMap)
{
    auto channel = std::make_unique<CapturingExchangeChannel>();
    CapturingExchangeChannel* const rawChannel = channel.get();

    mfd::CommandClient client(std::move(channel));
    ASSERT_TRUE(client.IsReady());

    const GeneratedRadarPage radarPage;
    ASSERT_TRUE(client.ActivatePage(radarPage)) << client.LastError();
    ASSERT_TRUE(client.SetPageView(radarPage, {0.2f, -0.3f}, 1.4f)) << client.LastError();
    ASSERT_EQ(rawChannel->SentPayloads().size(), 2U);

    const auto activateBatch = DecodeBatchPayload(rawChannel->SentPayloads()[0]);
    ASSERT_TRUE(activateBatch.has_value());
    EXPECT_EQ(activateBatch->mappingHash, "map_hash");
    const auto* activate = std::get_if<mfd::ActivatePageCommand>(&activateBatch->commands.front());
    ASSERT_NE(activate, nullptr);
    EXPECT_EQ(activate->pageId, 11U);
    EXPECT_TRUE(activate->page.empty());

    const auto pageViewBatch = DecodeBatchPayload(rawChannel->SentPayloads()[1]);
    ASSERT_TRUE(pageViewBatch.has_value());
    EXPECT_EQ(pageViewBatch->mappingHash, "map_hash");
    const auto* pageView = std::get_if<mfd::SetPageViewCommand>(&pageViewBatch->commands.front());
    ASSERT_NE(pageView, nullptr);
    EXPECT_EQ(pageView->pageId, 11U);
    EXPECT_TRUE(pageView->page.empty());
    EXPECT_FLOAT_EQ(pageView->view.center.x, 0.2f);
    EXPECT_FLOAT_EQ(pageView->view.center.y, -0.3f);
    EXPECT_FLOAT_EQ(pageView->view.zoom, 1.4f);
}

TEST(CommandClientTests, GeneratedPageHelpersRejectStaleMappingHashWhenTransportMapIsConfigured)
{
    auto channel = std::make_unique<CapturingExchangeChannel>();
    mfd::CommandClient client(std::move(channel), MakeTransportMap());
    ASSERT_TRUE(client.IsReady());

    const StaleGeneratedRadarPage radarPage;
    EXPECT_FALSE(client.ActivatePage(radarPage));
    EXPECT_NE(client.LastError().find("mappingHash does not match"), std::string::npos);

    EXPECT_FALSE(client.SetPageView(radarPage, {0.0f, 0.0f}, 1.0f));
    EXPECT_NE(client.LastError().find("mappingHash does not match"), std::string::npos);
}

TEST(CommandClientTests, NameBasedPageHelpersRequireConfiguredTransportMap)
{
    auto channel = std::make_unique<CapturingExchangeChannel>();
    mfd::CommandClient client(std::move(channel));
    ASSERT_TRUE(client.IsReady());

    EXPECT_FALSE(client.ActivatePage("Radar"));
    EXPECT_NE(client.LastError().find("generated transport map"), std::string::npos);

    EXPECT_FALSE(client.SetStrobeActive("Radar", true));
    EXPECT_NE(client.LastError().find("generated transport map"), std::string::npos);
}

TEST(CommandClientTests, MaxPayloadBytesUsesSharedUdpBounds)
{
    mfd::WindowUdpCommandTransport smallConfig;
    smallConfig.enabled = true;
    smallConfig.port = 47220U;
    smallConfig.maxPacketSize = 1U;

    mfd::CommandClient smallClient(smallConfig);
    EXPECT_EQ(smallClient.MaxPayloadBytes(), mfd::kUdpMinPayloadBytes);

    mfd::WindowUdpCommandTransport largeConfig;
    largeConfig.enabled = true;
    largeConfig.port = 47220U;
    largeConfig.maxPacketSize = mfd::kUdpMaxPayloadBytes + 1024U;

    mfd::CommandClient largeClient(largeConfig);
    EXPECT_EQ(largeClient.MaxPayloadBytes(), mfd::kUdpMaxPayloadBytes);
}

TEST(CommandClientTests, RejectsBulkDynamicReticleUpdatesBeyondRuntimeSafetyLimits)
{
    auto channel = std::make_unique<CapturingExchangeChannel>();
    mfd::CommandClient client(std::move(channel), MakeTransportMap());
    ASSERT_TRUE(client.IsReady());

    mfd::UpsertDynamicReticlesCommand command;
    command.page = "Radar";
    command.templateId = "radar_track";
    command.reticles.reserve(4097U);
    for (std::size_t index = 0; index < 4097U; ++index)
    {
        mfd::DynamicReticleState state;
        state.reticleId = "track_" + std::to_string(index);
        state.patch.visible = true;
        command.reticles.push_back(std::move(state));
    }

    mfd::CommandBatch batch;
    batch.commands.push_back(std::move(command));

    EXPECT_FALSE(client.SendBatch(batch));
    EXPECT_NE(client.LastError().find("safety limits"), std::string::npos);
}

/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for CommandProcessor.
 */

#include <gtest/gtest.h>

#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "mfd/control/CommandProcessor.h"
#include "mfd/control/CommandTypes.h"
#include "mfd/ipc/ExchangeChannel.h"
#include "mfd/model/PageDefinition.h"
#include "mfd/runtime/SceneRegistry.h"

namespace
{
constexpr std::string_view kDefaultLayerId = mfd::kDefaultPageLayerId;

class ScriptedExchangeChannel final : public mfd::IExchangeChannel
{
public:
    bool IsReady() const noexcept override
    {
        return true;
    }

    bool Send(const mfd::ByteView) override
    {
        return true;
    }

    std::optional<std::vector<std::byte>> TryReceive() override
    {
        if (payloads_.empty())
        {
            return std::nullopt;
        }

        auto payload = std::move(payloads_.front());
        payloads_.pop_front();
        return payload;
    }

    std::string LastError() const override
    {
        return lastError_;
    }

    void PushPayload(std::vector<std::byte> payload)
    {
        payloads_.push_back(std::move(payload));
    }

    void SetLastError(std::string error)
    {
        lastError_ = std::move(error);
    }

private:
    std::deque<std::vector<std::byte>> payloads_ {};
    std::string lastError_ {};
};

std::vector<std::byte> ToBytes(const std::string& payload)
{
    const auto* first = reinterpret_cast<const std::byte*>(payload.data());
    return std::vector<std::byte>(first, first + payload.size());
}

mfd::SceneRegistry MakeRegistry()
{
    mfd::PageDefinition page;
    page.name = "Radar";
    page.normalizedName = "radar";
    page.title = "Radar";
    page.layers.push_back(mfd::PageLayerDefinition {std::string(kDefaultLayerId)});

    mfd::ReticleGroup reticle;
    reticle.id = "heading_box";
    reticle.layerId = std::string(kDefaultLayerId);

    mfd::Primitive primitive;
    primitive.id = "heading_value";
    primitive.type = mfd::PrimitiveType::Text;
    primitive.geometry = mfd::TextGeometry {"000", 0.04f, 0.002f};
    reticle.primitives.push_back(std::move(primitive));
    page.staticReticles.push_back(std::move(reticle));

    page.blinkTypes.push_back({"slow", "slow", 750U});

    mfd::MfdDocument document;
    document.pages.push_back(std::move(page));

    mfd::GeneratedTransportMap map;
    map.mappingHash = "map_hash";
    map.pages.push_back({11U, "Radar", "radar", false, false});
    map.reticles.push_back({22U, 11U, "heading_box", "heading_box", "static"});
    map.primitives.push_back(
        {33U, mfd::TransportPrimitiveOwnerKind::Reticle, 22U, "heading_value", "heading_value", "text", true});
    map.blinkTypes.push_back({44U, 11U, "slow", "slow", 750U});

    return mfd::SceneRegistry(std::move(document), std::move(map));
}

mfd::SceneRegistry MakeRuntimeRegistry()
{
    mfd::PageDefinition page;
    page.name = "Radar";
    page.normalizedName = "radar";
    page.title = "Radar";
    page.defaultPage = true;
    page.layers.push_back(mfd::PageLayerDefinition {std::string(kDefaultLayerId)});
    page.dynamicReticleBindings.push_back(
        mfd::DynamicReticleLayerBinding {"radar_track", std::string(kDefaultLayerId), 0U});

    mfd::PageStrobeDefinition strobe;
    strobe.reticle.id = "strobe";
    strobe.capture.shape = mfd::StrobeCaptureShape::Circle;
    strobe.capture.radius = 0.12f;
    strobe.magnet.enabled = true;
    strobe.magnet.radius = 0.15f;
    strobe.magnet.strength = 1.0f;
    page.strobe = strobe;

    mfd::ReticleGroup templateReticle;
    templateReticle.id = "radar_track";

    mfd::Primitive label;
    label.id = "track_label";
    label.type = mfd::PrimitiveType::Text;
    label.geometry = mfd::TextGeometry {"T", 0.04f, 0.002f};
    templateReticle.primitives.push_back(std::move(label));

    mfd::MfdDocument document;
    document.pages.push_back(std::move(page));
    document.reticleLibrary.emplace("radar_track", templateReticle);

    mfd::GeneratedTransportMap map;
    map.mappingHash = "map_hash";
    map.pages.push_back({11U, "Radar", "radar", true, true});
    map.templates.push_back({55U, "radar_track", "radar_track"});

    return mfd::SceneRegistry(std::move(document), std::move(map));
}
} // namespace

TEST(CommandProcessorTests, PollDoesNotOverrideSuccessfulDispatchWithStickyChannelError)
{
    mfd::SceneRegistry registry = MakeRegistry();
    mfd::CommandProcessor processor(registry);
    ScriptedExchangeChannel channel;

    mfd::CommandBatch batch;
    batch.mappingHash = "map_hash";
    batch.commands.push_back(mfd::ActivatePageCommand {"Radar", 11U});
    channel.PushPayload(ToBytes(mfd::SerializeCommandBatch(batch)));
    channel.SetLastError("stale transport error");

    EXPECT_TRUE(processor.Poll(channel));
    EXPECT_TRUE(processor.LastError().empty());
}

TEST(CommandProcessorTests, PollReportsTransportErrorWhenNoPayloadWasProcessed)
{
    mfd::SceneRegistry registry = MakeRegistry();
    mfd::CommandProcessor processor(registry);
    ScriptedExchangeChannel channel;
    channel.SetLastError("transport unavailable");

    EXPECT_FALSE(processor.Poll(channel));
    EXPECT_EQ(processor.LastError(), "transport unavailable");
}

TEST(CommandProcessorTests, SubmitsIdBasedBatchWhenMappingHashMatchesLoadedTransportMap)
{
    mfd::SceneRegistry registry = MakeRegistry();
    mfd::CommandProcessor processor(registry);

    mfd::PrimitivePatch primitivePatch;
    primitivePatch.text = "123";

    mfd::ReticlePatch patch;
    patch.blinkTypeId = 44U;
    patch.primitivePatchesById.emplace(33U, primitivePatch);

    mfd::CommandBatch batch;
    batch.mappingHash = "map_hash";
    batch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"", "", 11U, 22U}, patch});

    EXPECT_TRUE(processor.Submit(batch));
    EXPECT_TRUE(processor.LastError().empty());

    const auto reticles = registry.CollectPageReticlePointers("Radar");
    ASSERT_EQ(reticles.size(), 1U);
    const auto* text = std::get_if<mfd::TextGeometry>(&reticles.front()->primitives.front().geometry);
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(text->text, "123");
    EXPECT_TRUE(reticles.front()->blink.enabled);
    EXPECT_EQ(reticles.front()->blink.typeName, "slow");
}

TEST(CommandProcessorTests, RejectsIdBasedBatchWhenMappingHashDoesNotMatchLoadedTransportMap)
{
    mfd::SceneRegistry registry = MakeRegistry();
    mfd::CommandProcessor processor(registry);

    mfd::CommandBatch batch;
    batch.mappingHash = "other_hash";
    batch.commands.push_back(
        mfd::ActivatePageCommand {"", 11U});

    EXPECT_FALSE(processor.Submit(batch));
    EXPECT_EQ(processor.LastError(), "Generated transport map hash mismatch between the client batch and the runtime window");
}

TEST(CommandProcessorTests, RejectsSerializedGeneratedTransportIdsWithoutMappingHash)
{
    mfd::SceneRegistry registry = MakeRegistry();
    mfd::CommandProcessor processor(registry);

    mfd::ActivatePageCommand command;
    command.page = "Radar";
    command.pageId = 11U;

    const std::string payload = mfd::SerializeUserCommand(command);

    EXPECT_FALSE(processor.Submit(payload));
    EXPECT_EQ(processor.LastError(), "Generated transport ids require a non-empty batch mapping hash");
}

TEST(CommandProcessorTests, RejectsUnknownGeneratedPrimitiveTransportIdInStaticPatch)
{
    mfd::SceneRegistry registry = MakeRegistry();
    mfd::CommandProcessor processor(registry);

    mfd::ReticlePatch patch;
    patch.textsById.emplace(999U, "BAD");

    mfd::CommandBatch batch;
    batch.mappingHash = "map_hash";
    batch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"", "", 11U, 22U}, patch});

    EXPECT_FALSE(processor.Submit(batch));
    EXPECT_EQ(processor.LastError(), "Unknown generated primitive transport id 999");

    const auto reticles = registry.CollectPageReticlePointers("Radar");
    ASSERT_EQ(reticles.size(), 1U);
    const auto* text = std::get_if<mfd::TextGeometry>(&reticles.front()->primitives.front().geometry);
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(text->text, "000");
}

TEST(CommandProcessorTests, RejectsConflictingNamedAndGeneratedPrimitiveTextOverrides)
{
    mfd::SceneRegistry registry = MakeRegistry();
    mfd::CommandProcessor processor(registry);

    mfd::ReticlePatch patch;
    patch.texts.emplace("heading_value", "OLD");
    patch.textsById.emplace(33U, "NEW");

    mfd::CommandBatch batch;
    batch.mappingHash = "map_hash";
    batch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"", "", 11U, 22U}, patch});

    EXPECT_FALSE(processor.Submit(batch));
    EXPECT_EQ(
        processor.LastError(),
        "Generated primitive transport id 33 conflicts with an explicit primitive text override");

    const auto reticles = registry.CollectPageReticlePointers("Radar");
    ASSERT_EQ(reticles.size(), 1U);
    const auto* text = std::get_if<mfd::TextGeometry>(&reticles.front()->primitives.front().geometry);
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(text->text, "000");
}

TEST(CommandProcessorTests, SerializedDynamicRuntimeIdsStillMagnetizeActiveStrobe)
{
    mfd::SceneRegistry registry = MakeRuntimeRegistry();
    mfd::CommandProcessor processor(registry);

    mfd::UpsertDynamicReticleCommand upsertCommand;
    upsertCommand.target.pageId = 11U;
    upsertCommand.target.runtimeReticleId = 9001U;
    upsertCommand.templateTransportId = 55U;
    upsertCommand.patch.visible = true;
    upsertCommand.patch.position = mfd::Vec2 {0.1f, 0.0f};

    mfd::CommandBatch dynamicBatch;
    dynamicBatch.mappingHash = "map_hash";
    dynamicBatch.commands.push_back(upsertCommand);
    EXPECT_TRUE(processor.Submit(mfd::SerializeCommandBatch(dynamicBatch)));
    EXPECT_TRUE(processor.LastError().empty());

    mfd::UpdateStrobeCommand strobeCommand;
    strobeCommand.pageId = 11U;
    strobeCommand.active = true;
    strobeCommand.position = mfd::Vec2 {0.08f, 0.02f};

    mfd::CommandBatch strobeBatch;
    strobeBatch.mappingHash = "map_hash";
    strobeBatch.commands.push_back(strobeCommand);
    EXPECT_TRUE(processor.Submit(mfd::SerializeCommandBatch(strobeBatch)));
    EXPECT_TRUE(processor.LastError().empty());

    const auto strobeSummary = registry.ActiveStrobeSummary();
    ASSERT_TRUE(strobeSummary.has_value());
    EXPECT_EQ(strobeSummary->pageId, 11U);
    EXPECT_TRUE(strobeSummary->visible);
    EXPECT_FLOAT_EQ(strobeSummary->position.x, 0.1f);
    EXPECT_FLOAT_EQ(strobeSummary->position.y, 0.0f);

    const auto magnet = registry.ActiveStrobeMagnetSummary();
    ASSERT_TRUE(magnet.has_value());
    EXPECT_TRUE(magnet->enabled);
    EXPECT_TRUE(magnet->magnetized);
    EXPECT_EQ(magnet->runtimeReticleId, 9001U);
    EXPECT_EQ(magnet->reticleId, "__runtime_dynamic_9001");
    EXPECT_FLOAT_EQ(magnet->targetPosition.x, 0.1f);
    EXPECT_FLOAT_EQ(magnet->targetPosition.y, 0.0f);

    const auto capture = registry.CaptureActivePageStrobe();
    ASSERT_TRUE(capture.has_value());
    EXPECT_EQ(capture->pageId, 11U);
    EXPECT_EQ(capture->runtimeReticleId, 9001U);
    EXPECT_EQ(capture->sourceTemplateTransportId, 55U);
    EXPECT_EQ(capture->reticleId, "__runtime_dynamic_9001");
}

TEST(CommandProcessorTests, BulkDynamicRadarBatchSupportsOneHundredTracks)
{
    mfd::SceneRegistry registry = MakeRuntimeRegistry();
    mfd::CommandProcessor processor(registry);

    mfd::UpsertDynamicReticlesCommand command;
    command.pageId = 11U;
    command.templateTransportId = 55U;
    command.reticles.reserve(100U);

    for (std::uint32_t index = 0; index < 100U; ++index)
    {
        mfd::ReticlePatch patch;
        patch.visible = true;
        patch.position = mfd::Vec2 {
            -0.95f + 0.019f * static_cast<float>(index),
            -0.60f + 0.012f * static_cast<float>(index % 50U)};
        patch.text = "T" + std::to_string(index);

        mfd::DynamicReticleState state;
        state.reticleId = "track_" + std::to_string(index);
        state.runtimeReticleId = 1000U + index;
        state.patch = std::move(patch);
        command.reticles.push_back(std::move(state));
    }

    mfd::CommandBatch batch;
    batch.mappingHash = "map_hash";
    batch.commands.push_back(std::move(command));

    EXPECT_TRUE(processor.Submit(mfd::SerializeCommandBatch(batch)));
    EXPECT_TRUE(processor.LastError().empty());

    const auto reticles = registry.CollectPageReticlePointers("Radar");
    std::size_t dynamicTrackCount = 0;
    for (const mfd::ReticleGroup* reticle : reticles)
    {
        if (reticle != nullptr && reticle->id.rfind("__runtime_dynamic_", 0U) == 0U)
        {
            ++dynamicTrackCount;
        }
    }

    EXPECT_EQ(reticles.size(), 101U);
    EXPECT_EQ(dynamicTrackCount, 100U);
}

TEST(CommandProcessorTests, BatchRollsBackEarlierMutationsWhenALaterCommandFails)
{
    mfd::SceneRegistry registry = MakeRegistry();
    mfd::CommandProcessor processor(registry);

    mfd::ReticlePatch firstPatch;
    firstPatch.text = "111";

    mfd::ReticlePatch thirdPatch;
    thirdPatch.text = "222";

    mfd::CommandBatch batch;
    batch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"Radar", "heading_box"}, firstPatch});
    batch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"Radar", "missing_reticle"}, {}});
    batch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"Radar", "heading_box"}, thirdPatch});

    EXPECT_FALSE(processor.Submit(batch));
    EXPECT_NE(processor.LastError().find("missing_reticle"), std::string::npos);

    const auto reticles = registry.CollectPageReticlePointers("Radar");
    ASSERT_EQ(reticles.size(), 1U);
    const auto* text = std::get_if<mfd::TextGeometry>(&reticles.front()->primitives.front().geometry);
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(text->text, "000");
}

TEST(CommandProcessorTests, ArrayViewSubmissionStopsOnFirstFailureAndKeepsDiagnostic)
{
    mfd::SceneRegistry registry = MakeRegistry();
    mfd::CommandProcessor processor(registry);

    mfd::ReticlePatch patch;
    patch.text = "999";

    std::vector<mfd::UserCommand> commands;
    commands.emplace_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"Radar", "missing_reticle"}, {}});
    commands.emplace_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"Radar", "heading_box"}, patch});

    EXPECT_FALSE(processor.Submit(mfd::ArrayView<const mfd::UserCommand>(commands)));
    EXPECT_NE(processor.LastError().find("missing_reticle"), std::string::npos);

    const auto reticles = registry.CollectPageReticlePointers("Radar");
    ASSERT_EQ(reticles.size(), 1U);
    const auto* text = std::get_if<mfd::TextGeometry>(&reticles.front()->primitives.front().geometry);
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(text->text, "000");
}

TEST(CommandProcessorTests, RejectsDuplicateOrOutOfOrderSequencedBatches)
{
    mfd::SceneRegistry registry = MakeRegistry();
    mfd::CommandProcessor processor(registry);

    mfd::CommandBatch firstBatch;
    firstBatch.mappingHash = "map_hash";
    firstBatch.sequence = 7U;
    firstBatch.commands.push_back(mfd::ActivatePageCommand {"", 11U});

    mfd::CommandBatch duplicateBatch = firstBatch;
    mfd::CommandBatch olderBatch = firstBatch;
    olderBatch.sequence = 6U;

    EXPECT_TRUE(processor.Submit(firstBatch));
    EXPECT_TRUE(processor.LastError().empty());

    EXPECT_FALSE(processor.Submit(duplicateBatch));
    EXPECT_EQ(processor.LastError(), "Dropped stale or duplicate command batch");

    EXPECT_FALSE(processor.Submit(olderBatch));
    EXPECT_EQ(processor.LastError(), "Dropped stale or duplicate command batch");
}

TEST(CommandProcessorTests, AllowsSameSequenceAcrossNameBasedBatchesWithoutMappingHash)
{
    mfd::SceneRegistry registry = MakeRegistry();
    mfd::CommandProcessor processor(registry);

    mfd::CommandBatch firstBatch;
    firstBatch.sequence = 1U;
    firstBatch.commands.push_back(mfd::ActivatePageCommand {"Radar", 0U});

    mfd::ReticlePatch patch;
    patch.text = "321";

    mfd::CommandBatch secondBatch;
    secondBatch.sequence = 1U;
    secondBatch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"Radar", "heading_box"}, patch});

    EXPECT_TRUE(processor.Submit(firstBatch));
    EXPECT_TRUE(processor.LastError().empty());

    EXPECT_TRUE(processor.Submit(secondBatch));
    EXPECT_TRUE(processor.LastError().empty());

    const auto reticles = registry.CollectPageReticlePointers("Radar");
    ASSERT_EQ(reticles.size(), 1U);
    const auto* text = std::get_if<mfd::TextGeometry>(&reticles.front()->primitives.front().geometry);
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(text->text, "321");
}

TEST(CommandProcessorTests, ArrayViewSubmissionRollsBackEarlierMutationsWhenALaterCommandFails)
{
    mfd::SceneRegistry registry = MakeRegistry();
    mfd::CommandProcessor processor(registry);

    mfd::ReticlePatch firstPatch;
    firstPatch.text = "777";

    std::vector<mfd::UserCommand> commands;
    commands.emplace_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"Radar", "heading_box"}, firstPatch});
    commands.emplace_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"Radar", "missing_reticle"}, {}});

    EXPECT_FALSE(processor.Submit(mfd::ArrayView<const mfd::UserCommand>(commands)));
    EXPECT_NE(processor.LastError().find("missing_reticle"), std::string::npos);

    const auto reticles = registry.CollectPageReticlePointers("Radar");
    ASSERT_EQ(reticles.size(), 1U);
    const auto* text = std::get_if<mfd::TextGeometry>(&reticles.front()->primitives.front().geometry);
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(text->text, "000");
}

TEST(CommandProcessorTests, ResetToInitialStatePreservesGeneratedTransportMapLookups)
{
    mfd::SceneRegistry registry = MakeRegistry();
    mfd::CommandProcessor processor(registry);

    registry.ResetToInitialState();
    ASSERT_TRUE(registry.HasTransportMap());

    mfd::PrimitivePatch primitivePatch;
    primitivePatch.text = "456";

    mfd::ReticlePatch patch;
    patch.primitivePatchesById.emplace(33U, primitivePatch);

    mfd::CommandBatch batch;
    batch.mappingHash = "map_hash";
    batch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"", "", 11U, 22U}, patch});

    EXPECT_TRUE(processor.Submit(batch));
    EXPECT_TRUE(processor.LastError().empty());

    const auto reticles = registry.CollectPageReticlePointers("Radar");
    ASSERT_EQ(reticles.size(), 1U);
    const auto* text = std::get_if<mfd::TextGeometry>(&reticles.front()->primitives.front().geometry);
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(text->text, "456");
}

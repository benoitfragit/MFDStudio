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
#include <limits>
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

mfd::MfdDocument MakeStaticRegistryDocument()
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

    return document;
}

mfd::GeneratedTransportMap MakeStaticTransportMap()
{
    mfd::GeneratedTransportMap map;
    map.mappingHash = "map_hash";
    map.pages.push_back({11U, "Radar", "radar", false, false});
    map.reticles.push_back({22U, 11U, "heading_box", "heading_box", "static"});
    map.primitives.push_back(
        {33U, mfd::TransportPrimitiveOwnerKind::Reticle, 22U, "heading_value", "heading_value", "text", true});
    map.blinkTypes.push_back({44U, 11U, "slow", "slow", 750U});

    return map;
}

mfd::SceneRegistry MakeRegistry()
{
    mfd::MfdDocument document = MakeStaticRegistryDocument();
    mfd::GeneratedTransportMap map = MakeStaticTransportMap();

    return mfd::SceneRegistry(std::move(document), std::move(map));
}

mfd::SceneRegistry MakeRegistryWithoutTransportMap()
{
    return mfd::SceneRegistry(MakeStaticRegistryDocument());
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
    strobe.name = "Default";
    strobe.normalizedName = "default";
    strobe.reticle.id = "strobe";
    strobe.capture.shape = mfd::StrobeCaptureShape::Circle;
    strobe.capture.radius = 0.12f;
    strobe.magnet.enabled = true;
    strobe.magnet.radius = 0.15f;
    strobe.magnet.strength = 1.0f;
    page.strobes.push_back(strobe);
    page.activeStrobeName = "Default";
    page.normalizedActiveStrobeName = "default";

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
    map.strobes.push_back({101U, 11U, "Default", "default", "strobe", true});
    map.templates.push_back({55U, "radar_track", "radar_track"});

    return mfd::SceneRegistry(std::move(document), std::move(map));
}

mfd::SceneRegistry MakeMultiStrobeRuntimeRegistry()
{
    mfd::PageDefinition page;
    page.name = "Radar";
    page.normalizedName = "radar";
    page.title = "Radar";
    page.defaultPage = true;
    page.layers.push_back(mfd::PageLayerDefinition {std::string(kDefaultLayerId)});

    mfd::PageStrobeDefinition defaultStrobe;
    defaultStrobe.name = "Default";
    defaultStrobe.normalizedName = "default";
    defaultStrobe.reticle.id = "strobe_default";
    defaultStrobe.capture.shape = mfd::StrobeCaptureShape::Circle;
    defaultStrobe.capture.radius = 0.12f;

    mfd::PageStrobeDefinition alternateStrobe;
    alternateStrobe.name = "Strobe1";
    alternateStrobe.normalizedName = "strobe1";
    alternateStrobe.reticle.id = "strobe_alt";
    alternateStrobe.reticle.transform.position = mfd::Vec2 {0.32f, -0.14f};
    alternateStrobe.capture.shape = mfd::StrobeCaptureShape::Circle;
    alternateStrobe.capture.radius = 0.12f;

    page.strobes.push_back(std::move(defaultStrobe));
    page.strobes.push_back(std::move(alternateStrobe));
    page.activeStrobeName = "Default";
    page.normalizedActiveStrobeName = "default";

    mfd::ReticleGroup reticle;
    reticle.id = "heading_box";
    reticle.layerId = std::string(kDefaultLayerId);

    mfd::Primitive primitive;
    primitive.id = "heading_value";
    primitive.type = mfd::PrimitiveType::Text;
    primitive.geometry = mfd::TextGeometry {"000", 0.04f, 0.002f};
    reticle.primitives.push_back(std::move(primitive));
    page.staticReticles.push_back(std::move(reticle));

    mfd::MfdDocument document;
    document.pages.push_back(std::move(page));

    mfd::GeneratedTransportMap map;
    map.mappingHash = "map_hash";
    map.pages.push_back({11U, "Radar", "radar", true, true});
    map.reticles.push_back({22U, 11U, "heading_box", "heading_box", "static"});
    map.primitives.push_back(
        {33U, mfd::TransportPrimitiveOwnerKind::Reticle, 22U, "heading_value", "heading_value", "text", true});
    map.strobes.push_back({101U, 11U, "Default", "default", "strobe_default", true});
    map.strobes.push_back({102U, 11U, "Strobe1", "strobe1", "strobe_alt", false});

    return mfd::SceneRegistry(std::move(document), std::move(map));
}

std::size_t CountRuntimeDynamicReticles(const mfd::SceneRegistry& registry, const std::string_view pageName)
{
    std::size_t dynamicTrackCount = 0;
    for (const mfd::ReticleGroup* reticle : registry.CollectPageReticlePointers(pageName))
    {
        if (reticle != nullptr && reticle->id.rfind("__runtime_dynamic_", 0U) == 0U)
        {
            ++dynamicTrackCount;
        }
    }

    return dynamicTrackCount;
}

mfd::UpsertDynamicReticleCommand MakeDynamicTrackUpsertCommand()
{
    mfd::UpsertDynamicReticleCommand command;
    command.target.pageId = 11U;
    command.target.runtimeReticleId = 9001U;
    command.templateTransportId = 55U;
    command.patch.visible = true;
    command.patch.position = mfd::Vec2 {0.1f, -0.2f};
    command.patch.text = "T1";
    return command;
}

mfd::SceneRegistry MakeTwoPageRegistry()
{
    mfd::MfdDocument document = MakeStaticRegistryDocument();

    mfd::PageDefinition navPage;
    navPage.name = "Nav";
    navPage.normalizedName = "nav";
    navPage.title = "Nav";
    navPage.layers.push_back(mfd::PageLayerDefinition {std::string(kDefaultLayerId)});

    mfd::ReticleGroup navReticle;
    navReticle.id = "nav_box";
    navReticle.layerId = std::string(kDefaultLayerId);

    mfd::Primitive navPrimitive;
    navPrimitive.id = "nav_value";
    navPrimitive.type = mfd::PrimitiveType::Text;
    navPrimitive.geometry = mfd::TextGeometry {"NAV", 0.04f, 0.002f};
    navReticle.primitives.push_back(std::move(navPrimitive));
    navPage.staticReticles.push_back(std::move(navReticle));
    document.pages.push_back(std::move(navPage));

    return mfd::SceneRegistry(std::move(document));
}

std::string ReadFirstReticleText(const mfd::SceneRegistry& registry, const std::string_view pageName)
{
    const auto reticles = registry.CollectPageReticlePointers(pageName);
    if (reticles.empty() || reticles.front() == nullptr || reticles.front()->primitives.empty())
    {
        return {};
    }

    const auto* text = std::get_if<mfd::TextGeometry>(&reticles.front()->primitives.front().geometry);
    return text == nullptr ? std::string {} : text->text;
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

TEST(CommandProcessorTests, RejectsIdBasedBatchWhenRuntimeDidNotLoadTheMatchingGeneratedMap)
{
    mfd::SceneRegistry registry = MakeRegistryWithoutTransportMap();
    mfd::CommandProcessor processor(registry);

    mfd::CommandBatch batch;
    batch.mappingHash = "map_hash";
    batch.commands.push_back(mfd::ActivatePageCommand {"", 11U});

    EXPECT_FALSE(processor.Submit(batch));
    EXPECT_EQ(
        processor.LastError(),
        "Client generated API requires the matching generated transport map loaded by the runtime window");
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
    EXPECT_TRUE(registry.SetDynamicReticleSetStrobeMagnetEnabled("Radar", "radar_track", true));

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

TEST(CommandProcessorTests, UpdateStrobeRejectsUnknownNamedStrobeWithoutMutatingCurrentStrobe)
{
    mfd::SceneRegistry registry = MakeRuntimeRegistry();
    mfd::CommandProcessor processor(registry);

    const auto before = registry.ActiveStrobeSummary();
    ASSERT_TRUE(before.has_value());

    mfd::UpdateStrobeCommand command;
    command.page = "Radar";
    command.strobe = "Ghost";
    command.active = false;
    command.position = mfd::Vec2 {0.25f, -0.40f};

    EXPECT_FALSE(processor.Submit(command));
    EXPECT_EQ(processor.LastError(), "Unable to update strobe on page 'Radar'");

    const auto after = registry.ActiveStrobeSummary();
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(after->strobeName, before->strobeName);
    EXPECT_EQ(after->reticleId, before->reticleId);
    EXPECT_EQ(after->visible, before->visible);
    EXPECT_FLOAT_EQ(after->position.x, before->position.x);
    EXPECT_FLOAT_EQ(after->position.y, before->position.y);
}

TEST(CommandProcessorTests, UpdateStrobeRollsBackSelectionWhenPositionIsInvalid)
{
    mfd::SceneRegistry registry = MakeMultiStrobeRuntimeRegistry();
    mfd::CommandProcessor processor(registry);

    mfd::UpdateStrobeCommand seed;
    seed.page = "Radar";
    seed.active = false;
    seed.position = mfd::Vec2 {0.18f, -0.06f};
    ASSERT_TRUE(processor.Submit(seed));

    const auto before = registry.ActiveStrobeSummary();
    ASSERT_TRUE(before.has_value());
    EXPECT_EQ(before->strobeName, "Default");
    EXPECT_FALSE(before->visible);
    EXPECT_FLOAT_EQ(before->position.x, 0.18f);
    EXPECT_FLOAT_EQ(before->position.y, -0.06f);

    mfd::UpdateStrobeCommand command;
    command.page = "Radar";
    command.strobe = "Strobe1";
    command.active = true;
    command.position = mfd::Vec2 {0.0f, std::numeric_limits<float>::quiet_NaN()};

    EXPECT_FALSE(processor.Submit(command));
    EXPECT_EQ(processor.LastError(), "Unable to update strobe on page 'Radar'");

    const auto after = registry.ActiveStrobeSummary();
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(after->strobeName, before->strobeName);
    EXPECT_EQ(after->reticleId, before->reticleId);
    EXPECT_EQ(after->visible, before->visible);
    EXPECT_FLOAT_EQ(after->position.x, before->position.x);
    EXPECT_FLOAT_EQ(after->position.y, before->position.y);
}

TEST(CommandProcessorTests, UpdateStrobeResolvesGeneratedStrobeIdWhenPageNameIsProvided)
{
    mfd::SceneRegistry registry = MakeMultiStrobeRuntimeRegistry();
    mfd::CommandProcessor processor(registry);

    mfd::UpdateStrobeCommand command;
    command.page = "Radar";
    command.strobeId = 102U;

    mfd::CommandBatch batch;
    batch.mappingHash = "map_hash";
    batch.commands.push_back(command);

    EXPECT_TRUE(processor.Submit(batch));
    EXPECT_TRUE(processor.LastError().empty());

    const auto summary = registry.ActiveStrobeSummary();
    ASSERT_TRUE(summary.has_value());
    EXPECT_EQ(summary->pageId, 11U);
    EXPECT_EQ(summary->strobeTransportId, 102U);
    EXPECT_EQ(summary->strobeName, "Strobe1");
    EXPECT_EQ(summary->reticleId, "strobe_alt");
}

TEST(CommandProcessorTests, UpdateStrobeRejectsGeneratedStrobeIdWithoutMappingHash)
{
    mfd::SceneRegistry registry = MakeMultiStrobeRuntimeRegistry();
    mfd::CommandProcessor processor(registry);

    const auto before = registry.ActiveStrobeSummary();
    ASSERT_TRUE(before.has_value());

    mfd::UpdateStrobeCommand command;
    command.page = "Radar";
    command.strobeId = 102U;

    mfd::CommandBatch batch;
    batch.commands.push_back(command);

    EXPECT_FALSE(processor.Submit(batch));
    EXPECT_EQ(processor.LastError(), "Generated transport ids require a non-empty batch mapping hash");

    const auto after = registry.ActiveStrobeSummary();
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(after->strobeName, before->strobeName);
    EXPECT_EQ(after->reticleId, before->reticleId);
    EXPECT_FLOAT_EQ(after->position.x, before->position.x);
    EXPECT_FLOAT_EQ(after->position.y, before->position.y);
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

TEST(CommandProcessorTests, NonTransactionalBatchKeepsEarlierMutationsWhenALaterCommandFails)
{
    mfd::SceneRegistry registry = MakeRegistry();
    mfd::CommandProcessor processor(registry);
    processor.SetBatchTransactionMode(mfd::CommandBatchTransactionMode::NonTransactional);

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
    EXPECT_EQ(text->text, "111");
}

TEST(CommandProcessorTests, BatchRollbackRestoresPreviouslySelectedRuntimeStrobe)
{
    mfd::SceneRegistry registry = MakeMultiStrobeRuntimeRegistry();
    mfd::CommandProcessor processor(registry);

    mfd::UpdateStrobeCommand selectAlternate;
    selectAlternate.page = "Radar";
    selectAlternate.strobe = "Strobe1";
    ASSERT_TRUE(processor.Submit(selectAlternate));

    const auto before = registry.ActiveStrobeSummary();
    ASSERT_TRUE(before.has_value());
    EXPECT_EQ(before->strobeName, "Strobe1");
    EXPECT_EQ(before->reticleId, "strobe_alt");

    mfd::UpdateStrobeCommand selectDefault;
    selectDefault.page = "Radar";
    selectDefault.strobe = "Default";

    mfd::CommandBatch batch;
    batch.commands.push_back(selectDefault);
    batch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"Radar", "missing_reticle"}, {}});

    EXPECT_FALSE(processor.Submit(batch));
    EXPECT_NE(processor.LastError().find("missing_reticle"), std::string::npos);

    const auto after = registry.ActiveStrobeSummary();
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(after->strobeName, "Strobe1");
    EXPECT_EQ(after->reticleId, "strobe_alt");
    EXPECT_FLOAT_EQ(after->position.x, before->position.x);
    EXPECT_FLOAT_EQ(after->position.y, before->position.y);
}

TEST(CommandProcessorTests, TransactionalDynamicUpsertBatchRollsBackFailedFollowUpCommand)
{
    mfd::SceneRegistry registry = MakeRuntimeRegistry();
    mfd::CommandProcessor processor(registry);

    mfd::CommandBatch batch;
    batch.mappingHash = "map_hash";
    batch.commands.push_back(MakeDynamicTrackUpsertCommand());
    batch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"Radar", "missing_reticle"}, {}});

    EXPECT_FALSE(processor.Submit(batch));
    EXPECT_NE(processor.LastError().find("missing_reticle"), std::string::npos);
    EXPECT_EQ(CountRuntimeDynamicReticles(registry, "Radar"), 0U);
}

TEST(CommandProcessorTests, NonTransactionalDynamicUpsertBatchKeepsCreatedReticleWhenFollowUpCommandFails)
{
    mfd::SceneRegistry registry = MakeRuntimeRegistry();
    mfd::CommandProcessor processor(registry);
    processor.SetBatchTransactionMode(mfd::CommandBatchTransactionMode::NonTransactional);

    mfd::CommandBatch batch;
    batch.mappingHash = "map_hash";
    batch.commands.push_back(MakeDynamicTrackUpsertCommand());
    batch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"Radar", "missing_reticle"}, {}});

    EXPECT_FALSE(processor.Submit(batch));
    EXPECT_NE(processor.LastError().find("missing_reticle"), std::string::npos);
    EXPECT_EQ(CountRuntimeDynamicReticles(registry, "Radar"), 1U);
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

TEST(CommandProcessorTests, AcceptsDistinctSequencedBatchesWithSameSequenceToSupportChunkedPayloads)
{
    mfd::SceneRegistry registry = MakeRegistry();
    mfd::CommandProcessor processor(registry);

    mfd::CommandBatch firstChunk;
    firstChunk.mappingHash = "map_hash";
    firstChunk.sequence = 12U;
    firstChunk.commands.push_back(mfd::ActivatePageCommand {"", 11U});

    mfd::ReticlePatch patch;
    patch.text = "321";

    mfd::CommandBatch secondChunk;
    secondChunk.mappingHash = "map_hash";
    secondChunk.sequence = 12U;
    secondChunk.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"", "", 11U, 22U}, patch});

    EXPECT_TRUE(processor.Submit(firstChunk));
    EXPECT_TRUE(processor.LastError().empty());

    EXPECT_TRUE(processor.Submit(secondChunk));
    EXPECT_TRUE(processor.LastError().empty());

    const auto reticles = registry.CollectPageReticlePointers("Radar");
    ASSERT_EQ(reticles.size(), 1U);
    const auto* text = std::get_if<mfd::TextGeometry>(&reticles.front()->primitives.front().geometry);
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(text->text, "321");
}

TEST(CommandProcessorTests, BoundsRetainedFingerprintsForASingleSequence)
{
    mfd::SceneRegistry registry = MakeRegistry();
    mfd::CommandProcessor processor(registry);

    constexpr std::size_t kCap = 256U;

    // Distinct chunked batches under a single sequence are accepted up to the documented cap.
    for (std::size_t index = 0; index < kCap; ++index)
    {
        mfd::ReticlePatch patch;
        patch.text = std::to_string(index);

        mfd::CommandBatch chunk;
        chunk.mappingHash = "map_hash";
        chunk.sequence = 7U;
        chunk.commands.push_back(
            mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"", "", 11U, 22U}, patch});

        EXPECT_TRUE(processor.Submit(chunk)) << "chunk index " << index;
        EXPECT_TRUE(processor.LastError().empty());
    }

    // A further distinct batch on the same sequence is refused instead of growing the history without bound.
    mfd::ReticlePatch overflowPatch;
    overflowPatch.text = "overflow";

    mfd::CommandBatch overflowChunk;
    overflowChunk.mappingHash = "map_hash";
    overflowChunk.sequence = 7U;
    overflowChunk.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"", "", 11U, 22U}, overflowPatch});

    EXPECT_FALSE(processor.Submit(overflowChunk));
    EXPECT_EQ(processor.LastError(), "Too many distinct command batches retained for the same sequence");

    // Advancing the sequence clears the per-sequence history, so legitimate traffic resumes.
    mfd::ReticlePatch nextPatch;
    nextPatch.text = "after-advance";

    mfd::CommandBatch nextSequence;
    nextSequence.mappingHash = "map_hash";
    nextSequence.sequence = 8U;
    nextSequence.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"", "", 11U, 22U}, nextPatch});

    EXPECT_TRUE(processor.Submit(nextSequence));
    EXPECT_TRUE(processor.LastError().empty());
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

TEST(CommandProcessorTests, TransactionalRollbackKeepsUntouchedPagesIntact)
{
    mfd::SceneRegistry registry = MakeTwoPageRegistry();
    mfd::CommandProcessor processor(registry);

    mfd::ReticlePatch navPatch;
    navPatch.text = "NAV-42";
    ASSERT_TRUE(processor.Submit(
        mfd::UserCommand {mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"Nav", "nav_box"}, navPatch}}));
    ASSERT_EQ(ReadFirstReticleText(registry, "Nav"), "NAV-42");

    mfd::ReticlePatch radarPatch;
    radarPatch.text = "111";

    mfd::CommandBatch batch;
    batch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"Radar", "heading_box"}, radarPatch});
    batch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"Radar", "missing_reticle"}, {}});

    EXPECT_FALSE(processor.Submit(batch));
    EXPECT_NE(processor.LastError().find("missing_reticle"), std::string::npos);

    EXPECT_EQ(ReadFirstReticleText(registry, "Radar"), "000");
    EXPECT_EQ(ReadFirstReticleText(registry, "Nav"), "NAV-42");
}

TEST(CommandProcessorTests, TransactionalRollbackRestoresPreviousActivePage)
{
    mfd::SceneRegistry registry = MakeTwoPageRegistry();
    mfd::CommandProcessor processor(registry);
    ASSERT_EQ(registry.ActivePageName(), "Radar");

    mfd::CommandBatch batch;
    batch.commands.push_back(mfd::ActivatePageCommand {"Nav"});
    batch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"Nav", "missing_reticle"}, {}});

    EXPECT_FALSE(processor.Submit(batch));
    EXPECT_NE(processor.LastError().find("missing_reticle"), std::string::npos);
    EXPECT_EQ(registry.ActivePageName(), "Radar");
}

TEST(CommandProcessorTests, TransactionalRollbackAfterWindowResetRestoresRuntimeState)
{
    mfd::SceneRegistry registry = MakeRegistry();
    mfd::CommandProcessor processor(registry);

    mfd::ReticlePatch patch;
    patch.text = "111";
    ASSERT_TRUE(processor.Submit(
        mfd::UserCommand {mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"Radar", "heading_box"}, patch}}));
    ASSERT_EQ(ReadFirstReticleText(registry, "Radar"), "111");

    mfd::CommandBatch batch;
    batch.commands.push_back(mfd::ResetWindowCommand {});
    batch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"Radar", "missing_reticle"}, {}});

    EXPECT_FALSE(processor.Submit(batch));
    EXPECT_NE(processor.LastError().find("missing_reticle"), std::string::npos);
    EXPECT_EQ(ReadFirstReticleText(registry, "Radar"), "111");
}

TEST(CommandProcessorTests, TransactionalRollbackRestoresDynamicTemplateVisibility)
{
    mfd::SceneRegistry registry = MakeRuntimeRegistry();
    mfd::CommandProcessor processor(registry);

    mfd::CommandBatch upsertBatch;
    upsertBatch.mappingHash = "map_hash";
    upsertBatch.commands.push_back(MakeDynamicTrackUpsertCommand());
    ASSERT_TRUE(processor.Submit(upsertBatch));
    ASSERT_EQ(CountRuntimeDynamicReticles(registry, "Radar"), 1U);

    const auto visibleBefore = registry.CollectPageReticles("Radar");
    ASSERT_FALSE(visibleBefore.empty());
    ASSERT_TRUE(visibleBefore.back().visible);

    mfd::SetDynamicReticleSetVisibilityCommand hideCommand;
    hideCommand.page = "Radar";
    hideCommand.templateId = "radar_track";
    hideCommand.visible = false;

    mfd::CommandBatch failingBatch;
    failingBatch.commands.push_back(hideCommand);
    failingBatch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"Radar", "missing_reticle"}, {}});

    EXPECT_FALSE(processor.Submit(failingBatch));
    EXPECT_NE(processor.LastError().find("missing_reticle"), std::string::npos);

    const auto visibleAfter = registry.CollectPageReticles("Radar");
    ASSERT_FALSE(visibleAfter.empty());
    EXPECT_TRUE(visibleAfter.back().visible);
    EXPECT_EQ(CountRuntimeDynamicReticles(registry, "Radar"), 1U);
}

TEST(CommandProcessorTests, TransactionalRollbackRestoresFirstPageWhenSecondPageCommandFails)
{
    mfd::SceneRegistry registry = MakeTwoPageRegistry();
    mfd::CommandProcessor processor(registry);

    mfd::ReticlePatch navPatch;
    navPatch.text = "NAV-99";

    mfd::CommandBatch batch;
    batch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"Nav", "nav_box"}, navPatch});
    batch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"Radar", "missing_reticle"}, {}});

    EXPECT_FALSE(processor.Submit(batch));
    EXPECT_NE(processor.LastError().find("missing_reticle"), std::string::npos);

    EXPECT_EQ(ReadFirstReticleText(registry, "Nav"), "NAV");
    EXPECT_EQ(ReadFirstReticleText(registry, "Radar"), "000");
}

TEST(CommandProcessorTests, TransactionalRollbackRestoresWindowDisplayState)
{
    mfd::SceneRegistry registry = MakeRegistry();
    mfd::CommandProcessor processor(registry);

    mfd::WindowDisplayPatch initialPatch;
    initialPatch.brightness = 0.8f;
    ASSERT_TRUE(processor.Submit(mfd::UserCommand {mfd::UpdateWindowDisplayCommand {initialPatch}}));
    ASSERT_FLOAT_EQ(registry.WindowDisplay().brightness, 0.8f);

    mfd::WindowDisplayPatch failedPatch;
    failedPatch.invertColors = true;
    failedPatch.brightness = 0.2f;

    mfd::CommandBatch batch;
    batch.commands.push_back(mfd::UpdateWindowDisplayCommand {failedPatch});
    batch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"Radar", "missing_reticle"}, {}});

    EXPECT_FALSE(processor.Submit(batch));
    EXPECT_NE(processor.LastError().find("missing_reticle"), std::string::npos);

    const mfd::WindowDisplayState display = registry.WindowDisplay();
    EXPECT_FALSE(display.invertColors);
    EXPECT_FLOAT_EQ(display.brightness, 0.8f);
    EXPECT_FALSE(display.disabled);
}

TEST(CommandProcessorTests, TransactionalRollbackRestoresDynamicReticleAndMagnetizedStrobeTogether)
{
    mfd::SceneRegistry registry = MakeRuntimeRegistry();
    mfd::CommandProcessor processor(registry);

    mfd::CommandBatch upsertBatch;
    upsertBatch.mappingHash = "map_hash";
    upsertBatch.commands.push_back(MakeDynamicTrackUpsertCommand());
    ASSERT_TRUE(processor.Submit(upsertBatch));
    ASSERT_EQ(CountRuntimeDynamicReticles(registry, "Radar"), 1U);

    mfd::SetDynamicReticleSetStrobeMagnetEnabledCommand enableMagnet;
    enableMagnet.page = "Radar";
    enableMagnet.templateId = "radar_track";
    enableMagnet.enabled = true;
    ASSERT_TRUE(processor.Submit(mfd::UserCommand {enableMagnet}));

    mfd::UpdateStrobeCommand magnetize;
    magnetize.page = "Radar";
    magnetize.position = mfd::Vec2 {0.12f, -0.18f};
    ASSERT_TRUE(processor.Submit(mfd::UserCommand {magnetize}));

    const auto strobeBefore = registry.ActiveStrobeSummary();
    ASSERT_TRUE(strobeBefore.has_value());
    // The magnet radius (0.15) covers the distance to the dynamic track at (0.1, -0.2),
    // so the strobe locks onto the track position before the failing batch runs.
    ASSERT_FLOAT_EQ(strobeBefore->position.x, 0.1f);
    ASSERT_FLOAT_EQ(strobeBefore->position.y, -0.2f);

    mfd::UpsertDynamicReticleCommand moveTrack = MakeDynamicTrackUpsertCommand();
    moveTrack.patch.position = mfd::Vec2 {0.6f, 0.6f};
    moveTrack.patch.text = "T2";

    mfd::UpdateStrobeCommand moveStrobe;
    moveStrobe.page = "Radar";
    moveStrobe.position = mfd::Vec2 {0.55f, 0.55f};

    mfd::CommandBatch failingBatch;
    failingBatch.mappingHash = "map_hash";
    failingBatch.commands.push_back(moveTrack);
    failingBatch.commands.push_back(moveStrobe);
    failingBatch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"Radar", "missing_reticle"}, {}});

    EXPECT_FALSE(processor.Submit(failingBatch));
    EXPECT_NE(processor.LastError().find("missing_reticle"), std::string::npos);

    EXPECT_EQ(CountRuntimeDynamicReticles(registry, "Radar"), 1U);

    const mfd::ReticleGroup* track = nullptr;
    for (const mfd::ReticleGroup* reticle : registry.CollectPageReticlePointers("Radar"))
    {
        if (reticle != nullptr && reticle->id.rfind("__runtime_dynamic_", 0U) == 0U)
        {
            track = reticle;
        }
    }

    ASSERT_NE(track, nullptr);
    EXPECT_FLOAT_EQ(track->transform.position.x, 0.1f);
    EXPECT_FLOAT_EQ(track->transform.position.y, -0.2f);
    const auto* trackText = std::get_if<mfd::TextGeometry>(&track->primitives.front().geometry);
    ASSERT_NE(trackText, nullptr);
    EXPECT_EQ(trackText->text, "T1");

    const auto strobeAfter = registry.ActiveStrobeSummary();
    ASSERT_TRUE(strobeAfter.has_value());
    EXPECT_FLOAT_EQ(strobeAfter->position.x, strobeBefore->position.x);
    EXPECT_FLOAT_EQ(strobeAfter->position.y, strobeBefore->position.y);
}

TEST(CommandProcessorTests, SingleBulkDynamicUpsertCommandIsAtomicWhenOneReticleIsInvalid)
{
    mfd::SceneRegistry registry = MakeRuntimeRegistry();
    mfd::CommandProcessor processor(registry);

    mfd::SetDynamicReticleSetStrobeMagnetEnabledCommand enableMagnet;
    enableMagnet.page = "Radar";
    enableMagnet.templateId = "radar_track";
    enableMagnet.enabled = true;
    ASSERT_TRUE(processor.Submit(mfd::UserCommand {enableMagnet}));

    const auto strobeBefore = registry.ActiveStrobeSummary();
    ASSERT_TRUE(strobeBefore.has_value());

    mfd::UpsertDynamicReticlesCommand bulk;
    bulk.page = "Radar";
    bulk.templateId = "radar_track";

    mfd::DynamicReticleState validState;
    validState.reticleId = "track_ok";
    validState.patch.visible = true;
    validState.patch.text = "T1";
    validState.patch.position = strobeBefore->position;
    bulk.reticles.push_back(std::move(validState));

    mfd::DynamicReticleState invalidState;
    invalidState.reticleId = "track_bad";
    invalidState.patch.position = mfd::Vec2 {std::numeric_limits<float>::quiet_NaN(), 0.0f};
    bulk.reticles.push_back(std::move(invalidState));

    mfd::CommandBatch batch;
    batch.commands.push_back(std::move(bulk));
    ASSERT_EQ(batch.commands.size(), 1U);

    EXPECT_FALSE(processor.Submit(batch));
    EXPECT_FALSE(processor.LastError().empty());

    // One failing reticle must fail the whole command without leaving partial state, even
    // though a single-command batch never takes the transactional multi-command path.
    EXPECT_EQ(CountRuntimeDynamicReticles(registry, "Radar"), 0U);

    const auto strobeAfter = registry.ActiveStrobeSummary();
    ASSERT_TRUE(strobeAfter.has_value());
    EXPECT_FLOAT_EQ(strobeAfter->position.x, strobeBefore->position.x);
    EXPECT_FLOAT_EQ(strobeAfter->position.y, strobeBefore->position.y);

    const auto magnet = registry.ActiveStrobeMagnetSummary();
    if (magnet.has_value())
    {
        EXPECT_FALSE(magnet->magnetized);
    }
}

TEST(CommandProcessorTests, TransactionalRollbackRestoresPagesTouchedByIdOnlyCommands)
{
    mfd::SceneRegistry registry = MakeRegistry();
    mfd::CommandProcessor processor(registry);

    mfd::ReticlePatch initialPatch;
    initialPatch.text = "111";

    mfd::CommandBatch initialBatch;
    initialBatch.mappingHash = "map_hash";
    initialBatch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"", "", 11U, 22U}, initialPatch});
    ASSERT_TRUE(processor.Submit(initialBatch));
    ASSERT_EQ(ReadFirstReticleText(registry, "Radar"), "111");

    // The failing batch touches the page exclusively through generated transport ids:
    // the rollback footprint must resolve the page key from the id, without any
    // rehydrated authored name inside the commands.
    mfd::ReticlePatch failedPatch;
    failedPatch.text = "222";

    mfd::CommandBatch failingBatch;
    failingBatch.mappingHash = "map_hash";
    failingBatch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"", "", 11U, 22U}, failedPatch});
    failingBatch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"Radar", "missing_reticle"}, {}});

    EXPECT_FALSE(processor.Submit(failingBatch));
    EXPECT_NE(processor.LastError().find("missing_reticle"), std::string::npos);
    EXPECT_EQ(ReadFirstReticleText(registry, "Radar"), "111");
}

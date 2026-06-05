/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for the integrated `mfd_window` runtime debug mode.
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "mfd/control/CommandTypes.h"
#include "mfd/model/PageDefinition.h"
#include "mfd/model/PageName.h"
#include "mfd/runtime/SceneRegistry.h"

#include "mfd/window/debug/RuntimeDebugInspectorFrameState.hpp"
#include "mfd/window/debug/RuntimeDebugPreview.hpp"
#include "mfd/window/debug/RuntimeDebugState.hpp"

namespace
{
mfd::Primitive MakeLinePrimitive(std::string id)
{
    mfd::Primitive primitive;
    primitive.id = std::move(id);
    primitive.type = mfd::PrimitiveType::Line;
    primitive.geometry = mfd::LineGeometry {};
    return primitive;
}

mfd::ReticleGroup MakeReticle(std::string id, const mfd::Vec2 position = {})
{
    mfd::ReticleGroup reticle;
    reticle.id = std::move(id);
    reticle.transform.position = position;
    reticle.primitives.push_back(MakeLinePrimitive("line"));
    return reticle;
}

mfd::ReticleGroup MakeTimeReticle(std::string id)
{
    mfd::ReticleGroup reticle;
    reticle.id = std::move(id);

    mfd::Primitive primitive;
    primitive.id = "clock";
    primitive.type = mfd::PrimitiveType::Time;
    primitive.geometry = mfd::TimeGeometry {};
    reticle.primitives.push_back(std::move(primitive));
    return reticle;
}

mfd::PageBlinkDefinition MakeBlinkType(std::string name, const std::uint32_t durationMs)
{
    mfd::PageBlinkDefinition blinkType;
    blinkType.name = std::move(name);
    blinkType.normalizedName = mfd::NormalizePageName(blinkType.name);
    blinkType.durationMs = durationMs;
    return blinkType;
}

mfd::PageStrobeDefinition MakeStrobe(std::string name, std::string reticleId, const mfd::Vec2 position = {})
{
    mfd::PageStrobeDefinition strobe;
    strobe.name = std::move(name);
    strobe.normalizedName = mfd::NormalizePageName(strobe.name);
    strobe.reticle = MakeReticle(std::move(reticleId), position);
    strobe.capture.shape = mfd::StrobeCaptureShape::Circle;
    strobe.capture.radius = 0.12f;
    return strobe;
}

mfd::PageDefinition MakePage(std::string name, const bool defaultPage, std::vector<mfd::ReticleGroup> staticReticles)
{
    mfd::PageDefinition page;
    page.name = std::move(name);
    page.normalizedName = mfd::NormalizePageName(page.name);
    page.title = page.name;
    page.defaultPage = defaultPage;
    page.staticReticles = std::move(staticReticles);
    return page;
}

mfd::MfdDocument MakeRuntimeDebugDocument()
{
    mfd::MfdDocument document;
    document.pages.push_back(
        MakePage(
            "Radar",
            true,
            {MakeReticle("Ownship", mfd::Vec2 {-0.15f, 0.05f}),
             MakeReticle("Target", mfd::Vec2 {0.20f, -0.10f})}));
    document.pages.push_back(
        MakePage(
            "Nav",
            false,
            {MakeReticle("Route", mfd::Vec2 {0.0f, 0.0f})}));
    return document;
}

mfd::MfdDocument MakeRuntimeDebugDocumentWithStrobes()
{
    mfd::MfdDocument document = MakeRuntimeDebugDocument();
    for (mfd::PageDefinition& page : document.pages)
    {
        if (page.name != "Radar")
        {
            continue;
        }

        page.strobes.push_back(MakeStrobe("Default", "DefaultStrobe"));
        page.strobes.push_back(MakeStrobe("Strobe1", "AlternativeStrobe", mfd::Vec2 {0.42f, -0.18f}));
        page.activeStrobeName = "Default";
        page.normalizedActiveStrobeName = mfd::NormalizePageName(page.activeStrobeName);
        break;
    }

    return document;
}

mfd::GeneratedTransportMap MakeRuntimeDebugTransportMapWithStrobes()
{
    mfd::GeneratedTransportMap map;
    map.mappingHash = "runtime_debug_map";
    map.pages.push_back({11U, "Radar", mfd::NormalizePageName("Radar"), true, true});
    map.pages.push_back({12U, "Nav", mfd::NormalizePageName("Nav"), false, false});
    map.strobes.push_back({101U, 11U, "Default", mfd::NormalizePageName("Default"), "DefaultStrobe", true});
    map.strobes.push_back({102U, 11U, "Strobe1", mfd::NormalizePageName("Strobe1"), "AlternativeStrobe", false});
    return map;
}

mfd::SceneRegistry MakeScene()
{
    mfd::SceneRegistry scene;
    scene.LoadDocument(MakeRuntimeDebugDocument());
    scene.SetActivePage("Radar");
    return scene;
}

mfd::SceneRegistry MakeMultiStrobeScene()
{
    mfd::SceneRegistry scene;
    scene.LoadDocument(MakeRuntimeDebugDocumentWithStrobes());
    scene.SetActivePage("Radar");
    return scene;
}

mfd::SceneRegistry MakeMultiStrobeSceneWithTransportMap()
{
    mfd::SceneRegistry scene;
    scene.LoadDocument(MakeRuntimeDebugDocumentWithStrobes(), MakeRuntimeDebugTransportMapWithStrobes());
    scene.SetActivePage("Radar");
    return scene;
}

mfd::UdpRuntimeBridgeMetrics MakeTransportMetrics()
{
    mfd::UdpRuntimeBridgeMetrics metrics;
    metrics.receivedPackets = 12U;
    metrics.decodedBatches = 9U;
    metrics.droppedBatches = 2U;
    metrics.appliedCommands = 21U;
    metrics.coalescedCommands = 5U;
    metrics.feedbackSent = 7U;
    metrics.feedbackDropped = 1U;
    metrics.inboundQueueDepth = 3U;
    metrics.outboundQueueDepth = 4U;
    return metrics;
}

void AddBlinkCatalogToPage(mfd::MfdDocument& document,
                           const std::string_view pageName,
                           std::vector<mfd::PageBlinkDefinition> blinkTypes,
                           const std::string_view defaultBlinkTypeName)
{
    for (mfd::PageDefinition& page : document.pages)
    {
        if (page.name != pageName)
        {
            continue;
        }

        page.blinkTypes = std::move(blinkTypes);
        page.defaultBlinkTypeName = std::string(defaultBlinkTypeName);
        page.normalizedDefaultBlinkTypeName = mfd::NormalizePageName(defaultBlinkTypeName);
        return;
    }
}

void AssignReticleBlink(mfd::MfdDocument& document,
                        const std::string_view pageName,
                        const std::string_view reticleId,
                        const std::string_view blinkTypeName)
{
    for (mfd::PageDefinition& page : document.pages)
    {
        if (page.name != pageName)
        {
            continue;
        }

        const mfd::PageBlinkDefinition* blinkType = mfd::FindPageBlinkDefinition(page, blinkTypeName);
        if (blinkType == nullptr)
        {
            return;
        }

        for (mfd::ReticleGroup& reticle : page.staticReticles)
        {
            if (reticle.id != reticleId)
            {
                continue;
            }

            reticle.blink.enabled = true;
            reticle.blink.typeName = blinkType->name;
            reticle.blink.normalizedTypeName = blinkType->normalizedName;
            reticle.blink.durationMs = blinkType->durationMs;
            return;
        }

        return;
    }
}

mfd::SceneRegistry MakeSceneWithBlinkTypes()
{
    mfd::MfdDocument document = MakeRuntimeDebugDocument();
    AddBlinkCatalogToPage(
        document,
        "Nav",
        {MakeBlinkType("flash", 250U), MakeBlinkType("slow", 1000U)},
        "flash");

    mfd::SceneRegistry scene;
    scene.LoadDocument(document);
    scene.SetActivePage("Radar");
    return scene;
}

mfd::SceneRegistry MakeSceneWithAuthoredBlinkReticle()
{
    mfd::MfdDocument document = MakeRuntimeDebugDocument();
    AddBlinkCatalogToPage(
        document,
        "Nav",
        {MakeBlinkType("flash", 250U), MakeBlinkType("slow", 1000U)},
        "flash");
    AssignReticleBlink(document, "Nav", "Route", "slow");

    mfd::SceneRegistry scene;
    scene.LoadDocument(document);
    scene.SetActivePage("Radar");
    return scene;
}

const mfd::ReticleGroup* FindReticle(const mfd::SceneRegistry& scene,
                                     const std::string_view pageName,
                                     const std::string_view reticleId)
{
    for (const mfd::ReticleGroup* reticle : scene.CollectPageReticlePointers(pageName))
    {
        if (reticle != nullptr && reticle->id == reticleId)
        {
            return reticle;
        }
    }

    return nullptr;
}

const mfd::TimeGeometry* FindTimeGeometry(const mfd::ReticleGroup& reticle, const std::string_view primitiveId)
{
    const mfd::Primitive* primitive = mfd::FindPrimitive(reticle, primitiveId);
    if (primitive == nullptr)
    {
        return nullptr;
    }

    return std::get_if<mfd::TimeGeometry>(&primitive->geometry);
}
} // namespace

/**
 * @brief Confirms the inspector frame starts with one valid snapshot.
 */
TEST(RuntimeDebugInspectorFrameStateTests, StartsWithValidSnapshot)
{
    mfd::window::debug::RuntimeDebugInspectorFrameState frameState;

    EXPECT_TRUE(frameState.SnapshotValid());
    EXPECT_EQ(
        mfd::window::debug::RuntimeDebugInspectorFrameState::RefreshNotice(),
        "The inspector state changed and will be refreshed on the next frame.");
}

/**
 * @brief Confirms one local preview rebuild invalidates the remaining widgets for the current frame.
 */
TEST(RuntimeDebugInspectorFrameStateTests, InvalidatesSnapshotAfterPreviewMutation)
{
    mfd::window::debug::RuntimeDebugInspectorFrameState frameState;

    frameState.InvalidateSnapshot();

    EXPECT_FALSE(frameState.SnapshotValid());
}

/**
 * @brief Ensures deactivation clears only interactive debug overrides.
 */
TEST(RuntimeDebugStateTests, DeactivateClearsInteractiveOverridesButKeepsObservedState)
{
    using namespace mfd::window::debug;

    RuntimeDebugState state;
    const ReticleKey key {"Radar", "Ownship", ReticleKind::Static};

    state.Activate();
    state.EnablePageBypass("Nav");
    state.EnableStrobeBypass("Radar", "Strobe1");
    state.SelectReticle(key);
    state.EnsureReticleBypass(key, MakeReticle("Ownship"));
    state.SetDynamicTemplateVisibility("Radar", "tracks", false);
    state.UpdateTransportState(true, true, false, false, MakeTransportMetrics(), "ready", "feedback disabled");
    state.NoteCommandTraffic(1U, 3U);
    state.SetTestPanelStatus("mutated");

    state.Deactivate();

    EXPECT_FALSE(state.Active());
    EXPECT_FALSE(state.PageBypassed());
    EXPECT_TRUE(state.ForcedActivePage().empty());
    EXPECT_FALSE(state.StrobeBypassed("Radar"));
    EXPECT_EQ(state.ForcedActiveStrobe("Radar"), nullptr);
    EXPECT_FALSE(state.SelectedReticle().has_value());
    EXPECT_TRUE(state.BypassedReticles().empty());
    EXPECT_TRUE(state.TestPanelStatus().empty());
    ASSERT_TRUE(state.DynamicTemplateVisibility("Radar", "tracks").has_value());
    EXPECT_FALSE(*state.DynamicTemplateVisibility("Radar", "tracks"));
    EXPECT_TRUE(state.Transport().commandConfigured);
    EXPECT_TRUE(state.Transport().observedCommandTraffic);
    EXPECT_EQ(state.Transport().metrics.appliedCommands, 21U);
}

/**
 * @brief Confirms preview ownership is required only when one local bypass is active.
 */
TEST(RuntimeDebugStateTests, HasInteractiveOverridesTracksPageAndReticleBypasses)
{
    using namespace mfd::window::debug;

    RuntimeDebugState state;
    EXPECT_FALSE(state.HasInteractiveOverrides());

    state.EnablePageBypass("Radar");
    EXPECT_TRUE(state.HasInteractiveOverrides());

    state.DisablePageBypass();
    EXPECT_FALSE(state.HasInteractiveOverrides());

    state.EnableStrobeBypass("Radar", "Strobe1");
    EXPECT_TRUE(state.HasInteractiveOverrides());

    state.DisableStrobeBypass("Radar");
    EXPECT_FALSE(state.HasInteractiveOverrides());

    const ReticleKey key {"Radar", "Ownship", ReticleKind::Static};
    state.EnsureReticleBypass(key, MakeReticle("Ownship"));
    EXPECT_TRUE(state.HasInteractiveOverrides());

    state.ReleaseReticleBypass(key);
    EXPECT_FALSE(state.HasInteractiveOverrides());
}

/**
 * @brief Confirms observed runtime telemetry can be reset independently from interactive state.
 */
TEST(RuntimeDebugStateTests, ResetObservedRuntimeStateClearsTransportAndTemplateVisibility)
{
    using namespace mfd::window::debug;

    RuntimeDebugState state;
    state.SetDynamicTemplateVisibility("Radar", "tracks", true);
    state.UpdateTransportState(true, true, true, true, MakeTransportMetrics(), "commands ready", "feedback ready");
    state.NoteCommandTraffic(2U, 5U);

    state.ResetObservedRuntimeState();

    EXPECT_FALSE(state.DynamicTemplateVisibility("Radar", "tracks").has_value());
    EXPECT_FALSE(state.Transport().commandConfigured);
    EXPECT_FALSE(state.Transport().feedbackConfigured);
    EXPECT_FALSE(state.Transport().observedCommandTraffic);
    EXPECT_EQ(state.Transport().metrics.receivedPackets, 0U);
    EXPECT_EQ(state.Transport().metrics.inboundQueueDepth, 0U);
    EXPECT_LT(state.SecondsSinceLastCommandTraffic(), 0.0);
}

/**
 * @brief Confirms the overlay state stores the UDP runtime metrics snapshot verbatim.
 */
TEST(RuntimeDebugStateTests, UpdateTransportStateStoresUdpMetricsSnapshot)
{
    using namespace mfd::window::debug;

    RuntimeDebugState state;
    const mfd::UdpRuntimeBridgeMetrics metrics = MakeTransportMetrics();

    state.UpdateTransportState(true, false, true, false, metrics, "command warning", "feedback warning");

    EXPECT_TRUE(state.Transport().commandConfigured);
    EXPECT_FALSE(state.Transport().commandReady);
    EXPECT_TRUE(state.Transport().feedbackConfigured);
    EXPECT_FALSE(state.Transport().feedbackReady);
    EXPECT_EQ(state.Transport().metrics.receivedPackets, 12U);
    EXPECT_EQ(state.Transport().metrics.decodedBatches, 9U);
    EXPECT_EQ(state.Transport().metrics.droppedBatches, 2U);
    EXPECT_EQ(state.Transport().metrics.appliedCommands, 21U);
    EXPECT_EQ(state.Transport().metrics.coalescedCommands, 5U);
    EXPECT_EQ(state.Transport().metrics.feedbackSent, 7U);
    EXPECT_EQ(state.Transport().metrics.feedbackDropped, 1U);
    EXPECT_EQ(state.Transport().metrics.inboundQueueDepth, 3U);
    EXPECT_EQ(state.Transport().metrics.outboundQueueDepth, 4U);
    EXPECT_EQ(state.Transport().commandStatus, "command warning");
    EXPECT_EQ(state.Transport().feedbackStatus, "feedback warning");
}

/**
 * @brief Verifies releasing one reticle bypass restores the last live UDP-driven state.
 */
TEST(RuntimeDebugPreviewTests, ReleasingBypassRestoresLastLiveReticleState)
{
    using namespace mfd::window::debug;

    mfd::SceneRegistry liveScene = MakeScene();
    mfd::ReticlePatch livePatch;
    livePatch.visible = false;
    livePatch.position = mfd::Vec2 {0.35f, -0.25f};
    ASSERT_TRUE(liveScene.ApplyReticlePatch("Radar", "Ownship", livePatch));

    RuntimeDebugState state;
    state.Activate();

    RuntimeDebugPreview preview;
    ASSERT_TRUE(preview.ResetFromLive(liveScene, state));

    const mfd::ReticleGroup* liveReticle = FindReticle(liveScene, "Radar", "Ownship");
    ASSERT_NE(liveReticle, nullptr);
    const mfd::ReticleGroup* previewReticle = FindReticle(preview.Scene(), "Radar", "Ownship");
    ASSERT_NE(previewReticle, nullptr);
    EXPECT_FALSE(previewReticle->visible);
    EXPECT_FLOAT_EQ(previewReticle->transform.position.x, liveReticle->transform.position.x);
    EXPECT_FLOAT_EQ(previewReticle->transform.position.y, liveReticle->transform.position.y);

    const ReticleKey key {"Radar", "Ownship", ReticleKind::Static};
    ReticleBypassState& bypass = state.EnsureReticleBypass(key, *previewReticle);
    bypass.draft.visible = true;
    bypass.draft.transform.position = mfd::Vec2 {-0.60f, 0.40f};
    ASSERT_TRUE(preview.ApplyStateOverrides(state));

    previewReticle = FindReticle(preview.Scene(), "Radar", "Ownship");
    ASSERT_NE(previewReticle, nullptr);
    EXPECT_TRUE(previewReticle->visible);
    EXPECT_FLOAT_EQ(previewReticle->transform.position.x, -0.60f);
    EXPECT_FLOAT_EQ(previewReticle->transform.position.y, 0.40f);

    state.ReleaseReticleBypass(key);
    ASSERT_TRUE(preview.ResetFromLive(liveScene, state));

    previewReticle = FindReticle(preview.Scene(), "Radar", "Ownship");
    ASSERT_NE(previewReticle, nullptr);
    EXPECT_FALSE(previewReticle->visible);
    EXPECT_FLOAT_EQ(previewReticle->transform.position.x, liveReticle->transform.position.x);
    EXPECT_FLOAT_EQ(previewReticle->transform.position.y, liveReticle->transform.position.y);
}

/**
 * @brief Verifies a local debug bypass can clear a live numeric time override.
 */
TEST(RuntimeDebugPreviewTests, ReticleBypassCanClearRuntimeTimeValue)
{
    using namespace mfd::window::debug;

    mfd::MfdDocument document = MakeRuntimeDebugDocument();
    for (mfd::PageDefinition& page : document.pages)
    {
        if (page.name == "Radar")
        {
            page.staticReticles.push_back(MakeTimeReticle("Clock"));
            break;
        }
    }

    mfd::SceneRegistry liveScene;
    liveScene.LoadDocument(document);
    liveScene.SetActivePage("Radar");

    mfd::PrimitivePatch liveTimePatch;
    liveTimePatch.timeValue = mfd::TimeValue {2026, 6, 5, 14, 3, 9};
    liveTimePatch.timeUtc = true;
    liveTimePatch.timeFields = mfd::TimeFieldVisibility {true, true, true, true, true, false};

    mfd::ReticlePatch livePatch;
    livePatch.primitivePatches.emplace("clock", liveTimePatch);
    ASSERT_TRUE(liveScene.ApplyReticlePatch("Radar", "Clock", livePatch));

    RuntimeDebugState state;
    state.Activate();

    RuntimeDebugPreview preview;
    ASSERT_TRUE(preview.ResetFromLive(liveScene, state));

    const mfd::ReticleGroup* previewClock = FindReticle(preview.Scene(), "Radar", "Clock");
    ASSERT_NE(previewClock, nullptr);
    const mfd::TimeGeometry* time = FindTimeGeometry(*previewClock, "clock");
    ASSERT_NE(time, nullptr);
    ASSERT_TRUE(time->runtimeValueOverride.has_value());

    const ReticleKey key {"Radar", "Clock", ReticleKind::Static};
    ReticleBypassState& bypass = state.EnsureReticleBypass(key, *previewClock);
    mfd::Primitive* draftPrimitive = mfd::FindPrimitive(bypass.draft, "clock");
    ASSERT_NE(draftPrimitive, nullptr);
    auto* draftTime = std::get_if<mfd::TimeGeometry>(&draftPrimitive->geometry);
    ASSERT_NE(draftTime, nullptr);
    draftTime->runtimeValueOverride.reset();
    draftTime->runtimeUtc = true;
    draftTime->runtimeFields = mfd::TimeFieldVisibility {true, true, true, true, true, false};

    ASSERT_TRUE(preview.ResetFromLive(liveScene, state));

    previewClock = FindReticle(preview.Scene(), "Radar", "Clock");
    ASSERT_NE(previewClock, nullptr);
    time = FindTimeGeometry(*previewClock, "clock");
    ASSERT_NE(time, nullptr);
    EXPECT_FALSE(time->runtimeValueOverride.has_value());
    ASSERT_TRUE(time->runtimeUtc.has_value());
    EXPECT_TRUE(*time->runtimeUtc);
    ASSERT_TRUE(time->runtimeFields.has_value());
    EXPECT_TRUE(time->runtimeFields->year);
    EXPECT_FALSE(time->runtimeFields->second);
}

/**
 * @brief Ensures preview rebuild mirrors the live active strobe selection on the current page.
 */
TEST(RuntimeDebugPreviewTests, ResetFromLiveMirrorsCurrentActiveStrobe)
{
    using namespace mfd::window::debug;

    mfd::SceneRegistry liveScene = MakeMultiStrobeScene();
    ASSERT_TRUE(liveScene.SelectStrobe("Radar", "Strobe1"));
    ASSERT_TRUE(liveScene.SetStrobePosition("Radar", {0.26f, -0.14f}));

    RuntimeDebugState state;
    state.Activate();

    RuntimeDebugPreview preview;
    ASSERT_TRUE(preview.ResetFromLive(liveScene, state));

    const auto previewStrobe = preview.Scene().ActiveStrobeSummary();
    ASSERT_TRUE(previewStrobe.has_value());
    EXPECT_EQ(previewStrobe->strobeName, "Strobe1");
    EXPECT_EQ(previewStrobe->reticleId, "AlternativeStrobe");
    EXPECT_FLOAT_EQ(previewStrobe->position.x, 0.26f);
    EXPECT_FLOAT_EQ(previewStrobe->position.y, -0.14f);
}

/**
 * @brief Ensures live commands keep updating the preview except for locally bypassed reticles.
 */
TEST(RuntimeDebugPreviewTests, ApplyLiveBatchesKeepsBypassedReticlesLocal)
{
    using namespace mfd::window::debug;

    mfd::SceneRegistry liveScene = MakeScene();
    RuntimeDebugState state;
    state.Activate();

    RuntimeDebugPreview preview;
    ASSERT_TRUE(preview.ResetFromLive(liveScene, state));

    const mfd::ReticleGroup* previewOwnship = FindReticle(preview.Scene(), "Radar", "Ownship");
    ASSERT_NE(previewOwnship, nullptr);
    const ReticleKey bypassedKey {"Radar", "Ownship", ReticleKind::Static};
    ReticleBypassState& bypass = state.EnsureReticleBypass(bypassedKey, *previewOwnship);
    bypass.draft.transform.position = mfd::Vec2 {-0.50f, 0.33f};
    ASSERT_TRUE(preview.ApplyStateOverrides(state));

    mfd::UpdateReticleCommand bypassedUpdate;
    bypassedUpdate.target.page = "Radar";
    bypassedUpdate.target.reticle = "Ownship";
    bypassedUpdate.patch.position = mfd::Vec2 {0.75f, 0.75f};

    mfd::UpdateReticleCommand liveUpdate;
    liveUpdate.target.page = "Radar";
    liveUpdate.target.reticle = "Target";
    liveUpdate.patch.position = mfd::Vec2 {0.65f, -0.45f};

    mfd::CommandBatch batch;
    batch.sequence = 1U;
    batch.commands.push_back(bypassedUpdate);
    batch.commands.push_back(liveUpdate);

    ASSERT_TRUE(preview.ApplyLiveBatches({batch}, state));

    previewOwnship = FindReticle(preview.Scene(), "Radar", "Ownship");
    const mfd::ReticleGroup* previewTarget = FindReticle(preview.Scene(), "Radar", "Target");
    ASSERT_NE(previewOwnship, nullptr);
    ASSERT_NE(previewTarget, nullptr);
    EXPECT_FLOAT_EQ(previewOwnship->transform.position.x, -0.50f);
    EXPECT_FLOAT_EQ(previewOwnship->transform.position.y, 0.33f);
    EXPECT_FLOAT_EQ(previewTarget->transform.position.x, 0.65f);
    EXPECT_FLOAT_EQ(previewTarget->transform.position.y, -0.45f);
}

/**
 * @brief Ensures one local strobe-selection bypass owns the preview selection and ignores live strobe commands.
 */
TEST(RuntimeDebugPreviewTests, ApplyLiveBatchesKeepsBypassedStrobeSelectionLocal)
{
    using namespace mfd::window::debug;

    mfd::SceneRegistry liveScene = MakeMultiStrobeScene();
    RuntimeDebugState state;
    state.Activate();
    state.EnableStrobeBypass("Radar", "Strobe1");

    RuntimeDebugPreview preview;
    ASSERT_TRUE(preview.ResetFromLive(liveScene, state));

    const auto initialPreviewStrobe = preview.Scene().ActiveStrobeSummary();
    ASSERT_TRUE(initialPreviewStrobe.has_value());
    EXPECT_EQ(initialPreviewStrobe->strobeName, "Strobe1");
    EXPECT_EQ(initialPreviewStrobe->reticleId, "AlternativeStrobe");

    mfd::UpdateStrobeCommand liveUpdate;
    liveUpdate.page = "Radar";
    liveUpdate.strobe = "Default";
    liveUpdate.active = false;
    liveUpdate.position = mfd::Vec2 {0.63f, 0.44f};

    mfd::CommandBatch batch;
    batch.sequence = 1U;
    batch.commands.push_back(liveUpdate);

    ASSERT_TRUE(preview.ApplyLiveBatches({batch}, state));

    const auto previewStrobeAfterBatch = preview.Scene().ActiveStrobeSummary();
    ASSERT_TRUE(previewStrobeAfterBatch.has_value());
    EXPECT_EQ(previewStrobeAfterBatch->strobeName, "Strobe1");
    EXPECT_EQ(previewStrobeAfterBatch->reticleId, "AlternativeStrobe");
    EXPECT_FLOAT_EQ(previewStrobeAfterBatch->position.x, initialPreviewStrobe->position.x);
    EXPECT_FLOAT_EQ(previewStrobeAfterBatch->position.y, initialPreviewStrobe->position.y);
    EXPECT_EQ(previewStrobeAfterBatch->visible, initialPreviewStrobe->visible);
}

/**
 * @brief Ensures strobe-selection bypass also holds when live batches use generated transport ids only.
 */
TEST(RuntimeDebugPreviewTests, ApplyLiveBatchesKeepsBypassedStrobeSelectionLocalWithGeneratedIds)
{
    using namespace mfd::window::debug;

    mfd::SceneRegistry liveScene = MakeMultiStrobeSceneWithTransportMap();
    RuntimeDebugState state;
    state.Activate();
    state.EnableStrobeBypass("Radar", "Strobe1");

    RuntimeDebugPreview preview;
    ASSERT_TRUE(preview.ResetFromLive(liveScene, state));

    const auto initialPreviewStrobe = preview.Scene().ActiveStrobeSummary();
    ASSERT_TRUE(initialPreviewStrobe.has_value());
    EXPECT_EQ(initialPreviewStrobe->strobeName, "Strobe1");

    mfd::UpdateStrobeCommand liveUpdate;
    liveUpdate.pageId = 11U;
    liveUpdate.strobeId = 101U;
    liveUpdate.active = false;
    liveUpdate.position = mfd::Vec2 {0.63f, 0.44f};

    mfd::CommandBatch batch;
    batch.sequence = 1U;
    batch.mappingHash = "runtime_debug_map";
    batch.commands.push_back(liveUpdate);

    ASSERT_TRUE(preview.ApplyLiveBatches({batch}, state));

    const auto previewStrobeAfterBatch = preview.Scene().ActiveStrobeSummary();
    ASSERT_TRUE(previewStrobeAfterBatch.has_value());
    EXPECT_EQ(previewStrobeAfterBatch->strobeName, "Strobe1");
    EXPECT_FLOAT_EQ(previewStrobeAfterBatch->position.x, initialPreviewStrobe->position.x);
    EXPECT_FLOAT_EQ(previewStrobeAfterBatch->position.y, initialPreviewStrobe->position.y);
    EXPECT_EQ(previewStrobeAfterBatch->visible, initialPreviewStrobe->visible);
}

/**
 * @brief Ensures disabling one strobe-selection bypass restores the last live active strobe cleanly.
 */
TEST(RuntimeDebugPreviewTests, DisablingStrobeBypassRestoresLiveActiveStrobe)
{
    using namespace mfd::window::debug;

    mfd::SceneRegistry liveScene = MakeMultiStrobeScene();
    ASSERT_TRUE(liveScene.SelectStrobe("Radar", "Strobe1"));
    ASSERT_TRUE(liveScene.SetStrobePosition("Radar", {0.26f, -0.14f}));

    RuntimeDebugState state;
    state.Activate();
    state.EnableStrobeBypass("Radar", "Default");

    RuntimeDebugPreview preview;
    ASSERT_TRUE(preview.ResetFromLive(liveScene, state));

    const auto bypassedPreviewStrobe = preview.Scene().ActiveStrobeSummary();
    ASSERT_TRUE(bypassedPreviewStrobe.has_value());
    EXPECT_EQ(bypassedPreviewStrobe->strobeName, "Default");
    EXPECT_EQ(bypassedPreviewStrobe->reticleId, "DefaultStrobe");

    state.DisableStrobeBypass("Radar");
    ASSERT_TRUE(preview.ResetFromLive(liveScene, state));

    const auto restoredPreviewStrobe = preview.Scene().ActiveStrobeSummary();
    ASSERT_TRUE(restoredPreviewStrobe.has_value());
    EXPECT_EQ(restoredPreviewStrobe->strobeName, "Strobe1");
    EXPECT_EQ(restoredPreviewStrobe->reticleId, "AlternativeStrobe");
    EXPECT_FLOAT_EQ(restoredPreviewStrobe->position.x, 0.26f);
    EXPECT_FLOAT_EQ(restoredPreviewStrobe->position.y, -0.14f);
}

/**
 * @brief Verifies page bypass follows the debug override and returns to the live active page when released.
 */
TEST(RuntimeDebugPreviewTests, PageBypassRestoresLiveActivePageWhenReleased)
{
    using namespace mfd::window::debug;

    mfd::SceneRegistry liveScene = MakeScene();
    liveScene.SetActivePage("Radar");

    RuntimeDebugState state;
    state.Activate();

    RuntimeDebugPreview preview;
    ASSERT_TRUE(preview.ResetFromLive(liveScene, state));
    EXPECT_EQ(preview.Scene().ActivePageName(), "Radar");

    state.EnablePageBypass("Nav");
    ASSERT_TRUE(preview.ApplyStateOverrides(state));
    EXPECT_EQ(preview.Scene().ActivePageName(), "Nav");

    state.DisablePageBypass();
    ASSERT_TRUE(preview.ResetFromLive(liveScene, state));
    EXPECT_EQ(preview.Scene().ActivePageName(), "Radar");
}

/**
 * @brief Ensures one local reticle blink bypass does not disturb the currently forced preview page.
 */
TEST(RuntimeDebugPreviewTests, ReticleBlinkBypassKeepsForcedPreviewPageStable)
{
    using namespace mfd::window::debug;

    mfd::SceneRegistry liveScene = MakeSceneWithBlinkTypes();

    RuntimeDebugState state;
    state.Activate();
    state.EnablePageBypass("Nav");

    RuntimeDebugPreview preview;
    ASSERT_TRUE(preview.ResetFromLive(liveScene, state));
    EXPECT_EQ(preview.Scene().ActivePageName(), "Nav");

    const ReticleKey key {"Nav", "Route", ReticleKind::Static};
    const mfd::ReticleGroup* previewReticle = FindReticle(preview.Scene(), key.pageName, key.reticleId);
    ASSERT_NE(previewReticle, nullptr);

    ReticleBypassState& bypass = state.EnsureReticleBypass(key, *previewReticle);
    bypass.draft.blink.enabled = true;
    bypass.draft.blink.typeName = "flash";
    bypass.draft.blink.normalizedTypeName = mfd::NormalizePageName("flash");
    bypass.draft.blink.durationMs = 250U;

    ASSERT_TRUE(preview.ApplyStateOverrides(state));
    EXPECT_EQ(preview.Scene().ActivePageName(), "Nav");

    previewReticle = FindReticle(preview.Scene(), key.pageName, key.reticleId);
    ASSERT_NE(previewReticle, nullptr);
    EXPECT_TRUE(previewReticle->blink.enabled);
    EXPECT_EQ(previewReticle->blink.typeName, "flash");
    EXPECT_EQ(previewReticle->blink.durationMs, 250U);
}

/**
 * @brief Ensures one bypassed reticle can restore its authored blink type even when live UDP state differs.
 */
TEST(RuntimeDebugPreviewTests, ReticleBlinkBypassOverridesDifferentLiveBlinkType)
{
    using namespace mfd::window::debug;

    mfd::SceneRegistry liveScene = MakeSceneWithAuthoredBlinkReticle();
    mfd::ReticlePatch livePatch;
    livePatch.blinkType = std::string {"flash"};
    ASSERT_TRUE(liveScene.ApplyReticlePatch("Nav", "Route", livePatch));

    RuntimeDebugState state;
    state.Activate();
    state.EnablePageBypass("Nav");

    RuntimeDebugPreview preview;
    ASSERT_TRUE(preview.ResetFromLive(liveScene, state));

    const ReticleKey key {"Nav", "Route", ReticleKind::Static};
    const mfd::ReticleGroup* previewReticle = FindReticle(preview.Scene(), key.pageName, key.reticleId);
    ASSERT_NE(previewReticle, nullptr);
    EXPECT_EQ(previewReticle->blink.typeName, "flash");

    ReticleBypassState& bypass = state.EnsureReticleBypass(key, *previewReticle);
    bypass.draft.blink.enabled = true;
    bypass.draft.blink.typeName = "slow";
    bypass.draft.blink.normalizedTypeName = mfd::NormalizePageName("slow");
    bypass.draft.blink.durationMs = 1000U;

    ASSERT_TRUE(preview.ApplyStateOverrides(state));

    previewReticle = FindReticle(preview.Scene(), key.pageName, key.reticleId);
    ASSERT_NE(previewReticle, nullptr);
    EXPECT_TRUE(previewReticle->blink.enabled);
    EXPECT_EQ(previewReticle->blink.typeName, "slow");
    EXPECT_EQ(previewReticle->blink.durationMs, 1000U);
    EXPECT_EQ(preview.Scene().ActivePageName(), "Nav");
}

/**
 * @brief Verifies enabling blink on a page without any blink catalog is rejected by the preview.
 */
TEST(RuntimeDebugPreviewTests, ReticleBlinkBypassWithoutPageBlinkCatalogIsRejected)
{
    using namespace mfd::window::debug;

    mfd::SceneRegistry liveScene = MakeScene();

    RuntimeDebugState state;
    state.Activate();
    state.EnablePageBypass("Nav");

    RuntimeDebugPreview preview;
    ASSERT_TRUE(preview.ResetFromLive(liveScene, state));

    const ReticleKey key {"Nav", "Route", ReticleKind::Static};
    const mfd::ReticleGroup* previewReticle = FindReticle(preview.Scene(), key.pageName, key.reticleId);
    ASSERT_NE(previewReticle, nullptr);

    ReticleBypassState& bypass = state.EnsureReticleBypass(key, *previewReticle);
    bypass.draft.blink.enabled = true;
    bypass.draft.blink.typeName.clear();
    bypass.draft.blink.normalizedTypeName.clear();
    bypass.draft.blink.durationMs = 0U;

    EXPECT_FALSE(preview.ApplyStateOverrides(state));
    EXPECT_FALSE(preview.Ready());
    EXPECT_FALSE(preview.LastError().empty());
}

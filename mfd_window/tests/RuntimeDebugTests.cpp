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

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "mfd/control/CommandTypes.h"
#include "mfd/model/PageDefinition.h"
#include "mfd/model/PageName.h"
#include "mfd/runtime/SceneRegistry.h"

#include "RuntimeDebugPreview.hpp"
#include "RuntimeDebugState.hpp"

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

mfd::SceneRegistry MakeScene()
{
    mfd::SceneRegistry scene;
    scene.LoadDocument(MakeRuntimeDebugDocument());
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
} // namespace

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
    state.SelectReticle(key);
    state.EnsureReticleBypass(key, MakeReticle("Ownship"));
    state.SetDynamicTemplateVisibility("Radar", "tracks", false);
    state.UpdateTransportState(true, true, false, false, "ready", "feedback disabled");
    state.NoteCommandTraffic(1U, 3U);
    state.SetTestPanelStatus("mutated");

    state.Deactivate();

    EXPECT_FALSE(state.Active());
    EXPECT_FALSE(state.PageBypassed());
    EXPECT_TRUE(state.ForcedActivePage().empty());
    EXPECT_FALSE(state.SelectedReticle().has_value());
    EXPECT_TRUE(state.BypassedReticles().empty());
    EXPECT_TRUE(state.TestPanelStatus().empty());
    ASSERT_TRUE(state.DynamicTemplateVisibility("Radar", "tracks").has_value());
    EXPECT_FALSE(*state.DynamicTemplateVisibility("Radar", "tracks"));
    EXPECT_TRUE(state.Transport().commandConfigured);
    EXPECT_TRUE(state.Transport().observedCommandTraffic);
}

/**
 * @brief Confirms observed runtime telemetry can be reset independently from interactive state.
 */
TEST(RuntimeDebugStateTests, ResetObservedRuntimeStateClearsTransportAndTemplateVisibility)
{
    using namespace mfd::window::debug;

    RuntimeDebugState state;
    state.SetDynamicTemplateVisibility("Radar", "tracks", true);
    state.UpdateTransportState(true, true, true, true, "commands ready", "feedback ready");
    state.NoteCommandTraffic(2U, 5U);

    state.ResetObservedRuntimeState();

    EXPECT_FALSE(state.DynamicTemplateVisibility("Radar", "tracks").has_value());
    EXPECT_FALSE(state.Transport().commandConfigured);
    EXPECT_FALSE(state.Transport().feedbackConfigured);
    EXPECT_FALSE(state.Transport().observedCommandTraffic);
    EXPECT_LT(state.SecondsSinceLastCommandTraffic(), 0.0);
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

/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests covering the page-preview layer-focus controller.
 */

#include "EditorLayerFocusController.h"

#include "internal/application/EditorApplicationInternal.h"

#include <gtest/gtest.h>

namespace
{
mfd::ReticleGroup MakeLayeredReticle(const std::string& id, const std::string& layerId)
{
    mfd::ReticleGroup reticle;
    reticle.id = id;
    reticle.layerId = layerId;
    return reticle;
}

mfd::PageDefinition MakeLayeredPage()
{
    mfd::PageDefinition page;
    page.name = "Radar";
    page.layers = {
        mfd::PageLayerDefinition {"base"},
        mfd::PageLayerDefinition {"overlay"},
        mfd::PageLayerDefinition {"hidden"}};
    page.editor.layers = {
        mfd::EditorLayerDefinition {"base", true},
        mfd::EditorLayerDefinition {"overlay", true},
        mfd::EditorLayerDefinition {"hidden", false}};
    page.staticReticles = {
        MakeLayeredReticle("r0", "base"),
        MakeLayeredReticle("r1", "overlay"),
        MakeLayeredReticle("r2", "hidden")};
    return page;
}
} // namespace

TEST(LayerFocusControllerTests, BuildStripModelKeepsSafeFullViewWhenNoLayerExists)
{
    mfd::PageDefinition page;
    editor::LayerFocusController controller;

    const editor::LayerFocusStripModel model = controller.BuildStripModel(page, editor::LayerFocusState {});

    ASSERT_EQ(model.entries.size(), 1U);
    EXPECT_FALSE(model.focusActive);
    EXPECT_TRUE(model.entries.front().fullView);
    EXPECT_TRUE(model.entries.front().selected);
    EXPECT_EQ(model.entries.front().label, "Full View");
    EXPECT_EQ(model.entries.front().reticleCount, 0U);
}

TEST(LayerFocusControllerTests, BuildStripModelReportsReticleCountsPerLayer)
{
    const mfd::PageDefinition page = MakeLayeredPage();
    editor::LayerFocusController controller;

    const editor::LayerFocusStripModel model = controller.BuildStripModel(page, editor::LayerFocusState {});

    ASSERT_EQ(model.entries.size(), 4U);
    EXPECT_EQ(model.entries[0].reticleCount, 3U);
    EXPECT_EQ(model.entries[1].layerId, "base");
    EXPECT_EQ(model.entries[1].reticleCount, 1U);
    EXPECT_EQ(model.entries[2].layerId, "overlay");
    EXPECT_EQ(model.entries[2].reticleCount, 1U);
    EXPECT_EQ(model.entries[3].layerId, "hidden");
    EXPECT_EQ(model.entries[3].reticleCount, 1U);
}

TEST(LayerFocusControllerTests, FullViewKeepsAllReticlesSelectable)
{
    const mfd::PageDefinition page = MakeLayeredPage();
    editor::LayerFocusController controller;
    const editor::LayerFocusState state {};

    EXPECT_TRUE(controller.IsReticleSelectable(page, page.staticReticles[0], state));
    EXPECT_TRUE(controller.IsReticleSelectable(page, page.staticReticles[1], state));
    EXPECT_TRUE(controller.IsReticleSelectable(page, page.staticReticles[2], state));
    EXPECT_FALSE(controller.ShouldReticleBeDimmed(page, page.staticReticles[0], state));
    EXPECT_FALSE(controller.ShouldReticleBeDimmed(page, page.staticReticles[1], state));
}

TEST(LayerFocusControllerTests, FocusedLayerOnlyKeepsMatchingReticlesEditable)
{
    const mfd::PageDefinition page = MakeLayeredPage();
    editor::LayerFocusController controller;
    const editor::LayerFocusState state = controller.MakeFocusedState(page, "overlay");

    ASSERT_TRUE(controller.IsFocusActive(page, state));
    EXPECT_FALSE(controller.IsReticleSelectable(page, page.staticReticles[0], state));
    EXPECT_TRUE(controller.IsReticleSelectable(page, page.staticReticles[1], state));
    EXPECT_FALSE(controller.IsReticleSelectable(page, page.staticReticles[2], state));
    EXPECT_TRUE(controller.ShouldReticleBeDimmed(page, page.staticReticles[0], state));
    EXPECT_FALSE(controller.ShouldReticleBeDimmed(page, page.staticReticles[1], state));

    const std::vector<int> filtered = controller.FilterSelectableReticleIndices(page, {0, 1, 2}, state);
    EXPECT_EQ(filtered, std::vector<int>({1}));
}

TEST(LayerFocusControllerTests, SanitizeFocusStateClearsMissingLayer)
{
    const mfd::PageDefinition page = MakeLayeredPage();
    editor::LayerFocusController controller;
    editor::LayerFocusState state {"ghost"};

    controller.SanitizeFocusState(page, state);

    EXPECT_TRUE(state.focusedLayerId.empty());
    EXPECT_FALSE(controller.IsFocusActive(page, state));
}

TEST(LayerFocusControllerTests, BuildStripModelReflectsLayerOrderAfterReorder)
{
    mfd::PageDefinition page = MakeLayeredPage();
    editor::LayerFocusController controller;

    ASSERT_TRUE(editor::detail::MovePageLayer(page, 1U, editor::detail::PageLayerMoveDirection::Down));

    const editor::LayerFocusStripModel model = controller.BuildStripModel(page, editor::LayerFocusState {});

    ASSERT_EQ(model.entries.size(), 4U);
    // Full View stays in front and never behaves like a movable layer.
    EXPECT_TRUE(model.entries[0].fullView);
    EXPECT_TRUE(model.entries[0].layerId.empty());
    // The displayed strip order follows the real runtime layer order after the reorder.
    EXPECT_EQ(model.entries[1].layerId, "base");
    EXPECT_EQ(model.entries[2].layerId, "hidden");
    EXPECT_EQ(model.entries[3].layerId, "overlay");
    // Editor-only visibility travels with its layer (hidden was not visible).
    EXPECT_FALSE(model.entries[2].visible);
    EXPECT_TRUE(model.entries[3].visible);
}

TEST(LayerFocusControllerTests, FocusByIdStaysStableAfterReorder)
{
    mfd::PageDefinition page = MakeLayeredPage();
    editor::LayerFocusController controller;
    editor::LayerFocusState state = controller.MakeFocusedState(page, "overlay");

    ASSERT_TRUE(editor::detail::MovePageLayer(page, 1U, editor::detail::PageLayerMoveDirection::Down));

    // The focused layer is tracked by id, so the reorder keeps focus on the same layer.
    controller.SanitizeFocusState(page, state);
    EXPECT_EQ(state.focusedLayerId, "overlay");
    EXPECT_TRUE(controller.IsFocusActive(page, state));
    EXPECT_TRUE(controller.IsReticleSelectable(page, page.staticReticles[1], state));
}

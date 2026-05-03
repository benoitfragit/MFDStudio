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

#include <gtest/gtest.h>

namespace
{
mfd::ReticleGroup MakeLayeredReticle(const std::string& id, const std::string& layerId)
{
    mfd::ReticleGroup reticle;
    reticle.id = id;
    reticle.editor.layerId = layerId;
    return reticle;
}

mfd::PageDefinition MakeLayeredPage()
{
    mfd::PageDefinition page;
    page.name = "Radar";
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

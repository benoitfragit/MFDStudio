/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests for focused LEVEL 1 studio framing and subtree extraction.
 */

#include "internal/application/EditorFocusedStudio.h"

#include <limits>

#include <gtest/gtest.h>

TEST(EditorFocusedStudioTests, FitsLogicalBoundsAndPreservesTheirCenter)
{
    const mfd::PageViewState view = editor::app::ComputeFocusedStudioView(
        {{0.25f, -0.50f}, {0.50f, 0.25f}}, 800.0f, 600.0f, 0.75f);

    EXPECT_FLOAT_EQ(view.center.x, 0.25f);
    EXPECT_FLOAT_EQ(view.center.y, -0.50f);
    EXPECT_FLOAT_EQ(view.zoom, 4.0f);
}

TEST(EditorFocusedStudioTests, RejectsInvalidViewportAndBounds)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const mfd::PageViewState invalidViewport =
        editor::app::ComputeFocusedStudioView({{}, {0.5f, 0.5f}}, nan, 600.0f);
    const mfd::PageViewState invalidBounds =
        editor::app::ComputeFocusedStudioView({{}, {0.0f, 0.5f}}, 800.0f, 600.0f);

    EXPECT_FLOAT_EQ(invalidViewport.zoom, 1.0f);
    EXPECT_FLOAT_EQ(invalidBounds.zoom, 1.0f);
}

TEST(EditorFocusedStudioTests, ClampsExtremeZoomAndSanitizesPadding)
{
    const mfd::PageViewState tinyBounds = editor::app::ComputeFocusedStudioView(
        {{}, {0.000001f, 0.000001f}}, 800.0f, 600.0f, 1.0f);
    const mfd::PageViewState hugeBounds = editor::app::ComputeFocusedStudioView(
        {{}, {100000.0f, 100000.0f}}, 800.0f, 600.0f, 1.0f);
    const mfd::PageViewState invalidPadding = editor::app::ComputeFocusedStudioView(
        {{}, {1.0f, 1.0f}}, 600.0f, 600.0f, std::numeric_limits<float>::quiet_NaN());

    EXPECT_FLOAT_EQ(tinyBounds.zoom, 20.0f);
    EXPECT_FLOAT_EQ(hugeBounds.zoom, 0.1f);
    EXPECT_FLOAT_EQ(invalidPadding.zoom, 1.56f);
}

TEST(EditorFocusedStudioTests, FindsProjectedWidgetLayoutByStableSourceIndex)
{
    std::vector<mfd::api1::ContainerLayoutNode> layouts(2U);
    layouts[0].id = "container/Panel";
    layouts[0].sourceIndex = 0U;
    layouts[1].id = "widget/RootAction";
    layouts[1].sourceIndex = 7U;

    const mfd::api1::ContainerLayoutNode* selected =
        editor::app::FindFocusedStudioLayout(layouts, 7U);

    ASSERT_NE(selected, nullptr);
    EXPECT_EQ(selected->id, "widget/RootAction");
    EXPECT_EQ(editor::app::FindFocusedStudioLayout(layouts, 3U), nullptr);
}

TEST(EditorFocusedStudioTests, ExtractsSelectedPreorderSubtreeAndPromotesItsRoot)
{
    std::vector<mfd::api1::ContainerLayoutNode> layouts(6U);
    layouts[0].sourceIndex = 0U;
    layouts[0].depth = 0U;
    layouts[1].sourceIndex = 1U;
    layouts[1].parentSourceIndex = 0U;
    layouts[1].depth = 1U;
    layouts[2].sourceIndex = 2U;
    layouts[2].parentSourceIndex = 1U;
    layouts[2].depth = 2U;
    layouts[3].sourceIndex = 8U;
    layouts[3].parentSourceIndex = 2U;
    layouts[3].depth = 3U;
    layouts[4].sourceIndex = 3U;
    layouts[4].parentSourceIndex = 0U;
    layouts[4].depth = 1U;
    layouts[5].sourceIndex = 4U;
    layouts[5].depth = 0U;

    const std::vector<mfd::api1::ContainerLayoutNode> focused =
        editor::app::CollectFocusedContainerLayouts(layouts, 1U);

    ASSERT_EQ(focused.size(), 3U);
    EXPECT_EQ(focused[0].sourceIndex, 1U);
    EXPECT_EQ(focused[0].parentSourceIndex, std::numeric_limits<std::size_t>::max());
    EXPECT_EQ(focused[1].sourceIndex, 2U);
    EXPECT_EQ(focused[2].sourceIndex, 8U);
}

TEST(EditorFocusedStudioTests, ReturnsEmptyLayoutWhenSelectionIsMissing)
{
    std::vector<mfd::api1::ContainerLayoutNode> layouts(1U);
    layouts[0].sourceIndex = 3U;

    EXPECT_TRUE(editor::app::CollectFocusedContainerLayouts(layouts, 9U).empty());
}

TEST(EditorFocusedStudioTests, RejectsLayoutsBeyondTheCombinedLevelOneLimit)
{
    const std::size_t excessiveCount =
        mfd::api1::kMaximumContainerCount + mfd::api1::kMaximumWidgetCount + 1U;
    std::vector<mfd::api1::ContainerLayoutNode> layouts(excessiveCount);
    layouts.front().sourceIndex = 0U;

    EXPECT_TRUE(editor::app::CollectFocusedContainerLayouts(layouts, 0U).empty());
}

TEST(EditorFocusedStudioTests, ReusesLayoutUntilAnAuthoredMutationInvalidatesTheCache)
{
    mfd::PageDefinition page;
    mfd::api1::ContainerDefinition container;
    container.id = "panel";
    container.geometry.size = {0.4f, 0.3f};
    page.containers.push_back(container);

    editor::app::FocusedStudioLayoutCache cache;
    ASSERT_TRUE(cache.Resolve(page, 0U, true));
    ASSERT_EQ(cache.Layouts().size(), 1U);
    EXPECT_FLOAT_EQ(cache.Layouts().front().bounds.size.x, 0.4f);

    page.containers.front().geometry.size.x = 0.8f;
    ASSERT_TRUE(cache.Resolve(page, 0U, true));
    EXPECT_FLOAT_EQ(cache.Layouts().front().bounds.size.x, 0.4f);

    cache.Invalidate();
    ASSERT_TRUE(cache.Resolve(page, 0U, true));
    EXPECT_FLOAT_EQ(cache.Layouts().front().bounds.size.x, 0.8f);
}

TEST(EditorFocusedStudioTests, DetectsOnlyRealContainerGeometryChanges)
{
    const mfd::api1::ContainerRect initial {{0.1f, -0.2f}, {0.4f, 0.3f}};
    mfd::api1::ContainerRect changed = initial;

    EXPECT_FALSE(editor::app::ContainerGeometryChanged(initial, initial));
    changed.size.x = 0.5f;
    EXPECT_TRUE(editor::app::ContainerGeometryChanged(initial, changed));
    changed = initial;
    changed.center.y = 0.25f;
    EXPECT_TRUE(editor::app::ContainerGeometryChanged(initial, changed));
}

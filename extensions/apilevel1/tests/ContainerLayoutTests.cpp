/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/** @file @brief Deterministic layout, hierarchy, scrolling and JSON tests for API LEVEL 1. */

#include "mfd/apilevel1/ContainerJson.h"
#include "mfd/apilevel1/ContainerLayoutEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace
{
using mfd::api1::ChildResizeMode;
using mfd::api1::ContainerAlignment;
using mfd::api1::ContainerChildPlacement;
using mfd::api1::ContainerDefinition;
using mfd::api1::ContainerLayoutEngine;
using mfd::api1::ContainerLayoutNode;
using mfd::api1::ContainerSizeMode;
using mfd::api1::ContainerType;
using mfd::api1::LinearOrientation;
using mfd::api1::ScrollMode;
using mfd::api1::kMaximumContainerDepth;

mfd::api1::ContainerDefinition MakeContainer(const std::string& id,
                                              const mfd::api1::ContainerType type,
                                              const mfd::Vec2 position,
                                              const mfd::Vec2 size)
{
    mfd::api1::ContainerDefinition container;
    container.id = id;
    container.name = id;
    container.type = type;
    container.geometry = {position, size};
    container.layoutReferenceSize = size;
    return container;
}

const ContainerLayoutNode* FindLayout(const std::vector<ContainerLayoutNode>& layouts,
                                      const std::string& id)
{
    const auto match = std::find_if(layouts.begin(), layouts.end(), [&id](const ContainerLayoutNode& node)
    {
        return node.id == id;
    });
    return match == layouts.end() ? nullptr : &*match;
}
} // namespace

TEST(ContainerLayoutTests, ResolvesAllContainerTypesAndNestedInheritance)
{
    ContainerDefinition canvas = MakeContainer("canvas", ContainerType::Canvas, {-0.6f, 0.5f}, {0.8f, 0.8f});
    ContainerDefinition canvasChild = MakeContainer("canvasChild", ContainerType::Overlay, {0.1f, 0.1f}, {0.2f, 0.3f});
    canvas.visible = false;
    canvas.children.push_back(ContainerChildPlacement {canvasChild.id});

    ContainerDefinition linear = MakeContainer("linear", ContainerType::Linear, {0.5f, 0.5f}, {0.8f, 0.8f});
    linear.orientation = LinearOrientation::Horizontal;
    linear.alignment = ContainerAlignment::Stretch;
    linear.spacing = 0.05f;
    ContainerDefinition linearA = MakeContainer("linearA", ContainerType::Canvas, {}, {0.1f, 0.1f});
    ContainerDefinition linearB = MakeContainer("linearB", ContainerType::Canvas, {}, {0.1f, 0.1f});
    linear.children.push_back(ContainerChildPlacement {linearA.id});
    linear.children.push_back(ContainerChildPlacement {linearB.id});

    ContainerDefinition grid = MakeContainer("grid", ContainerType::Grid, {-0.5f, -0.5f}, {0.8f, 0.8f});
    grid.rows = 2U;
    grid.columns = 2U;
    ContainerDefinition gridChild = MakeContainer("gridChild", ContainerType::Canvas, {}, {0.1f, 0.1f});
    ContainerChildPlacement gridPlacement {gridChild.id};
    gridPlacement.row = 1U;
    gridPlacement.column = 1U;
    gridPlacement.horizontalAlignment = ContainerAlignment::Stretch;
    gridPlacement.verticalAlignment = ContainerAlignment::Stretch;
    grid.children.push_back(gridPlacement);

    ContainerDefinition overlay = MakeContainer("overlay", ContainerType::Overlay, {0.5f, -0.5f}, {0.8f, 0.8f});
    ContainerDefinition overlayChild = MakeContainer("overlayChild", ContainerType::Canvas, {}, {0.2f, 0.2f});
    ContainerChildPlacement overlayPlacement {overlayChild.id};
    overlayPlacement.resizeMode = ChildResizeMode::Stretch;
    overlay.children.push_back(overlayPlacement);

    const std::vector<ContainerDefinition> definitions {
        canvas, canvasChild, linear, linearA, linearB, grid, gridChild, overlay, overlayChild};
    std::vector<ContainerLayoutNode> layouts;
    std::string error;
    ASSERT_TRUE(ContainerLayoutEngine {}.Evaluate(definitions, layouts, error)) << error;
    ASSERT_EQ(layouts.size(), definitions.size());

    const ContainerLayoutNode* hiddenChild = nullptr;
    const ContainerLayoutNode* stretchedOverlayChild = nullptr;
    for (const ContainerLayoutNode& layout : layouts)
    {
        if (layout.id == "canvasChild") hiddenChild = &layout;
        if (layout.id == "overlayChild") stretchedOverlayChild = &layout;
    }
    ASSERT_NE(hiddenChild, nullptr);
    EXPECT_FALSE(hiddenChild->effectiveVisible);
    ASSERT_NE(stretchedOverlayChild, nullptr);
    EXPECT_FLOAT_EQ(stretchedOverlayChild->bounds.size.x, overlay.geometry.size.x);
    EXPECT_FLOAT_EQ(stretchedOverlayChild->bounds.size.y, overlay.geometry.size.y);
}

TEST(ContainerLayoutTests, ClampsOverflowAndBuildsIndependentScrollbars)
{
    ContainerDefinition parent = MakeContainer("scroll", ContainerType::Canvas, {}, {0.5f, 0.5f});
    parent.scrolling.horizontal = ScrollMode::Automatic;
    parent.scrolling.vertical = ScrollMode::AlwaysVisible;
    parent.scrolling.offset = {100.0f, 100.0f};
    ContainerDefinition child = MakeContainer("large", ContainerType::Canvas, {0.5f, -0.5f}, {1.0f, 1.0f});
    parent.children.push_back(ContainerChildPlacement {child.id});

    std::vector<ContainerLayoutNode> layouts;
    std::string error;
    ASSERT_TRUE(ContainerLayoutEngine {}.Evaluate({parent, child}, layouts, error)) << error;
    ASSERT_EQ(layouts.size(), 2U);
    EXPECT_TRUE(layouts.front().horizontalScrollbar.visible);
    EXPECT_TRUE(layouts.front().verticalScrollbar.visible);
    EXPECT_GE(layouts.front().scrollOffset.x, 0.0f);
    EXPECT_GE(layouts.front().scrollOffset.y, 0.0f);
    EXPECT_LT(layouts.front().horizontalScrollbar.thumb.size.x,
              layouts.front().horizontalScrollbar.track.size.x);
}

TEST(ContainerLayoutTests, ResolvesContentAndSingleAxisFitModesBottomUp)
{
    ContainerDefinition horizontal =
        MakeContainer("horizontal", ContainerType::Linear, {}, {2.0f, 2.0f});
    horizontal.sizeMode = ContainerSizeMode::FitContent;
    horizontal.orientation = LinearOrientation::Horizontal;
    horizontal.spacing = 0.1f;
    horizontal.padding = {0.1f, 0.05f, 0.2f, 0.05f};
    ContainerDefinition first = MakeContainer("first", ContainerType::Canvas, {}, {0.4f, 0.2f});
    ContainerDefinition second = MakeContainer("second", ContainerType::Canvas, {}, {0.3f, 0.5f});
    horizontal.children.push_back(ContainerChildPlacement {first.id});
    horizontal.children.push_back(ContainerChildPlacement {second.id});

    ContainerDefinition fitHorizontal =
        MakeContainer("fitHorizontal", ContainerType::Canvas, {}, {1.8f, 0.9f});
    fitHorizontal.sizeMode = ContainerSizeMode::FitHorizontal;
    ContainerDefinition horizontalChild =
        MakeContainer("horizontalChild", ContainerType::Canvas, {0.3f, 0.0f}, {0.4f, 0.2f});
    fitHorizontal.children.push_back(ContainerChildPlacement {horizontalChild.id});

    ContainerDefinition fitVertical =
        MakeContainer("fitVertical", ContainerType::Canvas, {}, {0.8f, 1.7f});
    fitVertical.sizeMode = ContainerSizeMode::FitVertical;
    ContainerDefinition verticalChild =
        MakeContainer("verticalChild", ContainerType::Canvas, {0.0f, 0.25f}, {0.2f, 0.3f});
    fitVertical.children.push_back(ContainerChildPlacement {verticalChild.id});

    const std::vector<ContainerDefinition> definitions {
        horizontal, first, second, fitHorizontal, horizontalChild, fitVertical, verticalChild};
    std::vector<ContainerLayoutNode> layouts;
    std::string error;
    ASSERT_TRUE(ContainerLayoutEngine {}.Evaluate(definitions, layouts, error)) << error;

    const ContainerLayoutNode* horizontalLayout = FindLayout(layouts, "horizontal");
    const ContainerLayoutNode* fitHorizontalLayout = FindLayout(layouts, "fitHorizontal");
    const ContainerLayoutNode* fitVerticalLayout = FindLayout(layouts, "fitVertical");
    ASSERT_NE(horizontalLayout, nullptr);
    ASSERT_NE(fitHorizontalLayout, nullptr);
    ASSERT_NE(fitVerticalLayout, nullptr);
    EXPECT_NEAR(horizontalLayout->bounds.size.x, 1.1f, 0.0001f);
    EXPECT_NEAR(horizontalLayout->bounds.size.y, 0.6f, 0.0001f);
    EXPECT_NEAR(fitHorizontalLayout->bounds.size.x, 1.0f, 0.0001f);
    EXPECT_NEAR(fitHorizontalLayout->bounds.size.y, 0.9f, 0.0001f);
    EXPECT_NEAR(fitVerticalLayout->bounds.size.x, 0.8f, 0.0001f);
    EXPECT_NEAR(fitVerticalLayout->bounds.size.y, 0.8f, 0.0001f);
}

TEST(ContainerLayoutTests, LinearLayoutPreservesPreferredSizesAndExposesOverflow)
{
    ContainerDefinition parent = MakeContainer("linear", ContainerType::Linear, {}, {1.0f, 0.5f});
    parent.orientation = LinearOrientation::Horizontal;
    parent.alignment = ContainerAlignment::Center;
    parent.spacing = 0.1f;
    parent.scrolling.horizontal = ScrollMode::Automatic;
    ContainerDefinition first = MakeContainer("first", ContainerType::Canvas, {}, {0.4f, 0.2f});
    ContainerDefinition second = MakeContainer("second", ContainerType::Canvas, {}, {0.9f, 0.3f});
    parent.children.push_back(ContainerChildPlacement {first.id});
    parent.children.push_back(ContainerChildPlacement {second.id});

    std::vector<ContainerLayoutNode> layouts;
    std::string error;
    ASSERT_TRUE(ContainerLayoutEngine {}.Evaluate({parent, first, second}, layouts, error)) << error;

    const ContainerLayoutNode* firstLayout = FindLayout(layouts, "first");
    const ContainerLayoutNode* secondLayout = FindLayout(layouts, "second");
    ASSERT_NE(firstLayout, nullptr);
    ASSERT_NE(secondLayout, nullptr);
    EXPECT_FLOAT_EQ(firstLayout->bounds.size.x, 0.4f);
    EXPECT_FLOAT_EQ(secondLayout->bounds.size.x, 0.9f);
    EXPECT_LT(firstLayout->bounds.center.x, secondLayout->bounds.center.x);
    EXPECT_TRUE(layouts.front().horizontalScrollbar.visible);
    EXPECT_NEAR(layouts.front().contentSize.x, 1.4f, 0.0001f);
}

TEST(ContainerLayoutTests, AppliesRenderOrderOnlyToOverlayChildren)
{
    ContainerDefinition canvas = MakeContainer("canvas", ContainerType::Canvas, {}, {1.0f, 1.0f});
    ContainerDefinition canvasFirst = MakeContainer("canvasFirst", ContainerType::Canvas, {}, {0.1f, 0.1f});
    ContainerDefinition canvasSecond = MakeContainer("canvasSecond", ContainerType::Canvas, {}, {0.1f, 0.1f});
    ContainerChildPlacement canvasFirstPlacement {canvasFirst.id};
    ContainerChildPlacement canvasSecondPlacement {canvasSecond.id};
    canvasFirstPlacement.renderOrder = 10;
    canvasSecondPlacement.renderOrder = -10;
    canvas.children = {canvasFirstPlacement, canvasSecondPlacement};

    ContainerDefinition overlay = MakeContainer("overlay", ContainerType::Overlay, {}, {1.0f, 1.0f});
    ContainerDefinition overlayFirst = MakeContainer("overlayFirst", ContainerType::Canvas, {}, {0.1f, 0.1f});
    ContainerDefinition overlaySecond = MakeContainer("overlaySecond", ContainerType::Canvas, {}, {0.1f, 0.1f});
    ContainerChildPlacement overlayFirstPlacement {overlayFirst.id};
    ContainerChildPlacement overlaySecondPlacement {overlaySecond.id};
    overlayFirstPlacement.renderOrder = 10;
    overlaySecondPlacement.renderOrder = -10;
    overlay.children = {overlayFirstPlacement, overlaySecondPlacement};

    std::vector<ContainerLayoutNode> layouts;
    std::string error;
    ASSERT_TRUE(ContainerLayoutEngine {}.Evaluate(
        {canvas, canvasFirst, canvasSecond, overlay, overlayFirst, overlaySecond}, layouts, error)) << error;
    ASSERT_EQ(layouts.size(), 6U);
    EXPECT_EQ(layouts[1].id, "canvasFirst");
    EXPECT_EQ(layouts[2].id, "canvasSecond");
    EXPECT_EQ(layouts[4].id, "overlaySecond");
    EXPECT_EQ(layouts[5].id, "overlayFirst");
}

TEST(ContainerLayoutTests, HorizontalLinearStartAlignsChildrenAtTheTop)
{
    ContainerDefinition parent = MakeContainer("parent", ContainerType::Linear, {}, {1.0f, 1.0f});
    parent.orientation = LinearOrientation::Horizontal;
    parent.alignment = ContainerAlignment::Start;
    ContainerDefinition child = MakeContainer("child", ContainerType::Canvas, {}, {0.2f, 0.2f});
    parent.children = {{child.id}};
    std::vector<ContainerLayoutNode> layouts;
    std::string error;
    ASSERT_TRUE(ContainerLayoutEngine {}.Evaluate({parent, child}, layouts, error)) << error;
    ASSERT_EQ(layouts.size(), 2U);
    EXPECT_FLOAT_EQ(layouts[1].bounds.center.y, 0.4f);
}

TEST(ContainerLayoutTests, RejectsCyclesDuplicateOwnershipDepthAndNonFiniteGeometry)
{
    ContainerLayoutEngine engine;
    std::string error;

    ContainerDefinition first = MakeContainer("first", ContainerType::Canvas, {}, {0.5f, 0.5f});
    ContainerDefinition second = MakeContainer("second", ContainerType::Canvas, {}, {0.5f, 0.5f});
    first.children.push_back(ContainerChildPlacement {second.id});
    second.children.push_back(ContainerChildPlacement {first.id});
    EXPECT_FALSE(engine.Validate({first, second}, error));
    EXPECT_NE(error.find("cycle"), std::string::npos);

    ContainerDefinition invalid = MakeContainer("invalid", ContainerType::Canvas, {}, {0.5f, 0.5f});
    invalid.geometry.size.x = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(engine.Validate({invalid}, error));
    EXPECT_NE(error.find("non-finite"), std::string::npos);

    invalid = MakeContainer("invalidEnum", ContainerType::Canvas, {}, {0.5f, 0.5f});
    invalid.sizeMode = static_cast<ContainerSizeMode>(99U);
    EXPECT_FALSE(engine.Validate({invalid}, error));
    EXPECT_NE(error.find("enum"), std::string::npos);

    std::vector<ContainerDefinition> deep;
    deep.reserve(kMaximumContainerDepth + 1U);
    for (std::size_t index = 0U; index <= kMaximumContainerDepth; ++index)
    {
        deep.push_back(MakeContainer("node" + std::to_string(index), ContainerType::Canvas, {}, {0.5f, 0.5f}));
        if (index > 0U) deep[index - 1U].children.push_back(ContainerChildPlacement {deep[index].id});
    }
    EXPECT_FALSE(engine.Validate(deep, error));
    EXPECT_NE(error.find("depth"), std::string::npos);

    ContainerDefinition duplicateA = MakeContainer("Panel", ContainerType::Canvas, {}, {0.5f, 0.5f});
    ContainerDefinition duplicateB = MakeContainer(" panel ", ContainerType::Canvas, {}, {0.5f, 0.5f});
    EXPECT_FALSE(engine.Validate({duplicateA, duplicateB}, error));
    EXPECT_NE(error.find("normalized"), std::string::npos);
}

TEST(ContainerLayoutTests, RejectsReverseOrderedDeepHierarchyHugeGridAndArithmeticOverflow)
{
    std::vector<ContainerDefinition> deep;
    for (std::size_t index = 0U; index <= kMaximumContainerDepth; ++index)
    {
        deep.push_back(MakeContainer("node" + std::to_string(index), ContainerType::Canvas, {}, {1.0f, 1.0f}));
        if (index > 0U) deep.back().children.push_back({deep[index - 1U].id});
    }
    std::string error;
    std::vector<ContainerLayoutNode> layouts;
    EXPECT_FALSE(ContainerLayoutEngine {}.Evaluate(deep, layouts, error));
    EXPECT_NE(error.find("depth"), std::string::npos);
    ContainerDefinition grid = MakeContainer("grid", ContainerType::Grid, {}, {1.0f, 1.0f});
    grid.rows = std::numeric_limits<std::uint32_t>::max();
    EXPECT_FALSE(ContainerLayoutEngine {}.Evaluate({grid}, layouts, error));
    EXPECT_TRUE(layouts.empty());
    ContainerDefinition huge = MakeContainer("huge", ContainerType::Canvas, {}, {1.0f, 1.0f});
    huge.padding.left = std::numeric_limits<float>::max();
    huge.padding.right = std::numeric_limits<float>::max();
    huge.sizeMode = ContainerSizeMode::FitContent;
    EXPECT_FALSE(ContainerLayoutEngine {}.Evaluate({huge}, layouts, error));
    EXPECT_TRUE(layouts.empty());
}

TEST(ContainerLayoutTests, GridFitUsesTheSameUniformTracksAsPlacement)
{
    ContainerDefinition parent = MakeContainer("grid", ContainerType::Grid, {}, {0.1f, 0.1f});
    parent.columns = 2U;
    parent.sizeMode = ContainerSizeMode::FitContent;
    ContainerDefinition first = MakeContainer("first", ContainerType::Canvas, {}, {0.8f, 0.3f});
    ContainerDefinition second = MakeContainer("second", ContainerType::Canvas, {}, {0.2f, 0.2f});
    parent.children = {{first.id}, {second.id}};
    parent.children.back().column = 1U;
    std::vector<ContainerLayoutNode> layouts;
    std::string error;
    ASSERT_TRUE(ContainerLayoutEngine {}.Evaluate({parent, first, second}, layouts, error)) << error;
    EXPECT_FLOAT_EQ(layouts.front().bounds.size.x, 1.6f);
    EXPECT_FLOAT_EQ(layouts.front().bounds.size.y, 0.3f);
}

TEST(ContainerLayoutTests, JsonRoundTripPreservesHierarchyAndAppearance)
{
    const nlohmann::json page = nlohmann::json::parse(R"json({
      "containers": [
        {
          "id": "root", "name": "Root", "type": "linear", "sizeMode": "fitHorizontal",
          "position": [0.1, 0.2], "size": [1.0, 0.8],
          "orientation": "vertical", "alignment": "stretch", "spacing": 0.02,
          "appearance": {"backgroundVisible": true, "borderVisible": true},
          "scrolling": {"horizontal": "automatic", "vertical": "always", "offset": [0.1, 0.2]},
          "children": [{"id": "child", "resizeMode": "proportional", "renderOrder": 3}]
        },
        {"id": "child", "type": "grid", "grid": {"rows": 2, "columns": 3}}
      ]
    })json");
    const std::vector<mfd::api1::ContainerDefinition> parsed = mfd::api1::ParsePageContainers(page);
    ASSERT_EQ(parsed.size(), 2U);
    const nlohmann::json serialized = mfd::api1::SerializePageContainers(parsed);
    const std::vector<mfd::api1::ContainerDefinition> reparsed =
        mfd::api1::ParsePageContainers(nlohmann::json {{"containers", serialized}});
    ASSERT_EQ(reparsed.size(), 2U);
    EXPECT_EQ(reparsed.front().children.front().childId, "child");
    EXPECT_EQ(reparsed.front().orientation, mfd::api1::LinearOrientation::Vertical);
    EXPECT_EQ(reparsed.front().sizeMode, mfd::api1::ContainerSizeMode::FitHorizontal);
    EXPECT_TRUE(reparsed.front().appearance.backgroundVisible);
    EXPECT_EQ(reparsed.front().scrolling.vertical, mfd::api1::ScrollMode::AlwaysVisible);
}

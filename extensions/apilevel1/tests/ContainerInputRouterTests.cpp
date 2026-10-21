/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

#include <gtest/gtest.h>

#include <limits>

#include "mfd/apilevel1/ContainerInputRouter.h"
#include "mfd/apilevel1/ContainerLayoutEngine.h"

namespace
{
mfd::api1::ContainerDefinition MakeScrollable(const std::string& id,
                                              const mfd::api1::ContainerRect geometry)
{
    mfd::api1::ContainerDefinition result;
    result.id = id;
    result.name = id;
    result.geometry = geometry;
    result.layoutReferenceSize = geometry.size;
    result.scrolling.vertical = mfd::api1::ScrollMode::Automatic;
    return result;
}
} // namespace

TEST(ContainerInputRouterTests, RoutesToDeepestScrollableRegionAndBubblesAtBoundary)
{
    auto parent = MakeScrollable("parent", {{0.0f, 0.0f}, {1.0f, 1.0f}});
    auto child = MakeScrollable("child", {{0.0f, 0.0f}, {0.6f, 1.2f}});
    auto overflow = MakeScrollable("overflow", {{0.0f, -0.5f}, {0.5f, 1.8f}});
    overflow.scrolling.vertical = mfd::api1::ScrollMode::Disabled;
    parent.children.push_back({"child"});
    child.children.push_back({"overflow"});
    std::vector<mfd::api1::ContainerDefinition> definitions {parent, child, overflow};

    mfd::api1::ContainerLayoutEngine engine;
    std::vector<mfd::api1::ContainerLayoutNode> layouts;
    std::string error;
    ASSERT_TRUE(engine.Evaluate(definitions, layouts, error)) << error;

    mfd::api1::ContainerInputRouter router;
    mfd::api1::ContainerScrollRequest request;
    ASSERT_TRUE(router.RouteWheel(definitions, layouts, {0.0f, 0.0f}, {0.0f, -1.0f}, 0.1f, request));
    EXPECT_EQ(request.sourceIndex, 1U);
    EXPECT_GT(request.offset.y, 0.0f);

    definitions[1].scrolling.offset.y = 2.0f;
    ASSERT_TRUE(engine.Evaluate(definitions, layouts, error)) << error;
    ASSERT_TRUE(router.RouteWheel(definitions, layouts, {0.0f, 0.0f}, {0.0f, -1.0f}, 0.1f, request));
    EXPECT_EQ(request.sourceIndex, 0U);
}

TEST(ContainerInputRouterTests, MapsThumbCoordinatesAndRejectsDisabledInput)
{
    auto parent = MakeScrollable("parent", {{0.0f, 0.0f}, {1.0f, 1.0f}});
    auto overflow = MakeScrollable("overflow", {{0.0f, -0.5f}, {0.5f, 2.0f}});
    overflow.scrolling.vertical = mfd::api1::ScrollMode::Disabled;
    parent.children.push_back({"overflow"});
    std::vector<mfd::api1::ContainerDefinition> definitions {parent, overflow};

    mfd::api1::ContainerLayoutEngine engine;
    std::vector<mfd::api1::ContainerLayoutNode> layouts;
    std::string error;
    ASSERT_TRUE(engine.Evaluate(definitions, layouts, error)) << error;
    ASSERT_TRUE(layouts.front().verticalScrollbar.visible);

    mfd::api1::ContainerInputRouter router;
    const auto& scrollbar = layouts.front().verticalScrollbar;
    const float bottomCenter = scrollbar.track.center.y -
                               (scrollbar.track.size.y - scrollbar.thumb.size.y) * 0.5f;
    EXPECT_NEAR(router.OffsetForThumb(layouts.front(), false, bottomCenter),
                layouts.front().contentSize.y - layouts.front().viewport.size.y,
                0.0001f);

    definitions.front().enabled = false;
    ASSERT_TRUE(engine.Evaluate(definitions, layouts, error)) << error;
    mfd::api1::ContainerScrollRequest request;
    EXPECT_FALSE(router.RouteWheel(definitions, layouts, {0.0f, 0.0f}, {0.0f, -1.0f}, 0.1f, request));
}

TEST(ContainerInputRouterTests, RejectsPointersOutsideAnAncestorClip)
{
    auto parent = MakeScrollable("parent", {{0.0f, 0.0f}, {1.0f, 1.0f}});
    parent.scrolling.clipContent = true;
    auto child = MakeScrollable("child", {{0.6f, 0.0f}, {0.4f, 0.4f}});
    parent.children.push_back({"child"});
    std::vector<mfd::api1::ContainerDefinition> definitions {parent, child};

    mfd::api1::ContainerLayoutEngine engine;
    std::vector<mfd::api1::ContainerLayoutNode> layouts;
    std::string error;
    ASSERT_TRUE(engine.Evaluate(definitions, layouts, error)) << error;
    ASSERT_EQ(layouts.size(), 2U);

    const mfd::api1::ContainerInputRouter router;
    EXPECT_TRUE(router.CanReceivePointer(definitions, layouts, layouts[1], {0.45f, 0.0f}));
    EXPECT_FALSE(router.CanReceivePointer(definitions, layouts, layouts[1], {0.70f, 0.0f}));
    EXPECT_FALSE(router.CanReceivePointer(
        definitions,
        layouts,
        layouts[1],
        {std::numeric_limits<float>::quiet_NaN(), 0.0f}));
}

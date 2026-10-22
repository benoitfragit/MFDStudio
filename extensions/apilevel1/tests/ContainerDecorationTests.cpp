/* This file is part of MFDStudio. Project author: Benoit Fra. */
#include "ContainerDecorationRenderer.h"
#include <raylib.h>
#include <gtest/gtest.h>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
std::vector<std::array<int, 4>> clips;
std::vector<int> drawOrder;
int scopeDepth = 0;

void CheckRectangle(const Rectangle rectangle)
{
    EXPECT_TRUE(std::isfinite(rectangle.x));
    EXPECT_TRUE(std::isfinite(rectangle.y));
    EXPECT_TRUE(std::isfinite(rectangle.width));
    EXPECT_TRUE(std::isfinite(rectangle.height));
}

class ContainerDecorationTests : public testing::Test
{
protected:
    void SetUp() override { clips.clear(); drawOrder.clear(); scopeDepth = 0; }
    void TearDown() override { EXPECT_EQ(scopeDepth, 0); }
};
}

// Link this executable against the model, not raylib: these four commands are
// the complete drawing sink used by the real decoration renderer under test.
extern "C" void BeginScissorMode(int x, int y, int width, int height)
{
    EXPECT_EQ(scopeDepth++, 0);
    clips.push_back({x, y, width, height});
}
extern "C" void EndScissorMode() { EXPECT_EQ(--scopeDepth, 0); }
extern "C" void DrawRectangleRounded(Rectangle rect, float roundness, int, Color color)
{
    CheckRectangle(rect);
    EXPECT_TRUE(std::isfinite(roundness));
    drawOrder.push_back(color.r);
}
extern "C" void DrawRectangleRoundedLinesEx(Rectangle rect, float roundness, int, float thickness, Color color)
{
    CheckRectangle(rect);
    EXPECT_TRUE(std::isfinite(roundness));
    EXPECT_TRUE(std::isfinite(thickness));
    drawOrder.push_back(color.r);
}

TEST_F(ContainerDecorationTests, ExtremeGeometryNeverEscapesViewportScissors)
{
    std::vector<mfd::api1::ContainerDefinition> definitions(2);
    definitions[0].appearance.backgroundVisible = true;
    definitions[0].appearance.borderVisible = true;
    std::vector<mfd::api1::ContainerLayoutNode> layouts(2);
    layouts[0].sourceIndex = 0U;
    layouts[0].effectiveVisible = true;
    layouts[1].sourceIndex = 1U;
    layouts[1].parentSourceIndex = 0U;
    layouts[1].effectiveVisible = true;
    constexpr float extreme = std::numeric_limits<float>::max();
    for (const float position : {0.0f, extreme, -extreme, 4294967296.0f,
                                  std::numeric_limits<float>::infinity(),
                                  std::numeric_limits<float>::quiet_NaN()})
    {
        for (const float size : {0.0f, 0.001f, 2.0f, extreme})
        {
            for (const float zoom : {0.000001f, 1.0f, extreme})
            {
                clips.clear();
                layouts[0].bounds = layouts[0].viewport = {{position, position}, {size, size}};
                layouts[1].bounds = layouts[1].viewport = {{0.0f, 0.0f}, {1.0f, 1.0f}};
                mfd::PageViewState view;
                view.zoom = zoom;
                mfd::detail::DrawContainerDecorations(definitions, layouts, view, 1920, 1080, {});
                for (const auto& clip : clips)
                {
                    EXPECT_GE(clip[0], 0); EXPECT_GE(clip[1], 0);
                    EXPECT_GE(clip[2], 0); EXPECT_GE(clip[3], 0);
                    EXPECT_LE(clip[0] + clip[2], 1920);
                    EXPECT_LE(clip[1] + clip[3], 1080);
                }
            }
        }
    }
}

TEST_F(ContainerDecorationTests, IntegerMaximumViewportIsRepresentable)
{
    std::vector<mfd::api1::ContainerDefinition> definitions(1);
    std::vector<mfd::api1::ContainerLayoutNode> layouts(1);
    layouts[0].sourceIndex = 0U;
    layouts[0].effectiveVisible = true;
    const int maximum = std::numeric_limits<int>::max();
    mfd::detail::DrawContainerDecorations(definitions, layouts, {}, maximum, maximum, {});
    ASSERT_FALSE(clips.empty());
    EXPECT_EQ(clips.front(), (std::array<int, 4> {0, 0, maximum, maximum}));
}

TEST_F(ContainerDecorationTests, HiddenGrandchildrenDoNotCloseVisibleAncestor)
{
    std::vector<mfd::api1::ContainerDefinition> definitions(5);
    std::vector<mfd::api1::ContainerLayoutNode> layouts(5);
    for (std::size_t index = 0; index < definitions.size(); ++index)
    {
        definitions[index].appearance.backgroundVisible = true;
        definitions[index].appearance.borderVisible = true;
        definitions[index].appearance.backgroundColor.r = static_cast<std::uint8_t>(index + 1U);
        definitions[index].appearance.borderColor.r = static_cast<std::uint8_t>(index + 11U);
        layouts[index].sourceIndex = index;
        layouts[index].bounds = layouts[index].viewport = {{}, {1.0f, 1.0f}};
        layouts[index].effectiveVisible = index != 1U && index != 2U;
    }
    layouts[1].parentSourceIndex = 0U;
    layouts[2].parentSourceIndex = 1U;
    layouts[3].parentSourceIndex = 0U;
    mfd::detail::DrawContainerDecorations(definitions, layouts, {}, 800, 600, {});
    // Parent, visible child, child's border, parent's border, next root + border.
    EXPECT_EQ(drawOrder, (std::vector<int> {1, 4, 14, 11, 5, 15}));
}

/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests covering private editor helpers shared by the application implementation.
 */

#include "internal/application/EditorApplicationInternal.h"

#include <gtest/gtest.h>

#include <vector>

TEST(EditorApplicationInternalTests, SeedPrimitiveFillColorIfNeededUsesStrokeWhenFillIsTransparent)
{
    mfd::PrimitiveStyle style;
    style.color = mfd::ColorRgba {12, 34, 56, 255};
    style.fillColor = mfd::ColorRgba {0, 0, 0, 0};
    style.filled = true;

    editor::detail::SeedPrimitiveFillColorIfNeeded(style);

    EXPECT_EQ(style.fillColor.r, style.color.r);
    EXPECT_EQ(style.fillColor.g, style.color.g);
    EXPECT_EQ(style.fillColor.b, style.color.b);
    EXPECT_EQ(style.fillColor.a, style.color.a);
}

TEST(EditorApplicationInternalTests, SeedPrimitiveFillColorIfNeededPreservesExistingVisibleFill)
{
    mfd::PrimitiveStyle style;
    style.color = mfd::ColorRgba {12, 34, 56, 255};
    style.fillColor = mfd::ColorRgba {90, 80, 70, 60};
    style.filled = true;

    editor::detail::SeedPrimitiveFillColorIfNeeded(style);

    EXPECT_EQ(style.fillColor.r, 90U);
    EXPECT_EQ(style.fillColor.g, 80U);
    EXPECT_EQ(style.fillColor.b, 70U);
    EXPECT_EQ(style.fillColor.a, 60U);
}

TEST(EditorApplicationInternalTests, SeedReticleFillOverrideIfNeededUsesFallbackStrokeWhenEnabled)
{
    mfd::ReticleStyleOverride overrides;
    overrides.filled = true;

    editor::detail::SeedReticleFillOverrideIfNeeded(overrides, mfd::ColorRgba {10, 20, 30, 255});

    ASSERT_TRUE(overrides.fillColor.has_value());
    EXPECT_EQ(overrides.fillColor->r, 10U);
    EXPECT_EQ(overrides.fillColor->g, 20U);
    EXPECT_EQ(overrides.fillColor->b, 30U);
    EXPECT_EQ(overrides.fillColor->a, 255U);
}

TEST(EditorApplicationInternalTests, ReticleHasFillCapablePrimitiveDetectsRectangleTemplate)
{
    mfd::ReticleGroup reticle;

    mfd::Primitive rectangle;
    rectangle.id = "box";
    rectangle.type = mfd::PrimitiveType::Rectangle;
    rectangle.geometry = mfd::RectangleGeometry {0.20f, 0.10f};
    reticle.primitives.push_back(rectangle);

    EXPECT_TRUE(editor::detail::ReticleHasFillCapablePrimitive(reticle));
}

TEST(EditorApplicationInternalTests, ReticleHasFillCapablePrimitiveRejectsLineOnlyTemplate)
{
    mfd::ReticleGroup reticle;

    mfd::Primitive line;
    line.id = "guide";
    line.type = mfd::PrimitiveType::Line;
    line.geometry = mfd::LineGeometry {};
    reticle.primitives.push_back(line);

    EXPECT_FALSE(editor::detail::ReticleHasFillCapablePrimitive(reticle));
}

TEST(EditorApplicationInternalTests, ReticleLibraryIdExistsNormalizedDetectsCaseAndWhitespaceCollisions)
{
    mfd::ReticleLibrary library;

    mfd::ReticleGroup reticle;
    reticle.id = "Radar Track";
    library.emplace(reticle.id, reticle);

    EXPECT_TRUE(editor::detail::ReticleLibraryIdExistsNormalized(library, "radar track"));
    EXPECT_TRUE(editor::detail::ReticleLibraryIdExistsNormalized(library, " Radar Track "));
    EXPECT_FALSE(editor::detail::ReticleLibraryIdExistsNormalized(library, "nav_track"));
}

TEST(EditorApplicationInternalTests, MakeUniqueLibraryReticleIdSkipsNormalizedCollisions)
{
    mfd::ReticleLibrary library;

    mfd::ReticleGroup original;
    original.id = "Radar Track";
    library.emplace(original.id, original);

    mfd::ReticleGroup sibling;
    sibling.id = "radar track_1";
    library.emplace(sibling.id, sibling);

    EXPECT_EQ(editor::detail::MakeUniqueLibraryReticleId(library, "radar track"), "radar track_2");
}

TEST(EditorApplicationInternalTests, MakeUniquePageReticleInstanceIdSkipsStaticReticleAndStrobeCollisions)
{
    mfd::PageDefinition page;

    mfd::ReticleGroup reticle;
    reticle.id = "Ownship";
    page.staticReticles.push_back(reticle);

    mfd::PageStrobeDefinition strobe;
    strobe.reticle.id = "ownship_1";
    page.strobes.push_back(strobe);

    EXPECT_EQ(editor::detail::MakeUniquePageReticleInstanceId(page, "ownship"), "ownship_2");
}

TEST(EditorApplicationInternalTests, MakeUniquePageReticleInstanceIdAllowsEditingCurrentReticleWithoutSuffix)
{
    mfd::PageDefinition page;

    mfd::ReticleGroup reticle;
    reticle.id = "Page1Ownship";
    page.staticReticles.push_back(reticle);

    mfd::ReticleGroup sibling;
    sibling.id = "CircleMask";
    page.staticReticles.push_back(sibling);

    EXPECT_EQ(editor::detail::MakeUniquePageReticleInstanceId(page, "Page1Ownship", "Page1Ownship"), "Page1Ownship");
}

TEST(EditorApplicationInternalTests, MakeUniquePageReticleInstanceIdFallsBackToReticleWhenRequestedIdIsBlank)
{
    mfd::PageDefinition page;

    EXPECT_EQ(editor::detail::MakeUniquePageReticleInstanceId(page, "   "), "reticle");
}

TEST(EditorApplicationInternalTests, ShouldAdvanceTutorialOnSuccessfulSaveAcceptsShortcutOnTutorialSaveStep)
{
    EXPECT_TRUE(editor::detail::ShouldAdvanceTutorialOnSuccessfulSave(true, true, false));
}

TEST(EditorApplicationInternalTests, ShouldAdvanceTutorialOnSuccessfulSaveRequiresSuccessfulSave)
{
    EXPECT_FALSE(editor::detail::ShouldAdvanceTutorialOnSuccessfulSave(false, true, true));
}

TEST(EditorApplicationInternalTests, ShouldAdvanceTutorialOnSuccessfulSaveRejectsUnrelatedSave)
{
    EXPECT_FALSE(editor::detail::ShouldAdvanceTutorialOnSuccessfulSave(true, false, false));
}

namespace
{
mfd::PageDefinition MakeThreeLayerPage()
{
    mfd::PageDefinition page;
    page.layers = {
        mfd::PageLayerDefinition {"background"},
        mfd::PageLayerDefinition {"symbols"},
        mfd::PageLayerDefinition {"overlay"}};
    page.editor.layers = {
        mfd::EditorLayerDefinition {"background", true},
        mfd::EditorLayerDefinition {"symbols", false},
        mfd::EditorLayerDefinition {"overlay", true}};

    mfd::ReticleGroup symbolReticle;
    symbolReticle.id = "track";
    symbolReticle.layerId = "symbols";
    page.staticReticles.push_back(symbolReticle);

    mfd::DynamicReticleLayerBinding binding;
    binding.templateId = "radar";
    binding.layerId = "symbols";
    binding.orderInLayer = 7;
    page.dynamicReticleBindings.push_back(binding);

    return page;
}

std::vector<std::string> LayerIds(const mfd::PageDefinition& page)
{
    std::vector<std::string> ids;
    ids.reserve(page.layers.size());
    for (const mfd::PageLayerDefinition& layer : page.layers)
    {
        ids.push_back(layer.id);
    }
    return ids;
}

std::vector<std::string> EditorLayerIds(const mfd::PageDefinition& page)
{
    std::vector<std::string> ids;
    ids.reserve(page.editor.layers.size());
    for (const mfd::EditorLayerDefinition& layer : page.editor.layers)
    {
        ids.push_back(layer.id);
    }
    return ids;
}
} // namespace

TEST(EditorApplicationInternalTests, MovePageLayerUpMovesRuntimeAndEditorVectorsTogether)
{
    mfd::PageDefinition page = MakeThreeLayerPage();

    EXPECT_TRUE(editor::detail::MovePageLayer(page, 1U, editor::detail::PageLayerMoveDirection::Up));

    EXPECT_EQ(LayerIds(page), (std::vector<std::string> {"symbols", "background", "overlay"}));
    EXPECT_EQ(EditorLayerIds(page), (std::vector<std::string> {"symbols", "background", "overlay"}));
}

TEST(EditorApplicationInternalTests, MovePageLayerDownMovesRuntimeAndEditorVectorsTogether)
{
    mfd::PageDefinition page = MakeThreeLayerPage();

    EXPECT_TRUE(editor::detail::MovePageLayer(page, 1U, editor::detail::PageLayerMoveDirection::Down));

    EXPECT_EQ(LayerIds(page), (std::vector<std::string> {"background", "overlay", "symbols"}));
    EXPECT_EQ(EditorLayerIds(page), (std::vector<std::string> {"background", "overlay", "symbols"}));
}

TEST(EditorApplicationInternalTests, MovePageLayerFirstUpIsNoOp)
{
    mfd::PageDefinition page = MakeThreeLayerPage();

    EXPECT_FALSE(editor::detail::MovePageLayer(page, 0U, editor::detail::PageLayerMoveDirection::Up));
    EXPECT_EQ(LayerIds(page), (std::vector<std::string> {"background", "symbols", "overlay"}));
}

TEST(EditorApplicationInternalTests, MovePageLayerLastDownIsNoOp)
{
    mfd::PageDefinition page = MakeThreeLayerPage();

    EXPECT_FALSE(editor::detail::MovePageLayer(page, 2U, editor::detail::PageLayerMoveDirection::Down));
    EXPECT_EQ(LayerIds(page), (std::vector<std::string> {"background", "symbols", "overlay"}));
}

TEST(EditorApplicationInternalTests, MovePageLayerInvalidIndexIsNoOp)
{
    mfd::PageDefinition page = MakeThreeLayerPage();

    EXPECT_FALSE(editor::detail::MovePageLayer(page, 9U, editor::detail::PageLayerMoveDirection::Up));
    EXPECT_EQ(LayerIds(page), (std::vector<std::string> {"background", "symbols", "overlay"}));
}

TEST(EditorApplicationInternalTests, MovePageLayerKeepsEditorVisibilityWithItsLayer)
{
    mfd::PageDefinition page = MakeThreeLayerPage();

    ASSERT_TRUE(editor::detail::MovePageLayer(page, 1U, editor::detail::PageLayerMoveDirection::Up));

    const mfd::EditorLayerDefinition* symbols = nullptr;
    for (const mfd::EditorLayerDefinition& layer : page.editor.layers)
    {
        if (layer.id == "symbols")
        {
            symbols = &layer;
        }
    }
    ASSERT_NE(symbols, nullptr);
    EXPECT_FALSE(symbols->visible);
}

TEST(EditorApplicationInternalTests, MovePageLayerPreservesReticleAndBindingLayerReferences)
{
    mfd::PageDefinition page = MakeThreeLayerPage();

    ASSERT_TRUE(editor::detail::MovePageLayer(page, 1U, editor::detail::PageLayerMoveDirection::Down));

    ASSERT_EQ(page.staticReticles.size(), 1U);
    EXPECT_EQ(page.staticReticles.front().layerId, "symbols");
    ASSERT_EQ(page.dynamicReticleBindings.size(), 1U);
    EXPECT_EQ(page.dynamicReticleBindings.front().layerId, "symbols");
    EXPECT_EQ(page.dynamicReticleBindings.front().orderInLayer, 7);
}

TEST(EditorApplicationInternalTests, ForEachPrimitiveBoundsLocalPointEnumeratesImageCorners)
{
    mfd::Primitive primitive;
    mfd::ImageGeometry image;
    image.width = 0.40f;
    image.height = 0.20f;
    primitive.id = "image";
    primitive.type = mfd::PrimitiveType::Image;
    primitive.geometry = image;

    std::vector<mfd::Vec2> points;
    editor::detail::ForEachPrimitiveBoundsLocalPoint(
        primitive,
        [&points](const mfd::Vec2 point)
        {
            points.push_back(point);
        });

    ASSERT_EQ(points.size(), 4U);
    EXPECT_FLOAT_EQ(points[0].x, -0.20f);
    EXPECT_FLOAT_EQ(points[0].y, -0.10f);
    EXPECT_FLOAT_EQ(points[1].x, 0.20f);
    EXPECT_FLOAT_EQ(points[1].y, -0.10f);
    EXPECT_FLOAT_EQ(points[2].x, 0.20f);
    EXPECT_FLOAT_EQ(points[2].y, 0.10f);
    EXPECT_FLOAT_EQ(points[3].x, -0.20f);
    EXPECT_FLOAT_EQ(points[3].y, 0.10f);
}

TEST(EditorApplicationInternalTests, ForEachPrimitiveBoundsLocalPointSkipsInvisibleImage)
{
    mfd::Primitive primitive;
    mfd::ImageGeometry image;
    image.width = 0.40f;
    image.height = 0.20f;
    primitive.id = "image";
    primitive.type = mfd::PrimitiveType::Image;
    primitive.style.visible = false;
    primitive.geometry = image;

    std::vector<mfd::Vec2> points;
    editor::detail::ForEachPrimitiveBoundsLocalPoint(
        primitive,
        [&points](const mfd::Vec2 point)
        {
            points.push_back(point);
        });

    EXPECT_TRUE(points.empty());
}

TEST(EditorApplicationInternalTests, ForEachPrimitiveBoundsLocalPointKeepsCenteredTextBoundsByDefault)
{
    mfd::Primitive primitive;
    primitive.id = "caption";
    primitive.type = mfd::PrimitiveType::Text;
    primitive.geometry = mfd::TextGeometry {"UTC", 0.04f, 0.002f};

    std::vector<mfd::Vec2> points;
    editor::detail::ForEachPrimitiveBoundsLocalPoint(
        primitive,
        [&points](const mfd::Vec2 point)
        {
            points.push_back(point);
        });

    const auto& text = std::get<mfd::TextGeometry>(primitive.geometry);
    const float halfWidth = editor::detail::EstimatePrimitiveTextHalfWidth(text);
    const float halfHeight = editor::detail::EstimatePrimitiveTextHalfHeight(text);

    ASSERT_EQ(points.size(), 4U);
    EXPECT_FLOAT_EQ(points[0].x, -halfWidth);
    EXPECT_FLOAT_EQ(points[0].y, -halfHeight);
    EXPECT_FLOAT_EQ(points[1].x, halfWidth);
    EXPECT_FLOAT_EQ(points[1].y, -halfHeight);
    EXPECT_FLOAT_EQ(points[2].x, halfWidth);
    EXPECT_FLOAT_EQ(points[2].y, halfHeight);
    EXPECT_FLOAT_EQ(points[3].x, -halfWidth);
    EXPECT_FLOAT_EQ(points[3].y, halfHeight);
}

TEST(EditorApplicationInternalTests, ForEachPrimitiveBoundsLocalPointHonorsLeftAndRightTextAlignment)
{
    mfd::Primitive leftPrimitive;
    leftPrimitive.id = "left_caption";
    leftPrimitive.type = mfd::PrimitiveType::Text;
    leftPrimitive.geometry = mfd::TextGeometry {"UTC", 0.04f, 0.002f, mfd::Align::Left};

    mfd::Primitive rightPrimitive;
    rightPrimitive.id = "right_caption";
    rightPrimitive.type = mfd::PrimitiveType::Time;
    rightPrimitive.geometry = mfd::TimeGeometry {"%H:%M:%S", true, 0.04f, 0.002f, mfd::Align::Right};

    std::vector<mfd::Vec2> leftPoints;
    editor::detail::ForEachPrimitiveBoundsLocalPoint(
        leftPrimitive,
        [&leftPoints](const mfd::Vec2 point)
        {
            leftPoints.push_back(point);
        });

    std::vector<mfd::Vec2> rightPoints;
    editor::detail::ForEachPrimitiveBoundsLocalPoint(
        rightPrimitive,
        [&rightPoints](const mfd::Vec2 point)
        {
            rightPoints.push_back(point);
        });

    const auto& leftText = std::get<mfd::TextGeometry>(leftPrimitive.geometry);
    const auto& rightTime = std::get<mfd::TimeGeometry>(rightPrimitive.geometry);
    const float leftHalfWidth = editor::detail::EstimatePrimitiveTextHalfWidth(leftText);
    const float leftHalfHeight = editor::detail::EstimatePrimitiveTextHalfHeight(leftText);
    const float leftWidth = editor::detail::EstimatedPrimitiveTextWidth(leftHalfWidth);
    const float rightHalfWidth = editor::detail::EstimatePrimitiveTextHalfWidth(rightTime);
    const float rightHalfHeight = editor::detail::EstimatePrimitiveTextHalfHeight(rightTime);
    const float rightWidth = editor::detail::EstimatedPrimitiveTextWidth(rightHalfWidth);

    ASSERT_EQ(leftPoints.size(), 4U);
    EXPECT_FLOAT_EQ(leftPoints[0].x, 0.0f);
    EXPECT_FLOAT_EQ(leftPoints[0].y, -leftHalfHeight);
    EXPECT_FLOAT_EQ(leftPoints[1].x, leftWidth);
    EXPECT_FLOAT_EQ(leftPoints[2].x, leftWidth);
    EXPECT_FLOAT_EQ(leftPoints[3].x, 0.0f);

    ASSERT_EQ(rightPoints.size(), 4U);
    EXPECT_FLOAT_EQ(rightPoints[0].x, -rightWidth);
    EXPECT_FLOAT_EQ(rightPoints[0].y, -rightHalfHeight);
    EXPECT_FLOAT_EQ(rightPoints[1].x, 0.0f);
    EXPECT_FLOAT_EQ(rightPoints[2].x, 0.0f);
    EXPECT_FLOAT_EQ(rightPoints[3].x, -rightWidth);
}

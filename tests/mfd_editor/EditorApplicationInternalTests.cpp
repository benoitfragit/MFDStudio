/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests covering private editor helpers shared by the application implementation.
 */

#include "EditorApplicationInternal.h"

#include <gtest/gtest.h>

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
        [&](const mfd::Vec2 point)
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
        [&](const mfd::Vec2 point)
        {
            points.push_back(point);
        });

    EXPECT_TRUE(points.empty());
}

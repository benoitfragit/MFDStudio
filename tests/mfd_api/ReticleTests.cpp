/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for ReticleTests.
 */

#include <gtest/gtest.h>

#include "mfd/model/Reticle.h"

namespace
{
mfd::Primitive MakePrimitive(const std::string_view id, const mfd::PrimitiveType type)
{
    mfd::Primitive primitive;
    primitive.id = std::string(id);
    primitive.type = type;

    switch (type)
    {
    case mfd::PrimitiveType::Circle:
        primitive.geometry = mfd::CircleGeometry {};
        break;
    case mfd::PrimitiveType::Ring:
        primitive.geometry = mfd::RingGeometry {};
        break;
    case mfd::PrimitiveType::Rectangle:
        primitive.geometry = mfd::RectangleGeometry {};
        break;
    case mfd::PrimitiveType::Ellipse:
        primitive.geometry = mfd::EllipseGeometry {};
        break;
    case mfd::PrimitiveType::Square:
        primitive.geometry = mfd::SquareGeometry {};
        break;
    case mfd::PrimitiveType::Triangle:
        primitive.geometry = mfd::TriangleGeometry {};
        break;
    case mfd::PrimitiveType::Line:
        primitive.geometry = mfd::LineGeometry {};
        break;
    case mfd::PrimitiveType::Arc:
        primitive.geometry = mfd::ArcGeometry {};
        break;
    default:
        primitive.geometry = mfd::PolylineGeometry {};
        break;
    }

    return primitive;
}
} // namespace

TEST(ReticleTests, SupportsReticleClipPrimitiveOnlyForSupportedConvexShapes)
{
    EXPECT_TRUE(mfd::SupportsReticleClipPrimitive(MakePrimitive("circle", mfd::PrimitiveType::Circle)));
    EXPECT_TRUE(mfd::SupportsReticleClipPrimitive(MakePrimitive("rectangle", mfd::PrimitiveType::Rectangle)));
    EXPECT_TRUE(mfd::SupportsReticleClipPrimitive(MakePrimitive("ellipse", mfd::PrimitiveType::Ellipse)));
    EXPECT_TRUE(mfd::SupportsReticleClipPrimitive(MakePrimitive("square", mfd::PrimitiveType::Square)));
    EXPECT_TRUE(mfd::SupportsReticleClipPrimitive(MakePrimitive("triangle", mfd::PrimitiveType::Triangle)));
    EXPECT_FALSE(mfd::SupportsReticleClipPrimitive(MakePrimitive("line", mfd::PrimitiveType::Line)));
    EXPECT_FALSE(mfd::SupportsReticleClipPrimitive(MakePrimitive("ring", mfd::PrimitiveType::Ring)));
    EXPECT_FALSE(mfd::SupportsReticleClipPrimitive(MakePrimitive("arc", mfd::PrimitiveType::Arc)));
    EXPECT_FALSE(mfd::SupportsReticleClipPrimitive(MakePrimitive("poly", mfd::PrimitiveType::Polyline)));
}

TEST(ReticleTests, ResolveClipPrimitiveReturnsSupportedReferencedPrimitiveOnly)
{
    mfd::ReticleGroup reticle;
    reticle.primitives.push_back(MakePrimitive("outline", mfd::PrimitiveType::Rectangle));
    reticle.primitives.push_back(MakePrimitive("cross", mfd::PrimitiveType::Line));

    reticle.clipping.mode = mfd::ReticleClipMode::Outer;
    reticle.clipping.primitiveId = "outline";

    const mfd::Primitive* resolved = mfd::ResolveClipPrimitive(reticle);
    ASSERT_NE(resolved, nullptr);
    EXPECT_EQ(resolved->id, "outline");

    reticle.clipping.primitiveId = "cross";
    EXPECT_EQ(mfd::ResolveClipPrimitive(reticle), nullptr);

    reticle.clipping.primitiveId = "missing";
    EXPECT_EQ(mfd::ResolveClipPrimitive(reticle), nullptr);

    reticle.clipping.mode = mfd::ReticleClipMode::None;
    reticle.clipping.primitiveId = "outline";
    EXPECT_EQ(mfd::ResolveClipPrimitive(reticle), nullptr);
}

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

TEST(ReticleTests, ResolvePrimitiveWorldTransformKeepsParentAnchorButCanMaskGeometrySensitivity)
{
    mfd::ReticleGroup reticle;
    reticle.transform.position = {10.0f, 20.0f};
    reticle.transform.rotationDegrees = 90.0f;
    reticle.transform.scale = {2.0f, 3.0f};

    mfd::Primitive primitive = MakePrimitive("label", mfd::PrimitiveType::Text);
    primitive.transform.position = {1.0f, 0.0f};
    primitive.transform.rotationDegrees = 15.0f;
    primitive.transform.scale = {4.0f, 5.0f};

    const mfd::Transform2D inherited = mfd::ResolvePrimitiveWorldTransform(primitive, reticle);
    EXPECT_FLOAT_EQ(inherited.position.x, 10.0f);
    EXPECT_FLOAT_EQ(inherited.position.y, 22.0f);
    EXPECT_FLOAT_EQ(inherited.rotationDegrees, 105.0f);
    EXPECT_FLOAT_EQ(inherited.scale.x, 8.0f);
    EXPECT_FLOAT_EQ(inherited.scale.y, 15.0f);

    primitive.reticleRotationSensitive = false;
    primitive.reticleScaleSensitive = false;
    const mfd::Transform2D masked = mfd::ResolvePrimitiveWorldTransform(primitive, reticle);
    EXPECT_FLOAT_EQ(masked.position.x, 10.0f);
    EXPECT_FLOAT_EQ(masked.position.y, 22.0f);
    EXPECT_FLOAT_EQ(masked.rotationDegrees, 15.0f);
    EXPECT_FLOAT_EQ(masked.scale.x, 4.0f);
    EXPECT_FLOAT_EQ(masked.scale.y, 5.0f);
}

TEST(ReticleTests, ApplyPrimitiveWorldTransformCanKeepGeometryUprightWhileAnchorFollowsReticle)
{
    mfd::ReticleGroup reticle;
    reticle.transform.rotationDegrees = 90.0f;
    reticle.transform.scale = {2.0f, 2.0f};

    mfd::Primitive primitive = MakePrimitive("label", mfd::PrimitiveType::Text);
    primitive.transform.position = {1.0f, 0.0f};
    primitive.reticleRotationSensitive = false;
    primitive.reticleScaleSensitive = false;

    const mfd::Vec2 anchor = mfd::ApplyPrimitiveWorldTransform({}, primitive, reticle);
    const mfd::Vec2 point = mfd::ApplyPrimitiveWorldTransform({1.0f, 0.0f}, primitive, reticle);

    EXPECT_NEAR(anchor.x, 0.0f, 1.0e-5f);
    EXPECT_NEAR(anchor.y, 2.0f, 1.0e-5f);
    EXPECT_NEAR(point.x, 1.0f, 1.0e-5f);
    EXPECT_NEAR(point.y, 2.0f, 1.0e-5f);
    EXPECT_FLOAT_EQ(mfd::PrimitiveAverageScale(primitive, reticle), 1.0f);

    primitive.reticleRotationSensitive = true;
    primitive.reticleScaleSensitive = true;
    const mfd::Vec2 inheritedPoint = mfd::ApplyPrimitiveWorldTransform({1.0f, 0.0f}, primitive, reticle);
    EXPECT_NEAR(inheritedPoint.x, 0.0f, 1.0e-5f);
    EXPECT_NEAR(inheritedPoint.y, 4.0f, 1.0e-5f);
    EXPECT_FLOAT_EQ(mfd::PrimitiveAverageScale(primitive, reticle), 2.0f);
}

/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for shared runtime validation budgets.
 */

#include <gtest/gtest.h>

#include <limits>
#include <string>
#include <vector>

#include "mfd/model/RuntimeBudgets.h"
#include "mfd/model/internal/RuntimeModelValidation.h"
#include "mfd/ipc/UdpLimits.h"

TEST(RuntimeValidationTests, Vec2ValidationRejectsNonFiniteAndOutOfBudgetCoordinates)
{
    using namespace mfd::runtime_validation;

    EXPECT_TRUE(IsFiniteVec2({0.0f, 0.0f}));
    EXPECT_TRUE(IsValidVec2({kMaxAbsCoordinate, -kMaxAbsCoordinate}));
    EXPECT_FALSE(IsFiniteVec2({std::numeric_limits<float>::infinity(), 0.0f}));
    EXPECT_FALSE(IsValidVec2({kMaxAbsCoordinate + 1.0f, 0.0f}));
    EXPECT_FALSE(IsValidScale({kMaxAbsScale + 1.0f, 1.0f}));
}

TEST(RuntimeValidationTests, PayloadValidationRejectsOversizedTextAndPointLists)
{
    using namespace mfd::runtime_validation;

    EXPECT_TRUE(IsValidTextPayload(std::string(kMaxTextBytes, 'x')));
    EXPECT_FALSE(IsValidTextPayload(std::string(kMaxTextBytes + 1U, 'x')));

    std::vector<mfd::Vec2> validPoints = {{0.0f, 0.0f}, {0.25f, -0.25f}};
    EXPECT_TRUE(AreValidPoints(validPoints, 2U));

    validPoints.push_back({0.5f, 0.5f});
    EXPECT_TRUE(AreValidPoints(validPoints, 2U));

    std::vector<mfd::Vec2> oversizedPoints(kMaxPrimitivePoints + 1U, mfd::Vec2 {});
    EXPECT_FALSE(AreValidPoints(oversizedPoints, 2U));

    std::vector<mfd::Vec2> invalidPoints = {{0.0f, 0.0f}, {std::numeric_limits<float>::quiet_NaN(), 0.0f}};
    EXPECT_FALSE(AreValidPoints(invalidPoints, 2U));
}

TEST(RuntimeValidationTests, AuthoringBoundariesMatchRuntimeLimits)
{
    using mfd::runtime_validation::IsValidPrimitiveForRuntime;
    using mfd::runtime_validation::IsValidWindowExtent;
    using mfd::runtime_validation::kMaxBezierControlPoints;
    using mfd::runtime_validation::kMaxFilledPolygonPoints;
    using mfd::runtime_validation::kMaxPrimitivePoints;
    using mfd::runtime_validation::kMaxPrimitiveSegments;
    using mfd::runtime_validation::kMaxWindowExtent;

    EXPECT_TRUE(IsValidWindowExtent(1));
    EXPECT_FALSE(IsValidWindowExtent(0));
    EXPECT_TRUE(IsValidWindowExtent(kMaxWindowExtent));
    EXPECT_FALSE(IsValidWindowExtent(kMaxWindowExtent + 1));
    EXPECT_TRUE(mfd::IsValidUdpPayloadSize(mfd::kUdpMinPayloadBytes));
    EXPECT_FALSE(mfd::IsValidUdpPayloadSize(mfd::kUdpMinPayloadBytes - 1U));
    EXPECT_TRUE(mfd::IsValidUdpPayloadSize(mfd::kUdpMaxPayloadBytes));
    EXPECT_FALSE(mfd::IsValidUdpPayloadSize(mfd::kUdpMaxPayloadBytes + 1U));

    mfd::Primitive polylinePrimitive;
    polylinePrimitive.type = mfd::PrimitiveType::Polyline;
    polylinePrimitive.geometry = mfd::PolylineGeometry {
        std::vector<mfd::Vec2>(kMaxPrimitivePoints, mfd::Vec2 {}), false};
    EXPECT_TRUE(IsValidPrimitiveForRuntime(polylinePrimitive));
    std::get<mfd::PolylineGeometry>(polylinePrimitive.geometry).points.push_back({});
    EXPECT_FALSE(IsValidPrimitiveForRuntime(polylinePrimitive));

    polylinePrimitive.geometry = mfd::PolylineGeometry {
        std::vector<mfd::Vec2>(kMaxFilledPolygonPoints, mfd::Vec2 {}), true};
    EXPECT_TRUE(IsValidPrimitiveForRuntime(polylinePrimitive));
    std::get<mfd::PolylineGeometry>(polylinePrimitive.geometry).points.push_back({});
    EXPECT_FALSE(IsValidPrimitiveForRuntime(polylinePrimitive));

    mfd::Primitive bezierPrimitive;
    bezierPrimitive.type = mfd::PrimitiveType::Bezier;
    bezierPrimitive.geometry = mfd::BezierGeometry {
        std::vector<mfd::Vec2>(kMaxBezierControlPoints, mfd::Vec2 {}), kMaxPrimitiveSegments};
    EXPECT_TRUE(IsValidPrimitiveForRuntime(bezierPrimitive));
    std::get<mfd::BezierGeometry>(bezierPrimitive.geometry).controlPoints.push_back({});
    EXPECT_FALSE(IsValidPrimitiveForRuntime(bezierPrimitive));
    std::get<mfd::BezierGeometry>(bezierPrimitive.geometry).controlPoints.pop_back();
    std::get<mfd::BezierGeometry>(bezierPrimitive.geometry).segments = kMaxPrimitiveSegments + 1;
    EXPECT_FALSE(IsValidPrimitiveForRuntime(bezierPrimitive));
}

TEST(RuntimeValidationTests, PrimitiveValidationRejectsNonFiniteProgrammaticValues)
{
    mfd::Primitive primitive;
    primitive.type = mfd::PrimitiveType::Line;
    primitive.geometry = mfd::LineGeometry {};
    EXPECT_TRUE(mfd::runtime_validation::IsValidPrimitiveForRuntime(primitive));

    primitive.transform.position.x = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(mfd::runtime_validation::IsValidPrimitiveForRuntime(primitive));
    primitive.transform.position.x = 0.0f;
    std::get<mfd::LineGeometry>(primitive.geometry).end.y = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(mfd::runtime_validation::IsValidPrimitiveForRuntime(primitive));
}

TEST(RuntimeValidationTests, PrimitiveValidationRejectsTypeMismatchAndNonUniformSquare)
{
    mfd::Primitive primitive;
    primitive.type = mfd::PrimitiveType::Circle;
    primitive.geometry = mfd::LineGeometry {};
    EXPECT_FALSE(mfd::runtime_validation::IsValidPrimitiveForRuntime(primitive));

    primitive.type = mfd::PrimitiveType::Square;
    primitive.geometry = mfd::SquareGeometry {1.0f, 2.0f};
    EXPECT_FALSE(mfd::runtime_validation::IsValidPrimitiveForRuntime(primitive));
    primitive.geometry = mfd::SquareGeometry {1.0f, 1.0f};
    EXPECT_TRUE(mfd::runtime_validation::IsValidPrimitiveForRuntime(primitive));
}

TEST(RuntimeValidationTests, ModelValidationCoversReticlePageAndStrobeValues)
{
    using mfd::runtime_validation::internal::IsValidPage;
    using mfd::runtime_validation::internal::IsValidReticle;

    mfd::ReticleGroup reticle;
    EXPECT_TRUE(IsValidReticle(reticle));
    reticle.transform.scale.x = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(IsValidReticle(reticle));
    reticle.transform.scale.x = 1.0f;
    reticle.overrides.thickness = 0.0f;
    EXPECT_FALSE(IsValidReticle(reticle));
    reticle.overrides.thickness.reset();
    reticle.clipping.primitiveId.assign(mfd::runtime_validation::kMaxTextBytes + 1U, 'x');
    EXPECT_FALSE(IsValidReticle(reticle));

    mfd::PageDefinition page;
    EXPECT_TRUE(IsValidPage(page));
    page.view.zoom = 0.0f;
    EXPECT_FALSE(IsValidPage(page));
    page.view.zoom = 1.0f;
    page.strobes.emplace_back();
    page.strobes.front().capture.size.x = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(IsValidPage(page));
    page.strobes.front().capture.size.x = 0.1f;
    page.strobes.front().magnet.strength = 1.01f;
    EXPECT_FALSE(IsValidPage(page));
}

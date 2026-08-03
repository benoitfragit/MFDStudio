/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for AnimationTests.
 */

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "mfd/client/Animation.h"

namespace
{
class PrimitiveFixtureReticle final : public mfd::client::Reticle
{
public:
    PrimitiveFixtureReticle()
        : mfd::client::Reticle("Radar", "fixture"),
          headingValue(MutableDesiredPatch(), DirtyFlag(), "heading_value"),
          horizonLine(MutableDesiredPatch(), DirtyFlag(), "horizon_line"),
          compassRing(MutableDesiredPatch(), DirtyFlag(), "compass_ring"),
          lockBox(MutableDesiredPatch(), DirtyFlag(), "lock_box"),
          warningTriangle(MutableDesiredPatch(), DirtyFlag(), "warning_triangle"),
          routePolyline(MutableDesiredPatch(), DirtyFlag(), "route_polyline"),
          guideBezier(MutableDesiredPatch(), DirtyFlag(), "guide_bezier"),
          scanArc(MutableDesiredPatch(), DirtyFlag(), "scan_arc"),
          missionTime(MutableDesiredPatch(), DirtyFlag(), "mission_time"),
          overlayImage(MutableDesiredPatch(), DirtyFlag(), "overlay_image")
    {
    }

    mfd::client::TextHandle headingValue;
    mfd::client::LineHandle horizonLine;
    mfd::client::RingHandle compassRing;
    mfd::client::RectangleHandle lockBox;
    mfd::client::TriangleHandle warningTriangle;
    mfd::client::PolylineHandle routePolyline;
    mfd::client::BezierHandle guideBezier;
    mfd::client::ArcHandle scanArc;
    mfd::client::TimeHandle missionTime;
    mfd::client::ImageHandle overlayImage;
};

class GeneratedPrimitiveFixtureReticle final : public mfd::client::Reticle
{
public:
    GeneratedPrimitiveFixtureReticle()
        : mfd::client::Reticle("Radar", "fixture", 11U, 22U),
          headingValue(MutableDesiredPatch(), DirtyFlag(), "heading_value", 33U, PrimitiveTransportIds())
    {
    }

    mfd::client::BlinkType slow {"slow", 44U};
    mfd::client::TextHandle headingValue;
};

class GeneratedGeometryFixtureReticle final : public mfd::client::Reticle
{
public:
    GeneratedGeometryFixtureReticle()
        : mfd::client::Reticle("Radar", "geometry_fixture", 11U, 22U),
          horizonLine(MutableDesiredPatch(), DirtyFlag(), "horizon_line", 101U, PrimitiveTransportIds()),
          cursorCircle(MutableDesiredPatch(), DirtyFlag(), "cursor_circle", 102U, PrimitiveTransportIds()),
          scopeRing(MutableDesiredPatch(), DirtyFlag(), "scope_ring", 103U, PrimitiveTransportIds()),
          lockBox(MutableDesiredPatch(), DirtyFlag(), "lock_box", 104U, PrimitiveTransportIds()),
          uncertaintyEllipse(MutableDesiredPatch(), DirtyFlag(), "uncertainty_ellipse", 105U, PrimitiveTransportIds()),
          targetSquare(MutableDesiredPatch(), DirtyFlag(), "target_square", 106U, PrimitiveTransportIds()),
          steerDiamond(MutableDesiredPatch(), DirtyFlag(), "steer_diamond", 107U, PrimitiveTransportIds()),
          warningTriangle(MutableDesiredPatch(), DirtyFlag(), "warning_triangle", 108U, PrimitiveTransportIds()),
          routePolyline(MutableDesiredPatch(), DirtyFlag(), "route_polyline", 109U, PrimitiveTransportIds()),
          guideBezier(MutableDesiredPatch(), DirtyFlag(), "guide_bezier", 110U, PrimitiveTransportIds()),
          scanArc(MutableDesiredPatch(), DirtyFlag(), "scan_arc", 111U, PrimitiveTransportIds())
    {
    }

    mfd::client::LineHandle horizonLine;
    mfd::client::CircleHandle cursorCircle;
    mfd::client::RingHandle scopeRing;
    mfd::client::RectangleHandle lockBox;
    mfd::client::EllipseHandle uncertaintyEllipse;
    mfd::client::SquareHandle targetSquare;
    mfd::client::DiamondHandle steerDiamond;
    mfd::client::TriangleHandle warningTriangle;
    mfd::client::PolylineHandle routePolyline;
    mfd::client::BezierHandle guideBezier;
    mfd::client::ArcHandle scanArc;
};

mfd::client::PrimitiveBaseline MakeNonDefaultPrimitiveBaseline()
{
    mfd::client::PrimitiveBaseline baseline {};
    baseline.visible = false;
    baseline.position = mfd::Vec2 {0.11f, 0.22f};
    baseline.rotationDegrees = 15.0f;
    baseline.scale = mfd::Vec2 {1.5f, 2.5f};
    baseline.text = "baseline-text";
    baseline.lineStart = mfd::Vec2 {-1.0f, -2.0f};
    baseline.lineEnd = mfd::Vec2 {3.0f, 4.0f};
    baseline.radius = 0.75f;
    baseline.innerRadius = 0.30f;
    baseline.outerRadius = 0.90f;
    baseline.width = 1.10f;
    baseline.height = 2.20f;
    baseline.points = {mfd::Vec2 {0.0f, 0.0f}, mfd::Vec2 {1.0f, 1.0f}, mfd::Vec2 {2.0f, 2.0f}};
    baseline.closed = true;
    baseline.segments = 7;
    baseline.startAngleDegrees = 10.0f;
    baseline.endAngleDegrees = 200.0f;
    return baseline;
}

mfd::client::ReticleBaseline MakeNonDefaultReticleBaseline()
{
    mfd::client::ReticleBaseline baseline {};
    baseline.visible = false;
    baseline.position = mfd::Vec2 {1.5f, -2.5f};
    baseline.rotationDegrees = 45.0f;
    baseline.scale = mfd::Vec2 {2.0f, 3.0f};
    baseline.text = "reticle-baseline-text";
    return baseline;
}

class BaselineFixtureReticle final : public mfd::client::Reticle
{
public:
    BaselineFixtureReticle()
        : mfd::client::Reticle("Radar", "baseline_fixture", 0U, 0U, MakeNonDefaultReticleBaseline()),
          horizonLine(MutableDesiredPatch(), DirtyFlag(), "horizon_line", 0U, nullptr, MakeNonDefaultPrimitiveBaseline()),
          cursorCircle(MutableDesiredPatch(), DirtyFlag(), "cursor_circle", 0U, nullptr, MakeNonDefaultPrimitiveBaseline()),
          scopeRing(MutableDesiredPatch(), DirtyFlag(), "scope_ring", 0U, nullptr, MakeNonDefaultPrimitiveBaseline()),
          lockBox(MutableDesiredPatch(), DirtyFlag(), "lock_box", 0U, nullptr, MakeNonDefaultPrimitiveBaseline()),
          uncertaintyEllipse(MutableDesiredPatch(), DirtyFlag(), "uncertainty_ellipse", 0U, nullptr, MakeNonDefaultPrimitiveBaseline()),
          targetSquare(MutableDesiredPatch(), DirtyFlag(), "target_square", 0U, nullptr, MakeNonDefaultPrimitiveBaseline()),
          steerDiamond(MutableDesiredPatch(), DirtyFlag(), "steer_diamond", 0U, nullptr, MakeNonDefaultPrimitiveBaseline()),
          warningTriangle(MutableDesiredPatch(), DirtyFlag(), "warning_triangle", 0U, nullptr, MakeNonDefaultPrimitiveBaseline()),
          routePolyline(MutableDesiredPatch(), DirtyFlag(), "route_polyline", 0U, nullptr, MakeNonDefaultPrimitiveBaseline()),
          guideBezier(MutableDesiredPatch(), DirtyFlag(), "guide_bezier", 0U, nullptr, MakeNonDefaultPrimitiveBaseline()),
          scanArc(MutableDesiredPatch(), DirtyFlag(), "scan_arc", 0U, nullptr, MakeNonDefaultPrimitiveBaseline()),
          headingValue(MutableDesiredPatch(), DirtyFlag(), "heading_value", 0U, nullptr, MakeNonDefaultPrimitiveBaseline()),
          missionTime(MutableDesiredPatch(), DirtyFlag(), "mission_time", 0U, nullptr, MakeNonDefaultPrimitiveBaseline())
    {
    }

    mfd::client::LineHandle horizonLine;
    mfd::client::CircleHandle cursorCircle;
    mfd::client::RingHandle scopeRing;
    mfd::client::RectangleHandle lockBox;
    mfd::client::EllipseHandle uncertaintyEllipse;
    mfd::client::SquareHandle targetSquare;
    mfd::client::DiamondHandle steerDiamond;
    mfd::client::TriangleHandle warningTriangle;
    mfd::client::PolylineHandle routePolyline;
    mfd::client::BezierHandle guideBezier;
    mfd::client::ArcHandle scanArc;
    mfd::client::TextHandle headingValue;
    mfd::client::TimeHandle missionTime;
};

class GeneratedDynamicFixtureReticle final : public mfd::client::DynamicReticle
{
public:
    explicit GeneratedDynamicFixtureReticle(const std::string_view reticleId)
        : mfd::client::DynamicReticle(reticleId),
          label(MutableDesiredPatch(), DirtyFlag(), "track_label", 33U, PrimitiveTransportIds()),
          vectorLine(MutableDesiredPatch(), DirtyFlag(), "vector_line", 34U, PrimitiveTransportIds())
    {
    }

    mfd::client::TextHandle label;
    mfd::client::LineHandle vectorLine;
};

class GeneratedDynamicFixtureSet final : public mfd::client::GeneratedDynamicReticleSet
{
public:
    explicit GeneratedDynamicFixtureSet(mfd::client::RuntimeFeedbackState* feedbackState = nullptr)
        : mfd::client::GeneratedDynamicReticleSet("Radar", "radar_track", 11U, 77U, feedbackState)
    {
    }

    GeneratedDynamicFixtureReticle& CreateTrack()
    {
        return static_cast<GeneratedDynamicFixtureReticle&>(mfd::client::GeneratedDynamicReticleSet::Create());
    }

    void Remove(GeneratedDynamicFixtureReticle& reticle)
    {
        mfd::client::GeneratedDynamicReticleSet::Remove(reticle);
    }

protected:
    std::unique_ptr<mfd::client::DynamicReticle> CreateReticle(const std::string_view reticleId) override
    {
        return std::make_unique<GeneratedDynamicFixtureReticle>(reticleId);
    }
};

class RawPatchFixtureReticle final : public mfd::client::Reticle
{
public:
    RawPatchFixtureReticle()
        : mfd::client::Reticle("HUD", "raw_patch")
    {
    }

    void SetRawText(std::string value)
    {
        MutableDesiredPatch().text = std::move(value);
    }
};
} // namespace

TEST(AnimationTests, ReticleEmitsSingleUpdateAndTracksPrimitiveSpecificFields)
{
    mfd::client::BlinkType fast("fast");
    mfd::client::TextReticle reticle("Radar", "caption", "value");

    reticle.SetVisible(true);
    reticle.SetPosition({0.25f, -0.50f});
    reticle.SetRotationDegrees(33.0f);
    reticle.SetScale({1.10f, 0.90f});
    reticle.SetColor({1, 2, 3, 255});
    reticle.SetThickness(0.01f);
    reticle.SetValue("LOCK");
    reticle.SetLetterSpacing("value", 0.02f);
    reticle.SetBlinkType(fast);

    std::vector<mfd::UserCommand> commands;
    EXPECT_TRUE(reticle.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);

    const auto* update = std::get_if<mfd::UpdateReticleCommand>(&commands.front());
    ASSERT_NE(update, nullptr);
    EXPECT_EQ(update->target.page, "Radar");
    EXPECT_EQ(update->target.reticle, "caption");
    ASSERT_TRUE(update->patch.visible.has_value());
    ASSERT_TRUE(update->patch.position.has_value());
    ASSERT_TRUE(update->patch.rotationDegrees.has_value());
    ASSERT_TRUE(update->patch.scale.has_value());
    ASSERT_TRUE(update->patch.color.has_value());
    ASSERT_TRUE(update->patch.thickness.has_value());
    ASSERT_TRUE(update->patch.blinkEnabled.has_value());
    ASSERT_TRUE(update->patch.blinkType.has_value());
    EXPECT_TRUE(*update->patch.visible);
    EXPECT_FLOAT_EQ(update->patch.position->x, 0.25f);
    EXPECT_FLOAT_EQ(update->patch.position->y, -0.50f);
    EXPECT_FLOAT_EQ(*update->patch.rotationDegrees, 33.0f);
    EXPECT_FLOAT_EQ(update->patch.scale->x, 1.10f);
    EXPECT_FLOAT_EQ(update->patch.scale->y, 0.90f);
    EXPECT_EQ(update->patch.color->r, 1U);
    EXPECT_FLOAT_EQ(*update->patch.thickness, 0.01f);
    EXPECT_TRUE(*update->patch.blinkEnabled);
    EXPECT_EQ(*update->patch.blinkType, "fast");
    EXPECT_EQ(update->patch.texts.at("value"), "LOCK");
    EXPECT_FLOAT_EQ(update->patch.letterSpacings.at("value"), 0.02f);

    commands.clear();
    EXPECT_FALSE(reticle.AppendCommands(commands));
    EXPECT_TRUE(commands.empty());

    reticle.Blink = nullptr;
    EXPECT_TRUE(reticle.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);
    const auto* blinkDisable = std::get_if<mfd::UpdateReticleCommand>(&commands.front());
    ASSERT_NE(blinkDisable, nullptr);
    ASSERT_TRUE(blinkDisable->patch.blinkEnabled.has_value());
    ASSERT_TRUE(blinkDisable->patch.blinkType.has_value());
    EXPECT_FALSE(*blinkDisable->patch.blinkEnabled);
    EXPECT_TRUE(blinkDisable->patch.blinkType->empty());
    EXPECT_FALSE(blinkDisable->patch.visible.has_value());
    EXPECT_FALSE(blinkDisable->patch.position.has_value());
    EXPECT_FALSE(blinkDisable->patch.rotationDegrees.has_value());
    EXPECT_FALSE(blinkDisable->patch.scale.has_value());
    EXPECT_FALSE(blinkDisable->patch.color.has_value());
    EXPECT_FALSE(blinkDisable->patch.thickness.has_value());
    EXPECT_TRUE(blinkDisable->patch.texts.empty());
    EXPECT_TRUE(blinkDisable->patch.letterSpacings.empty());
}

TEST(AnimationTests, PrimitiveHandlesEmitRichPrimitivePatchesThroughReticleDelta)
{
    PrimitiveFixtureReticle reticle;

    reticle.headingValue.SetVisible(true);
    reticle.headingValue.SetPosition({0.10f, -0.15f});
    reticle.headingValue.SetScale({1.2f, 0.8f});
    reticle.headingValue.SetColor({10, 20, 30, 255});
    reticle.headingValue.SetThickness(0.005f);
    reticle.headingValue.SetText("123");
    reticle.headingValue.SetLetterSpacing(0.02f);
    reticle.horizonLine.SetLineStyle(mfd::client::LineStyle::Dashed);
    reticle.horizonLine.SetStart({-0.5f, 0.0f});
    reticle.horizonLine.SetEnd({0.5f, 0.0f});
    reticle.compassRing.SetInnerRadius(0.15f);
    reticle.compassRing.SetOuterRadius(0.2f);
    reticle.compassRing.SetSegments(48);
    reticle.lockBox.SetWidth(0.30f);
    reticle.lockBox.SetHeight(0.12f);
    reticle.lockBox.SetSize({0.32f, 0.14f});
    reticle.lockBox.SetFillColor({1, 2, 3, 200});
    reticle.lockBox.SetFilled(true);
    reticle.warningTriangle.SetPoints(std::array<mfd::Vec2, 3> {{{-0.2f, -0.1f}, {0.0f, 0.25f}, {0.18f, -0.08f}}});
    reticle.routePolyline.SetPoints({{-0.3f, -0.1f}, {-0.1f, 0.15f}, {0.12f, 0.08f}, {0.28f, -0.04f}});
    reticle.routePolyline.SetClosed(true);
    reticle.routePolyline.SetFilled(true);
    reticle.guideBezier.SetControlPoints({{-0.2f, -0.12f}, {-0.05f, 0.18f}, {0.05f, 0.18f}, {0.2f, -0.02f}});
    reticle.guideBezier.SetSegments(20);
    reticle.scanArc.SetRadius(0.22f);
    reticle.scanArc.SetStartAngleDegrees(-45.0f);
    reticle.scanArc.SetEndAngleDegrees(135.0f);
    reticle.scanArc.SetSegments(24);
    reticle.missionTime.SetTimeValue(mfd::TimeValue {2026, 6, 5, 14, 3, 9});
    reticle.missionTime.SetUtc(true);
    reticle.missionTime.SetFieldVisibility(mfd::TimeFieldVisibility {true, true, true, true, true, false});
    reticle.missionTime.SetLetterSpacing(0.015f);
    reticle.overlayImage.SetPosition({0.05f, 0.09f});
    reticle.overlayImage.SetRotationDegrees(18.0f);
    reticle.overlayImage.SetScale({1.5f, 0.75f});
    reticle.overlayImage.SetVisible(true);

    std::vector<mfd::UserCommand> commands;
    ASSERT_TRUE(reticle.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);

    const auto* update = std::get_if<mfd::UpdateReticleCommand>(&commands.front());
    ASSERT_NE(update, nullptr);
    ASSERT_EQ(update->patch.primitivePatches.size(), 10U);

    const auto& textPatch = update->patch.primitivePatches.at("heading_value");
    ASSERT_TRUE(textPatch.visible.has_value());
    ASSERT_TRUE(textPatch.position.has_value());
    ASSERT_TRUE(textPatch.scale.has_value());
    ASSERT_TRUE(textPatch.color.has_value());
    ASSERT_TRUE(textPatch.thickness.has_value());
    ASSERT_TRUE(textPatch.text.has_value());
    ASSERT_TRUE(textPatch.letterSpacing.has_value());
    EXPECT_TRUE(*textPatch.visible);
    EXPECT_FLOAT_EQ(textPatch.position->x, 0.10f);
    EXPECT_FLOAT_EQ(textPatch.position->y, -0.15f);
    EXPECT_FLOAT_EQ(textPatch.scale->x, 1.2f);
    EXPECT_FLOAT_EQ(textPatch.scale->y, 0.8f);
    EXPECT_EQ(textPatch.color->r, 10U);
    EXPECT_FLOAT_EQ(*textPatch.thickness, 0.005f);
    EXPECT_EQ(*textPatch.text, "123");
    EXPECT_FLOAT_EQ(*textPatch.letterSpacing, 0.02f);

    const auto& linePatch = update->patch.primitivePatches.at("horizon_line");
    ASSERT_TRUE(linePatch.lineStyle.has_value());
    ASSERT_TRUE(linePatch.lineStart.has_value());
    ASSERT_TRUE(linePatch.lineEnd.has_value());
    EXPECT_EQ(*linePatch.lineStyle, mfd::LineStyle::Dashed);
    EXPECT_FLOAT_EQ(linePatch.lineStart->x, -0.5f);
    EXPECT_FLOAT_EQ(linePatch.lineEnd->x, 0.5f);

    const auto& ringPatch = update->patch.primitivePatches.at("compass_ring");
    ASSERT_TRUE(ringPatch.innerRadius.has_value());
    ASSERT_TRUE(ringPatch.outerRadius.has_value());
    ASSERT_TRUE(ringPatch.segments.has_value());
    EXPECT_FLOAT_EQ(*ringPatch.innerRadius, 0.15f);
    EXPECT_FLOAT_EQ(*ringPatch.outerRadius, 0.2f);
    EXPECT_EQ(*ringPatch.segments, 48);

    const auto& rectanglePatch = update->patch.primitivePatches.at("lock_box");
    ASSERT_TRUE(rectanglePatch.width.has_value());
    ASSERT_TRUE(rectanglePatch.height.has_value());
    ASSERT_TRUE(rectanglePatch.size.has_value());
    ASSERT_TRUE(rectanglePatch.fillColor.has_value());
    ASSERT_TRUE(rectanglePatch.filled.has_value());
    EXPECT_FLOAT_EQ(*rectanglePatch.width, 0.30f);
    EXPECT_FLOAT_EQ(*rectanglePatch.height, 0.12f);
    EXPECT_FLOAT_EQ(rectanglePatch.size->x, 0.32f);
    EXPECT_FLOAT_EQ(rectanglePatch.size->y, 0.14f);
    EXPECT_EQ(rectanglePatch.fillColor->r, 1U);
    EXPECT_TRUE(*rectanglePatch.filled);

    const auto& trianglePatch = update->patch.primitivePatches.at("warning_triangle");
    ASSERT_TRUE(trianglePatch.points.has_value());
    ASSERT_EQ(trianglePatch.points->size(), 3U);
    EXPECT_FLOAT_EQ(trianglePatch.points->at(1).y, 0.25f);

    const auto& polylinePatch = update->patch.primitivePatches.at("route_polyline");
    ASSERT_TRUE(polylinePatch.points.has_value());
    ASSERT_TRUE(polylinePatch.closed.has_value());
    ASSERT_TRUE(polylinePatch.filled.has_value());
    ASSERT_EQ(polylinePatch.points->size(), 4U);
    EXPECT_TRUE(*polylinePatch.closed);
    EXPECT_TRUE(*polylinePatch.filled);

    const auto& bezierPatch = update->patch.primitivePatches.at("guide_bezier");
    ASSERT_TRUE(bezierPatch.points.has_value());
    ASSERT_TRUE(bezierPatch.segments.has_value());
    ASSERT_EQ(bezierPatch.points->size(), 4U);
    EXPECT_EQ(*bezierPatch.segments, 20);

    const auto& arcPatch = update->patch.primitivePatches.at("scan_arc");
    ASSERT_TRUE(arcPatch.radius.has_value());
    ASSERT_TRUE(arcPatch.startAngleDegrees.has_value());
    ASSERT_TRUE(arcPatch.endAngleDegrees.has_value());
    ASSERT_TRUE(arcPatch.segments.has_value());
    EXPECT_FLOAT_EQ(*arcPatch.radius, 0.22f);
    EXPECT_FLOAT_EQ(*arcPatch.startAngleDegrees, -45.0f);
    EXPECT_FLOAT_EQ(*arcPatch.endAngleDegrees, 135.0f);
    EXPECT_EQ(*arcPatch.segments, 24);

    const auto& timePatch = update->patch.primitivePatches.at("mission_time");
    ASSERT_TRUE(timePatch.timeValue.has_value());
    EXPECT_EQ(timePatch.timeValue->year, 2026);
    EXPECT_EQ(timePatch.timeValue->hour, 14);
    ASSERT_TRUE(timePatch.timeUtc.has_value());
    EXPECT_TRUE(*timePatch.timeUtc);
    ASSERT_TRUE(timePatch.timeFields.has_value());
    EXPECT_TRUE(timePatch.timeFields->year);
    EXPECT_FALSE(timePatch.timeFields->second);
    ASSERT_TRUE(timePatch.letterSpacing.has_value());
    EXPECT_FLOAT_EQ(*timePatch.letterSpacing, 0.015f);

    const auto& imagePatch = update->patch.primitivePatches.at("overlay_image");
    ASSERT_TRUE(imagePatch.visible.has_value());
    ASSERT_TRUE(imagePatch.position.has_value());
    ASSERT_TRUE(imagePatch.rotationDegrees.has_value());
    ASSERT_TRUE(imagePatch.scale.has_value());
    EXPECT_TRUE(*imagePatch.visible);
    EXPECT_FLOAT_EQ(imagePatch.position->x, 0.05f);
    EXPECT_FLOAT_EQ(imagePatch.position->y, 0.09f);
    EXPECT_FLOAT_EQ(*imagePatch.rotationDegrees, 18.0f);
    EXPECT_FLOAT_EQ(imagePatch.scale->x, 1.5f);
    EXPECT_FLOAT_EQ(imagePatch.scale->y, 0.75f);
}

TEST(AnimationTests, TimeHandleCanClearNumericRuntimeBypass)
{
    PrimitiveFixtureReticle reticle;

    reticle.missionTime.SetTimeValue(mfd::TimeValue {2026, 6, 5, 14, 3, 9});

    std::vector<mfd::UserCommand> commands;
    ASSERT_TRUE(reticle.AppendCommands(commands));
    commands.clear();

    reticle.missionTime.ClearTimeValue();
    ASSERT_TRUE(reticle.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);

    const auto* update = std::get_if<mfd::UpdateReticleCommand>(&commands.front());
    ASSERT_NE(update, nullptr);
    const auto& timePatch = update->patch.primitivePatches.at("mission_time");
    EXPECT_FALSE(timePatch.timeValue.has_value());
    EXPECT_TRUE(timePatch.clearTimeValue);
}

TEST(AnimationTests, GeneratedStaticHandlesCarryTransportIdsAlongsideLegacyFields)
{
    GeneratedPrimitiveFixtureReticle reticle;

    reticle.Blink = reticle.slow;
    reticle.headingValue.SetText("123");

    std::vector<mfd::UserCommand> commands;
    ASSERT_TRUE(reticle.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);

    const auto* update = std::get_if<mfd::UpdateReticleCommand>(&commands.front());
    ASSERT_NE(update, nullptr);
    EXPECT_EQ(update->target.page, "Radar");
    EXPECT_EQ(update->target.reticle, "fixture");
    EXPECT_EQ(update->target.pageId, 11U);
    EXPECT_EQ(update->target.reticleId, 22U);
    ASSERT_TRUE(update->patch.blinkTypeId.has_value());
    EXPECT_EQ(*update->patch.blinkTypeId, 44U);
    EXPECT_FALSE(update->patch.blinkType.has_value());
    ASSERT_EQ(update->patch.primitivePatches.find("heading_value"), update->patch.primitivePatches.end());
    ASSERT_NE(update->patch.primitivePatchesById.find(33U), update->patch.primitivePatchesById.end());
    ASSERT_TRUE(update->patch.primitivePatchesById.at(33U).text.has_value());
    EXPECT_EQ(*update->patch.primitivePatchesById.at(33U).text, "123");
}

TEST(AnimationTests, GeneratedPrimitiveLevelGeometryHandlesEmitTypeSpecificPatchesById)
{
    GeneratedGeometryFixtureReticle reticle;

    reticle.horizonLine.SetLineStyle(mfd::client::LineStyle::Dashed);
    reticle.horizonLine.SetStart({-0.5f, 0.0f});
    reticle.horizonLine.SetEnd({0.5f, 0.0f});
    reticle.cursorCircle.SetRadius(0.07f);
    reticle.scopeRing.SetInnerRadius(0.15f);
    reticle.scopeRing.SetOuterRadius(0.21f);
    reticle.scopeRing.SetSegments(40);
    reticle.lockBox.SetWidth(0.30f);
    reticle.lockBox.SetHeight(0.12f);
    reticle.lockBox.SetSize({0.32f, 0.14f});
    reticle.uncertaintyEllipse.SetWidth(0.40f);
    reticle.uncertaintyEllipse.SetHeight(0.20f);
    reticle.targetSquare.SetSize(0.11f);
    reticle.steerDiamond.SetWidth(0.18f);
    reticle.steerDiamond.SetHeight(0.24f);
    reticle.warningTriangle.SetPoints(std::array<mfd::Vec2, 3> {{{-0.2f, -0.1f}, {0.0f, 0.2f}, {0.2f, -0.1f}}});
    reticle.routePolyline.SetPoints({{-0.25f, -0.08f}, {-0.04f, 0.14f}, {0.18f, -0.02f}});
    reticle.routePolyline.SetClosed(true);
    reticle.guideBezier.SetControlPoints({{-0.2f, -0.12f}, {-0.05f, 0.18f}, {0.05f, 0.18f}, {0.2f, -0.02f}});
    reticle.guideBezier.SetSegments(22);
    reticle.scanArc.SetRadius(0.19f);
    reticle.scanArc.SetStartAngleDegrees(-60.0f);
    reticle.scanArc.SetEndAngleDegrees(120.0f);
    reticle.scanArc.SetSegments(26);

    std::vector<mfd::UserCommand> commands;
    ASSERT_TRUE(reticle.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);

    const auto* update = std::get_if<mfd::UpdateReticleCommand>(&commands.front());
    ASSERT_NE(update, nullptr);
    EXPECT_EQ(update->target.pageId, 11U);
    EXPECT_EQ(update->target.reticleId, 22U);
    EXPECT_TRUE(update->patch.primitivePatches.empty());
    ASSERT_EQ(update->patch.primitivePatchesById.size(), 11U);

    const auto& linePatch = update->patch.primitivePatchesById.at(101U);
    ASSERT_TRUE(linePatch.lineStyle.has_value());
    ASSERT_TRUE(linePatch.lineStart.has_value());
    ASSERT_TRUE(linePatch.lineEnd.has_value());
    EXPECT_EQ(*linePatch.lineStyle, mfd::LineStyle::Dashed);
    EXPECT_FLOAT_EQ(linePatch.lineStart->x, -0.5f);
    EXPECT_FLOAT_EQ(linePatch.lineEnd->x, 0.5f);

    const auto& circlePatch = update->patch.primitivePatchesById.at(102U);
    ASSERT_TRUE(circlePatch.radius.has_value());
    EXPECT_FLOAT_EQ(*circlePatch.radius, 0.07f);

    const auto& ringPatch = update->patch.primitivePatchesById.at(103U);
    ASSERT_TRUE(ringPatch.innerRadius.has_value());
    ASSERT_TRUE(ringPatch.outerRadius.has_value());
    ASSERT_TRUE(ringPatch.segments.has_value());
    EXPECT_FLOAT_EQ(*ringPatch.innerRadius, 0.15f);
    EXPECT_FLOAT_EQ(*ringPatch.outerRadius, 0.21f);
    EXPECT_EQ(*ringPatch.segments, 40);

    const auto& rectanglePatch = update->patch.primitivePatchesById.at(104U);
    ASSERT_TRUE(rectanglePatch.width.has_value());
    ASSERT_TRUE(rectanglePatch.height.has_value());
    ASSERT_TRUE(rectanglePatch.size.has_value());
    EXPECT_FLOAT_EQ(*rectanglePatch.width, 0.30f);
    EXPECT_FLOAT_EQ(*rectanglePatch.height, 0.12f);
    EXPECT_FLOAT_EQ(rectanglePatch.size->x, 0.32f);
    EXPECT_FLOAT_EQ(rectanglePatch.size->y, 0.14f);

    const auto& ellipsePatch = update->patch.primitivePatchesById.at(105U);
    ASSERT_TRUE(ellipsePatch.width.has_value());
    ASSERT_TRUE(ellipsePatch.height.has_value());
    EXPECT_FLOAT_EQ(*ellipsePatch.width, 0.40f);
    EXPECT_FLOAT_EQ(*ellipsePatch.height, 0.20f);

    const auto& squarePatch = update->patch.primitivePatchesById.at(106U);
    ASSERT_TRUE(squarePatch.size.has_value());
    EXPECT_FLOAT_EQ(squarePatch.size->x, 0.11f);
    EXPECT_FLOAT_EQ(squarePatch.size->y, 0.11f);

    const auto& diamondPatch = update->patch.primitivePatchesById.at(107U);
    ASSERT_TRUE(diamondPatch.width.has_value());
    ASSERT_TRUE(diamondPatch.height.has_value());
    EXPECT_FLOAT_EQ(*diamondPatch.width, 0.18f);
    EXPECT_FLOAT_EQ(*diamondPatch.height, 0.24f);

    const auto& trianglePatch = update->patch.primitivePatchesById.at(108U);
    ASSERT_TRUE(trianglePatch.points.has_value());
    ASSERT_EQ(trianglePatch.points->size(), 3U);
    EXPECT_FLOAT_EQ(trianglePatch.points->at(1).y, 0.2f);

    const auto& polylinePatch = update->patch.primitivePatchesById.at(109U);
    ASSERT_TRUE(polylinePatch.points.has_value());
    ASSERT_TRUE(polylinePatch.closed.has_value());
    ASSERT_EQ(polylinePatch.points->size(), 3U);
    EXPECT_TRUE(*polylinePatch.closed);

    const auto& bezierPatch = update->patch.primitivePatchesById.at(110U);
    ASSERT_TRUE(bezierPatch.points.has_value());
    ASSERT_TRUE(bezierPatch.segments.has_value());
    ASSERT_EQ(bezierPatch.points->size(), 4U);
    EXPECT_EQ(*bezierPatch.segments, 22);

    const auto& arcPatch = update->patch.primitivePatchesById.at(111U);
    ASSERT_TRUE(arcPatch.radius.has_value());
    ASSERT_TRUE(arcPatch.startAngleDegrees.has_value());
    ASSERT_TRUE(arcPatch.endAngleDegrees.has_value());
    ASSERT_TRUE(arcPatch.segments.has_value());
    EXPECT_FLOAT_EQ(*arcPatch.radius, 0.19f);
    EXPECT_FLOAT_EQ(*arcPatch.startAngleDegrees, -60.0f);
    EXPECT_FLOAT_EQ(*arcPatch.endAngleDegrees, 120.0f);
    EXPECT_EQ(*arcPatch.segments, 26);
}

TEST(AnimationTests, PrimitiveGettersReturnAuthoredBaselineWhenNoOverrideStaged)
{
    BaselineFixtureReticle reticle;

    EXPECT_FALSE(reticle.horizonLine.GetVisible());
    EXPECT_FLOAT_EQ(reticle.horizonLine.GetPosition().x, 0.11f);
    EXPECT_FLOAT_EQ(reticle.horizonLine.GetPosition().y, 0.22f);
    EXPECT_FLOAT_EQ(reticle.horizonLine.GetRotationDegrees(), 15.0f);
    EXPECT_FLOAT_EQ(reticle.horizonLine.GetScale().x, 1.5f);
    EXPECT_FLOAT_EQ(reticle.horizonLine.GetScale().y, 2.5f);
    EXPECT_FLOAT_EQ(reticle.horizonLine.GetStart().x, -1.0f);
    EXPECT_FLOAT_EQ(reticle.horizonLine.GetEnd().x, 3.0f);

    EXPECT_FLOAT_EQ(reticle.cursorCircle.GetRadius(), 0.75f);

    EXPECT_FLOAT_EQ(reticle.scopeRing.GetInnerRadius(), 0.30f);
    EXPECT_FLOAT_EQ(reticle.scopeRing.GetOuterRadius(), 0.90f);
    EXPECT_EQ(reticle.scopeRing.GetSegments(), 7);

    EXPECT_FLOAT_EQ(reticle.lockBox.GetWidth(), 1.10f);
    EXPECT_FLOAT_EQ(reticle.lockBox.GetHeight(), 2.20f);
    EXPECT_FLOAT_EQ(reticle.lockBox.GetSize().x, 1.10f);
    EXPECT_FLOAT_EQ(reticle.lockBox.GetSize().y, 2.20f);

    EXPECT_FLOAT_EQ(reticle.uncertaintyEllipse.GetWidth(), 1.10f);
    EXPECT_FLOAT_EQ(reticle.uncertaintyEllipse.GetHeight(), 2.20f);

    EXPECT_FLOAT_EQ(reticle.targetSquare.GetSize(), 1.10f);

    EXPECT_FLOAT_EQ(reticle.steerDiamond.GetWidth(), 1.10f);
    EXPECT_FLOAT_EQ(reticle.steerDiamond.GetHeight(), 2.20f);

    const std::array<mfd::Vec2, 3> trianglePoints = reticle.warningTriangle.GetPoints();
    EXPECT_FLOAT_EQ(trianglePoints[1].x, 1.0f);
    EXPECT_FLOAT_EQ(trianglePoints[1].y, 1.0f);

    const std::vector<mfd::Vec2> polylinePoints = reticle.routePolyline.GetPoints();
    ASSERT_EQ(polylinePoints.size(), 3U);
    EXPECT_TRUE(reticle.routePolyline.GetClosed());

    const std::vector<mfd::Vec2> bezierPoints = reticle.guideBezier.GetControlPoints();
    ASSERT_EQ(bezierPoints.size(), 3U);
    EXPECT_EQ(reticle.guideBezier.GetSegments(), 7);

    EXPECT_FLOAT_EQ(reticle.scanArc.GetRadius(), 0.75f);
    EXPECT_FLOAT_EQ(reticle.scanArc.GetStartAngleDegrees(), 10.0f);
    EXPECT_FLOAT_EQ(reticle.scanArc.GetEndAngleDegrees(), 200.0f);
    EXPECT_EQ(reticle.scanArc.GetSegments(), 7);

    EXPECT_EQ(reticle.headingValue.GetText(), "baseline-text");

    EXPECT_FALSE(reticle.GetVisible());
    EXPECT_FLOAT_EQ(reticle.GetPosition().x, 1.5f);
    EXPECT_FLOAT_EQ(reticle.GetPosition().y, -2.5f);
    EXPECT_FLOAT_EQ(reticle.GetRotationDegrees(), 45.0f);
    EXPECT_FLOAT_EQ(reticle.GetScale().x, 2.0f);
    EXPECT_FLOAT_EQ(reticle.GetScale().y, 3.0f);
    EXPECT_EQ(reticle.GetText(), "reticle-baseline-text");
}

TEST(AnimationTests, PrimitiveGettersReturnModelDefaultWhenConstructedWithoutAuthoredBaseline)
{
    PrimitiveFixtureReticle reticle;

    EXPECT_TRUE(reticle.horizonLine.GetVisible());
    EXPECT_FLOAT_EQ(reticle.horizonLine.GetPosition().x, 0.0f);
    EXPECT_FLOAT_EQ(reticle.horizonLine.GetRotationDegrees(), 0.0f);
    EXPECT_FLOAT_EQ(reticle.horizonLine.GetScale().x, 1.0f);
    EXPECT_FLOAT_EQ(reticle.horizonLine.GetScale().y, 1.0f);
    EXPECT_FLOAT_EQ(reticle.horizonLine.GetStart().x, -0.0208f);
    EXPECT_FLOAT_EQ(reticle.horizonLine.GetEnd().x, 0.0208f);

    EXPECT_FLOAT_EQ(reticle.compassRing.GetInnerRadius(), 0.0167f);
    EXPECT_FLOAT_EQ(reticle.compassRing.GetOuterRadius(), 0.0208f);
    // RingGeometry's own model default is 64 segments (not the generic 32).
    EXPECT_EQ(reticle.compassRing.GetSegments(), 64);

    EXPECT_FLOAT_EQ(reticle.lockBox.GetWidth(), 0.0417f);
    EXPECT_FLOAT_EQ(reticle.lockBox.GetHeight(), 0.0208f);

    // BezierGeometry's own model default is 32 segments.
    EXPECT_EQ(reticle.guideBezier.GetSegments(), 32);
    EXPECT_FALSE(reticle.routePolyline.GetClosed());

    EXPECT_FLOAT_EQ(reticle.scanArc.GetRadius(), 0.0208f);
    EXPECT_FLOAT_EQ(reticle.scanArc.GetStartAngleDegrees(), 0.0f);
    EXPECT_FLOAT_EQ(reticle.scanArc.GetEndAngleDegrees(), 180.0f);
    // ArcGeometry's own model default is 48 segments (not the generic 32).
    EXPECT_EQ(reticle.scanArc.GetSegments(), 48);

    EXPECT_EQ(reticle.headingValue.GetText(), "");
    EXPECT_EQ(reticle.missionTime.GetTimeValue(), std::nullopt);

    GeneratedGeometryFixtureReticle geometry;
    EXPECT_FLOAT_EQ(geometry.cursorCircle.GetRadius(), 0.0208f);
    EXPECT_FLOAT_EQ(geometry.uncertaintyEllipse.GetWidth(), 0.0417f);
    EXPECT_FLOAT_EQ(geometry.uncertaintyEllipse.GetHeight(), 0.0208f);
    // SquareGeometry/DiamondGeometry's own model default is 0.0208f for both
    // sides, distinct from RectangleGeometry/EllipseGeometry's 0.0417f/0.0208f.
    EXPECT_FLOAT_EQ(geometry.targetSquare.GetSize(), 0.0208f);
    EXPECT_FLOAT_EQ(geometry.steerDiamond.GetWidth(), 0.0208f);
    EXPECT_FLOAT_EQ(geometry.steerDiamond.GetHeight(), 0.0208f);
}

TEST(AnimationTests, PrimitiveAndReticleGettersReturnUserOverrideAfterCorrespondingSetter)
{
    GeneratedGeometryFixtureReticle reticle;

    reticle.horizonLine.SetVisible(false);
    reticle.horizonLine.SetPosition({0.5f, 0.6f});
    reticle.horizonLine.SetRotationDegrees(33.0f);
    reticle.horizonLine.SetScale({1.2f, 1.3f});
    reticle.horizonLine.SetColor({1, 2, 3, 4});
    reticle.horizonLine.SetLineStyle(mfd::client::LineStyle::Dotted);
    reticle.horizonLine.SetStart({-0.5f, 0.0f});
    reticle.horizonLine.SetEnd({0.5f, 0.0f});

    EXPECT_FALSE(reticle.horizonLine.GetVisible());
    EXPECT_FLOAT_EQ(reticle.horizonLine.GetPosition().x, 0.5f);
    EXPECT_FLOAT_EQ(reticle.horizonLine.GetPosition().y, 0.6f);
    EXPECT_FLOAT_EQ(reticle.horizonLine.GetRotationDegrees(), 33.0f);
    EXPECT_FLOAT_EQ(reticle.horizonLine.GetScale().x, 1.2f);
    EXPECT_FLOAT_EQ(reticle.horizonLine.GetScale().y, 1.3f);
    EXPECT_FLOAT_EQ(reticle.horizonLine.GetStart().x, -0.5f);
    EXPECT_FLOAT_EQ(reticle.horizonLine.GetEnd().x, 0.5f);

    reticle.cursorCircle.SetRadius(0.42f);
    EXPECT_FLOAT_EQ(reticle.cursorCircle.GetRadius(), 0.42f);

    reticle.scopeRing.SetInnerRadius(0.15f);
    reticle.scopeRing.SetOuterRadius(0.21f);
    reticle.scopeRing.SetSegments(40);
    EXPECT_FLOAT_EQ(reticle.scopeRing.GetInnerRadius(), 0.15f);
    EXPECT_FLOAT_EQ(reticle.scopeRing.GetOuterRadius(), 0.21f);
    EXPECT_EQ(reticle.scopeRing.GetSegments(), 40);

    reticle.targetSquare.SetSize(0.11f);
    EXPECT_FLOAT_EQ(reticle.targetSquare.GetSize(), 0.11f);

    reticle.steerDiamond.SetWidth(0.18f);
    reticle.steerDiamond.SetHeight(0.24f);
    EXPECT_FLOAT_EQ(reticle.steerDiamond.GetWidth(), 0.18f);
    EXPECT_FLOAT_EQ(reticle.steerDiamond.GetHeight(), 0.24f);

    reticle.warningTriangle.SetPoints(std::array<mfd::Vec2, 3> {{{-0.2f, -0.1f}, {0.0f, 0.2f}, {0.2f, -0.1f}}});
    const std::array<mfd::Vec2, 3> trianglePoints = reticle.warningTriangle.GetPoints();
    EXPECT_FLOAT_EQ(trianglePoints[1].y, 0.2f);

    reticle.routePolyline.SetPoints({{-0.25f, -0.08f}, {-0.04f, 0.14f}, {0.18f, -0.02f}});
    reticle.routePolyline.SetClosed(true);
    EXPECT_EQ(reticle.routePolyline.GetPoints().size(), 3U);
    EXPECT_TRUE(reticle.routePolyline.GetClosed());

    reticle.guideBezier.SetControlPoints({{-0.2f, -0.12f}, {-0.05f, 0.18f}, {0.05f, 0.18f}, {0.2f, -0.02f}});
    reticle.guideBezier.SetSegments(22);
    EXPECT_EQ(reticle.guideBezier.GetControlPoints().size(), 4U);
    EXPECT_EQ(reticle.guideBezier.GetSegments(), 22);

    reticle.scanArc.SetRadius(0.19f);
    reticle.scanArc.SetStartAngleDegrees(-60.0f);
    reticle.scanArc.SetEndAngleDegrees(120.0f);
    reticle.scanArc.SetSegments(26);
    EXPECT_FLOAT_EQ(reticle.scanArc.GetRadius(), 0.19f);
    EXPECT_FLOAT_EQ(reticle.scanArc.GetStartAngleDegrees(), -60.0f);
    EXPECT_FLOAT_EQ(reticle.scanArc.GetEndAngleDegrees(), 120.0f);
    EXPECT_EQ(reticle.scanArc.GetSegments(), 26);

    PrimitiveFixtureReticle textFixture;
    textFixture.headingValue.SetText("override-text");
    EXPECT_EQ(textFixture.headingValue.GetText(), "override-text");

    mfd::client::TextReticle textReticle("Radar", "caption", "value");
    textReticle.SetVisible(false);
    textReticle.SetPosition({0.7f, 0.8f});
    textReticle.SetRotationDegrees(12.0f);
    textReticle.SetScale({1.4f, 1.6f});
    textReticle.SetColor({9, 8, 7, 6});
    textReticle.SetText("new-value");
    EXPECT_FALSE(textReticle.GetVisible());
    EXPECT_FLOAT_EQ(textReticle.GetPosition().x, 0.7f);
    EXPECT_FLOAT_EQ(textReticle.GetRotationDegrees(), 12.0f);
    EXPECT_FLOAT_EQ(textReticle.GetScale().x, 1.4f);
    EXPECT_EQ(textReticle.GetText(), "new-value");
}

TEST(AnimationTests, RectangleEllipseDiamondWidthHeightSizeGettersStayCoherentAcrossSetters)
{
    GeneratedGeometryFixtureReticle reticle;

    reticle.lockBox.SetWidth(0.30f);
    EXPECT_FLOAT_EQ(reticle.lockBox.GetWidth(), 0.30f);
    EXPECT_FLOAT_EQ(reticle.lockBox.GetSize().x, 0.30f);
    EXPECT_FLOAT_EQ(reticle.lockBox.GetSize().y, 0.0208f);

    reticle.lockBox.SetHeight(0.12f);
    EXPECT_FLOAT_EQ(reticle.lockBox.GetHeight(), 0.12f);
    EXPECT_FLOAT_EQ(reticle.lockBox.GetSize().x, 0.30f);
    EXPECT_FLOAT_EQ(reticle.lockBox.GetSize().y, 0.12f);

    reticle.lockBox.SetSize({0.32f, 0.14f});
    EXPECT_FLOAT_EQ(reticle.lockBox.GetSize().x, 0.32f);
    EXPECT_FLOAT_EQ(reticle.lockBox.GetSize().y, 0.14f);

    GeneratedGeometryFixtureReticle freshSize;
    freshSize.lockBox.SetSize({0.36f, 0.16f});
    EXPECT_FLOAT_EQ(freshSize.lockBox.GetWidth(), 0.36f);
    EXPECT_FLOAT_EQ(freshSize.lockBox.GetHeight(), 0.16f);

    reticle.uncertaintyEllipse.SetWidth(0.40f);
    EXPECT_FLOAT_EQ(reticle.uncertaintyEllipse.GetSize().x, 0.40f);
    EXPECT_FLOAT_EQ(reticle.uncertaintyEllipse.GetSize().y, 0.0208f);

    reticle.steerDiamond.SetHeight(0.24f);
    EXPECT_FLOAT_EQ(reticle.steerDiamond.GetSize().x, 0.0208f);
    EXPECT_FLOAT_EQ(reticle.steerDiamond.GetSize().y, 0.24f);

    // A `size` override always wins over a later individual `width`/`height`
    // override: GetWidth() reports the newer value, GetSize() keeps reporting
    // the earlier staged size pair.
    GeneratedGeometryFixtureReticle sizeThenWidth;
    sizeThenWidth.lockBox.SetSize({0.30f, 0.12f});
    sizeThenWidth.lockBox.SetWidth(0.50f);
    EXPECT_FLOAT_EQ(sizeThenWidth.lockBox.GetWidth(), 0.50f);
    EXPECT_FLOAT_EQ(sizeThenWidth.lockBox.GetSize().x, 0.30f);
    EXPECT_FLOAT_EQ(sizeThenWidth.lockBox.GetSize().y, 0.12f);
}

TEST(AnimationTests, TimeHandleGetTimeValueIsNulloptUntilSetAndAfterClear)
{
    PrimitiveFixtureReticle reticle;

    EXPECT_EQ(reticle.missionTime.GetTimeValue(), std::nullopt);

    mfd::TimeValue value {};
    value.hour = 13;
    value.minute = 45;
    reticle.missionTime.SetTimeValue(value);
    ASSERT_TRUE(reticle.missionTime.GetTimeValue().has_value());
    EXPECT_EQ(reticle.missionTime.GetTimeValue()->hour, 13);

    reticle.missionTime.ClearTimeValue();
    EXPECT_EQ(reticle.missionTime.GetTimeValue(), std::nullopt);
}

TEST(AnimationTests, TimeHandleExposesVisibleGetter)
{
    BaselineFixtureReticle baselineReticle;

    EXPECT_FALSE(baselineReticle.missionTime.GetVisible());

    baselineReticle.missionTime.SetVisible(true);
    EXPECT_TRUE(baselineReticle.missionTime.GetVisible());
}

TEST(AnimationTests, PrimitiveGetterCallsDoNotMutateReticlePatchOrAffectAppendedCommands)
{
    GeneratedGeometryFixtureReticle reticle;

    reticle.horizonLine.GetVisible();
    reticle.horizonLine.GetPosition();
    reticle.cursorCircle.GetRadius();
    reticle.scopeRing.GetInnerRadius();
    reticle.lockBox.GetWidth();
    reticle.lockBox.GetSize();
    reticle.uncertaintyEllipse.GetHeight();
    reticle.targetSquare.GetSize();
    reticle.steerDiamond.GetWidth();
    reticle.warningTriangle.GetPoints();
    reticle.routePolyline.GetPoints();
    reticle.guideBezier.GetControlPoints();
    reticle.scanArc.GetRadius();

    std::vector<mfd::UserCommand> commands;
    EXPECT_FALSE(reticle.AppendCommands(commands));
    EXPECT_TRUE(commands.empty());
}

TEST(AnimationTests, DynamicReticleBaselineFeedsGettersUntilOverridden)
{
    mfd::client::DynamicReticle dynamic("track", MakeNonDefaultReticleBaseline());

    EXPECT_FALSE(dynamic.GetVisible());
    EXPECT_FLOAT_EQ(dynamic.GetPosition().x, 1.5f);
    EXPECT_FLOAT_EQ(dynamic.GetPosition().y, -2.5f);
    EXPECT_FLOAT_EQ(dynamic.GetRotationDegrees(), 45.0f);
    EXPECT_FLOAT_EQ(dynamic.GetScale().x, 2.0f);
    EXPECT_FLOAT_EQ(dynamic.GetScale().y, 3.0f);
    EXPECT_EQ(dynamic.GetText(), "reticle-baseline-text");

    dynamic.SetVisible(true);
    dynamic.SetPosition({0.4f, 0.5f});
    dynamic.SetText("override");
    EXPECT_TRUE(dynamic.GetVisible());
    EXPECT_FLOAT_EQ(dynamic.GetPosition().x, 0.4f);
    EXPECT_EQ(dynamic.GetText(), "override");
}

TEST(AnimationTests, TextReticleValueRoundTripsAndIsIndependentOfReticleText)
{
    mfd::client::TextReticle textReticle("Radar", "status", "status_value");

    // No SetValue staged yet, and the hand-written TextReticle carries no
    // authored primitive baseline, so the read-back is empty.
    EXPECT_EQ(textReticle.GetValue(), "");

    textReticle.SetValue("LOCK");
    EXPECT_EQ(textReticle.GetValue(), "LOCK");

    // SetValue writes the targeted text primitive, not the reticle-level text,
    // so GetValue() is not a wrapper over the reticle-level GetText(): the
    // reticle-level text stays empty.
    EXPECT_EQ(textReticle.GetText(), "");
}

TEST(AnimationTests, PrimitiveHandleExposesCommonGettersThroughBaseReference)
{
    GeneratedGeometryFixtureReticle reticle;
    mfd::client::PrimitiveHandle& base = reticle.cursorCircle;

    base.SetVisible(false);
    base.SetPosition({0.3f, 0.4f});
    base.SetRotationDegrees(7.0f);
    base.SetScale({1.1f, 1.2f});

    EXPECT_FALSE(base.GetVisible());
    EXPECT_FLOAT_EQ(base.GetPosition().x, 0.3f);
    EXPECT_FLOAT_EQ(base.GetPosition().y, 0.4f);
    EXPECT_FLOAT_EQ(base.GetRotationDegrees(), 7.0f);
    EXPECT_FLOAT_EQ(base.GetScale().x, 1.1f);
    EXPECT_FLOAT_EQ(base.GetScale().y, 1.2f);
}

TEST(AnimationTests, RingBezierArcSegmentDefaultsMatchPerTypeModelDefaults)
{
    PrimitiveFixtureReticle reticle;

    EXPECT_EQ(reticle.compassRing.GetSegments(), 64);
    EXPECT_EQ(reticle.guideBezier.GetSegments(), 32);
    EXPECT_EQ(reticle.scanArc.GetSegments(), 48);

    // An explicit authored baseline overrides the per-type model default.
    BaselineFixtureReticle authored;
    EXPECT_EQ(authored.scopeRing.GetSegments(), 7);
    EXPECT_EQ(authored.guideBezier.GetSegments(), 7);
    EXPECT_EQ(authored.scanArc.GetSegments(), 7);

    // A user override wins over both the baseline and the model default.
    reticle.compassRing.SetSegments(20);
    EXPECT_EQ(reticle.compassRing.GetSegments(), 20);
}

TEST(AnimationTests, NonAllocatingGettersAliasHandleStorageAcrossRepeatedReads)
{
    BaselineFixtureReticle reticle;

    // Repeated reads return a reference to the same handle-owned storage.
    const std::vector<mfd::Vec2>& pointsA = reticle.routePolyline.GetPoints();
    const std::vector<mfd::Vec2>& pointsB = reticle.routePolyline.GetPoints();
    EXPECT_EQ(&pointsA, &pointsB);

    const std::vector<mfd::Vec2>& controlA = reticle.guideBezier.GetControlPoints();
    const std::vector<mfd::Vec2>& controlB = reticle.guideBezier.GetControlPoints();
    EXPECT_EQ(&controlA, &controlB);

    const std::string_view text = reticle.headingValue.GetText();
    EXPECT_EQ(text, "baseline-text");

    // Reading never stages a command.
    std::vector<mfd::UserCommand> commands;
    EXPECT_FALSE(reticle.AppendCommands(commands));
    EXPECT_TRUE(commands.empty());
}

TEST(AnimationTests, DynamicReticleSetBatchesUpsertsAndEmitsRemovalsForMissingReticles)
{
    mfd::client::BlinkType caution("caution");
    mfd::client::DynamicReticleSet set("Radar", "radar_track");

    mfd::client::DynamicReticle& alpha = set.Upsert("alpha");
    alpha.SetPosition({0.10f, 0.20f});
    alpha.SetText("label", "A1");

    mfd::client::DynamicReticle& bravo = set.Upsert("bravo");
    bravo.SetColor({9, 8, 7, 255});
    bravo.SetBlinkType(caution);

    std::vector<mfd::UserCommand> commands;
    EXPECT_EQ(set.AppendCommands(commands), 2U);
    ASSERT_EQ(commands.size(), 1U);

    const auto* initialBatch = std::get_if<mfd::UpsertDynamicReticlesCommand>(&commands.front());
    ASSERT_NE(initialBatch, nullptr);
    EXPECT_EQ(initialBatch->page, "Radar");
    EXPECT_EQ(initialBatch->templateId, "radar_track");
    ASSERT_EQ(initialBatch->reticles.size(), 2U);
    EXPECT_EQ(initialBatch->reticles[0].reticleId, "alpha");
    EXPECT_FLOAT_EQ(initialBatch->reticles[0].patch.position->x, 0.10f);
    EXPECT_EQ(initialBatch->reticles[0].patch.texts.at("label"), "A1");
    EXPECT_EQ(initialBatch->reticles[1].reticleId, "bravo");
    EXPECT_EQ(initialBatch->reticles[1].patch.color->r, 9U);
    EXPECT_EQ(*initialBatch->reticles[1].patch.blinkType, "caution");

    set.Reset();
    mfd::client::DynamicReticle& alphaOnly = set.Upsert("alpha");
    alphaOnly.SetPosition({0.40f, -0.15f});

    commands.clear();
    EXPECT_EQ(set.AppendCommands(commands), 2U);
    ASSERT_EQ(commands.size(), 2U);

    const auto* remove = std::get_if<mfd::RemoveDynamicReticleCommand>(&commands[0]);
    ASSERT_NE(remove, nullptr);
    EXPECT_EQ(remove->target.page, "Radar");
    EXPECT_EQ(remove->target.reticleId, "bravo");

    const auto* updateBatch = std::get_if<mfd::UpsertDynamicReticlesCommand>(&commands[1]);
    ASSERT_NE(updateBatch, nullptr);
    ASSERT_EQ(updateBatch->reticles.size(), 1U);
    EXPECT_EQ(updateBatch->reticles[0].reticleId, "alpha");
    EXPECT_FLOAT_EQ(updateBatch->reticles[0].patch.position->x, 0.40f);
    EXPECT_FLOAT_EQ(updateBatch->reticles[0].patch.position->y, -0.15f);
    EXPECT_FALSE(updateBatch->reticles[0].patch.color.has_value());
    EXPECT_FALSE(updateBatch->reticles[0].patch.blinkEnabled.has_value());
    EXPECT_FALSE(updateBatch->reticles[0].patch.blinkType.has_value());
    EXPECT_TRUE(updateBatch->reticles[0].patch.texts.empty());
    EXPECT_TRUE(updateBatch->reticles[0].patch.letterSpacings.empty());
}

TEST(AnimationTests, DynamicReticleSetCarriesGeneratedPageAndTemplateIdsWhenKnown)
{
    mfd::client::DynamicReticleSet set("Radar", "radar_track", 11U, 77U);
    set.SetVisible(false);
    set.Upsert("alpha").SetPosition({0.15f, -0.10f});

    std::vector<mfd::UserCommand> commands;
    const std::size_t count = set.AppendCommands(commands);
    ASSERT_EQ(count, 2U);
    ASSERT_EQ(commands.size(), 2U);

    const auto* visibility = std::get_if<mfd::SetDynamicReticleSetVisibilityCommand>(&commands[0]);
    ASSERT_NE(visibility, nullptr);
    EXPECT_EQ(visibility->pageId, 11U);
    EXPECT_EQ(visibility->templateTransportId, 77U);

    const auto* upsert = std::get_if<mfd::UpsertDynamicReticlesCommand>(&commands[1]);
    ASSERT_NE(upsert, nullptr);
    EXPECT_EQ(upsert->pageId, 11U);
    EXPECT_EQ(upsert->templateTransportId, 77U);

    set.Reset();
    commands.clear();
    const std::size_t removalCount = set.AppendCommands(commands);
    ASSERT_EQ(removalCount, 1U);
    ASSERT_EQ(commands.size(), 1U);

    const auto* removal = std::get_if<mfd::RemoveDynamicReticleCommand>(&commands.front());
    ASSERT_NE(removal, nullptr);
    EXPECT_EQ(removal->target.pageId, 11U);
}

TEST(AnimationTests, GeneratedDynamicReticleSetCreatesPersistentEntriesWithoutUserIds)
{
    GeneratedDynamicFixtureSet set;
    GeneratedDynamicFixtureReticle& track = set.CreateTrack();

    track.label.SetText("A1");
    track.vectorLine.SetLineStyle(mfd::client::LineStyle::Dotted);
    track.vectorLine.SetStart({-0.25f, 0.15f});
    track.vectorLine.SetEnd({0.25f, 0.15f});
    track.SetPosition({0.12f, -0.08f});

    std::vector<mfd::UserCommand> commands;
    const std::size_t firstCount = set.AppendCommands(commands);
    ASSERT_EQ(firstCount, 1U);
    ASSERT_EQ(commands.size(), 1U);

    const auto* upsert = std::get_if<mfd::UpsertDynamicReticlesCommand>(&commands.front());
    ASSERT_NE(upsert, nullptr);
    EXPECT_EQ(upsert->pageId, 11U);
    EXPECT_EQ(upsert->templateTransportId, 77U);
    ASSERT_EQ(upsert->reticles.size(), 1U);
    EXPECT_FALSE(upsert->reticles.front().reticleId.empty());
    EXPECT_FALSE(upsert->reticles.front().patch.blinkType.has_value());
    ASSERT_EQ(upsert->reticles.front().patch.primitivePatches.find("track_label"),
              upsert->reticles.front().patch.primitivePatches.end());
    ASSERT_NE(upsert->reticles.front().patch.primitivePatchesById.find(33U),
              upsert->reticles.front().patch.primitivePatchesById.end());
    ASSERT_TRUE(upsert->reticles.front().patch.primitivePatchesById.at(33U).text.has_value());
    EXPECT_EQ(*upsert->reticles.front().patch.primitivePatchesById.at(33U).text, "A1");
    ASSERT_NE(upsert->reticles.front().patch.primitivePatchesById.find(34U),
              upsert->reticles.front().patch.primitivePatchesById.end());
    ASSERT_TRUE(upsert->reticles.front().patch.primitivePatchesById.at(34U).lineStyle.has_value());
    ASSERT_TRUE(upsert->reticles.front().patch.primitivePatchesById.at(34U).lineStart.has_value());
    ASSERT_TRUE(upsert->reticles.front().patch.primitivePatchesById.at(34U).lineEnd.has_value());
    EXPECT_EQ(*upsert->reticles.front().patch.primitivePatchesById.at(34U).lineStyle, mfd::LineStyle::Dotted);
    EXPECT_FLOAT_EQ(upsert->reticles.front().patch.primitivePatchesById.at(34U).lineStart->x, -0.25f);
    EXPECT_FLOAT_EQ(upsert->reticles.front().patch.primitivePatchesById.at(34U).lineEnd->x, 0.25f);

    commands.clear();
    EXPECT_EQ(set.AppendCommands(commands), 0U);
    EXPECT_TRUE(commands.empty());

    set.Remove(track);
    EXPECT_FALSE(track.IsAlive());
    EXPECT_EQ(set.AppendCommands(commands), 1U);
    ASSERT_EQ(commands.size(), 1U);

    const auto* remove = std::get_if<mfd::RemoveDynamicReticleCommand>(&commands.front());
    ASSERT_NE(remove, nullptr);
    EXPECT_EQ(remove->target.page, "Radar");
    EXPECT_EQ(remove->target.pageId, 11U);
    EXPECT_FALSE(remove->target.reticleId.empty());
    EXPECT_NE(remove->target.runtimeReticleId, 0U);
}

TEST(AnimationTests, DynamicReticleSetBoundsDistinctIdsWhileKeepingExistingReferencesStable)
{
    constexpr std::size_t kDistinctIdentifierLimit = 4096U;
    mfd::client::DynamicReticleSet set("Radar", "radar_track");
    mfd::client::DynamicReticle& firstReticle = set.Upsert("track_0");

    for (std::size_t index = 1; index < kDistinctIdentifierLimit; ++index)
    {
        set.Upsert("track_" + std::to_string(index));
    }

    EXPECT_EQ(&set.Upsert("track_0"), &firstReticle);
    EXPECT_THROW(set.Upsert("beyond_retained_limit"), std::length_error);
    EXPECT_EQ(firstReticle.Id(), "track_0");
}

TEST(AnimationTests, GeneratedDynamicReticleSetBoundsTombstonesWithoutAliasingStableReferences)
{
    constexpr std::size_t kRetainedReferenceLimit = 4096U;
    GeneratedDynamicFixtureSet set;
    GeneratedDynamicFixtureReticle* firstReticle = nullptr;

    for (std::size_t index = 0; index < kRetainedReferenceLimit; ++index)
    {
        GeneratedDynamicFixtureReticle& reticle = set.CreateTrack();
        if (index == 0)
        {
            firstReticle = &reticle;
        }
        set.Remove(reticle);
    }

    ASSERT_NE(firstReticle, nullptr);
    EXPECT_FALSE(firstReticle->IsAlive());
    EXPECT_THROW(set.CreateTrack(), std::length_error);
    EXPECT_EQ(firstReticle->Id(), "__generated_dynamic_1");
}

TEST(AnimationTests, GeneratedDynamicReticleSetResetInvalidatesPublishedHandlesWithoutEmittingPerReticleRemoval)
{
    GeneratedDynamicFixtureSet set;
    GeneratedDynamicFixtureReticle& track = set.CreateTrack();
    track.label.SetText("A1");

    std::vector<mfd::UserCommand> commands;
    EXPECT_EQ(set.AppendCommands(commands), 1U);
    ASSERT_TRUE(track.IsAlive());

    set.Reset();
    EXPECT_FALSE(track.IsAlive());

    commands.clear();
    EXPECT_EQ(set.AppendCommands(commands), 0U);
    EXPECT_TRUE(commands.empty());

    commands.clear();
    EXPECT_EQ(set.AppendRemovalCommands(commands), 0U);
    EXPECT_TRUE(commands.empty());
}

TEST(AnimationTests, GeneratedDynamicReticleInitialPatchCarriesExplicitVisibility)
{
    GeneratedDynamicFixtureSet set;
    GeneratedDynamicFixtureReticle& track = set.CreateTrack();
    track.SetVisible(true);
    track.SetPosition({0.12f, -0.08f});

    std::vector<mfd::UserCommand> commands;
    EXPECT_EQ(set.AppendCommands(commands), 1U);
    ASSERT_EQ(commands.size(), 1U);

    const auto* upsert = std::get_if<mfd::UpsertDynamicReticlesCommand>(&commands.front());
    ASSERT_NE(upsert, nullptr);
    ASSERT_EQ(upsert->reticles.size(), 1U);
    ASSERT_TRUE(upsert->reticles.front().patch.visible.has_value());
    EXPECT_TRUE(*upsert->reticles.front().patch.visible);
}

TEST(AnimationTests, GeneratedDynamicReticleSetAppendRemovalCommandsClearsPublishedEntries)
{
    GeneratedDynamicFixtureSet set;
    GeneratedDynamicFixtureReticle& alpha = set.CreateTrack();
    GeneratedDynamicFixtureReticle& bravo = set.CreateTrack();

    alpha.label.SetText("A1");
    bravo.label.SetText("B2");

    std::vector<mfd::UserCommand> commands;
    EXPECT_EQ(set.AppendCommands(commands), 2U);
    ASSERT_EQ(commands.size(), 1U);

    commands.clear();
    EXPECT_EQ(set.AppendRemovalCommands(commands), 2U);
    ASSERT_EQ(commands.size(), 2U);

    std::vector<mfd::RuntimeDynamicId> runtimeIds;
    for (const mfd::UserCommand& command : commands)
    {
        const auto* remove = std::get_if<mfd::RemoveDynamicReticleCommand>(&command);
        ASSERT_NE(remove, nullptr);
        EXPECT_EQ(remove->target.page, "Radar");
        EXPECT_EQ(remove->target.pageId, 11U);
        EXPECT_FALSE(remove->target.reticleId.empty());
        EXPECT_NE(remove->target.runtimeReticleId, 0U);
        runtimeIds.push_back(remove->target.runtimeReticleId);
    }

    ASSERT_EQ(runtimeIds.size(), 2U);
    EXPECT_NE(runtimeIds[0], runtimeIds[1]);

    commands.clear();
    EXPECT_EQ(set.AppendRemovalCommands(commands), 0U);
    EXPECT_TRUE(commands.empty());

    GeneratedDynamicFixtureReticle& replacement = set.CreateTrack();
    replacement.label.SetText("C3");

    EXPECT_EQ(set.AppendCommands(commands), 1U);
    ASSERT_EQ(commands.size(), 1U);
    const auto* upsert = std::get_if<mfd::UpsertDynamicReticlesCommand>(&commands.front());
    ASSERT_NE(upsert, nullptr);
    ASSERT_EQ(upsert->reticles.size(), 1U);
    EXPECT_EQ(*upsert->reticles.front().patch.primitivePatchesById.at(33U).text, "C3");
}

TEST(AnimationTests, RuntimeFeedbackStateTracksActivePageAndCapturedDynamicReticle)
{
    mfd::client::RuntimeFeedbackState feedbackState;
    EXPECT_FALSE(feedbackState.HasActivePage());
    EXPECT_FALSE(feedbackState.IsPageActive("Radar"));
    EXPECT_FALSE(feedbackState.IsDynamicReticleCaptured(42U, 1001U));

    mfd::ActivePageFeedback activePage;
    activePage.sequence = 3U;
    activePage.pageName = "Radar";
    EXPECT_TRUE(feedbackState.Apply(activePage));
    EXPECT_TRUE(feedbackState.HasActivePage());
    EXPECT_EQ(feedbackState.ActivePageName(), "Radar");
    EXPECT_TRUE(feedbackState.IsPageActive("Radar"));
    EXPECT_FALSE(feedbackState.IsPageActive("Navigation"));

    mfd::StrobeStatusFeedback strobe;
    strobe.sequence = 4U;
    strobe.pageId = 42U;
    strobe.pageName = "Radar";
    mfd::StrobeFeedbackCapture capture;
    capture.runtimeReticleId = 1001U;
    strobe.captureResult = std::move(capture);
    EXPECT_TRUE(feedbackState.Apply(strobe));
    EXPECT_TRUE(feedbackState.IsDynamicReticleCaptured(42U, 1001U));
    EXPECT_FALSE(feedbackState.IsDynamicReticleCaptured(42U, 1002U));

    mfd::StrobeStatusFeedback cleared;
    cleared.sequence = 5U;
    cleared.pageId = 42U;
    cleared.pageName = "Radar";
    EXPECT_TRUE(feedbackState.Apply(cleared));
    EXPECT_FALSE(feedbackState.IsDynamicReticleCaptured(42U, 1001U));
}

TEST(AnimationTests, RuntimeFeedbackStateEvictsLeastRecentlyAppliedHistoricalPages)
{
    mfd::client::RuntimeFeedbackState feedbackState;
    constexpr mfd::TransportId kNewestPageId = 257U;

    for (mfd::TransportId pageId = 1U; pageId <= kNewestPageId; ++pageId)
    {
        mfd::StrobeStatusFeedback feedback;
        feedback.sequence = static_cast<std::uint32_t>(pageId);
        feedback.pageId = pageId;
        feedback.pageName = "Page_" + std::to_string(pageId);
        mfd::StrobeFeedbackCapture capture;
        capture.runtimeReticleId = pageId;
        feedback.captureResult = std::move(capture);
        EXPECT_TRUE(feedbackState.Apply(feedback));
    }

    mfd::ActivePageFeedback activePage;
    activePage.sequence = 1U;
    activePage.pageName = "Page_257";
    ASSERT_TRUE(feedbackState.Apply(activePage));

    EXPECT_TRUE(feedbackState.IsDynamicReticleCaptured(kNewestPageId, kNewestPageId));
    EXPECT_FALSE(feedbackState.IsDynamicReticleCaptured(1U, 1U));
}

TEST(AnimationTests, RuntimeFeedbackStateTracksWindowLifecycleAndDecodedPacketCount)
{
    mfd::client::RuntimeFeedbackState feedbackState;
    EXPECT_FALSE(feedbackState.HasWindowLifecycle());
    EXPECT_FALSE(feedbackState.WindowReportedClosing());
    EXPECT_EQ(feedbackState.TotalDecodedFeedbackPackets(), 0U);

    mfd::WindowLifecycleFeedback alive;
    alive.sequence = 1U;
    alive.state = mfd::WindowLifecycleState::Alive;
    EXPECT_TRUE(feedbackState.Apply(alive));
    EXPECT_TRUE(feedbackState.HasWindowLifecycle());
    EXPECT_FALSE(feedbackState.WindowReportedClosing());
    EXPECT_EQ(feedbackState.LastWindowLifecycleState(), mfd::WindowLifecycleState::Alive);

    // An unchanged heartbeat reports no state change but still counts as a decoded packet.
    EXPECT_FALSE(feedbackState.ApplyPayload(mfd::SerializeWindowLifecycleFeedback(alive)));
    EXPECT_EQ(feedbackState.TotalDecodedFeedbackPackets(), 1U);

    mfd::WindowLifecycleFeedback closing;
    closing.sequence = 2U;
    closing.state = mfd::WindowLifecycleState::Closing;
    EXPECT_TRUE(feedbackState.ApplyPayload(mfd::SerializeWindowLifecycleFeedback(closing)));
    EXPECT_EQ(feedbackState.TotalDecodedFeedbackPackets(), 2U);
    EXPECT_TRUE(feedbackState.WindowReportedClosing());
    EXPECT_EQ(feedbackState.LastWindowLifecycleState(), mfd::WindowLifecycleState::Closing);

    // A reordered older Alive must not override the more recent Closing.
    mfd::WindowLifecycleFeedback staleAlive;
    staleAlive.sequence = 1U;
    staleAlive.state = mfd::WindowLifecycleState::Alive;
    EXPECT_FALSE(feedbackState.Apply(staleAlive));
    EXPECT_TRUE(feedbackState.WindowReportedClosing());

    feedbackState.Reset();
    EXPECT_FALSE(feedbackState.HasWindowLifecycle());
    EXPECT_FALSE(feedbackState.WindowReportedClosing());
    EXPECT_EQ(feedbackState.TotalDecodedFeedbackPackets(), 0U);
}

TEST(AnimationTests, RuntimeFeedbackStateTracksCapturedDynamicReticleByTransportIds)
{
    mfd::client::RuntimeFeedbackState feedbackState;

    mfd::ActivePageFeedback activePage;
    activePage.sequence = 20U;
    activePage.pageName = "Radar";
    EXPECT_TRUE(feedbackState.Apply(activePage));

    mfd::StrobeStatusFeedback feedback;
    feedback.sequence = 21U;
    feedback.pageId = 42U;
    feedback.pageName = "Radar";
    mfd::StrobeFeedbackCapture capture;
    capture.runtimeReticleId = 1002U;
    capture.reticleId = "legacy_alias";
    feedback.captureResult = std::move(capture);

    EXPECT_TRUE(feedbackState.Apply(feedback));
    EXPECT_TRUE(feedbackState.IsDynamicReticleCaptured(42U, 1002U));
    EXPECT_FALSE(feedbackState.IsDynamicReticleCaptured(42U, 1001U));
}

TEST(AnimationTests, RuntimeFeedbackStateClearsStaleCaptureWhenActivePageChanges)
{
    mfd::client::RuntimeFeedbackState feedbackState;

    mfd::ActivePageFeedback activePage;
    activePage.sequence = 30U;
    activePage.pageName = "Radar";
    EXPECT_TRUE(feedbackState.Apply(activePage));

    mfd::StrobeStatusFeedback captureOnRadar;
    captureOnRadar.sequence = 31U;
    captureOnRadar.pageId = 42U;
    captureOnRadar.pageName = "Radar";
    mfd::StrobeFeedbackCapture capture;
    capture.runtimeReticleId = 9001U;
    captureOnRadar.captureResult = std::move(capture);
    EXPECT_TRUE(feedbackState.Apply(captureOnRadar));
    EXPECT_TRUE(feedbackState.IsDynamicReticleCaptured(42U, 9001U));

    mfd::ActivePageFeedback switchToNav;
    switchToNav.sequence = 32U;
    switchToNav.pageName = "Navigation";
    EXPECT_TRUE(feedbackState.Apply(switchToNav));
    EXPECT_FALSE(feedbackState.IsDynamicReticleCaptured(42U, 9001U));

    mfd::StrobeStatusFeedback reorderedFreshRadarCapture;
    reorderedFreshRadarCapture.sequence = 34U;
    reorderedFreshRadarCapture.pageId = 42U;
    reorderedFreshRadarCapture.pageName = "Radar";
    mfd::StrobeFeedbackCapture reorderedCapture;
    reorderedCapture.runtimeReticleId = 9002U;
    reorderedFreshRadarCapture.captureResult = std::move(reorderedCapture);
    EXPECT_TRUE(feedbackState.Apply(reorderedFreshRadarCapture));
    EXPECT_FALSE(feedbackState.IsDynamicReticleCaptured(42U, 9002U));

    mfd::ActivePageFeedback switchBackToRadar;
    switchBackToRadar.sequence = 33U;
    switchBackToRadar.pageName = "Radar";
    EXPECT_TRUE(feedbackState.Apply(switchBackToRadar));
    EXPECT_TRUE(feedbackState.IsDynamicReticleCaptured(42U, 9002U));
}

TEST(AnimationTests, RuntimeFeedbackStateIgnoresOlderOutOfOrderFeedback)
{
    mfd::client::RuntimeFeedbackState feedbackState;

    mfd::ActivePageFeedback activePage;
    activePage.sequence = 10U;
    activePage.pageName = "Radar";
    EXPECT_TRUE(feedbackState.Apply(activePage));

    mfd::ActivePageFeedback staleActivePage;
    staleActivePage.sequence = 9U;
    staleActivePage.pageName = "Navigation";
    EXPECT_FALSE(feedbackState.Apply(staleActivePage));
    EXPECT_TRUE(feedbackState.IsPageActive("Radar"));

    mfd::StrobeStatusFeedback freshStrobe;
    freshStrobe.sequence = 12U;
    freshStrobe.pageId = 42U;
    freshStrobe.pageName = "Radar";
    mfd::StrobeFeedbackCapture freshCapture;
    freshCapture.runtimeReticleId = 9001U;
    freshStrobe.captureResult = std::move(freshCapture);
    EXPECT_TRUE(feedbackState.Apply(freshStrobe));

    mfd::StrobeStatusFeedback staleStrobe;
    staleStrobe.sequence = 11U;
    staleStrobe.pageId = 42U;
    staleStrobe.pageName = "Radar";
    EXPECT_FALSE(feedbackState.Apply(staleStrobe));
    EXPECT_TRUE(feedbackState.IsDynamicReticleCaptured(42U, 9001U));
}

TEST(AnimationTests, GeneratedDynamicReticleReportsCaptureFromRuntimeFeedbackState)
{
    mfd::client::RuntimeFeedbackState feedbackState;
    GeneratedDynamicFixtureSet set(&feedbackState);
    GeneratedDynamicFixtureReticle& track = set.CreateTrack();

    EXPECT_FALSE(track.IsStrobeCaptured());

    std::vector<mfd::UserCommand> commands;
    EXPECT_EQ(set.AppendCommands(commands), 1U);
    ASSERT_EQ(commands.size(), 1U);
    const auto* upsert = std::get_if<mfd::UpsertDynamicReticlesCommand>(&commands.front());
    ASSERT_NE(upsert, nullptr);
    ASSERT_EQ(upsert->reticles.size(), 1U);

    mfd::ActivePageFeedback activePage;
    activePage.sequence = 13U;
    activePage.pageName = "Radar";
    EXPECT_TRUE(feedbackState.Apply(activePage));

    mfd::StrobeStatusFeedback feedback;
    feedback.sequence = 14U;
    feedback.pageId = 11U;
    feedback.pageName = "Radar";
    mfd::StrobeFeedbackCapture capture;
    capture.runtimeReticleId = upsert->reticles.front().runtimeReticleId;
    capture.reticleId = track.Id();
    feedback.captureResult = std::move(capture);
    EXPECT_TRUE(feedbackState.Apply(feedback));
    EXPECT_TRUE(track.IsStrobeCaptured());

    mfd::StrobeStatusFeedback cleared;
    cleared.sequence = 15U;
    cleared.pageId = 11U;
    cleared.pageName = "Radar";
    EXPECT_TRUE(feedbackState.Apply(cleared));
    EXPECT_FALSE(track.IsStrobeCaptured());
}

TEST(AnimationTests, GeneratedDynamicReticlePrefersIdBasedCaptureWhenAvailable)
{
    mfd::client::RuntimeFeedbackState feedbackState;
    GeneratedDynamicFixtureSet set(&feedbackState);
    GeneratedDynamicFixtureReticle& alpha = set.CreateTrack();
    GeneratedDynamicFixtureReticle& bravo = set.CreateTrack();

    std::vector<mfd::UserCommand> commands;
    ASSERT_EQ(set.AppendCommands(commands), 2U);
    ASSERT_EQ(commands.size(), 1U);

    const auto* upsert = std::get_if<mfd::UpsertDynamicReticlesCommand>(&commands.front());
    ASSERT_NE(upsert, nullptr);
    ASSERT_EQ(upsert->reticles.size(), 2U);

    const mfd::RuntimeDynamicId alphaRuntimeId = upsert->reticles[0].runtimeReticleId;
    const mfd::RuntimeDynamicId bravoRuntimeId = upsert->reticles[1].runtimeReticleId;
    ASSERT_NE(alphaRuntimeId, 0U);
    ASSERT_NE(bravoRuntimeId, 0U);
    ASSERT_NE(alphaRuntimeId, bravoRuntimeId);

    mfd::ActivePageFeedback activePage;
    activePage.sequence = 21U;
    activePage.pageName = "Radar";
    EXPECT_TRUE(feedbackState.Apply(activePage));

    mfd::StrobeStatusFeedback feedback;
    feedback.sequence = 22U;
    feedback.pageId = 11U;
    feedback.pageName = "Radar";
    mfd::StrobeFeedbackCapture capture;
    capture.runtimeReticleId = bravoRuntimeId;
    capture.reticleId = alpha.Id();
    feedback.captureResult = std::move(capture);

    EXPECT_TRUE(feedbackState.Apply(feedback));
    EXPECT_FALSE(alpha.IsStrobeCaptured());
    EXPECT_TRUE(bravo.IsStrobeCaptured());
}

TEST(AnimationTests, WindowDisplaySuppressesDuplicateUpdatesAndSupportsShutdownRemoval)
{
    mfd::client::DynamicReticleSet set("Radar", "radar_track");
    mfd::client::DynamicReticle& track = set.Upsert("shutdown");
    track.SetVisible(true);

    std::vector<mfd::UserCommand> commands;
    EXPECT_EQ(set.AppendCommands(commands), 1U);

    commands.clear();
    EXPECT_EQ(set.AppendRemovalCommands(commands), 1U);
    ASSERT_EQ(commands.size(), 1U);
    const auto* remove = std::get_if<mfd::RemoveDynamicReticleCommand>(&commands.front());
    ASSERT_NE(remove, nullptr);
    EXPECT_EQ(remove->target.reticleId, "shutdown");

    mfd::client::WindowDisplay display;
    display.SetColorInverted(true);
    display.SetBrightness(0.35f);
    display.SetDisabled(true);

    commands.clear();
    EXPECT_TRUE(display.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);

    const auto* update = std::get_if<mfd::UpdateWindowDisplayCommand>(&commands.front());
    ASSERT_NE(update, nullptr);
    ASSERT_TRUE(update->patch.invertColors.has_value());
    ASSERT_TRUE(update->patch.brightness.has_value());
    ASSERT_TRUE(update->patch.disabled.has_value());
    EXPECT_TRUE(*update->patch.invertColors);
    EXPECT_FLOAT_EQ(*update->patch.brightness, 0.35f);
    EXPECT_TRUE(*update->patch.disabled);

    commands.clear();
    EXPECT_FALSE(display.AppendCommands(commands));
    EXPECT_TRUE(commands.empty());

    display.SetBrightness(0.60f);
    EXPECT_TRUE(display.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);

    const auto* brightnessOnly = std::get_if<mfd::UpdateWindowDisplayCommand>(&commands.front());
    ASSERT_NE(brightnessOnly, nullptr);
    EXPECT_FALSE(brightnessOnly->patch.invertColors.has_value());
    EXPECT_TRUE(brightnessOnly->patch.brightness.has_value());
    EXPECT_FALSE(brightnessOnly->patch.disabled.has_value());
    EXPECT_FLOAT_EQ(*brightnessOnly->patch.brightness, 0.60f);
}

TEST(AnimationTests, DynamicReticleSetVisibilityEmitsDedicatedTemplateCommand)
{
    mfd::client::DynamicReticleSet set("Radar", "radar_track");
    set.SetVisible(false);

    mfd::client::DynamicReticle& track = set.Upsert("alpha");
    track.SetPosition({0.2f, 0.3f});

    std::vector<mfd::UserCommand> commands;
    EXPECT_EQ(set.AppendCommands(commands), 2U);
    ASSERT_EQ(commands.size(), 2U);

    const auto* visibility = std::get_if<mfd::SetDynamicReticleSetVisibilityCommand>(&commands[0]);
    ASSERT_NE(visibility, nullptr);
    EXPECT_EQ(visibility->page, "Radar");
    EXPECT_EQ(visibility->templateId, "radar_track");
    EXPECT_FALSE(visibility->visible);

    const auto* upsert = std::get_if<mfd::UpsertDynamicReticlesCommand>(&commands[1]);
    ASSERT_NE(upsert, nullptr);
    ASSERT_EQ(upsert->reticles.size(), 1U);
    EXPECT_EQ(upsert->reticles[0].reticleId, "alpha");

    commands.clear();
    EXPECT_EQ(set.AppendCommands(commands), 0U);
    EXPECT_TRUE(commands.empty());

    set.SetVisible(true);
    commands.clear();
    EXPECT_EQ(set.AppendCommands(commands), 1U);
    ASSERT_EQ(commands.size(), 1U);

    const auto* showVisibility = std::get_if<mfd::SetDynamicReticleSetVisibilityCommand>(&commands[0]);
    ASSERT_NE(showVisibility, nullptr);
    EXPECT_TRUE(showVisibility->visible);
}

TEST(AnimationTests, DynamicReticleSetStrobeMagnetEligibilityEmitsDedicatedTemplateCommand)
{
    mfd::client::DynamicReticleSet set("Radar", "radar_track", 11U, 77U);
    set.SetStrobeMagnetEnabled(true);

    std::vector<mfd::UserCommand> commands;
    EXPECT_EQ(set.AppendCommands(commands), 1U);
    ASSERT_EQ(commands.size(), 1U);

    const auto* magnet = std::get_if<mfd::SetDynamicReticleSetStrobeMagnetEnabledCommand>(&commands[0]);
    ASSERT_NE(magnet, nullptr);
    EXPECT_EQ(magnet->page, "Radar");
    EXPECT_EQ(magnet->templateId, "radar_track");
    EXPECT_EQ(magnet->pageId, 11U);
    EXPECT_EQ(magnet->templateTransportId, 77U);
    EXPECT_TRUE(magnet->enabled);

    commands.clear();
    EXPECT_EQ(set.AppendCommands(commands), 0U);
    EXPECT_TRUE(commands.empty());

    set.SetStrobeMagnetEnabled(false);
    EXPECT_EQ(set.AppendCommands(commands), 1U);
    ASSERT_EQ(commands.size(), 1U);

    const auto* disabledMagnet =
        std::get_if<mfd::SetDynamicReticleSetStrobeMagnetEnabledCommand>(&commands[0]);
    ASSERT_NE(disabledMagnet, nullptr);
    EXPECT_FALSE(disabledMagnet->enabled);
}

TEST(AnimationTests, GeneratedDynamicReticleSetStrobeMagnetEligibilityEmitsDedicatedTemplateCommand)
{
    GeneratedDynamicFixtureSet set;
    set.SetStrobeMagnetEnabled(true);

    std::vector<mfd::UserCommand> commands;
    EXPECT_EQ(set.AppendCommands(commands), 1U);
    ASSERT_EQ(commands.size(), 1U);

    const auto* magnet = std::get_if<mfd::SetDynamicReticleSetStrobeMagnetEnabledCommand>(&commands[0]);
    ASSERT_NE(magnet, nullptr);
    EXPECT_EQ(magnet->page, "Radar");
    EXPECT_EQ(magnet->templateId, "radar_track");
    EXPECT_EQ(magnet->pageId, 11U);
    EXPECT_EQ(magnet->templateTransportId, 77U);
    EXPECT_TRUE(magnet->enabled);
}

TEST(AnimationTests, StrobeHandleReportsValidityAndEmitsPageScopedCommands)
{
    mfd::client::StrobeInfo info;
    info.valid = true;
    info.capture.shape = mfd::StrobeCaptureShape::Rectangle;
    info.capture.size = {0.4f, 0.2f};
    info.magnet.enabled = true;
    info.magnet.radius = 0.3f;
    info.magnet.strength = 0.6f;

    const mfd::client::StrobeType defaultStrobe("Default", info, 101U, true);
    const mfd::client::StrobeType designatorStrobe("Designator", info, 102U, false);

    mfd::client::StrobeHandle strobe("Radar", {defaultStrobe, designatorStrobe}, 11U);
    EXPECT_TRUE(strobe.IsValid());
    EXPECT_EQ(strobe.PageName(), "Radar");
    ASSERT_NE(strobe.SelectedType(), nullptr);
    EXPECT_EQ(strobe.SelectedType()->GeneratedId(), 101U);
    EXPECT_EQ(strobe.Info().capture.shape, mfd::StrobeCaptureShape::Rectangle);
    EXPECT_TRUE(strobe.Info().magnet.enabled);

    strobe = designatorStrobe;
    strobe.SetActive(false);
    strobe.SetPosition({0.25f, -0.35f});

    std::vector<mfd::UserCommand> commands;
    ASSERT_TRUE(strobe.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);

    const auto* update = std::get_if<mfd::UpdateStrobeCommand>(&commands.front());
    ASSERT_NE(update, nullptr);
    EXPECT_EQ(update->page, "Radar");
    EXPECT_EQ(update->pageId, 11U);
    EXPECT_EQ(update->strobeId, 102U);
    ASSERT_TRUE(update->active.has_value());
    ASSERT_TRUE(update->position.has_value());
    EXPECT_FALSE(*update->active);
    EXPECT_FLOAT_EQ(update->position->x, 0.25f);
    EXPECT_FLOAT_EQ(update->position->y, -0.35f);

    commands.clear();
    EXPECT_FALSE(strobe.AppendCommands(commands));
    EXPECT_TRUE(commands.empty());
}

TEST(AnimationTests, StrobeHandleUsesOnlyThePageScopeEvenWhenGeneratedPageIdsExist)
{
    mfd::client::StrobeInfo info;
    info.valid = true;

    const mfd::client::StrobeType defaultStrobe("Default", info, 21U, true);
    const mfd::client::StrobeType searchStrobe("Search", info, 22U, false);

    mfd::client::StrobeHandle strobe("Radar", {defaultStrobe, searchStrobe}, 11U);
    strobe.Use(searchStrobe);
    strobe.SetActive(true);

    std::vector<mfd::UserCommand> commands;
    ASSERT_TRUE(strobe.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);

    const auto* update = std::get_if<mfd::UpdateStrobeCommand>(&commands.front());
    ASSERT_NE(update, nullptr);
    EXPECT_EQ(update->page, "Radar");
    EXPECT_EQ(update->pageId, 11U);
    EXPECT_EQ(update->strobeId, 22U);
}

TEST(AnimationTests, StrobeHandleSelectsNamedStrobeTypesWithoutGeneratedIds)
{
    mfd::client::StrobeInfo info;
    info.valid = true;

    const mfd::client::StrobeType defaultStrobe("Default", info, 0U, true);
    const mfd::client::StrobeType searchStrobe("Search", info, 0U, false);

    mfd::client::StrobeHandle strobe("Radar", {defaultStrobe, searchStrobe}, 11U);
    strobe.Use(searchStrobe);

    ASSERT_NE(strobe.SelectedType(), nullptr);
    EXPECT_EQ(strobe.SelectedType()->Name(), "Search");
    EXPECT_TRUE(strobe.IsSelected(searchStrobe));
    EXPECT_FALSE(strobe.IsSelected(defaultStrobe));

    std::vector<mfd::UserCommand> commands;
    ASSERT_TRUE(strobe.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);

    const auto* update = std::get_if<mfd::UpdateStrobeCommand>(&commands.front());
    ASSERT_NE(update, nullptr);
    EXPECT_EQ(update->page, "Radar");
    EXPECT_EQ(update->pageId, 11U);
    EXPECT_EQ(update->strobeId, 0U);
    EXPECT_EQ(update->strobe, "Search");
}

TEST(AnimationTests, StrobeHandlePrefersGeneratedIdsWhenSelectingStrobeTypes)
{
    mfd::client::StrobeInfo info;
    info.valid = true;

    const mfd::client::StrobeType defaultStrobe("Default", info, 41U, true);
    const mfd::client::StrobeType searchStrobe("Search", info, 42U, false);

    mfd::client::StrobeHandle strobe("Radar", {defaultStrobe, searchStrobe}, 11U);
    strobe.Use(searchStrobe);

    ASSERT_NE(strobe.SelectedType(), nullptr);
    EXPECT_EQ(strobe.SelectedType()->GeneratedId(), 42U);
    EXPECT_TRUE(strobe.IsSelected(searchStrobe));
    EXPECT_FALSE(strobe.IsSelected(defaultStrobe));

    std::vector<mfd::UserCommand> commands;
    ASSERT_TRUE(strobe.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);

    const auto* update = std::get_if<mfd::UpdateStrobeCommand>(&commands.front());
    ASSERT_NE(update, nullptr);
    EXPECT_EQ(update->page, "Radar");
    EXPECT_EQ(update->pageId, 11U);
    EXPECT_EQ(update->strobeId, 42U);
    EXPECT_TRUE(update->strobe.empty());
}

TEST(AnimationTests, StrobeHandleReplaysCurrentStateWhenSwitchingSelectedStrobe)
{
    mfd::client::StrobeInfo info;
    info.valid = true;

    const mfd::client::StrobeType defaultStrobe("Default", info, 31U, true);
    const mfd::client::StrobeType strobe1("Strobe1", info, 32U, false);

    mfd::client::StrobeHandle strobe("Radar", {defaultStrobe, strobe1}, 11U);
    strobe.SetActive(true);
    strobe.SetPosition({0.05f, -0.10f});

    std::vector<mfd::UserCommand> commands;
    ASSERT_TRUE(strobe.AppendCommands(commands));
    commands.clear();

    strobe = strobe1;
    ASSERT_TRUE(strobe.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);

    const auto* update = std::get_if<mfd::UpdateStrobeCommand>(&commands.front());
    ASSERT_NE(update, nullptr);
    EXPECT_EQ(update->strobeId, 32U);
    ASSERT_TRUE(update->active.has_value());
    ASSERT_TRUE(update->position.has_value());
    EXPECT_TRUE(*update->active);
    EXPECT_FLOAT_EQ(update->position->x, 0.05f);
    EXPECT_FLOAT_EQ(update->position->y, -0.10f);
}

TEST(AnimationTests, InvalidStrobeHandleSuppressesMutationsAndCommands)
{
    mfd::client::StrobeHandle strobe("System", mfd::client::StrobeInfo {});
    EXPECT_FALSE(strobe.IsValid());

    strobe.SetActive(true);
    strobe.SetPosition({0.1f, 0.2f});

    std::vector<mfd::UserCommand> commands;
    EXPECT_FALSE(strobe.AppendCommands(commands));
    EXPECT_TRUE(commands.empty());
}


/**
 * @brief Covers Reticle blink API variants and global text/letter spacing fields.
 */
TEST(AnimationTests, ReticleBlinkApiVariantsEmitExpectedDeltaFields)
{
    mfd::client::BlinkType slow("slow");
    mfd::client::Reticle reticle("HUD", "waterline");

    reticle.SetBlinkEnabled(true);
    reticle.SetBlink(false, slow);
    reticle.SetBlinkType(slow);
    reticle.ClearBlinkType();
    reticle.SetText("GLOBAL");
    reticle.SetLetterSpacing(0.11f);

    std::vector<mfd::UserCommand> commands;
    ASSERT_TRUE(reticle.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);

    const auto* update = std::get_if<mfd::UpdateReticleCommand>(&commands.front());
    ASSERT_NE(update, nullptr);
    ASSERT_TRUE(update->patch.blinkEnabled.has_value());
    ASSERT_TRUE(update->patch.blinkType.has_value());
    ASSERT_TRUE(update->patch.text.has_value());
    ASSERT_TRUE(update->patch.letterSpacing.has_value());
    EXPECT_TRUE(*update->patch.blinkEnabled);
    EXPECT_TRUE(update->patch.blinkType->empty());
    EXPECT_EQ(*update->patch.text, "GLOBAL");
    EXPECT_FLOAT_EQ(*update->patch.letterSpacing, 0.11f);
}

/**
 * @brief Ensures Reset() only clears dirty state and therefore suppresses redundant emissions.
 */
TEST(AnimationTests, ReticleResetSuppressesEmissionUntilANewMutation)
{
    mfd::client::Reticle reticle("HUD", "waterline");
    reticle.SetVisible(true);
    reticle.Reset();

    std::vector<mfd::UserCommand> commands;
    EXPECT_FALSE(reticle.AppendCommands(commands));
    EXPECT_TRUE(commands.empty());

    reticle.SetVisible(false);
    EXPECT_TRUE(reticle.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);
    const auto* update = std::get_if<mfd::UpdateReticleCommand>(&commands.front());
    ASSERT_NE(update, nullptr);
    ASSERT_TRUE(update->patch.visible.has_value());
    EXPECT_FALSE(*update->patch.visible);
}

TEST(AnimationTests, ReticleResetToAuthoredRebasesFutureDeltasOnTheAuthoredBaseline)
{
    mfd::client::Reticle reticle("HUD", "waterline");
    reticle.SetVisible(true);

    std::vector<mfd::UserCommand> commands;
    ASSERT_TRUE(reticle.AppendCommands(commands));
    commands.clear();

    reticle.ResetToAuthored();
    reticle.SetColor({1, 2, 3, 255});

    ASSERT_TRUE(reticle.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);
    const auto* update = std::get_if<mfd::UpdateReticleCommand>(&commands.front());
    ASSERT_NE(update, nullptr);
    ASSERT_TRUE(update->patch.color.has_value());
    EXPECT_EQ(update->patch.color->r, 1U);
    EXPECT_FALSE(update->patch.visible.has_value());
}

TEST(AnimationTests, ReticleReapplyingSameStateDoesNotEmitRedundantCommand)
{
    mfd::client::Reticle reticle("HUD", "waterline");
    reticle.SetVisible(true);

    std::vector<mfd::UserCommand> commands;
    ASSERT_TRUE(reticle.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);

    commands.clear();
    reticle.SetVisible(true);
    EXPECT_FALSE(reticle.AppendCommands(commands));
    EXPECT_TRUE(commands.empty());

    commands.clear();
    EXPECT_FALSE(reticle.AppendCommands(commands));
    EXPECT_TRUE(commands.empty());
}

TEST(AnimationTests, DirectReticlePatchMutationStillEmitsAfterPreviousPublish)
{
    RawPatchFixtureReticle reticle;

    reticle.SetVisible(true);
    std::vector<mfd::UserCommand> commands;
    ASSERT_TRUE(reticle.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);

    commands.clear();
    reticle.SetRawText("RAW");
    EXPECT_TRUE(reticle.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);

    const auto* update = std::get_if<mfd::UpdateReticleCommand>(&commands.front());
    ASSERT_NE(update, nullptr);
    ASSERT_TRUE(update->patch.text.has_value());
    EXPECT_EQ(*update->patch.text, "RAW");
}

/**
 * @brief Validates dynamic reticle updates emit only field deltas after first publish.
 */
TEST(AnimationTests, DynamicReticleSetEmitsOnlyChangedFieldsOnSecondPublish)
{
    mfd::client::DynamicReticleSet set("Radar", "radar_track");
    mfd::client::DynamicReticle& track = set.Upsert("trk_01");
    track.SetPosition({0.2f, 0.1f});
    track.SetColor({10, 20, 30, 255});

    std::vector<mfd::UserCommand> commands;
    EXPECT_EQ(set.AppendCommands(commands), 1U);
    ASSERT_EQ(commands.size(), 1U);

    set.Reset();
    mfd::client::DynamicReticle& sameTrack = set.Upsert("trk_01");
    sameTrack.SetPosition({0.6f, -0.4f});
    sameTrack.SetScale({1.25f, 0.80f});

    commands.clear();
    EXPECT_EQ(set.AppendCommands(commands), 1U);
    ASSERT_EQ(commands.size(), 1U);

    const auto* upsert = std::get_if<mfd::UpsertDynamicReticlesCommand>(&commands.front());
    ASSERT_NE(upsert, nullptr);
    ASSERT_EQ(upsert->reticles.size(), 1U);
    const auto& patch = upsert->reticles.front().patch;
    ASSERT_TRUE(patch.position.has_value());
    ASSERT_TRUE(patch.scale.has_value());
    EXPECT_FLOAT_EQ(patch.position->x, 0.6f);
    EXPECT_FLOAT_EQ(patch.position->y, -0.4f);
    EXPECT_FLOAT_EQ(patch.scale->x, 1.25f);
    EXPECT_FLOAT_EQ(patch.scale->y, 0.80f);
    EXPECT_FALSE(patch.color.has_value());
}

TEST(AnimationTests, GeneratedDynamicReticleSetReapplyingSameStateDoesNotEmitRedundantUpdate)
{
    GeneratedDynamicFixtureSet set;
    GeneratedDynamicFixtureReticle& track = set.CreateTrack();
    track.label.SetText("A1");

    std::vector<mfd::UserCommand> commands;
    EXPECT_EQ(set.AppendCommands(commands), 1U);
    ASSERT_EQ(commands.size(), 1U);

    commands.clear();
    track.label.SetText("A1");
    EXPECT_EQ(set.AppendCommands(commands), 0U);
    EXPECT_TRUE(commands.empty());

    commands.clear();
    EXPECT_EQ(set.AppendCommands(commands), 0U);
    EXPECT_TRUE(commands.empty());
}

/**
 * @brief Confirms Reset() on dynamic set does not delete published reticles until removal cycle.
 */
TEST(AnimationTests, DynamicReticleSetResetThenNoUpsertProducesRemoval)
{
    mfd::client::DynamicReticleSet set("Radar", "radar_track");
    set.Upsert("trk_09").SetVisible(true);

    std::vector<mfd::UserCommand> commands;
    EXPECT_EQ(set.AppendCommands(commands), 1U);

    set.Reset();
    commands.clear();
    EXPECT_EQ(set.AppendCommands(commands), 1U);
    ASSERT_EQ(commands.size(), 1U);
    const auto* remove = std::get_if<mfd::RemoveDynamicReticleCommand>(&commands.front());
    ASSERT_NE(remove, nullptr);
    EXPECT_EQ(remove->target.reticleId, "trk_09");
}

/**
 * @brief Ensures WindowDisplay Reset() suppresses an unsent mutation exactly like Reticle Reset().
 */
TEST(AnimationTests, WindowDisplayResetSuppressesEmissionUntilNextMutation)
{
    mfd::client::WindowDisplay display;
    display.SetDisabled(true);
    display.Reset();

    std::vector<mfd::UserCommand> commands;
    EXPECT_FALSE(display.AppendCommands(commands));
    EXPECT_TRUE(commands.empty());

    display.SetDisabled(false);
    EXPECT_TRUE(display.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);
    const auto* update = std::get_if<mfd::UpdateWindowDisplayCommand>(&commands.front());
    ASSERT_NE(update, nullptr);
    ASSERT_TRUE(update->patch.disabled.has_value());
    EXPECT_FALSE(*update->patch.disabled);
}

TEST(AnimationTests, WindowDisplayResetToAuthoredRebasesFutureDeltasOnAuthoredDefaults)
{
    mfd::client::WindowDisplay display;
    display.SetDisabled(true);

    std::vector<mfd::UserCommand> commands;
    ASSERT_TRUE(display.AppendCommands(commands));
    commands.clear();

    display.ResetToAuthored();
    display.SetBrightness(0.40f);

    ASSERT_TRUE(display.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);
    const auto* update = std::get_if<mfd::UpdateWindowDisplayCommand>(&commands.front());
    ASSERT_NE(update, nullptr);
    ASSERT_TRUE(update->patch.brightness.has_value());
    EXPECT_FLOAT_EQ(*update->patch.brightness, 0.40f);
    EXPECT_FALSE(update->patch.disabled.has_value());
}

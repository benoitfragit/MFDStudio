/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests covering the reticle-studio primitive clipboard helper.
 */

#include "EditorPrimitiveClipboardService.h"

#include <initializer_list>

#include <gtest/gtest.h>

namespace
{
mfd::Primitive MakeRectanglePrimitive(const std::string& id, const float x, const float y)
{
    mfd::Primitive primitive;
    primitive.id = id;
    primitive.type = mfd::PrimitiveType::Rectangle;
    primitive.transform.position = {x, y};
    primitive.geometry = mfd::RectangleGeometry {0.12f, 0.08f};
    return primitive;
}

mfd::ReticleGroup MakeReticleWithPrimitives(std::initializer_list<mfd::Primitive> primitives)
{
    mfd::ReticleGroup reticle;
    reticle.id = "hud_template";
    reticle.primitives.assign(primitives.begin(), primitives.end());
    return reticle;
}
} // namespace

TEST(PrimitiveClipboardServiceTests, BuildPastePlanInsertsAfterSelectedPrimitiveWithVisibleOffset)
{
    const mfd::ReticleGroup reticle = MakeReticleWithPrimitives(
        {MakeRectanglePrimitive("frame", 0.10f, -0.20f), MakeRectanglePrimitive("label", -0.15f, 0.25f)});
    editor::PrimitiveClipboardService service;

    const editor::PrimitivePastePlan plan = service.BuildPastePlan(
        editor::PrimitivePasteRequest {reticle, reticle.primitives.front(), 0, mfd::Vec2 {0.02f, -0.03f}});

    EXPECT_EQ(plan.insertionIndex, 1);
    EXPECT_EQ(plan.primitive.id, "frame_copy");
    EXPECT_EQ(plan.primitive.type, mfd::PrimitiveType::Rectangle);
    EXPECT_FLOAT_EQ(plan.primitive.transform.position.x, 0.12f);
    EXPECT_FLOAT_EQ(plan.primitive.transform.position.y, -0.23f);
}

TEST(PrimitiveClipboardServiceTests, BuildPastePlanAvoidsNormalizedIdentifierCollisions)
{
    const mfd::ReticleGroup reticle = MakeReticleWithPrimitives(
        {MakeRectanglePrimitive("Frame", 0.0f, 0.0f), MakeRectanglePrimitive("frame_copy", 0.1f, 0.1f)});
    editor::PrimitiveClipboardService service;

    const editor::PrimitivePastePlan plan = service.BuildPastePlan(
        editor::PrimitivePasteRequest {reticle, reticle.primitives.front(), 0, {}});

    EXPECT_EQ(plan.primitive.id, "Frame_copy_1");
}

TEST(PrimitiveClipboardServiceTests, BuildPastePlanAppendsWhenNoPrimitiveIsFocused)
{
    const mfd::ReticleGroup reticle = MakeReticleWithPrimitives(
        {MakeRectanglePrimitive("frame", 0.0f, 0.0f), MakeRectanglePrimitive("label", 0.1f, 0.1f)});
    editor::PrimitiveClipboardService service;

    const editor::PrimitivePastePlan plan = service.BuildPastePlan(
        editor::PrimitivePasteRequest {reticle, reticle.primitives.back(), -1, {}});

    EXPECT_EQ(plan.insertionIndex, 2);
    EXPECT_EQ(plan.primitive.id, "label_copy");
}

TEST(PrimitiveClipboardServiceTests, BuildPastePlanGeneratesStableIdentifierWhenSourceIdIsEmpty)
{
    mfd::Primitive unnamed = MakeRectanglePrimitive("", -0.05f, 0.15f);
    const mfd::ReticleGroup reticle =
        MakeReticleWithPrimitives({MakeRectanglePrimitive("primitive_copy", 0.0f, 0.0f), unnamed});
    editor::PrimitiveClipboardService service;

    const editor::PrimitivePastePlan plan =
        service.BuildPastePlan(editor::PrimitivePasteRequest {reticle, unnamed, 1, {}});

    EXPECT_EQ(plan.primitive.id, "primitive_copy_1");
}

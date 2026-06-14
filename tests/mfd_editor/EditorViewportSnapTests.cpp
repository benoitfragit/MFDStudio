/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests covering shared reticle-studio snapping helpers.
 */

#include "internal/application/EditorViewportSnap.h"

#include <gtest/gtest.h>

TEST(EditorViewportSnapTests, ResolvePrimitiveMovePositionSnapsWorldOriginToSharedGrid)
{
    const mfd::Transform2D reticleTransform {{0.13f, -0.07f}, 0.0f, {1.0f, 1.0f}};

    mfd::Primitive primitive;
    primitive.transform.position = {0.02f, 0.04f};

    const mfd::Vec2 nextLocal = editor::app::ResolvePrimitiveMovePosition(
        reticleTransform,
        primitive,
        mfd::Vec2 {0.15f, 0.15f},
        mfd::Vec2 {0.181f, 0.181f},
        true,
        0.05f);

    const mfd::Vec2 snappedWorld = mfd::ApplyTransform(nextLocal, reticleTransform);
    EXPECT_FLOAT_EQ(snappedWorld.x, 0.20f);
    EXPECT_FLOAT_EQ(snappedWorld.y, 0.00f);
}

TEST(EditorViewportSnapTests, ResolvePrimitiveHandleLocalPointSnapsThroughWorldGrid)
{
    mfd::ReticleGroup reticle;
    reticle.transform.position = {0.10f, -0.05f};

    mfd::Primitive primitive;
    primitive.transform.position = {0.04f, 0.06f};

    const mfd::Vec2 nextLocalPoint = editor::app::ResolvePrimitiveHandleLocalPoint(
        reticle,
        primitive,
        mfd::Vec2 {0.173f, -0.024f},
        true,
        0.05f);

    const mfd::Vec2 snappedWorld = mfd::ApplyPrimitiveWorldTransform(nextLocalPoint, primitive, reticle);
    EXPECT_FLOAT_EQ(snappedWorld.x, 0.15f);
    EXPECT_FLOAT_EQ(snappedWorld.y, 0.00f);
}

TEST(EditorViewportSnapTests, ResolvePrimitiveMovePositionKeepsRawDragWhenSnapIsDisabled)
{
    const mfd::Transform2D reticleTransform {{0.0f, 0.0f}, 0.0f, {1.0f, 1.0f}};

    mfd::Primitive primitive;
    primitive.transform.position = {0.10f, -0.02f};

    const mfd::Vec2 nextLocal = editor::app::ResolvePrimitiveMovePosition(
        reticleTransform,
        primitive,
        mfd::Vec2 {0.00f, 0.00f},
        mfd::Vec2 {0.023f, -0.012f},
        false,
        0.05f);

    EXPECT_FLOAT_EQ(nextLocal.x, 0.123f);
    EXPECT_FLOAT_EQ(nextLocal.y, -0.032f);
}

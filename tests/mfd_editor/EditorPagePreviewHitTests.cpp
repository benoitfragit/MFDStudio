/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests covering typed page-preview hit targets used by the editor selection logic.
 */

#include "EditorPagePreviewHit.h"

#include <cmath>
#include <gtest/gtest.h>

TEST(EditorPagePreviewHitTests, DistinguishesTitleFromSecondStrobeWithoutIndexEncoding)
{
    const editor::PagePreviewHitTarget title = editor::PagePreviewHitTarget::PageTitle();
    const editor::PagePreviewHitTarget designator = editor::PagePreviewHitTarget::PageStrobe(1);

    EXPECT_EQ(title.kind, editor::PagePreviewHitKind::PageTitle);
    EXPECT_EQ(designator.kind, editor::PagePreviewHitKind::PageStrobe);
    EXPECT_EQ(designator.index, 1);
    EXPECT_NE(title, designator);
}

TEST(EditorPagePreviewHitTests, KeepsReticleAndStrobeTargetsSeparatedAtTheSameIndex)
{
    const editor::PagePreviewHitTarget reticle = editor::PagePreviewHitTarget::StaticReticle(0);
    const editor::PagePreviewHitTarget strobe = editor::PagePreviewHitTarget::PageStrobe(0);

    EXPECT_EQ(reticle.kind, editor::PagePreviewHitKind::StaticReticle);
    EXPECT_EQ(strobe.kind, editor::PagePreviewHitKind::PageStrobe);
    EXPECT_NE(reticle, strobe);
}

TEST(EditorPagePreviewHitTests, BezierPicksRenderedStrokeAndLeavesClosingChordEmpty)
{
    mfd::BezierGeometry curve;
    curve.controlPoints = {{-100, 0}, {0, 100}, {100, 0}};
    mfd::BezierPolylineCache cache;
    const auto project = [](mfd::Vec2 point) { return point; };
    const auto hitAt = [&](float y)
    {
        return editor::BezierStrokeHitDistance(cache, curve, project, [y](mfd::Vec2 a, mfd::Vec2 b)
        {
            const float dx = b.x - a.x;
            const float dy = b.y - a.y;
            const float t = std::clamp((-a.x * dx + (y - a.y) * dy) / (dx * dx + dy * dy), 0.0f, 1.0f);
            return std::hypot(a.x + t * dx, a.y + t * dy - y);
        });
    };
    EXPECT_GT(hitAt(0.0f), 49.0f);
    EXPECT_FLOAT_EQ(hitAt(50.0f), 0.0f);
}

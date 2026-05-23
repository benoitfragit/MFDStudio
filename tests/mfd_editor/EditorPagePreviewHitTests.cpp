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

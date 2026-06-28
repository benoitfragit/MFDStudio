/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests covering the pure sidebar section split used by the editor shell.
 */

#include "internal/application/EditorSidebarLayout.h"

#include <limits>

#include <gtest/gtest.h>

TEST(EditorSidebarLayoutTests, KeepsRequestedPagesHeightWhenRoomIsAmple)
{
    const editor::app::SidebarSectionLayoutResult layout =
        editor::app::ComputeSidebarSectionLayout({600.0f, 8.0f, 220.0f, 120.0f, 160.0f});

    EXPECT_FLOAT_EQ(layout.pagesHeight, 220.0f);
    EXPECT_FLOAT_EQ(layout.sectionSpacing, 8.0f);
    EXPECT_FLOAT_EQ(layout.reticlesHeight, 372.0f);
}

TEST(EditorSidebarLayoutTests, ClampsPagesHeightToProtectReticleMinimum)
{
    const editor::app::SidebarSectionLayoutResult layout =
        editor::app::ComputeSidebarSectionLayout({320.0f, 8.0f, 220.0f, 120.0f, 160.0f});

    EXPECT_FLOAT_EQ(layout.pagesHeight, 152.0f);
    EXPECT_FLOAT_EQ(layout.sectionSpacing, 8.0f);
    EXPECT_FLOAT_EQ(layout.reticlesHeight, 160.0f);
}

TEST(EditorSidebarLayoutTests, ShrinksGapBeforeSqueezingSectionMinimums)
{
    const editor::app::SidebarSectionLayoutResult layout =
        editor::app::ComputeSidebarSectionLayout({284.0f, 8.0f, 220.0f, 120.0f, 160.0f});

    EXPECT_FLOAT_EQ(layout.pagesHeight, 120.0f);
    EXPECT_FLOAT_EQ(layout.sectionSpacing, 4.0f);
    EXPECT_FLOAT_EQ(layout.reticlesHeight, 160.0f);
}

TEST(EditorSidebarLayoutTests, SharesTightHeightsProportionallyWhenBothMinimumsCannotFit)
{
    const editor::app::SidebarSectionLayoutResult layout =
        editor::app::ComputeSidebarSectionLayout({140.0f, 8.0f, 220.0f, 120.0f, 160.0f});

    EXPECT_FLOAT_EQ(layout.pagesHeight, 60.0f);
    EXPECT_FLOAT_EQ(layout.sectionSpacing, 0.0f);
    EXPECT_FLOAT_EQ(layout.reticlesHeight, 80.0f);
}

TEST(EditorSidebarLayoutTests, SanitizesInvalidInputs)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const editor::app::SidebarSectionLayoutResult layout =
        editor::app::ComputeSidebarSectionLayout({nan, -8.0f, nan, -12.0f, nan});

    EXPECT_FLOAT_EQ(layout.pagesHeight, 0.0f);
    EXPECT_FLOAT_EQ(layout.sectionSpacing, 0.0f);
    EXPECT_FLOAT_EQ(layout.reticlesHeight, 0.0f);
}

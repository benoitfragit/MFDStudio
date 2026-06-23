/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests covering the pure docked-workspace layout helper used by the editor preview.
 */

#include "EditorWorkspaceLayout.h"

#include <gtest/gtest.h>

TEST(EditorWorkspaceLayoutTests, UsesFullPreviewAreaWhenNoDockedPanelsAreEnabled)
{
    editor::WorkspaceLayoutRequest request;
    request.width = 960.0f;
    request.height = 540.0f;
    request.spacing = 8.0f;

    const editor::WorkspaceLayoutResult layout = editor::ComputeWorkspaceLayout(request);

    EXPECT_FALSE(layout.leadingPanel.IsVisible());
    EXPECT_FALSE(layout.bottomPanel.IsVisible());
    EXPECT_TRUE(layout.previewPanel.IsVisible());
    EXPECT_FLOAT_EQ(layout.previewPanel.x, 0.0f);
    EXPECT_FLOAT_EQ(layout.previewPanel.y, 0.0f);
    EXPECT_FLOAT_EQ(layout.previewPanel.width, 960.0f);
    EXPECT_FLOAT_EQ(layout.previewPanel.height, 540.0f);
}

TEST(EditorWorkspaceLayoutTests, BottomPanelConsumesTheFullPreviewWidth)
{
    editor::WorkspaceLayoutRequest request;
    request.width = 960.0f;
    request.height = 540.0f;
    request.spacing = 8.0f;
    request.showBottomPanel = true;
    request.bottomPanelHeight = 160.0f;
    request.minBottomPanelHeight = 96.0f;
    request.minCenterHeight = 140.0f;

    const editor::WorkspaceLayoutResult layout = editor::ComputeWorkspaceLayout(request);

    EXPECT_TRUE(layout.previewPanel.IsVisible());
    EXPECT_TRUE(layout.bottomPanel.IsVisible());
    EXPECT_FLOAT_EQ(layout.previewPanel.width, 960.0f);
    EXPECT_FLOAT_EQ(layout.bottomPanel.width, 960.0f);
    EXPECT_FLOAT_EQ(layout.previewPanel.height, 372.0f);
    EXPECT_FLOAT_EQ(layout.bottomPanel.y, 380.0f);
    EXPECT_FLOAT_EQ(layout.bottomPanel.height, 160.0f);
}

TEST(EditorWorkspaceLayoutTests, BottomPanelShrinksToPreserveMinimumPreviewHeight)
{
    editor::WorkspaceLayoutRequest request;
    request.width = 640.0f;
    request.height = 220.0f;
    request.spacing = 8.0f;
    request.showBottomPanel = true;
    request.bottomPanelHeight = 160.0f;
    request.minBottomPanelHeight = 48.0f;
    request.minCenterHeight = 140.0f;

    const editor::WorkspaceLayoutResult layout = editor::ComputeWorkspaceLayout(request);

    EXPECT_TRUE(layout.previewPanel.IsVisible());
    EXPECT_TRUE(layout.bottomPanel.IsVisible());
    EXPECT_FLOAT_EQ(layout.previewPanel.height, 140.0f);
    EXPECT_FLOAT_EQ(layout.bottomPanel.height, 72.0f);
    EXPECT_FLOAT_EQ(layout.bottomPanel.y, 148.0f);
}

TEST(EditorWorkspaceLayoutTests, LeadingAndBottomPanelsShareThePreviewColumn)
{
    editor::WorkspaceLayoutRequest request;
    request.width = 900.0f;
    request.height = 600.0f;
    request.spacing = 8.0f;
    request.showLeadingPanel = true;
    request.leadingPanelWidth = 220.0f;
    request.showBottomPanel = true;
    request.bottomPanelHeight = 140.0f;
    request.minLeadingPanelWidth = 160.0f;
    request.minBottomPanelHeight = 96.0f;
    request.minCenterWidth = 240.0f;
    request.minCenterHeight = 160.0f;

    const editor::WorkspaceLayoutResult layout = editor::ComputeWorkspaceLayout(request);

    EXPECT_TRUE(layout.leadingPanel.IsVisible());
    EXPECT_TRUE(layout.previewPanel.IsVisible());
    EXPECT_TRUE(layout.bottomPanel.IsVisible());
    EXPECT_FLOAT_EQ(layout.leadingPanel.width, 220.0f);
    EXPECT_FLOAT_EQ(layout.previewPanel.x, 228.0f);
    EXPECT_FLOAT_EQ(layout.previewPanel.width, 672.0f);
    EXPECT_FLOAT_EQ(layout.previewPanel.height, 452.0f);
    EXPECT_FLOAT_EQ(layout.bottomPanel.x, 228.0f);
    EXPECT_FLOAT_EQ(layout.bottomPanel.width, 672.0f);
    EXPECT_FLOAT_EQ(layout.bottomPanel.y, 460.0f);
    EXPECT_FLOAT_EQ(layout.bottomPanel.height, 140.0f);
}

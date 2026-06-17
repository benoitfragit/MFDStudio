/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests covering the pure responsive editor-shell layout helper.
 */

#include "EditorResponsiveLayout.h"

#include <cmath>
#include <limits>

#include <gtest/gtest.h>

namespace
{
/**
 * @brief Builds a layout request mirroring the editor shell defaults used in production.
 * @param totalWidth Available width to resolve.
 * @return A request with both panels wanted at their default preferred and minimum widths.
 */
editor::ShellLayoutRequest MakeDefaultRequest(const float totalWidth)
{
    editor::ShellLayoutRequest request;
    request.totalWidth = totalWidth;
    request.splitterWidth = 8.0f;
    request.minWorkspaceWidth = 360.0f;
    request.sidebar.wantVisible = true;
    request.sidebar.preferredWidth = 320.0f;
    request.sidebar.minWidth = 200.0f;
    request.inspector.wantVisible = true;
    request.inspector.preferredWidth = 360.0f;
    request.inspector.minWidth = 280.0f;
    return request;
}
} // namespace

TEST(EditorResponsiveLayoutTests, WideModeHonorsPreferredWidthsWhenSpaceIsAmple)
{
    const editor::ShellLayoutResult layout = editor::ComputeShellLayout(MakeDefaultRequest(1200.0f));

    EXPECT_EQ(layout.mode, editor::ShellLayoutMode::Wide);
    EXPECT_TRUE(layout.sidebar.visible);
    EXPECT_TRUE(layout.inspector.visible);
    EXPECT_FALSE(layout.sidebar.autoCollapsed);
    EXPECT_FALSE(layout.inspector.autoCollapsed);
    EXPECT_FLOAT_EQ(layout.sidebar.width, 320.0f);
    EXPECT_FLOAT_EQ(layout.inspector.width, 360.0f);
    EXPECT_FLOAT_EQ(layout.workspaceWidth, 504.0f);
}

TEST(EditorResponsiveLayoutTests, CompressesSidebarBeforeInspectorToProtectWorkspaceFloor)
{
    editor::ShellLayoutRequest request = MakeDefaultRequest(900.0f);
    request.sidebar.preferredWidth = 400.0f;
    request.inspector.preferredWidth = 400.0f;

    const editor::ShellLayoutResult layout = editor::ComputeShellLayout(request);

    EXPECT_EQ(layout.mode, editor::ShellLayoutMode::Wide);
    EXPECT_TRUE(layout.sidebar.visible);
    EXPECT_TRUE(layout.inspector.visible);
    // The lower-priority sidebar yields all its slack first, then the inspector gives
    // up the remainder, so the workspace lands exactly on its floor.
    EXPECT_FLOAT_EQ(layout.sidebar.width, 200.0f);
    EXPECT_FLOAT_EQ(layout.inspector.width, 324.0f);
    EXPECT_FLOAT_EQ(layout.workspaceWidth, 360.0f);
}

TEST(EditorResponsiveLayoutTests, CompactModeKeepsInspectorAndAutoCollapsesSidebar)
{
    const editor::ShellLayoutResult layout = editor::ComputeShellLayout(MakeDefaultRequest(700.0f));

    EXPECT_EQ(layout.mode, editor::ShellLayoutMode::Compact);
    EXPECT_FALSE(layout.sidebar.visible);
    EXPECT_TRUE(layout.sidebar.autoCollapsed);
    EXPECT_TRUE(layout.inspector.visible);
    EXPECT_FALSE(layout.inspector.autoCollapsed);
    EXPECT_FLOAT_EQ(layout.inspector.width, 332.0f);
    EXPECT_FLOAT_EQ(layout.workspaceWidth, 360.0f);
}

TEST(EditorResponsiveLayoutTests, FocusModeAutoCollapsesBothWhenInspectorCannotFit)
{
    // 600 px fits the sidebar footprint alone, but the higher-priority inspector does
    // not fit: the strict ladder collapses both instead of swapping in the sidebar.
    const editor::ShellLayoutResult layout = editor::ComputeShellLayout(MakeDefaultRequest(600.0f));

    EXPECT_EQ(layout.mode, editor::ShellLayoutMode::Focus);
    EXPECT_FALSE(layout.sidebar.visible);
    EXPECT_TRUE(layout.sidebar.autoCollapsed);
    EXPECT_FALSE(layout.inspector.visible);
    EXPECT_TRUE(layout.inspector.autoCollapsed);
    EXPECT_FLOAT_EQ(layout.workspaceWidth, 600.0f);
}

TEST(EditorResponsiveLayoutTests, UserHiddenSidebarIsNotReportedAsAutoCollapsed)
{
    editor::ShellLayoutRequest request = MakeDefaultRequest(1200.0f);
    request.sidebar.wantVisible = false;

    const editor::ShellLayoutResult layout = editor::ComputeShellLayout(request);

    EXPECT_EQ(layout.mode, editor::ShellLayoutMode::Compact);
    EXPECT_FALSE(layout.sidebar.visible);
    EXPECT_FALSE(layout.sidebar.autoCollapsed);
    EXPECT_TRUE(layout.inspector.visible);
    EXPECT_FLOAT_EQ(layout.inspector.width, 360.0f);
}

TEST(EditorResponsiveLayoutTests, WideningRestoresAutoCollapsedPanelWithItsPreferredWidth)
{
    // A narrow window auto-collapses the sidebar...
    const editor::ShellLayoutResult narrow = editor::ComputeShellLayout(MakeDefaultRequest(700.0f));
    ASSERT_TRUE(narrow.sidebar.autoCollapsed);

    // ...and widening back restores it at its untouched preferred width. This mirrors
    // the shell never overwriting the stored preference during auto-collapse.
    const editor::ShellLayoutResult wide = editor::ComputeShellLayout(MakeDefaultRequest(1200.0f));
    EXPECT_TRUE(wide.sidebar.visible);
    EXPECT_FALSE(wide.sidebar.autoCollapsed);
    EXPECT_FLOAT_EQ(wide.sidebar.width, 320.0f);
}

TEST(EditorResponsiveLayoutTests, SanitizesNonFiniteAndNegativeInputs)
{
    editor::ShellLayoutRequest request = MakeDefaultRequest(std::numeric_limits<float>::quiet_NaN());
    request.splitterWidth = -8.0f;
    request.minWorkspaceWidth = std::numeric_limits<float>::infinity();
    request.sidebar.preferredWidth = -100.0f;
    request.inspector.preferredWidth = std::numeric_limits<float>::quiet_NaN();

    const editor::ShellLayoutResult layout = editor::ComputeShellLayout(request);

    EXPECT_TRUE(std::isfinite(layout.workspaceWidth));
    EXPECT_GE(layout.workspaceWidth, 0.0f);
    EXPECT_TRUE(std::isfinite(layout.sidebar.width));
    EXPECT_GE(layout.sidebar.width, 0.0f);
    EXPECT_TRUE(std::isfinite(layout.inspector.width));
    EXPECT_GE(layout.inspector.width, 0.0f);
}

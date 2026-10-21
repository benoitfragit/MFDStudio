/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests validating shared editor ImGui styling helpers.
 */

#include "EditorUiTheme.h"

#include <limits>

#include <gtest/gtest.h>

#include <imgui.h>

namespace
{
class ScopedImGuiContext
{
public:
    ScopedImGuiContext()
    {
        IMGUI_CHECKVERSION();
        context_ = ImGui::CreateContext();
    }

    ~ScopedImGuiContext()
    {
        ImGui::DestroyContext(context_);
    }

private:
    ImGuiContext* context_ = nullptr;
};
} // namespace

TEST(EditorUiThemeTests, PaneSplitterWidthMatchesSharedLayoutConstant)
{
    EXPECT_FLOAT_EQ(editor::ui::kPaneSplitterWidth, 8.0f);
}

TEST(EditorUiThemeTests, EditorFontPixelSizeSanitizesDisplayScale)
{
    EXPECT_FLOAT_EQ(editor::ui::EditorFontPixelSize(1.0f), 17.0f);
    EXPECT_FLOAT_EQ(editor::ui::EditorFontPixelSize(1.25f), 22.0f);
    EXPECT_FLOAT_EQ(editor::ui::EditorFontPixelSize(0.0f), 13.0f);
    EXPECT_FLOAT_EQ(editor::ui::EditorFontPixelSize(8.0f), 51.0f);
    EXPECT_FLOAT_EQ(editor::ui::EditorFontPixelSize(std::numeric_limits<float>::infinity()), 17.0f);
}

TEST(EditorUiThemeTests, InspectorFieldLabelWidthKeepsFormsReadableAcrossPaneSizes)
{
    EXPECT_FLOAT_EQ(editor::ui::InspectorFieldLabelWidth(200.0f), 104.0f);
    EXPECT_FLOAT_EQ(editor::ui::InspectorFieldLabelWidth(320.0f), 128.0f);
    EXPECT_FLOAT_EQ(editor::ui::InspectorFieldLabelWidth(600.0f), 160.0f);
    EXPECT_FLOAT_EQ(editor::ui::InspectorFieldLabelWidth(std::numeric_limits<float>::infinity()), 104.0f);
}

TEST(EditorUiThemeTests, ApplyEditorThemeConfiguresRoundedEditorPalette)
{
    ScopedImGuiContext context;

    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle {};

    editor::ui::ApplyEditorTheme();

    EXPECT_FLOAT_EQ(style.WindowRounding, 10.0f);
    EXPECT_FLOAT_EQ(style.ChildRounding, 10.0f);
    EXPECT_FLOAT_EQ(style.FrameRounding, 8.0f);
    EXPECT_FLOAT_EQ(style.PopupRounding, 8.0f);
    EXPECT_FLOAT_EQ(style.ScrollbarRounding, 10.0f);
    EXPECT_FLOAT_EQ(style.GrabRounding, 8.0f);
    EXPECT_FLOAT_EQ(style.WindowPadding.x, 14.0f);
    EXPECT_FLOAT_EQ(style.WindowPadding.y, 14.0f);
    EXPECT_FLOAT_EQ(style.FramePadding.x, 10.0f);
    EXPECT_FLOAT_EQ(style.FramePadding.y, 8.0f);
    EXPECT_FLOAT_EQ(style.ItemSpacing.x, 10.0f);
    EXPECT_FLOAT_EQ(style.ItemSpacing.y, 8.0f);
    EXPECT_FLOAT_EQ(style.IndentSpacing, 18.0f);
    EXPECT_FLOAT_EQ(style.ScrollbarSize, 15.0f);
    EXPECT_FLOAT_EQ(style.GrabMinSize, 12.0f);
    EXPECT_FLOAT_EQ(style.DisabledAlpha, 0.70f);

    EXPECT_FLOAT_EQ(style.Colors[ImGuiCol_WindowBg].x, 0.051f);
    EXPECT_FLOAT_EQ(style.Colors[ImGuiCol_WindowBg].y, 0.067f);
    EXPECT_FLOAT_EQ(style.Colors[ImGuiCol_WindowBg].z, 0.106f);
    EXPECT_FLOAT_EQ(style.Colors[ImGuiCol_Text].x, 0.949f);
    EXPECT_FLOAT_EQ(style.Colors[ImGuiCol_Text].y, 0.965f);
    EXPECT_FLOAT_EQ(style.Colors[ImGuiCol_Text].z, 0.988f);
    EXPECT_FLOAT_EQ(style.Colors[ImGuiCol_CheckMark].x, 0.333f);
    EXPECT_FLOAT_EQ(style.Colors[ImGuiCol_CheckMark].y, 0.843f);
    EXPECT_FLOAT_EQ(style.Colors[ImGuiCol_CheckMark].z, 0.745f);
    EXPECT_FLOAT_EQ(style.Colors[ImGuiCol_NavHighlight].x, 0.341f);
    EXPECT_FLOAT_EQ(style.Colors[ImGuiCol_NavHighlight].y, 0.682f);
    EXPECT_FLOAT_EQ(style.Colors[ImGuiCol_NavHighlight].z, 1.000f);
    EXPECT_FLOAT_EQ(style.Colors[ImGuiCol_Header].w, 0.13f);
}

TEST(EditorUiThemeTests, FormatViewportToolbarInfoLabelIncludesZoomOnlyWhenMouseIsUnavailable)
{
    EXPECT_EQ(editor::ui::FormatViewportToolbarInfoLabel(1.25f, std::nullopt), "Zoom: 125%");
}

TEST(EditorUiThemeTests, FormatViewportToolbarInfoLabelIncludesZoomAndLogicalMouseCoordinates)
{
    const std::string label = editor::ui::FormatViewportToolbarInfoLabel(0.875f, mfd::Vec2 {0.125f, -0.5f});

    EXPECT_EQ(label, "Zoom: 88%  X +0.125  Y -0.500");
}

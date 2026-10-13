/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "internal/application/EditorColorPicker.h"
#include "EditorUndoHistory.h"

#include <array>
#include <gtest/gtest.h>
#include <imgui.h>
#include <imgui_internal.h>

namespace
{
class ColorPickerFrame : public testing::Test
{
protected:
    void SetUp() override
    {
        context_ = ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(800, 700);
        io.DeltaTime = 1.0f / 60.0f;
        unsigned char* pixels = nullptr;
        int width = 0;
        int height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        Frame();
    }

    void TearDown() override
    {
        ImGui::DestroyContext(context_);
    }

    editor::ui::ColorPickerInteraction Frame(const ImVec2 mouse = ImVec2(-100, -100),
                                             const bool down = false, const bool disabled = false)
    {
        auto& io = ImGui::GetIO();
        io.AddMousePosEvent(mouse.x, mouse.y);
        io.AddMouseButtonEvent(0, down);
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(700, 650));
        ImGui::Begin("Picker test", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
        origin_ = ImGui::GetCursorScreenPos();
        ImGui::BeginDisabled(disabled);
        const auto result = editor::ui::DrawColorPicker("Stroke", color_.data());
        ImGui::EndDisabled();
        ImGui::End();
        ImGui::Render();
        return result;
    }

    editor::ui::ColorPickerInteraction ClickSwatch(const int index, const bool disabled = false)
    {
        const auto& style = ImGui::GetStyle();
        const float size = ImGui::GetFrameHeight();
        const ImVec2 center(origin_.x + (index % 3) * (size + style.ItemSpacing.x) + size / 2,
                            origin_.y + ImGui::GetTextLineHeightWithSpacing() +
                                (index / 3) * (size + style.ItemSpacing.y) + size / 2);
        Frame(center, false, disabled);
        Frame(center, true, disabled);
        return Frame(center, false, disabled);
    }

    editor::ui::ColorPickerInteraction ClickPopupButton(const bool apply)
    {
        // Inspect the actual rendered Cancel button row, avoiding hard-coded popup placement.
        ImGuiWindow* popup = ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
        const auto& style = ImGui::GetStyle();
        const float cancelWidth = ImGui::CalcTextSize("Cancel").x + 2 * style.FramePadding.x;
        const float applyWidth = ImGui::CalcTextSize("Apply").x + 2 * style.FramePadding.x;
        const ImVec2 row = popup->DC.CursorPosPrevLine;
        const ImVec2 center(row.x - (apply ? cancelWidth + style.ItemSpacing.x + applyWidth / 2 : cancelWidth / 2),
                            row.y + ImGui::GetFrameHeight() / 2);
        Frame(center);
        Frame(center, true);
        return Frame(center);
    }

    std::array<float, 4> color_ {{80 / 255.0f, 160 / 255.0f, 1.0f, 180 / 255.0f}};

private:
    ImGuiContext* context_ = nullptr;
    ImVec2 origin_ {};
};
} // namespace

TEST_F(ColorPickerFrame, PresetsCommitExactValuesAndSupportUndoRedo)
{
    editor::UndoHistory<std::array<float, 4>> history;
    const auto original = color_;
    EXPECT_FALSE(Frame().changed);
    const auto interaction = ClickSwatch(4); // Blue
    ASSERT_TRUE(interaction.changed);
    EXPECT_EQ(interaction.selected, mfd::ColorPreset::Blue);
    EXPECT_FLOAT_EQ(color_[0], 0);
    EXPECT_FLOAT_EQ(color_[1], 120 / 255.0f);
    EXPECT_FLOAT_EQ(color_[2], 1);
    EXPECT_FLOAT_EQ(color_[3], 1);
    history.Record(original);
    const auto blue = color_;
    color_ = *history.Undo(color_);
    EXPECT_EQ(color_, original);
    color_ = *history.Redo(color_);
    EXPECT_EQ(color_, blue);
    EXPECT_FALSE(ClickSwatch(4).changed);
}

TEST_F(ColorPickerFrame, DisabledSwatchesAndCustomDoNotMutateOrOpen)
{
    const auto original = color_;
    EXPECT_FALSE(ClickSwatch(0, true).changed);
    EXPECT_FALSE(ClickSwatch(8, true).customOpened);
    EXPECT_EQ(color_, original);
}

TEST_F(ColorPickerFrame, CustomOpensWithoutMutationAndRetainsAlpha)
{
    const auto original = color_;
    const auto interaction = ClickSwatch(8);
    EXPECT_TRUE(interaction.customOpened);
    EXPECT_TRUE(interaction.customVisible);
    EXPECT_FALSE(interaction.changed);
    EXPECT_EQ(color_, original);
    EXPECT_FALSE(Frame().changed);
    EXPECT_FLOAT_EQ(color_[3], 180 / 255.0f);
    const auto cancelled = ClickPopupButton(false);
    EXPECT_FALSE(cancelled.changed);
    EXPECT_FALSE(cancelled.customVisible);
    EXPECT_EQ(color_, original);
}

TEST_F(ColorPickerFrame, CustomPickerCommitsRgbOnlyOnApplyAndPreservesAlpha)
{
    const auto original = color_;
    ASSERT_TRUE(ClickSwatch(8).customOpened);
    Frame();
    ImGuiWindow* popup = ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
    const ImVec2 pickerPoint(popup->Pos.x + 45, popup->Pos.y + 65);
    Frame(pickerPoint);
    Frame(pickerPoint, true);
    Frame(pickerPoint);
    EXPECT_EQ(color_, original); // The picker edits a popup draft until Apply.
    const auto applied = ClickPopupButton(true);
    EXPECT_TRUE(applied.changed);
    EXPECT_FALSE(applied.customVisible);
    EXPECT_NE(color_, original);
    EXPECT_FLOAT_EQ(color_[3], original[3]);
}

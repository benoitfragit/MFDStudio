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
        const auto before = color_;
        const auto result = editor::ui::DrawColorPicker(hiddenLabel_ ? "##value" : "Stroke", color_.data());
        colorPickerWidth_ = ImGui::GetItemRectSize().x;
        if (result.changed && result.beganEdit)
        {
            history_.Record(before);
        }
        ImGui::EndDisabled();
        otherInteraction_ = editor::ui::DrawColorPicker("Fill", otherColor_.data());
        ImGui::End();
        ImGui::Render();
        return result;
    }

    editor::ui::ColorPickerInteraction Click(const ImVec2 center, const bool disabled = false)
    {
        Frame(center, false, disabled);
        Frame(center, true, disabled);
        return Frame(center, false, disabled);
    }

    editor::ui::ColorPickerInteraction OpenSelector(const bool disabled = false)
    {
        const float halfSize = ImGui::GetFrameHeight() / 2;
        return Click(ImVec2(origin_.x + halfSize, origin_.y + halfSize), disabled);
    }

    ImGuiWindow* Popup() const
    {
        return ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
    }

    editor::ui::ColorPickerInteraction ClickSwatch(const int index, const bool disabled = false)
    {
        const auto& style = ImGui::GetStyle();
        const float size = ImGui::GetFrameHeight();
        // The final popup group is the two-column palette, to the right of the picker.
        const float left = Popup()->DC.CursorMaxPos.x - (2 * size + style.ItemSpacing.x);
        const float top = Popup()->DC.CursorStartPos.y + ImGui::GetTextLineHeightWithSpacing();
        const ImVec2 center(left + (index % 2) * (size + style.ItemSpacing.x) + size / 2,
                            top + (index / 2) * (size + style.ItemSpacing.y) + size / 2);
        return Click(center, disabled);
    }

    editor::ui::ColorPickerInteraction CloseSelector()
    {
        return Click(ImVec2(650, 600));
    }

    std::array<float, 4> color_ {{80 / 255.0f, 160 / 255.0f, 1.0f, 180 / 255.0f}};
    std::array<float, 4> otherColor_ {{0.2f, 0.4f, 0.6f, 0.8f}};
    editor::ui::ColorPickerInteraction otherInteraction_;
    editor::UndoHistory<std::array<float, 4>> history_;
    bool hiddenLabel_ = false;
    float colorPickerWidth_ = 0.0f;

private:
    ImGuiContext* context_ = nullptr;
    ImVec2 origin_ {};
};
} // namespace

TEST_F(ColorPickerFrame, HiddenIdentifierDoesNotLeakIntoAlignedForms)
{
    Frame();
    const float labeledWidth = colorPickerWidth_;
    hiddenLabel_ = true;
    Frame();

    EXPECT_LT(colorPickerWidth_, labeledWidth);
    EXPECT_FLOAT_EQ(colorPickerWidth_, ImGui::GetFrameHeight());
}

TEST_F(ColorPickerFrame, CurrentColorButtonOpensAndClosesWithoutMutation)
{
    const auto original = color_;
    EXPECT_FALSE(Frame().popupVisible);
    const auto opened = OpenSelector();
    EXPECT_TRUE(opened.popupOpened);
    EXPECT_TRUE(opened.popupVisible);
    EXPECT_FALSE(opened.changed);
    EXPECT_FALSE(opened.beganEdit);
    EXPECT_EQ(color_, original);
    EXPECT_FALSE(Frame().changed);
    const auto closed = CloseSelector();
    EXPECT_FALSE(closed.popupVisible);
    EXPECT_FALSE(closed.changed);
    EXPECT_EQ(color_, original);
    EXPECT_FALSE(history_.CanUndo());
}

TEST_F(ColorPickerFrame, EightPopupPresetsCommitExactValuesAndSupportUndoRedo)
{
    ASSERT_TRUE(OpenSelector().popupOpened);
    Frame();
    const auto otherOriginal = otherColor_;
    for (int index = 0; index < 8; ++index)
    {
        SCOPED_TRACE(index);
        const auto original = color_;
        const auto interaction = ClickSwatch(index);
        const auto preset = static_cast<mfd::ColorPreset>(index + 1);
        const auto expected = *mfd::TryColorPreset(preset);
        EXPECT_TRUE(interaction.changed);
        EXPECT_TRUE(interaction.beganEdit);
        EXPECT_TRUE(interaction.popupVisible);
        EXPECT_EQ(interaction.selected, preset);
        EXPECT_FLOAT_EQ(color_[0], expected.r / 255.0f);
        EXPECT_FLOAT_EQ(color_[1], expected.g / 255.0f);
        EXPECT_FLOAT_EQ(color_[2], expected.b / 255.0f);
        EXPECT_FLOAT_EQ(color_[3], 1);
        EXPECT_EQ(history_.UndoDepth(), 1U);
        const auto selected = color_;
        color_ = *history_.Undo(color_);
        EXPECT_EQ(color_, original);
        color_ = *history_.Redo(color_);
        EXPECT_EQ(color_, selected);
        EXPECT_FALSE(ClickSwatch(index).changed);
        EXPECT_EQ(history_.UndoDepth(), 1U);
        history_.Clear();
    }
    // The old ninth Custom shortcut is gone: clicking below the eight presets is inert.
    EXPECT_FALSE(ClickSwatch(8).changed);
    EXPECT_FALSE(history_.CanUndo());
    EXPECT_EQ(otherColor_, otherOriginal);
    EXPECT_FALSE(otherInteraction_.popupVisible);
    const auto selected = color_;
    EXPECT_FALSE(CloseSelector().popupVisible);
    EXPECT_EQ(color_, selected);
    EXPECT_TRUE(OpenSelector().popupVisible);
    EXPECT_EQ(color_, selected);
}

TEST_F(ColorPickerFrame, DisabledCurrentColorButtonDoesNotMutateOrOpen)
{
    const auto original = color_;
    const auto result = OpenSelector(true);
    EXPECT_FALSE(result.changed);
    EXPECT_FALSE(result.popupOpened);
    EXPECT_FALSE(result.popupVisible);
    EXPECT_EQ(color_, original);
    EXPECT_FALSE(history_.CanUndo());
}

TEST_F(ColorPickerFrame, RgbDragAppliesImmediatelyAndCreatesOneUndoSnapshot)
{
    const auto original = color_;
    ASSERT_TRUE(OpenSelector().popupOpened);
    Frame();
    const ImVec2 start(Popup()->DC.CursorStartPos.x + 40, Popup()->DC.CursorStartPos.y + 40);
    Frame(start);
    const auto first = Frame(start, true);
    EXPECT_TRUE(first.changed);
    EXPECT_TRUE(first.beganEdit);
    EXPECT_NE(color_, original);
    const ImVec2 next(start.x + 30, start.y + 25);
    const auto continued = Frame(next, true);
    EXPECT_TRUE(continued.changed);
    EXPECT_FALSE(continued.beganEdit);
    Frame(next);
    EXPECT_FLOAT_EQ(color_[3], original[3]);
    EXPECT_EQ(history_.UndoDepth(), 1U);
    const auto edited = color_;
    EXPECT_FALSE(CloseSelector().popupVisible);
    EXPECT_EQ(color_, edited);
    color_ = *history_.Undo(color_);
    EXPECT_EQ(color_, original);
    color_ = *history_.Redo(color_);
    EXPECT_EQ(color_, edited);
}

TEST_F(ColorPickerFrame, AlphaBarEditsAlphaImmediatelyAndPresetRestoresOpaqueColor)
{
    ASSERT_TRUE(OpenSelector().popupOpened);
    Frame();
    const auto original = color_;
    const float size = ImGui::GetFrameHeight();
    const ImVec2 point(Popup()->DC.CursorStartPos.x + size * 12 - size / 2,
                       Popup()->DC.CursorStartPos.y + 80);
    Frame(point);
    EXPECT_TRUE(Frame(point, true).changed);
    Frame(point);
    EXPECT_FLOAT_EQ(color_[0], original[0]);
    EXPECT_FLOAT_EQ(color_[1], original[1]);
    EXPECT_FLOAT_EQ(color_[2], original[2]);
    EXPECT_NE(color_[3], original[3]);
    EXPECT_TRUE(ClickSwatch(4).changed);
    EXPECT_FLOAT_EQ(color_[3], 1);
    EXPECT_EQ(history_.UndoDepth(), 2U);
}

TEST_F(ColorPickerFrame, DisablingAnOpenSelectorPreventsPresetAndRgbaEdits)
{
    ASSERT_TRUE(OpenSelector().popupOpened);
    Frame();
    const auto original = color_;
    const ImVec2 point(Popup()->DC.CursorStartPos.x + 40, Popup()->DC.CursorStartPos.y + 40);
    EXPECT_FALSE(ClickSwatch(4, true).changed);
    Frame(point, false, true);
    EXPECT_FALSE(Frame(point, true, true).changed);
    Frame(point, false, true);
    EXPECT_EQ(color_, original);
    EXPECT_FALSE(history_.CanUndo());
}

TEST_F(ColorPickerFrame, EscapeClosesTheSelectorAndRetainsLiveEdits)
{
    ASSERT_TRUE(OpenSelector().popupOpened);
    Frame();
    EXPECT_TRUE(ClickSwatch(5).changed);
    const auto selected = color_;
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
    EXPECT_FALSE(Frame().popupVisible);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
    EXPECT_FALSE(Frame().changed);
    EXPECT_EQ(color_, selected);
    EXPECT_EQ(history_.UndoDepth(), 1U);
}

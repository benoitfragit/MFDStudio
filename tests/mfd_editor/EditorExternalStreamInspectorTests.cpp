/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "internal/application/EditorExternalStreamInspector.h"
#include "EditorUndoHistory.h"

#include <gtest/gtest.h>
#include <imgui.h>
#include <imgui_internal.h>

namespace
{
class ExternalStreamInspectorFrame : public testing::Test
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
        geometry_.streamId = "camera";
        geometry_.width = 0.5f;
        geometry_.height = 0.5f;
        Frame();
    }

    void TearDown() override
    {
        ImGui::DestroyContext(context_);
    }

    bool Frame(const ImVec2 mouse = ImVec2(-100, -100), const bool down = false)
    {
        auto& io = ImGui::GetIO();
        io.AddMousePosEvent(mouse.x, mouse.y);
        io.AddMouseButtonEvent(0, down);
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(700, 650));
        ImGui::Begin("Stream test", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        sizePoint_ = ImVec2(origin.x + 40, origin.y + ImGui::GetFrameHeightWithSpacing() +
                                               ImGui::GetFrameHeight() / 2);
        const auto before = geometry_;
        bool beganEdit = false;
        const auto edit = editor::app::DrawExternalStreamInspector(geometry_, registry_, beganEdit);
        if (edit)
        {
            if (beganEdit) history_.Record(before);
            geometry_ = *edit;
        }
        ImGui::End();
        ImGui::Render();
        return edit.has_value();
    }

    void DragSize(const float delta)
    {
        const ImVec2 start = sizePoint_;
        Frame(start);
        EXPECT_FALSE(Frame(start, true)); // activation precedes the first change
        EXPECT_TRUE(ImGui::GetCurrentContext()->ActiveId != 0);
        EXPECT_TRUE(Frame(ImVec2(start.x + delta, start.y), true));
        EXPECT_TRUE(Frame(ImVec2(start.x + delta * 2, start.y), true));
        Frame(ImVec2(start.x + delta * 2, start.y));
    }

    ImGuiContext* context_ = nullptr;
    ImVec2 sizePoint_ {};
    mfd::ExternalStreamGeometry geometry_ {};
    mfd::ExternalStreamRegistry registry_;
    editor::UndoHistory<mfd::ExternalStreamGeometry> history_;
};
} // namespace

TEST_F(ExternalStreamInspectorFrame, ContinuousSizeDragRecordsOneUndoAndRedoRestoresFinalValue)
{
    const auto original = geometry_;
    DragSize(20);
    EXPECT_GT(geometry_.width, original.width);
    EXPECT_FLOAT_EQ(geometry_.height, original.height);
    ASSERT_EQ(history_.UndoDepth(), 1U);
    const auto final = geometry_;
    const auto previous = history_.Undo(geometry_);
    ASSERT_TRUE(previous.has_value());
    geometry_ = *previous;
    EXPECT_FLOAT_EQ(geometry_.width, original.width);
    EXPECT_FLOAT_EQ(geometry_.height, original.height);
    const auto redone = history_.Redo(geometry_);
    ASSERT_TRUE(redone.has_value());
    EXPECT_FLOAT_EQ(redone->width, final.width);
    EXPECT_FLOAT_EQ(redone->height, final.height);
}

TEST_F(ExternalStreamInspectorFrame, TwoSizeGesturesProduceTwoUndoOperations)
{
    const auto original = geometry_;
    DragSize(20);
    const auto intermediate = geometry_;
    // Let the next click be a separate gesture rather than ImGui's double-click
    // shortcut, which switches DragFloat to text input.
    for (int frame = 0; frame < 25; ++frame) Frame();
    DragSize(15);
    EXPECT_GT(geometry_.width, intermediate.width);
    ASSERT_EQ(history_.UndoDepth(), 2U);
    geometry_ = *history_.Undo(geometry_);
    EXPECT_FLOAT_EQ(geometry_.width, intermediate.width);
    geometry_ = *history_.Undo(geometry_);
    EXPECT_FLOAT_EQ(geometry_.width, original.width);
}

TEST_F(ExternalStreamInspectorFrame, ClickingSizeWithoutEditingDoesNotRecordUndo)
{
    const auto original = geometry_;
    const ImVec2 start = sizePoint_;
    EXPECT_FALSE(Frame(start));
    EXPECT_FALSE(Frame(start, true));
    EXPECT_FALSE(Frame(start));
    EXPECT_EQ(history_.UndoDepth(), 0U);
    EXPECT_FLOAT_EQ(geometry_.width, original.width);
}

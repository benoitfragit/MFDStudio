/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "internal/application/EditorPrimitiveInspectorScope.h"

#include <array>
#include <cstdio>
#include <gtest/gtest.h>

namespace
{
class PrimitiveInspectorFrame : public testing::Test
{
protected:
    void SetUp() override
    {
        context_ = ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {640, 480};
        io.DeltaTime = 1.0f / 60.0f;
        unsigned char* pixels = nullptr;
        int width = 0;
        int height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        Frame();
    }

    void TearDown() override { ImGui::DestroyContext(context_); }

    void Frame()
    {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({500, 300});
        ImGui::Begin("Reticle Studio test");
        ImGui::Selectable("Second primitive");
        if (ImGui::IsItemClicked()) selected_ = 1;
        const auto row = ImGui::GetItemRectMin();
        secondRow_ = {row.x + 6, row.y + 6};
        {
            const editor::app::PrimitiveInspectorScope scope(window_, template_, selected_);
            fieldId_ = ImGui::GetID("Text");
            auto& text = texts_.at(static_cast<std::size_t>(selected_));
            std::array<char, 128> buffer {};
            std::snprintf(buffer.data(), buffer.size(), "%s", text.c_str());
            if (ImGui::InputText("Text", buffer.data(), buffer.size())) text = buffer.data();
            const auto field = ImGui::GetItemRectMin();
            textField_ = {field.x + 6, field.y + 6};
        }
        ImGui::End();
        ImGui::Render();
    }

    void Click(ImVec2 point)
    {
        auto& io = ImGui::GetIO();
        io.AddMousePosEvent(point.x, point.y);
        io.AddMouseButtonEvent(0, true);
        Frame();
        io.AddMouseButtonEvent(0, false);
        Frame();
    }

    ImGuiContext* context_ = nullptr;
    std::filesystem::path window_ = "first.json";
    std::string template_ = "HUD###shared";
    int selected_ = 0;
    std::array<std::string, 2> texts_ {"", "second"};
    ImVec2 textField_ {};
    ImVec2 secondRow_ {};
    ImGuiID fieldId_ {};
};

TEST_F(PrimitiveInspectorFrame, SwitchingPrimitiveWhileTypingKeepsEachTextOnItsOwner)
{
    Click(textField_);
    ImGui::GetIO().AddInputCharactersUTF8("tata");
    Frame();
    ASSERT_EQ(texts_[0], "tata");
    Click(secondRow_);
    ASSERT_EQ(selected_, 1);
    EXPECT_EQ(texts_[0], "tata");
    EXPECT_EQ(texts_[1], "second");
}

TEST_F(PrimitiveInspectorFrame, TheSamePrimitiveIndexInOtherTemplatesAndDocumentsHasItsOwnId)
{
    const ImGuiID first = fieldId_;
    template_ = "Radar###shared";
    Frame();
    EXPECT_NE(first, fieldId_);
    template_ = "HUD###shared";
    window_ = "other.json";
    Frame();
    EXPECT_NE(first, fieldId_);
    window_ = "first.json";
    Frame();
    EXPECT_EQ(first, fieldId_);
}
}

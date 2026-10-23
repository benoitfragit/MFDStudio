/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Regression tests for the API LEVEL 1 widget inspector host.
 */

#include "mfd/apilevel1/widgets/WidgetEditorHost.h"

#include <filesystem>

#include <gtest/gtest.h>
#include <imgui.h>

namespace
{
class WidgetEditorHostFrame : public testing::Test
{
protected:
    void SetUp() override
    {
        context_ = ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(800.0f, 700.0f);
        io.DeltaTime = 1.0f / 60.0f;

        unsigned char* pixels = nullptr;
        int width = 0;
        int height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    }

    void TearDown() override
    {
        ImGui::DestroyContext(context_);
    }

    static MfdWidgetEditorDraftV1 MakeDraft()
    {
        MfdWidgetEditorDraftV1 draft {};
        draft.struct_size = sizeof(draft);
        draft.visible = 1U;
        draft.enabled = 1U;
        draft.minimum = 0.0;
        draft.maximum = 100.0;
        draft.width = 0.48;
        draft.height = 0.18;
        draft.preferred_width = 0.48;
        draft.preferred_height = 0.18;
        draft.hit_width = 0.50;
        draft.hit_height = 0.20;
        draft.row_span = 1.0;
        draft.column_span = 1.0;
        return draft;
    }

    ImGuiContext* context_ = nullptr;
};
} // namespace

TEST_F(WidgetEditorHostFrame, NumericFieldsRenderWithoutTriggeringAnImGuiAssertion)
{
#if defined(_WIN32)
    mfd::api1::WidgetEditorHost host;
    ASSERT_TRUE(host.EnsureLoaded(std::filesystem::current_path() / "widgets_window.json")) << host.Diagnostic();

    MfdWidgetEditorDraftV1 draft = MakeDraft();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(700.0f, 650.0f));
    ImGui::Begin("Widget inspector test", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
    EXPECT_EQ(host.Draw(draft), 0U);
    ImGui::End();
    ImGui::Render();
#else
    GTEST_SKIP() << "The widget editor host currently loads extension modules through Win32.";
#endif
}

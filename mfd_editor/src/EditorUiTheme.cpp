/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorUiTheme.h"

/**
 * @file
 * @brief ImGui theme and helper widgets shared by editor panels.
 */

#include <imgui.h>

namespace editor::ui
{
namespace
{
constexpr float kPaneSplitterWidth = 8.0f;
}

void ApplyEditorTheme()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 10.0f;
    style.ChildRounding = 10.0f;
    style.FrameRounding = 8.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 10.0f;
    style.GrabRounding = 8.0f;
    style.WindowPadding = ImVec2(14.0f, 14.0f);
    style.FramePadding = ImVec2(10.0f, 8.0f);
    style.ItemSpacing = ImVec2(10.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.IndentSpacing = 18.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.92f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.53f, 0.62f, 0.69f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.08f, 0.11f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.07f, 0.10f, 0.14f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.11f, 0.15f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.16f, 0.24f, 0.29f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.11f, 0.16f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.16f, 0.25f, 0.31f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.19f, 0.31f, 0.38f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.12f, 0.22f, 0.28f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.18f, 0.33f, 0.40f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.22f, 0.40f, 0.48f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.11f, 0.22f, 0.27f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.16f, 0.31f, 0.37f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.39f, 0.46f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.33f, 0.86f, 0.78f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.72f, 0.83f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.33f, 0.86f, 0.78f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.18f, 0.28f, 0.34f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.10f, 0.18f, 0.23f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.17f, 0.31f, 0.38f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.15f, 0.27f, 0.34f, 1.00f);
}

bool AccentButton(const char* label)
{
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.54f, 0.61f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.66f, 0.73f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.44f, 0.52f, 1.00f));
    const bool pressed = ImGui::Button(label);
    ImGui::PopStyleColor(3);
    return pressed;
}

bool DrawVerticalSplitter(const char* id, const float height)
{
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.16f, 0.21f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.41f, 0.49f, 0.18f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.24f, 0.56f, 0.66f, 0.28f));
    const bool pressed = ImGui::Button(id, ImVec2(kPaneSplitterWidth, height));
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    ImGui::PopStyleColor(3);

    if (hovered || active)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const ImU32 color =
        ImGui::GetColorU32(active
                               ? ImVec4(0.33f, 0.86f, 0.78f, 0.95f)
                               : hovered
                                   ? ImVec4(0.24f, 0.72f, 0.83f, 0.70f)
                                   : ImVec4(0.16f, 0.28f, 0.34f, 0.75f));
    const float centerX = (min.x + max.x) * 0.5f;
    drawList->AddLine(ImVec2(centerX, min.y + 4.0f), ImVec2(centerX, max.y - 4.0f), color, 2.0f);

    return pressed || active;
}

void ShowItemTooltip(const char* text)
{
    if (text == nullptr || text[0] == '\0')
    {
        return;
    }

    if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_NoSharedDelay))
    {
        return;
    }

    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 32.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}
} // namespace editor::ui

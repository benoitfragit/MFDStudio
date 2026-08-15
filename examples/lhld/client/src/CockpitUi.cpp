/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of the LHLD cockpit-shell drawing primitives.
 */

#include "CockpitUi.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace lhld::cockpit
{
namespace
{
constexpr float kWideLayoutThreshold = 1040.0f;
constexpr float kConsoleGap = 12.0f;
constexpr float kMinimumControlWidth = 330.0f;
constexpr float kMaximumControlWidth = 390.0f;
constexpr int kThemeColorCount = 18;
constexpr int kThemeStyleVarCount = 7;
constexpr ImU32 kPanelTopColor = IM_COL32(28, 31, 29, 255);
constexpr ImU32 kPanelBottomColor = IM_COL32(10, 12, 12, 255);
constexpr ImU32 kPanelEdgeColor = IM_COL32(72, 76, 69, 255);
constexpr ImU32 kEngravedColor = IM_COL32(219, 221, 202, 255);

void ShowLastItemTooltip(const char* tooltip)
{
    if (tooltip != nullptr && tooltip[0] != '\0' && ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", tooltip);
    }
}

void DrawFastener(ImDrawList& drawList, const ImVec2 center)
{
    drawList.AddCircleFilled(center, 4.5f, IM_COL32(7, 8, 8, 255), 16);
    drawList.AddCircle(center, 4.5f, IM_COL32(102, 105, 96, 255), 16, 1.0f);
    drawList.AddLine(
        ImVec2(center.x - 2.6f, center.y),
        ImVec2(center.x + 2.6f, center.y),
        IM_COL32(72, 75, 69, 255),
        1.0f);
}

ImVec4 ButtonColor(const bool active) noexcept
{
    return active ? ImVec4(0.14f, 0.25f, 0.13f, 1.0f) : ImVec4(0.09f, 0.11f, 0.10f, 1.0f);
}
} // namespace

ConsoleLayout ResolveConsoleLayout(const ImVec2& available) noexcept
{
    ConsoleLayout layout;
    layout.sideBySide = available.x >= kWideLayoutThreshold;
    if (!layout.sideBySide)
    {
        layout.mfdWidth = available.x;
        return layout;
    }

    layout.gap = kConsoleGap;
    layout.controlWidth = std::clamp(available.x * 0.29f, kMinimumControlWidth, kMaximumControlWidth);
    layout.mfdWidth = std::max(320.0f, available.x - layout.controlWidth - layout.gap);
    return layout;
}

void PushTheme()
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.83f, 0.84f, 0.77f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.38f, 0.39f, 0.36f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.025f, 0.030f, 0.029f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.050f, 0.057f, 0.054f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.045f, 0.052f, 0.050f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.30f, 0.32f, 0.28f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_BorderShadow, ImVec4(0.0f, 0.0f, 0.0f, 0.60f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.12f, 0.11f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.16f, 0.19f, 0.17f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.20f, 0.24f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.15f, 0.14f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.25f, 0.21f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.09f, 0.11f, 0.10f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.18f, 0.16f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.21f, 0.25f, 0.21f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.51f, 0.88f, 0.35f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.47f, 0.55f, 0.31f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.62f, 0.82f, 0.32f, 1.0f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 5.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(7.0f, 7.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 12.0f);
}

void PopTheme()
{
    ImGui::PopStyleVar(kThemeStyleVarCount);
    ImGui::PopStyleColor(kThemeColorCount);
}

void DrawBackplate()
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 minimum = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    const ImVec2 maximum {minimum.x + size.x, minimum.y + size.y};
    drawList->AddRectFilledMultiColor(
        minimum,
        maximum,
        IM_COL32(21, 24, 23, 255),
        IM_COL32(21, 24, 23, 255),
        IM_COL32(5, 7, 7, 255),
        IM_COL32(5, 7, 7, 255));

    constexpr float seamOffset = 7.0f;
    drawList->AddRect(
        ImVec2(minimum.x + seamOffset, minimum.y + seamOffset),
        ImVec2(maximum.x - seamOffset, maximum.y - seamOffset),
        IM_COL32(58, 62, 57, 160),
        3.0f,
        0,
        1.0f);
}

void DrawPanelHeader(const char* title, const char* subtitle)
{
    const ImVec2 minimum = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    constexpr float height = 42.0f;
    const ImVec2 maximum {minimum.x + width, minimum.y + height};
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilledMultiColor(minimum, maximum, kPanelTopColor, kPanelTopColor, kPanelBottomColor, kPanelBottomColor);
    drawList->AddRect(minimum, maximum, kPanelEdgeColor, 2.0f, 0, 1.0f);
    DrawFastener(*drawList, ImVec2(minimum.x + 10.0f, minimum.y + height * 0.5f));
    DrawFastener(*drawList, ImVec2(maximum.x - 10.0f, minimum.y + height * 0.5f));

    const ImVec2 titleSize = ImGui::CalcTextSize(title);
    drawList->AddText(
        ImVec2(minimum.x + 22.0f, minimum.y + (height - titleSize.y) * 0.5f),
        kEngravedColor,
        title);
    if (subtitle != nullptr && subtitle[0] != '\0')
    {
        const ImVec2 subtitleSize = ImGui::CalcTextSize(subtitle);
        drawList->AddText(
            ImVec2(maximum.x - subtitleSize.x - 22.0f, minimum.y + (height - subtitleSize.y) * 0.5f),
            IM_COL32(137, 151, 128, 255),
            subtitle);
    }
    ImGui::Dummy(ImVec2(width, height));
}

void DrawSectionHeader(const char* title, const char* subtitle)
{
    ImGui::Spacing();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddLine(origin, ImVec2(origin.x + width, origin.y), IM_COL32(78, 82, 73, 210), 1.0f);
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::TextColored(ImVec4(0.86f, 0.87f, 0.79f, 1.0f), "%s", title);
    if (subtitle != nullptr && subtitle[0] != '\0')
    {
        const ImVec2 subtitleSize = ImGui::CalcTextSize(subtitle);
        ImGui::SameLine(std::max(0.0f, ImGui::GetContentRegionAvail().x - subtitleSize.x));
        ImGui::TextDisabled("%s", subtitle);
    }
}

void DrawStatusLamp(const char* label, const bool active, const bool caution)
{
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    const ImU32 lampColor = active
        ? (caution ? IM_COL32(238, 163, 39, 255) : IM_COL32(95, 221, 94, 255))
        : IM_COL32(44, 51, 44, 255);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddCircleFilled(ImVec2(cursor.x + 7.0f, cursor.y + 8.0f), 5.0f, IM_COL32(4, 5, 5, 255), 16);
    drawList->AddCircleFilled(ImVec2(cursor.x + 7.0f, cursor.y + 8.0f), 3.3f, lampColor, 16);
    ImGui::SetCursorScreenPos(ImVec2(cursor.x + 17.0f, cursor.y));
    ImGui::TextUnformatted(label);
}

bool ModeButton(const char* label, const bool active, const ImVec2 size, const char* tooltip)
{
    ImGui::PushStyleColor(ImGuiCol_Button, ButtonColor(active));
    ImGui::PushStyleColor(
        ImGuiCol_ButtonHovered,
        active ? ImVec4(0.20f, 0.36f, 0.18f, 1.0f) : ImVec4(0.20f, 0.23f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, active ? ImVec4(0.57f, 0.91f, 0.44f, 1.0f) : ImVec4(0.86f, 0.87f, 0.79f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    const bool pressed = ImGui::Button(label, size);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    ShowLastItemTooltip(tooltip);
    return pressed;
}

bool ActionButton(const char* label,
                  const ImVec2 size,
                  const char* tooltip,
                  const ActionTone tone)
{
    if (tone == ActionTone::Caution)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.38f, 0.13f, 0.07f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.58f, 0.20f, 0.08f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.76f, 0.33f, 1.0f));
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.15f, 0.14f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.27f, 0.23f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.86f, 0.84f, 0.70f, 1.0f));
    }
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    const bool pressed = ImGui::Button(label, size);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    ShowLastItemTooltip(tooltip);
    return pressed;
}

bool TwoPositionSwitch(const char* id,
                       const char* label,
                       const bool on,
                       const bool enabled,
                       const char* tooltip)
{
    constexpr ImVec2 switchSize {112.0f, 96.0f};
    ImGui::PushID(id);
    const bool pressed = ImGui::InvisibleButton("##toggle", switchSize) && enabled;
    const ImVec2 minimum = ImGui::GetItemRectMin();
    const ImVec2 maximum = ImGui::GetItemRectMax();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImU32 textColor = enabled ? kEngravedColor : IM_COL32(83, 86, 78, 255);
    const ImVec2 labelSize = ImGui::CalcTextSize(label);
    drawList->AddText(
        ImVec2(minimum.x + (switchSize.x - labelSize.x) * 0.5f, minimum.y),
        textColor,
        label);

    const float centerX = (minimum.x + maximum.x) * 0.5f;
    const float pivotY = minimum.y + 55.0f;
    drawList->AddText(ImVec2(minimum.x + 7.0f, minimum.y + 27.0f), textColor, "ON");
    drawList->AddText(ImVec2(minimum.x + 7.0f, minimum.y + 68.0f), textColor, "OFF");
    drawList->AddRectFilled(
        ImVec2(centerX - 18.0f, minimum.y + 25.0f),
        ImVec2(centerX + 18.0f, minimum.y + 84.0f),
        IM_COL32(5, 6, 6, 255),
        6.0f);
    drawList->AddRect(
        ImVec2(centerX - 18.0f, minimum.y + 25.0f),
        ImVec2(centerX + 18.0f, minimum.y + 84.0f),
        enabled ? IM_COL32(91, 94, 85, 255) : IM_COL32(48, 50, 46, 255),
        6.0f,
        0,
        1.0f);

    const float knobY = on ? minimum.y + 35.0f : minimum.y + 76.0f;
    drawList->AddLine(
        ImVec2(centerX, pivotY),
        ImVec2(centerX, knobY),
        enabled ? IM_COL32(176, 179, 164, 255) : IM_COL32(70, 72, 67, 255),
        5.0f);
    drawList->AddCircleFilled(
        ImVec2(centerX, knobY),
        7.0f,
        enabled ? IM_COL32(124, 127, 116, 255) : IM_COL32(55, 57, 53, 255),
        20);
    drawList->AddCircle(ImVec2(centerX, knobY), 7.0f, IM_COL32(205, 207, 189, enabled ? 255 : 80), 20, 1.0f);
    drawList->AddCircleFilled(
        ImVec2(maximum.x - 13.0f, minimum.y + 32.0f),
        3.0f,
        on && enabled ? IM_COL32(97, 224, 91, 255) : IM_COL32(34, 43, 35, 255),
        12);
    ShowLastItemTooltip(tooltip);
    ImGui::PopID();
    return pressed;
}

bool VerticalThumbwheel(const char* id,
                        const char* label,
                        float& value,
                        const float minimum,
                        const float maximum,
                        const float unitsPerPixel,
                        const char* format,
                        const char* tooltip)
{
    constexpr ImVec2 controlSize {112.0f, 126.0f};
    ImGui::PushID(id);
    ImGui::InvisibleButton("##thumbwheel", controlSize);
    bool changed = !std::isfinite(value);
    if (changed)
    {
        value = std::clamp(0.0f, minimum, maximum);
    }
    if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        const float updated = std::clamp(value - ImGui::GetIO().MouseDelta.y * unitsPerPixel, minimum, maximum);
        changed = updated != value;
        value = updated;
    }
    if (ImGui::IsItemHovered() && ImGui::GetIO().MouseWheel != 0.0f)
    {
        const float updated = std::clamp(value + ImGui::GetIO().MouseWheel * unitsPerPixel * 4.0f, minimum, maximum);
        changed = changed || updated != value;
        value = updated;
    }

    const ImVec2 minimumPosition = ImGui::GetItemRectMin();
    const ImVec2 maximumPosition = ImGui::GetItemRectMax();
    const float centerX = (minimumPosition.x + maximumPosition.x) * 0.5f;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 labelSize = ImGui::CalcTextSize(label);
    drawList->AddText(
        ImVec2(minimumPosition.x + (controlSize.x - labelSize.x) * 0.5f, minimumPosition.y),
        kEngravedColor,
        label);

    const ImVec2 wheelMinimum {centerX - 23.0f, minimumPosition.y + 24.0f};
    const ImVec2 wheelMaximum {centerX + 23.0f, minimumPosition.y + 92.0f};
    drawList->AddRectFilled(wheelMinimum, wheelMaximum, IM_COL32(5, 6, 6, 255), 5.0f);
    drawList->AddRect(wheelMinimum, wheelMaximum, IM_COL32(92, 95, 86, 255), 5.0f, 0, 1.0f);
    for (int tick = -3; tick <= 3; ++tick)
    {
        const float y = minimumPosition.y + 58.0f + static_cast<float>(tick) * 8.0f;
        const float halfWidth = tick == 0 ? 18.0f : 13.0f;
        drawList->AddLine(
            ImVec2(centerX - halfWidth, y),
            ImVec2(centerX + halfWidth, y),
            tick == 0 ? IM_COL32(205, 193, 117, 255) : IM_COL32(108, 112, 101, 255),
            tick == 0 ? 2.0f : 1.0f);
    }

    char readout[32] {};
    std::snprintf(readout, sizeof(readout), format, value);
    const ImVec2 readoutSize = ImGui::CalcTextSize(readout);
    drawList->AddText(
        ImVec2(minimumPosition.x + (controlSize.x - readoutSize.x) * 0.5f, maximumPosition.y - 22.0f),
        IM_COL32(152, 224, 129, 255),
        readout);
    ShowLastItemTooltip(tooltip);
    ImGui::PopID();
    return changed;
}

bool SlewControl(const char* id,
                 const char* label,
                 float& x,
                 float& y,
                 const char* tooltip)
{
    constexpr ImVec2 controlSize {150.0f, 126.0f};
    constexpr float padSide = 92.0f;
    ImGui::PushID(id);
    ImGui::InvisibleButton("##slew", controlSize);
    const ImVec2 minimumPosition = ImGui::GetItemRectMin();
    const ImVec2 maximumPosition = ImGui::GetItemRectMax();
    const float padLeft = minimumPosition.x + (controlSize.x - padSide) * 0.5f;
    const ImVec2 padMinimum {padLeft, minimumPosition.y + 25.0f};
    const ImVec2 padMaximum {padLeft + padSide, minimumPosition.y + 25.0f + padSide};
    bool changed = !std::isfinite(x) || !std::isfinite(y);
    if (changed)
    {
        x = 0.0f;
        y = 0.0f;
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        changed = x != 0.0f || y != 0.0f;
        x = 0.0f;
        y = 0.0f;
    }
    else if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        const ImVec2 mouse = ImGui::GetMousePos();
        const float updatedX = std::clamp(2.0f * (mouse.x - padMinimum.x) / padSide - 1.0f, -1.0f, 1.0f);
        const float updatedY = std::clamp(1.0f - 2.0f * (mouse.y - padMinimum.y) / padSide, -1.0f, 1.0f);
        changed = updatedX != x || updatedY != y;
        x = updatedX;
        y = updatedY;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 labelSize = ImGui::CalcTextSize(label);
    drawList->AddText(
        ImVec2(minimumPosition.x + (controlSize.x - labelSize.x) * 0.5f, minimumPosition.y),
        kEngravedColor,
        label);
    drawList->AddRectFilled(padMinimum, padMaximum, IM_COL32(5, 7, 7, 255), 8.0f);
    drawList->AddRect(padMinimum, padMaximum, IM_COL32(91, 95, 87, 255), 8.0f, 0, 1.0f);
    const ImVec2 center {(padMinimum.x + padMaximum.x) * 0.5f, (padMinimum.y + padMaximum.y) * 0.5f};
    drawList->AddLine(
        ImVec2(padMinimum.x + 8.0f, center.y),
        ImVec2(padMaximum.x - 8.0f, center.y),
        IM_COL32(57, 70, 61, 255),
        1.0f);
    drawList->AddLine(
        ImVec2(center.x, padMinimum.y + 8.0f),
        ImVec2(center.x, padMaximum.y - 8.0f),
        IM_COL32(57, 70, 61, 255),
        1.0f);
    drawList->AddCircle(center, 28.0f, IM_COL32(46, 55, 49, 255), 32, 1.0f);

    const ImVec2 handle {
        center.x + std::clamp(x, -1.0f, 1.0f) * 34.0f,
        center.y - std::clamp(y, -1.0f, 1.0f) * 34.0f};
    drawList->AddCircleFilled(handle, 8.0f, IM_COL32(12, 15, 14, 255), 24);
    drawList->AddCircle(handle, 8.0f, IM_COL32(163, 169, 150, 255), 24, 1.5f);
    drawList->AddCircleFilled(handle, 2.5f, IM_COL32(107, 223, 94, 255), 12);
    ShowLastItemTooltip(tooltip);
    ImGui::PopID();
    return changed;
}
} // namespace lhld::cockpit

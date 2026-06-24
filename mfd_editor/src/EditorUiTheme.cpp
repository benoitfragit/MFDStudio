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

#include <cmath>
#include <cstdio>
#include <map>
#include <string>

namespace editor::ui
{
namespace
{
/**
 * @brief Optional caller-owned store used to persist inspector-section open states across sessions.
 *
 * @note Single injected pointer rather than a parameter on every section call: the editor sets it
 * once at startup and clears it at shutdown, keeping the section helper call sites terse.
 */
std::map<std::string, bool>* g_inspectorSectionStateStore = nullptr;
} // namespace
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
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(kPaneSplitterWidth, height));
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();

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
    const float centerX = std::floor((min.x + max.x) * 0.5f) + 0.5f;
    drawList->AddLine(ImVec2(centerX, min.y + 4.0f), ImVec2(centerX, max.y - 4.0f), color, active ? 2.0f : 1.5f);

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

bool BeginInspectorSection(const char* strId, const char* label, const bool defaultOpen, const bool forceOpen)
{
    const std::string key(strId != nullptr ? strId : "");
    const auto storedIterator =
        g_inspectorSectionStateStore != nullptr ? g_inspectorSectionStateStore->find(key)
                                                : std::map<std::string, bool>::iterator {};
    const bool hasStoredState =
        g_inspectorSectionStateStore != nullptr && storedIterator != g_inspectorSectionStateStore->end();

    if (forceOpen)
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    }
    else if (hasStoredState)
    {
        // Seed the live ImGui state from the persisted preference the first time the section is shown.
        ImGui::SetNextItemOpen(storedIterator->second, ImGuiCond_Once);
    }
    else if (defaultOpen)
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
    }

    // Push a stable id so the persistent open/closed state is keyed on the caller-provided id
    // rather than the visible caption, which several inspectors reuse.
    ImGui::PushID(strId);
    const bool open = ImGui::CollapsingHeader(label != nullptr ? label : strId);
    ImGui::PopID();

    // Mirror live changes back into the store, but never while force-opened so the tutorial does not
    // overwrite the user's real preference.
    if (g_inspectorSectionStateStore != nullptr && !forceOpen)
    {
        (*g_inspectorSectionStateStore)[key] = open;
    }

    return open;
}

void SetInspectorSectionStateStore(std::map<std::string, bool>* const store) noexcept
{
    g_inspectorSectionStateStore = store;
}

void DisabledTextWrapped(const char* text)
{
    if (text == nullptr || text[0] == '\0')
    {
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

void InspectorHelpMarker(const char* text)
{
    if (text == nullptr || text[0] == '\0')
    {
        return;
    }

    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    ShowItemTooltip(text);
}

std::string FormatViewportToolbarInfoLabel(const float zoom, const std::optional<mfd::Vec2>& mouseLogical)
{
    char buffer[96] {};
    if (mouseLogical.has_value())
    {
        std::snprintf(buffer,
                      sizeof(buffer),
                      "Zoom: %.0f%%  X %+0.3f  Y %+0.3f",
                      std::round(static_cast<double>(zoom) * 100.0),
                      static_cast<double>(mouseLogical->x),
                      static_cast<double>(mouseLogical->y));
    }
    else
    {
        std::snprintf(buffer,
                      sizeof(buffer),
                      "Zoom: %.0f%%",
                      std::round(static_cast<double>(zoom) * 100.0));
    }

    return buffer;
}
} // namespace editor::ui

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

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>
#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace editor::ui
{
namespace
{
constexpr float kBaseFontPixelSize = 17.0f;
constexpr float kMinimumDisplayScale = 0.75f;
constexpr float kMaximumDisplayScale = 3.0f;
constexpr float kMinimumInspectorLabelWidth = 104.0f;
constexpr float kMaximumInspectorLabelWidth = 160.0f;
constexpr float kInspectorLabelWidthRatio = 0.40f;

/**
 * @brief Optional caller-owned store used to persist inspector-section open states across sessions.
 *
 * @note Single injected pointer rather than a parameter on every section call: the editor sets it
 * once at startup and clears it at shutdown, keeping the section helper call sites terse.
 */
std::map<std::string, bool>* g_inspectorSectionStateStore = nullptr;

float SanitizeDisplayScale(const float displayScale) noexcept
{
    if (!std::isfinite(displayScale))
    {
        return 1.0f;
    }

    return std::clamp(displayScale, kMinimumDisplayScale, kMaximumDisplayScale);
}

std::filesystem::path ResolveSegoeUiPath()
{
#if defined(_WIN32)
    std::array<wchar_t, MAX_PATH + 1U> windowsDirectory {};
    const UINT length = GetWindowsDirectoryW(windowsDirectory.data(), static_cast<UINT>(windowsDirectory.size()));
    if (length == 0U || length >= windowsDirectory.size())
    {
        return {};
    }

    const std::filesystem::path fontPath =
        std::filesystem::path(std::wstring(windowsDirectory.data(), length)) / "Fonts" / "segoeui.ttf";
    std::error_code error;
    return std::filesystem::is_regular_file(fontPath, error) && !error ? fontPath : std::filesystem::path {};
#else
    return {};
#endif
}

bool DrawSectionHeader(const char* const label, const ImGuiTreeNodeFlags flags)
{
    const ImVec4 surface = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
    const ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_NavHighlight);
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(surface.x, surface.y, surface.z, 0.72f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(accent.x, accent.y, accent.z, 0.20f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(accent.x, accent.y, accent.z, 0.30f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(9.0f, 7.0f));
    const bool open = ImGui::CollapsingHeader(label, flags);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    const ImVec2 headerMin = ImGui::GetItemRectMin();
    const ImVec2 headerMax = ImGui::GetItemRectMax();
    const ImU32 markerColor = ImGui::GetColorU32(
        open ? ImGui::GetStyleColorVec4(ImGuiCol_CheckMark) : ImVec4(accent.x, accent.y, accent.z, 0.48f));
    ImGui::GetWindowDrawList()->AddRectFilled(
        headerMin, ImVec2(headerMin.x + 3.0f, headerMax.y), markerColor, ImGui::GetStyle().FrameRounding,
        ImDrawFlags_RoundCornersLeft);
    return open;
}
} // namespace

float EditorFontPixelSize(const float displayScale) noexcept
{
    return std::ceil(kBaseFontPixelSize * SanitizeDisplayScale(displayScale));
}

bool ConfigureEditorFont(const float displayScale)
{
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig configuration;
    configuration.OversampleH = 2;
    configuration.OversampleV = 1;
    configuration.PixelSnapH = false;
    configuration.RasterizerMultiply = 1.0f;

    ImFont* editorFont = nullptr;
    const std::filesystem::path systemFontPath = ResolveSegoeUiPath();
    if (!systemFontPath.empty())
    {
        const std::string path = systemFontPath.string();
        editorFont = io.Fonts->AddFontFromFileTTF(
            path.c_str(), EditorFontPixelSize(displayScale), &configuration, io.Fonts->GetGlyphRangesDefault());
    }

    const bool loadedSystemFont = editorFont != nullptr;
    if (!loadedSystemFont)
    {
        configuration.SizePixels = EditorFontPixelSize(displayScale);
        editorFont = io.Fonts->AddFontDefault(&configuration);
    }

    if (editorFont != nullptr)
    {
        io.FontDefault = editorFont;
    }
    return loadedSystemFont;
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
    style.ScrollbarSize = 15.0f;
    style.GrabMinSize = 12.0f;
    style.DisabledAlpha = 0.70f;
    style.AntiAliasedLines = true;
    style.AntiAliasedFill = true;

    ImVec4* colors = style.Colors;
    const ImVec4 background(0.051f, 0.067f, 0.106f, 1.00f);
    const ImVec4 panel(0.086f, 0.118f, 0.173f, 1.00f);
    const ImVec4 raisedSurface(0.125f, 0.169f, 0.235f, 1.00f);
    const ImVec4 border(0.204f, 0.255f, 0.333f, 1.00f);
    const ImVec4 primary(0.341f, 0.682f, 1.000f, 1.00f);
    const ImVec4 secondary(0.333f, 0.843f, 0.745f, 1.00f);

    colors[ImGuiCol_Text] = ImVec4(0.949f, 0.965f, 0.988f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.718f, 0.776f, 0.851f, 1.00f);
    colors[ImGuiCol_WindowBg] = background;
    colors[ImGuiCol_ChildBg] = panel;
    colors[ImGuiCol_PopupBg] = ImVec4(panel.x, panel.y, panel.z, 0.98f);
    colors[ImGuiCol_Border] = border;
    colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_FrameBg] = raisedSurface;
    colors[ImGuiCol_FrameBgHovered] = ImVec4(primary.x, primary.y, primary.z, 0.34f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(primary.x, primary.y, primary.z, 0.48f);
    colors[ImGuiCol_TitleBg] = background;
    colors[ImGuiCol_TitleBgActive] = panel;
    colors[ImGuiCol_TitleBgCollapsed] = background;
    colors[ImGuiCol_MenuBarBg] = background;
    colors[ImGuiCol_ScrollbarBg] = background;
    colors[ImGuiCol_ScrollbarGrab] = border;
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(primary.x, primary.y, primary.z, 0.58f);
    colors[ImGuiCol_ScrollbarGrabActive] = primary;
    colors[ImGuiCol_CheckMark] = secondary;
    colors[ImGuiCol_SliderGrab] = primary;
    colors[ImGuiCol_SliderGrabActive] = secondary;
    colors[ImGuiCol_Button] = raisedSurface;
    colors[ImGuiCol_ButtonHovered] = ImVec4(primary.x, primary.y, primary.z, 0.42f);
    colors[ImGuiCol_ButtonActive] = ImVec4(primary.x, primary.y, primary.z, 0.60f);
    colors[ImGuiCol_Header] = ImVec4(primary.x, primary.y, primary.z, 0.13f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(primary.x, primary.y, primary.z, 0.25f);
    colors[ImGuiCol_HeaderActive] = ImVec4(primary.x, primary.y, primary.z, 0.36f);
    colors[ImGuiCol_Separator] = border;
    colors[ImGuiCol_SeparatorHovered] = primary;
    colors[ImGuiCol_SeparatorActive] = secondary;
    colors[ImGuiCol_ResizeGrip] = ImVec4(primary.x, primary.y, primary.z, 0.24f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(primary.x, primary.y, primary.z, 0.62f);
    colors[ImGuiCol_ResizeGripActive] = primary;
    colors[ImGuiCol_Tab] = panel;
    colors[ImGuiCol_TabHovered] = ImVec4(primary.x, primary.y, primary.z, 0.45f);
    colors[ImGuiCol_TabActive] = raisedSurface;
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.000f, 0.471f, 0.537f, 1.00f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(primary.x, primary.y, primary.z, 0.35f);
    colors[ImGuiCol_NavHighlight] = primary;
}

bool AccentButton(const char* label)
{
    const ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(accent.x * 0.62f, accent.y * 0.62f, accent.z * 0.62f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(accent.x * 0.80f, accent.y * 0.80f, accent.z * 0.80f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, accent);
    const bool pressed = ImGui::Button(label);
    ImGui::PopStyleColor(3);
    return pressed;
}

void DrawInspectorHeading(const char* const title, const char* const description)
{
    if (title == nullptr || title[0] == '\0')
    {
        return;
    }

    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    const float titleHeight = ImGui::GetTextLineHeight();
    const ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    ImGui::GetWindowDrawList()->AddRectFilled(
        cursor, ImVec2(cursor.x + 3.0f, cursor.y + titleHeight), ImGui::GetColorU32(accent), 2.0f);
    ImGui::SetCursorScreenPos(ImVec2(cursor.x + 11.0f, cursor.y));
    ImGui::TextUnformatted(title);
    if (description != nullptr && description[0] != '\0')
    {
        DisabledTextWrapped(description);
    }
    ImGui::Spacing();
}

float InspectorFieldLabelWidth(const float availableWidth) noexcept
{
    if (!std::isfinite(availableWidth))
    {
        return kMinimumInspectorLabelWidth;
    }
    return std::clamp(availableWidth * kInspectorLabelWidthRatio,
                      kMinimumInspectorLabelWidth,
                      kMaximumInspectorLabelWidth);
}

void BeginInspectorField(const char* const label)
{
    const char* const safeLabel = label != nullptr ? label : "";
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float rowStartX = ImGui::GetCursorPosX();
    ImGui::PushID(safeLabel);
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", safeLabel);
    ImGui::SameLine(rowStartX + InspectorFieldLabelWidth(availableWidth));
    ImGui::SetNextItemWidth(-1.0f);
}

void EndInspectorField()
{
    ImGui::PopID();
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

bool DrawHorizontalSplitter(const char* id, const float width, const float thickness)
{
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(width, std::max(1.0f, thickness)));
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();

    if (hovered || active)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
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
    const float centerY = std::floor((min.y + max.y) * 0.5f) + 0.5f;
    drawList->AddLine(ImVec2(min.x + 4.0f, centerY), ImVec2(max.x - 4.0f, centerY), color, active ? 2.0f : 1.5f);

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
    const bool open = DrawSectionHeader(label != nullptr ? label : strId, ImGuiTreeNodeFlags_None);
    ImGui::PopID();

    // Mirror live changes back into the store, but never while force-opened so the tutorial does not
    // overwrite the user's real preference.
    if (g_inspectorSectionStateStore != nullptr && !forceOpen)
    {
        (*g_inspectorSectionStateStore)[key] = open;
    }

    if (open)
    {
        ImGui::Spacing();
    }
    return open;
}

bool BeginNavigationSection(const char* const label, const bool forceOpen)
{
    if (forceOpen)
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    }
    else
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
    }
    return DrawSectionHeader(label != nullptr ? label : "", ImGuiTreeNodeFlags_None);
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

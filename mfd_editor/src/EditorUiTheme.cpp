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
constexpr float kNavigationHeaderVerticalPadding = 6.0f;

/**
 * @brief Optional caller-owned store used to persist inspector-section open states across sessions.
 *
 * @note Single injected pointer rather than a parameter on every section call: the editor sets it
 * once at startup and clears it at shutdown, keeping the section helper call sites terse.
 */
std::map<std::string, bool>* g_inspectorSectionStateStore = nullptr;
ImFont* g_headingFont = nullptr;

float SanitizeDisplayScale(const float displayScale) noexcept
{
    if (!std::isfinite(displayScale))
    {
        return 1.0f;
    }

    return std::clamp(displayScale, kMinimumDisplayScale, kMaximumDisplayScale);
}

std::filesystem::path ResolveWindowsFontPath(const wchar_t* const fileName)
{
    if (fileName == nullptr || fileName[0] == L'\0')
    {
        return {};
    }

#if defined(_WIN32)
    std::array<wchar_t, MAX_PATH + 1U> windowsDirectory {};
    const UINT length = GetWindowsDirectoryW(windowsDirectory.data(), static_cast<UINT>(windowsDirectory.size()));
    if (length == 0U || length >= windowsDirectory.size())
    {
        return {};
    }

    const std::filesystem::path fontPath =
        std::filesystem::path(std::wstring(windowsDirectory.data(), length)) / "Fonts" / fileName;
    std::error_code error;
    return std::filesystem::is_regular_file(fontPath, error) && !error ? fontPath : std::filesystem::path {};
#else
    return {};
#endif
}

bool DrawSectionHeader(const char* const label, const ImGuiTreeNodeFlags flags, const bool tonalSurface)
{
    const ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_NavHighlight);
    const ImVec4 surface = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
    const ImVec4 restingColor =
        tonalSurface ? ImVec4(surface.x, surface.y, surface.z, 0.76f) : ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Header, restingColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(accent.x, accent.y, accent.z, tonalSurface ? 0.18f : 0.12f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(accent.x, accent.y, accent.z, tonalSurface ? 0.26f : 0.20f));
    ImGui::PushStyleVar(
        ImGuiStyleVar_FramePadding,
        tonalSurface ? ImVec2(9.0f, 7.0f) : ImVec2(5.0f, kNavigationHeaderVerticalPadding));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, tonalSurface ? 9.0f : 7.0f);
    const bool open = ImGui::CollapsingHeader(label, flags);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);
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
    const std::filesystem::path systemFontPath = ResolveWindowsFontPath(L"segoeui.ttf");
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

    g_headingFont = editorFont;
    const std::filesystem::path headingFontPath = ResolveWindowsFontPath(L"seguisb.ttf");
    if (!headingFontPath.empty())
    {
        ImFontConfig headingConfiguration = configuration;
        const std::string path = headingFontPath.string();
        if (ImFont* const headingFont = io.Fonts->AddFontFromFileTTF(
                path.c_str(),
                std::ceil(EditorFontPixelSize(displayScale) * 1.12f),
                &headingConfiguration,
                io.Fonts->GetGlyphRangesDefault()))
        {
            g_headingFont = headingFont;
        }
    }
    return loadedSystemFont;
}

void ApplyEditorTheme()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 12.0f;
    style.ChildRounding = 12.0f;
    style.FrameRounding = 9.0f;
    style.PopupRounding = 12.0f;
    style.ScrollbarRounding = 10.0f;
    style.GrabRounding = 9.0f;
    style.WindowPadding = ImVec2(14.0f, 14.0f);
    style.FramePadding = ImVec2(10.0f, 7.0f);
    style.ItemSpacing = ImVec2(10.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 11.0f;
    style.GrabMinSize = 14.0f;
    style.DisabledAlpha = 0.62f;
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.TabBorderSize = 0.0f;
    style.AntiAliasedLines = true;
    style.AntiAliasedFill = true;

    ImVec4* colors = style.Colors;
    // Material-inspired dark surfaces: hierarchy comes from tone and elevation, not outlines.
    const ImVec4 background(0.027f, 0.039f, 0.063f, 1.00f);
    const ImVec4 panel(0.055f, 0.075f, 0.110f, 1.00f);
    const ImVec4 raisedSurface(0.094f, 0.125f, 0.173f, 1.00f);
    const ImVec4 border(0.337f, 0.400f, 0.494f, 0.28f);
    const ImVec4 primary(0.427f, 0.718f, 1.000f, 1.00f);
    const ImVec4 secondary(0.384f, 0.875f, 0.792f, 1.00f);

    colors[ImGuiCol_Text] = ImVec4(0.925f, 0.953f, 0.992f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.650f, 0.714f, 0.804f, 1.00f);
    colors[ImGuiCol_WindowBg] = background;
    colors[ImGuiCol_ChildBg] = panel;
    colors[ImGuiCol_PopupBg] = ImVec4(panel.x, panel.y, panel.z, 0.98f);
    colors[ImGuiCol_Border] = border;
    colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_FrameBg] = raisedSurface;
    colors[ImGuiCol_FrameBgHovered] = ImVec4(primary.x, primary.y, primary.z, 0.18f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(primary.x, primary.y, primary.z, 0.27f);
    colors[ImGuiCol_TitleBg] = background;
    colors[ImGuiCol_TitleBgActive] = panel;
    colors[ImGuiCol_TitleBgCollapsed] = background;
    colors[ImGuiCol_MenuBarBg] = background;
    colors[ImGuiCol_ScrollbarBg] = ImVec4(background.x, background.y, background.z, 0.18f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.337f, 0.400f, 0.494f, 0.42f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(primary.x, primary.y, primary.z, 0.50f);
    colors[ImGuiCol_ScrollbarGrabActive] = primary;
    colors[ImGuiCol_CheckMark] = secondary;
    colors[ImGuiCol_SliderGrab] = primary;
    colors[ImGuiCol_SliderGrabActive] = secondary;
    colors[ImGuiCol_Button] = raisedSurface;
    colors[ImGuiCol_ButtonHovered] = ImVec4(primary.x, primary.y, primary.z, 0.20f);
    colors[ImGuiCol_ButtonActive] = ImVec4(primary.x, primary.y, primary.z, 0.30f);
    colors[ImGuiCol_Header] = ImVec4(primary.x, primary.y, primary.z, 0.20f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(primary.x, primary.y, primary.z, 0.26f);
    colors[ImGuiCol_HeaderActive] = ImVec4(primary.x, primary.y, primary.z, 0.34f);
    colors[ImGuiCol_Separator] = border;
    colors[ImGuiCol_SeparatorHovered] = primary;
    colors[ImGuiCol_SeparatorActive] = secondary;
    colors[ImGuiCol_ResizeGrip] = ImVec4(primary.x, primary.y, primary.z, 0.24f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(primary.x, primary.y, primary.z, 0.62f);
    colors[ImGuiCol_ResizeGripActive] = primary;
    colors[ImGuiCol_Tab] = panel;
    colors[ImGuiCol_TabHovered] = ImVec4(primary.x, primary.y, primary.z, 0.34f);
    colors[ImGuiCol_TabActive] = raisedSurface;
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.000f, 0.471f, 0.537f, 1.00f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(primary.x, primary.y, primary.z, 0.30f);
    colors[ImGuiCol_NavHighlight] = primary;
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.957f, 0.765f, 0.420f, 0.92f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.58f);
}

bool AccentButton(const char* label)
{
    const ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_NavHighlight);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(accent.x * 0.72f, accent.y * 0.72f, accent.z * 0.72f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(accent.x * 0.88f, accent.y * 0.88f, accent.z * 0.88f, 1.00f));
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

    if (g_headingFont != nullptr)
    {
        ImGui::PushFont(g_headingFont);
    }
    ImGui::TextUnformatted(title);
    if (g_headingFont != nullptr)
    {
        ImGui::PopFont();
    }
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
                                   : ImVec4(0.16f, 0.28f, 0.34f, 0.16f));
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
                                   : ImVec4(0.16f, 0.28f, 0.34f, 0.16f));
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
    const bool open = DrawSectionHeader(label != nullptr ? label : strId, ImGuiTreeNodeFlags_None, true);
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

bool BeginNavigationSection(const char* const label, const bool forceOpen, bool& expanded)
{
    ImGui::SetNextItemOpen(forceOpen || expanded, ImGuiCond_Always);
    const bool open = DrawSectionHeader(label != nullptr ? label : "", ImGuiTreeNodeFlags_None, false);
    if (!forceOpen)
    {
        expanded = open;
    }
    return open;
}

float NavigationSectionCollapsedHeight() noexcept
{
    return ImGui::GetTextLineHeight() + 2.0f * kNavigationHeaderVerticalPadding +
           ImGui::GetStyle().ItemSpacing.y;
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

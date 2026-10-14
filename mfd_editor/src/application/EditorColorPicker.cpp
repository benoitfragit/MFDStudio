/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "internal/application/EditorColorPicker.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <imgui.h>

namespace editor::ui
{
namespace
{
constexpr std::array<const char*, 8> kNames {{
    "Black", "White", "Red", "Green", "Blue", "Yellow", "Cyan", "Amber"}};

int ColorChannel(const float channel) noexcept
{
    return std::isfinite(channel)
               ? static_cast<int>(std::lround(std::clamp(channel, 0.0f, 1.0f) * 255.0f))
               : 0;
}

bool SameColor(const float* color, const mfd::ColorRgba value) noexcept
{
    return ColorChannel(color[0]) == value.r && ColorChannel(color[1]) == value.g &&
           ColorChannel(color[2]) == value.b && ColorChannel(color[3]) == value.a;
}

ImVec4 ToSwatch(const mfd::ColorRgba color) noexcept
{
    return {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f};
}

void ShowColorTooltip(const char* name, const ImVec4 color)
{
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        ImGui::SetTooltip("%s\nRGBA (%d, %d, %d, %d)\n#%02X%02X%02X%02X", name,
                          ColorChannel(color.x), ColorChannel(color.y), ColorChannel(color.z), ColorChannel(color.w),
                          ColorChannel(color.x), ColorChannel(color.y), ColorChannel(color.z), ColorChannel(color.w));
    }
}

void DrawSelectionOutline()
{
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    draw->AddRect(min, max, IM_COL32(0, 0, 0, 255), 2.0f, 0, 4.0f);
    draw->AddRect(min, max, IM_COL32(255, 255, 255, 255), 2.0f, 0, 2.0f);
}

/** @brief Copies effective byte colors only when the user changes their value. */
bool CopyEditedColor(float* color, const float* edited) noexcept
{
    bool changed = false;
    for (std::size_t channel = 0; channel < 4; ++channel)
    {
        changed = changed || ColorChannel(color[channel]) != ColorChannel(edited[channel]);
    }
    if (changed)
    {
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            color[channel] = ColorChannel(edited[channel]) / 255.0f;
        }
    }
    return changed;
}

/** @brief Draws exactly eight built-in shortcuts beside the standard RGBA picker. */
void DrawPresetShortcuts(float* color, ColorPickerInteraction& result)
{
    ImGui::BeginGroup();
    ImGui::TextUnformatted("Presets");
    const float size = ImGui::GetFrameHeight();
    for (std::size_t index = 0; index < kNames.size(); ++index)
    {
        if (index % 2 != 0)
        {
            ImGui::SameLine();
        }
        const auto preset = static_cast<mfd::ColorPreset>(index + 1);
        const mfd::ColorRgba value = *mfd::TryColorPreset(preset);
        const ImVec4 swatch = ToSwatch(value);
        if (ImGui::ColorButton(kNames[index], swatch, ImGuiColorEditFlags_NoTooltip,
                               ImVec2(size, size)))
        {
            result.selected = preset;
            result.changed = CopyEditedColor(color, &swatch.x);
            result.beganEdit = result.changed;
        }
        ShowColorTooltip(kNames[index], swatch);
        if (SameColor(color, value))
        {
            DrawSelectionOutline();
        }
    }
    ImGui::EndGroup();
}

/** @brief Applies live RGBA edits and groups each continuous gesture for undo. */
void DrawPickerPopup(float* color, ImGuiStorage& storage, const ImGuiID editingId,
                     ColorPickerInteraction& result)
{
    if (!ImGui::BeginPopup("Color selector"))
    {
        storage.SetBool(editingId, false);
        return;
    }

    std::array<float, 4> edited {{color[0], color[1], color[2], color[3]}};
    ImGui::PushItemWidth(ImGui::GetFrameHeight() * 12.0f);
    const bool pickerChanged = ImGui::ColorPicker4("##rgba", edited.data(),
        ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf |
        ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoLabel);
    const bool pickerActive = ImGui::IsItemActive();
    ImGui::PopItemWidth();
    if (pickerChanged)
    {
        result.changed = CopyEditedColor(color, edited.data());
        result.beganEdit = result.changed && !storage.GetBool(editingId);
    }
    storage.SetBool(editingId, pickerActive && (result.changed || storage.GetBool(editingId)));

    ImGui::SameLine();
    DrawPresetShortcuts(color, result);
    // The editor does not enable ImGui keyboard navigation, so close explicitly.
    if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
} // namespace

ColorPickerInteraction DrawColorPicker(const char* label, float* color)
{
    ColorPickerInteraction result;
    ImGui::PushID(label);
    ImGui::BeginGroup();
    ImGuiStorage& storage = *ImGui::GetStateStorage();
    const ImGuiID editingId = ImGui::GetID("colorEditing");
    if (ImGui::ColorButton("##current", ImVec4(color[0], color[1], color[2], color[3]),
                           ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_NoTooltip))
    {
        storage.SetBool(editingId, false);
        ImGui::OpenPopup("Color selector");
        result.popupOpened = true;
    }
    ShowColorTooltip(label, ImVec4(color[0], color[1], color[2], color[3]));
    ImGui::SameLine();
    ImGui::TextUnformatted(label);
    DrawPickerPopup(color, storage, editingId, result);
    result.popupVisible = ImGui::IsPopupOpen("Color selector");
    ImGui::EndGroup();
    ImGui::PopID();
    return result;
}
} // namespace editor::ui

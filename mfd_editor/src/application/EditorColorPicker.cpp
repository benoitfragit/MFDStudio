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
constexpr std::array<const char*, 9> kNames {{
    "Black", "White", "Red", "Green", "Blue", "Yellow", "Cyan", "Amber", "Custom"}};

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

bool DrawCustomPopup(ImGuiStorage& storage, const ImGuiID draftId, float* color)
{
    bool changed = false;
    if (ImGui::BeginPopup("Custom RGBA"))
    {
        std::array<float, 4> draft {};
        for (unsigned int channel = 0; channel < draft.size(); ++channel)
        {
            draft[channel] = storage.GetFloat(draftId + channel);
        }
        ImGui::TextUnformatted("Custom RGBA");
        ImGui::ColorPicker4("##rgba", draft.data(), ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf);
        for (unsigned int channel = 0; channel < draft.size(); ++channel)
        {
            storage.SetFloat(draftId + channel, draft[channel]);
        }
        if (ImGui::Button("Apply"))
        {
            for (std::size_t channel = 0; channel < draft.size(); ++channel)
            {
                changed = changed || ColorChannel(color[channel]) != ColorChannel(draft[channel]);
                color[channel] = ColorChannel(draft[channel]) / 255.0f;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    return changed;
}
} // namespace

ColorPickerInteraction DrawColorPicker(const char* label, float* color)
{
    ColorPickerInteraction result;
    ImGui::PushID(label);
    ImGui::BeginGroup();
    ImGui::TextUnformatted(label);
    ImGuiStorage& storage = *ImGui::GetStateStorage();
    const ImGuiID draftId = ImGui::GetID("customDraft");
    int selected = 8;
    for (int index = 0; index < 8; ++index)
    {
        if (SameColor(color, *mfd::TryColorPreset(static_cast<mfd::ColorPreset>(index + 1))))
        {
            selected = index;
            break;
        }
    }
    const float size = ImGui::GetFrameHeight();
    for (int index = 0; index < 9; ++index)
    {
        if (index % 3 != 0)
        {
            ImGui::SameLine();
        }
        const auto preset = index == 8 ? mfd::ColorPreset::Custom : static_cast<mfd::ColorPreset>(index + 1);
        const ImVec4 swatch = index == 8 ? ImVec4(color[0], color[1], color[2], color[3])
                                        : ToSwatch(*mfd::TryColorPreset(preset));
        if (ImGui::ColorButton(kNames[index], swatch, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_AlphaPreviewHalf,
                               ImVec2(size, size)))
        {
            if (index == 8)
            {
                for (unsigned int channel = 0; channel < 4; ++channel)
                {
                    storage.SetFloat(draftId + channel, color[channel]);
                }
                ImGui::OpenPopup("Custom RGBA");
                result.customOpened = true;
            }
            else
            {
                result.selected = preset;
                result.changed = selected != index;
                color[0] = swatch.x;
                color[1] = swatch.y;
                color[2] = swatch.z;
                color[3] = swatch.w;
                selected = index;
            }
        }
        ShowColorTooltip(kNames[index], swatch);
        if (selected == index)
        {
            DrawSelectionOutline();
        }
        if (index == 8)
        {
            const ImVec2 min = ImGui::GetItemRectMin();
            ImGui::GetWindowDrawList()->AddText(ImVec2(min.x + 5, min.y + 1), IM_COL32(0, 0, 0, 255), "+");
            ImGui::GetWindowDrawList()->AddText(ImVec2(min.x + 4, min.y), IM_COL32(255, 255, 255, 255), "+");
        }
    }
    result.changed = DrawCustomPopup(storage, draftId, color) || result.changed;
    result.customVisible = ImGui::IsPopupOpen("Custom RGBA");
    ImGui::EndGroup();
    ImGui::PopID();
    return result;
}
} // namespace editor::ui

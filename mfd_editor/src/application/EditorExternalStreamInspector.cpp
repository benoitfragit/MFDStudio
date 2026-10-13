#include "internal/application/EditorColorPicker.h"

#include "internal/application/EditorExternalStreamInspector.h"
#include "mfd/model/RuntimeBudgets.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <imgui.h>

namespace editor::app
{
std::optional<mfd::ExternalStreamGeometry> DrawExternalStreamInspector(
    const mfd::ExternalStreamGeometry& geometry, const mfd::ExternalStreamRegistry& registry)
{
    mfd::ExternalStreamGeometry draft = geometry;
    std::array<char, 65> id {};
    std::copy_n(geometry.streamId.data(), std::min(geometry.streamId.size(), id.size() - 1), id.data());
    bool changed = ImGui::InputText("Stream identifier", id.data(), id.size(), ImGuiInputTextFlags_EnterReturnsTrue);
    if (changed) draft.streamId = id.data();
    std::array<float, 2> size {draft.width, draft.height};
    if (ImGui::DragFloat2("Display size", size.data(), 0.002f, 0.001f, 4.0f))
    { draft.width = size[0]; draft.height = size[1]; changed = true; }
    int fit = static_cast<int>(draft.fit);
    if (ImGui::Combo("Fit", &fit, "Keep proportions\0Stretch\0"))
    { draft.fit = static_cast<mfd::StreamFit>(fit); changed = true; }
    int playback = draft.defaultPlayback == mfd::StreamPlayback::Stop ? 1 : 0;
    if (ImGui::Combo("Default playback", &playback, "Play\0Stop\0"))
    {
        draft.defaultPlayback = playback ? mfd::StreamPlayback::Stop : mfd::StreamPlayback::Play;
        draft.playback = draft.defaultPlayback; changed = true;
    }
    std::array<float, 4> color {draft.fallback.r / 255.0f, draft.fallback.g / 255.0f,
                                draft.fallback.b / 255.0f, draft.fallback.a / 255.0f};
    if (editor::ui::DrawColorPicker("Replacement color", color.data()).changed)
    {
        for (float& channel : color) channel = std::isfinite(channel) ? std::clamp(channel, 0.0f, 1.0f) : 0.0f;
        draft.fallback = {static_cast<std::uint8_t>(color[0] * 255), static_cast<std::uint8_t>(color[1] * 255),
                          static_cast<std::uint8_t>(color[2] * 255), static_cast<std::uint8_t>(color[3] * 255)};
        changed = true;
    }
    ImGui::Text("Instance: %s", registry.Instance().c_str());
    for (const auto& feedback : registry.Feedback())
        if (feedback.streamId == geometry.streamId)
        {
            ImGui::TextWrapped("%s", feedback.status.diagnostic.c_str());
            ImGui::Text("Consumed frame: %llu", static_cast<unsigned long long>(feedback.status.consumedFrame));
        }
    if (!mfd::IsValidStreamId(draft.streamId))
    { ImGui::TextUnformatted("Use 1-64 ASCII letters, digits, '-' or '_'."); return std::nullopt; }
    return changed ? std::optional<mfd::ExternalStreamGeometry>(std::move(draft)) : std::nullopt;
}
bool ApplyExternalStreamEdit(mfd::Primitive& primitive, const mfd::ExternalStreamGeometry& geometry)
{
    mfd::Primitive draft = primitive;
    draft.geometry = geometry;
    if (!mfd::runtime_validation::IsValidPrimitiveForRuntime(draft)) return false;
    primitive.geometry = geometry;
    return true;
}
}

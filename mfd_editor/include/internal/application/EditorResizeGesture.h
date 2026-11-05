/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

#include <imgui.h>
#include <optional>

namespace editor::app
{
/** @brief Tracks a radial resize from the actual pointer press, in viewport pixels. */
class EditorResizeGesture
{
public:
    void Begin(ImVec2 press, ImVec2 pivot) noexcept;
    /** @brief Returns a scale only after the pointer has crossed the drag threshold. */
    std::optional<float> ScaleAt(ImVec2 pointer, float dragThreshold) noexcept;

private:
    ImVec2 press_ {};
    ImVec2 pivot_ {};
    float initialRadius_ = 0.0f;
    bool dragging_ = false;
};
} // namespace editor::app

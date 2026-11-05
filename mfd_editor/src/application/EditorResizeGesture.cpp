/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "internal/application/EditorResizeGesture.h"

#include <algorithm>
#include <cmath>

namespace editor::app
{
namespace
{
float Radius(const ImVec2 pointer, const ImVec2 pivot) noexcept
{
    // The same floor on both radii keeps scale neutral even for a press at the pivot.
    return std::max(4.0f, std::hypot(pointer.x - pivot.x, pointer.y - pivot.y));
}
} // namespace

void EditorResizeGesture::Begin(const ImVec2 press, const ImVec2 pivot) noexcept
{
    press_ = press;
    pivot_ = pivot;
    initialRadius_ = Radius(press, pivot);
    dragging_ = false;
}

std::optional<float> EditorResizeGesture::ScaleAt(const ImVec2 pointer, const float dragThreshold) noexcept
{
    if (!dragging_)
    {
        const float travel = std::hypot(pointer.x - press_.x, pointer.y - press_.y);
        if (travel == 0.0f || travel < dragThreshold)
        {
            return std::nullopt;
        }
        dragging_ = true;
    }
    return std::clamp(Radius(pointer, pivot_) / initialRadius_, 0.1f, 10.0f);
}
} // namespace editor::app

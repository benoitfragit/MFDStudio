/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

#include <imgui.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace editor::app
{
/** @brief Position non-interactive Start/End pins in screen space, including coincident endpoints. */
inline std::array<std::optional<ImVec2>, 2> PlaceLineEndpointMarkers(
    ImVec2 min, ImVec2 max, ImVec2 start, ImVec2 end, float radius)
{
    std::array<std::optional<ImVec2>, 2> pins {};
    if (!(radius > 0 && max.x - min.x >= 2 * radius && max.y - min.y >= 2 * radius)) return pins;

    const std::array<ImVec2, 2> anchors {start, end};
    const float direction = start.x <= end.x ? -1.0f : 1.0f;
    for (std::size_t i = 0; i < anchors.size(); ++i)
    {
        const ImVec2 point = anchors[i];
        if (!(point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y)) continue;
        const float dx = (i == 0 ? direction : -direction) * 1.8f * radius;
        const float dy = point.y - min.y < 3 * radius ? 2 * radius : -2 * radius;
        pins[i] = ImVec2(std::clamp(point.x + dx, min.x + radius, max.x - radius),
                         std::clamp(point.y + dy, min.y + radius, max.y - radius));
    }
    if (pins[0] && pins[1] && std::hypot(pins[0]->x - pins[1]->x, pins[0]->y - pins[1]->y) < 2 * radius)
    {
        // At a corner both offsets can clamp to the same side. Separate the pair within the view.
        if (max.x - min.x >= 4 * radius)
        {
            const float mid = std::clamp((pins[0]->x + pins[1]->x) / 2, min.x + 2 * radius, max.x - 2 * radius);
            pins[0]->x = mid + direction * radius;
            pins[1]->x = mid - direction * radius;
        }
        else if (max.y - min.y >= 4 * radius)
        {
            const float mid = std::clamp((pins[0]->y + pins[1]->y) / 2, min.y + 2 * radius, max.y - 2 * radius);
            pins[0]->y = mid - radius;
            pins[1]->y = mid + radius;
        }
    }
    return pins;
}
}

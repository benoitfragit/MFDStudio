/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal helpers keeping text anchors stable on the pixel grid.
 */

#include <cmath>

#include <raylib.h>

namespace mfd::detail
{
/**
 * @brief Snaps one text anchor to the nearest screen pixel.
 *
 * @details Text-like primitives use one logical anchor that can represent the
 * left edge, the visual center, or the right edge depending on the authored
 * alignment. Snapping that anchor directly avoids visible drift when glyph
 * measurements vary slightly while the window is being resized.
 *
 * @param anchorScreenPosition Screen-space anchor passed to `DrawTextPro`.
 * @return Pixel-snapped anchor, or the original value when it is not finite.
 */
inline Vector2 SnapTextAnchor(const Vector2 anchorScreenPosition) noexcept
{
    if (!std::isfinite(anchorScreenPosition.x) || !std::isfinite(anchorScreenPosition.y))
    {
        return anchorScreenPosition;
    }

    return Vector2 {
        std::round(anchorScreenPosition.x),
        std::round(anchorScreenPosition.y)};
}
} // namespace mfd::detail

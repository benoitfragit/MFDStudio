/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal helpers keeping centered text anchors stable on the pixel grid.
 */

#include <cmath>

#include <raylib.h>

namespace mfd::detail
{
/**
 * @brief Snaps one centered text anchor to the nearest screen pixel.
 *
 * @details The title chrome and other centered text primitives use their visual
 * center as the authored logical anchor. Snapping that center directly avoids
 * visible drift when glyph measurements vary slightly while the window is being
 * resized.
 *
 * @param anchorScreenPosition Screen-space center anchor passed to `DrawTextPro`.
 * @return Pixel-snapped center anchor, or the original value when it is not finite.
 */
inline Vector2 SnapCenteredTextAnchor(const Vector2 anchorScreenPosition) noexcept
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

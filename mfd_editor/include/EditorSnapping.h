/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Pure geometry helpers backing editor grid snapping and keyboard nudging.
 */

#include <cmath>

#include "mfd/model/Types.h"

namespace editor::app
{
/**
 * @brief Snaps one scalar to the nearest multiple of a grid step.
 * @param value Scalar to snap.
 * @param gridStep Grid spacing; non-positive or non-finite steps leave the value unchanged.
 * @return The nearest grid-aligned value.
 */
[[nodiscard]] inline float SnapScalarToGrid(const float value, const float gridStep) noexcept
{
    if (!std::isfinite(value) || !std::isfinite(gridStep) || gridStep <= 0.0f)
    {
        return value;
    }

    return std::round(value / gridStep) * gridStep;
}

/**
 * @brief Snaps a logical position to the nearest grid intersection.
 * @param value Logical position to snap.
 * @param gridStep Grid spacing; non-positive steps leave the position unchanged.
 * @return The grid-aligned position.
 */
[[nodiscard]] inline mfd::Vec2 SnapToGrid(const mfd::Vec2 value, const float gridStep) noexcept
{
    return mfd::Vec2 {SnapScalarToGrid(value.x, gridStep), SnapScalarToGrid(value.y, gridStep)};
}

/**
 * @brief Computes the logical translation produced by the arrow keys.
 * @param left Whether the left arrow is active.
 * @param right Whether the right arrow is active.
 * @param up Whether the up arrow is active (logical +y).
 * @param down Whether the down arrow is active (logical -y).
 * @param step Per-press logical distance; non-positive steps produce no movement.
 * @return The combined translation delta.
 */
[[nodiscard]] inline mfd::Vec2 ArrowNudgeDelta(const bool left,
                                               const bool right,
                                               const bool up,
                                               const bool down,
                                               const float step) noexcept
{
    mfd::Vec2 delta {0.0f, 0.0f};
    if (!std::isfinite(step) || step <= 0.0f)
    {
        return delta;
    }

    if (left)
    {
        delta.x -= step;
    }
    if (right)
    {
        delta.x += step;
    }
    if (up)
    {
        delta.y += step;
    }
    if (down)
    {
        delta.y -= step;
    }

    return delta;
}
} // namespace editor::app

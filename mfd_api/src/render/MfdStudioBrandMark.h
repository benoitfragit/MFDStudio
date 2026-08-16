/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Private geometry and raylib drawing helpers for the MFDStudio brand mark.
 */

#include <array>

#include <raylib.h>

namespace mfd::internal
{
/**
 * @brief Resolved screen-space geometry for one MFDStudio mark.
 */
struct MfdStudioBrandMarkGeometry
{
    Rectangle frame {};
    float frameRoundness = 0.0f;
    float monogramStrokeWidth = 0.0f;
    std::array<Vector2, 5> monogramPoints {};
};

/**
 * @brief Builds the centered rounded-frame and monogram geometry for one square mark.
 * @param bounds Maximum screen-space rectangle available to the mark.
 * @return Sanitized geometry centered in `bounds`, or empty geometry for invalid bounds.
 */
[[nodiscard]] MfdStudioBrandMarkGeometry BuildMfdStudioBrandMarkGeometry(Rectangle bounds) noexcept;

/**
 * @brief Draws one flat MFDStudio brand mark using raylib primitives.
 * @param bounds Maximum screen-space rectangle available to the mark.
 * @param surfaceColor Rounded display-frame color.
 * @param monogramColor M-monogram color.
 */
void DrawMfdStudioBrandMark(Rectangle bounds, Color surfaceColor, Color monogramColor) noexcept;
} // namespace mfd::internal

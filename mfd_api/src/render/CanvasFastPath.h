/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal fast-path predicates used by `Canvas2D` and its focused tests.
 */

#include "mfd/model/Reticle.h"

namespace mfd::detail
{
/**
 * @brief Returns whether one ring can use the exact direct circular fast path.
 * @param lineStyle Stroke style of the ring primitive.
 * @param scale World transform scale applied to the ring primitive.
 * @param innerRadiusPixels Inner screen-space radius in pixels.
 * @param outerRadiusPixels Outer screen-space radius in pixels.
 * @return `true` when direct `DrawRing` plus circular stroke calls remain exact.
 */
[[nodiscard]] bool CanUseFastSolidRingPath(LineStyle lineStyle,
                                           const Vec2& scale,
                                           float innerRadiusPixels,
                                           float outerRadiusPixels) noexcept;
} // namespace mfd::detail

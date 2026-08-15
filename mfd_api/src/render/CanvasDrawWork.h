/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal logical-work estimates shared by Canvas2D render paths.
 */

#include <cstddef>
#include <string_view>

namespace mfd::detail
{
/**
 * @brief Estimates the glyph-processing work of one text draw.
 * @param text UTF-8 payload passed to the renderer.
 * @return At least one work unit, proportional to the payload byte count.
 */
[[nodiscard]] std::size_t EstimateTextDrawWorkUnits(std::string_view text) noexcept;

/**
 * @brief Estimates the triangle work required to fill one convex polygon.
 * @param pointCount Number of polygon vertices.
 * @return Triangle count emitted by a fan fill, or zero for a degenerate polygon.
 */
[[nodiscard]] std::size_t EstimateConvexFillWorkUnits(std::size_t pointCount) noexcept;

/**
 * @brief Estimates the segment work required to stroke one closed polygon.
 * @param pointCount Number of polygon vertices.
 * @return Closed segment count, or zero for a degenerate stroke.
 */
[[nodiscard]] std::size_t EstimateClosedStrokeWorkUnits(std::size_t pointCount) noexcept;

/**
 * @brief Estimates the triangle work required to fill one ring band.
 * @param segmentCount Number of segments around the ring.
 * @return Two triangles per segment, saturated on arithmetic overflow.
 */
[[nodiscard]] std::size_t EstimateRingFillWorkUnits(std::size_t segmentCount) noexcept;

/** @brief Returns the minimum logical cost of one direct Canvas draw operation. */
[[nodiscard]] std::size_t EstimateDirectDrawWorkUnits() noexcept;
} // namespace mfd::detail

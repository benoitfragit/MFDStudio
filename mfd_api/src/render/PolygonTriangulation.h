/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal polygon triangulation helpers used by Canvas2D and render tests.
 */

#include <cstddef>
#include <vector>

#include <raylib.h>

#include "mfd/core/ArrayView.h"

namespace mfd::detail
{
/** @brief Maximum geometric tests allowed while triangulating one polygon. */
constexpr std::size_t kMaxTriangulationTests = 131072U;

/**
 * @brief Returns whether every non-degenerate turn of a polygon uses the same orientation.
 * @param points Polygon vertices in order.
 * @return `true` when the polygon is convex.
 */
bool PolygonIsConvex(ArrayView<const Vector2> points) noexcept;

/**
 * @brief Triangulates one simple polygon into triangle indices.
 * @param points Polygon vertices in order.
 * @param triangleIndices Output indices written as consecutive triplets.
 * @return `true` when triangulation succeeded.
 */
bool TriangulateSimplePolygon(ArrayView<const Vector2> points,
                              std::vector<std::size_t>& triangleIndices);

/**
 * @brief Triangulates one polygon with an explicit deterministic work limit.
 * @param points Polygon vertices in order.
 * @param triangleIndices Output indices written as consecutive triplets.
 * @param maximumTests Maximum ear candidates and point-in-triangle tests.
 * @return `true` when triangulation completed before exhausting the budget.
 */
bool TriangulateSimplePolygonWithBudget(ArrayView<const Vector2> points,
                                        std::vector<std::size_t>& triangleIndices,
                                        std::size_t maximumTests);
} // namespace mfd::detail

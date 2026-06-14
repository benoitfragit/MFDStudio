/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal snapping helpers shared by editor viewport interactions.
 */

#include "mfd/model/Reticle.h"

namespace editor::app
{
/**
 * @brief Resolves the next primitive origin while dragging it in the reticle studio.
 * @param reticleTransform Reticle transform applied around the primitive.
 * @param startPrimitive Primitive snapshot captured when the drag started.
 * @param dragStartLogical Mouse logical position captured when the drag started.
 * @param currentMouseLogical Current mouse logical position.
 * @param snapToGrid Enables grid snapping when `true`.
 * @param gridStepLogical Shared logical grid step reused by the visible grid.
 * @return Next primitive origin expressed in reticle-local logical coordinates.
 */
[[nodiscard]] mfd::Vec2 ResolvePrimitiveMovePosition(const mfd::Transform2D& reticleTransform,
                                                     const mfd::Primitive& startPrimitive,
                                                     mfd::Vec2 dragStartLogical,
                                                     mfd::Vec2 currentMouseLogical,
                                                     bool snapToGrid,
                                                     float gridStepLogical) noexcept;

/**
 * @brief Resolves one primitive-local handle point from the current studio mouse position.
 * @param reticle Reticle owning the edited primitive.
 * @param startPrimitive Primitive snapshot captured when the handle drag started.
 * @param currentMouseLogical Current mouse logical position.
 * @param snapToGrid Enables grid snapping when `true`.
 * @param gridStepLogical Shared logical grid step reused by the visible grid.
 * @return Primitive-local point aligned to the shared grid when snapping is enabled.
 */
[[nodiscard]] mfd::Vec2 ResolvePrimitiveHandleLocalPoint(const mfd::ReticleGroup& reticle,
                                                         const mfd::Primitive& startPrimitive,
                                                         mfd::Vec2 currentMouseLogical,
                                                         bool snapToGrid,
                                                         float gridStepLogical) noexcept;
} // namespace editor::app

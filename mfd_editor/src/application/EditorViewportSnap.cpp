/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "internal/application/EditorViewportSnap.h"

/**
 * @file
 * @brief Implementation of the shared editor viewport snapping helpers.
 */

#include <cmath>

#include "EditorSnapping.h"
#include "internal/application/EditorViewportGrid.h"

namespace editor::app
{
namespace
{
mfd::Vec2 InverseTransformPoint(const mfd::Vec2& point, const mfd::Transform2D& transform) noexcept
{
    const mfd::Vec2 translated = point - transform.position;
    const mfd::Vec2 rotated = mfd::Rotate(translated, -transform.rotationDegrees);

    return {
        std::abs(transform.scale.x) <= 0.0001f ? 0.0f : rotated.x / transform.scale.x,
        std::abs(transform.scale.y) <= 0.0001f ? 0.0f : rotated.y / transform.scale.y};
}

mfd::Vec2 SnapWorldPointIfEnabled(const mfd::Vec2 value,
                                  const bool snapToGrid,
                                  const float gridStepLogical) noexcept
{
    return snapToGrid ? editor::app::SnapToGrid(value, SanitizeGridStepLogical(gridStepLogical)) : value;
}
} // namespace

mfd::Vec2 ResolvePrimitiveMovePosition(const mfd::Transform2D& reticleTransform,
                                       const mfd::Primitive& startPrimitive,
                                       const mfd::Vec2 dragStartLogical,
                                       const mfd::Vec2 currentMouseLogical,
                                       const bool snapToGrid,
                                       const float gridStepLogical) noexcept
{
    mfd::Vec2 nextPrimitiveOriginWorld =
        mfd::ApplyTransform(startPrimitive.transform.position, reticleTransform) +
        (currentMouseLogical - dragStartLogical);
    nextPrimitiveOriginWorld = SnapWorldPointIfEnabled(nextPrimitiveOriginWorld, snapToGrid, gridStepLogical);
    return InverseTransformPoint(nextPrimitiveOriginWorld, reticleTransform);
}

mfd::Vec2 ResolvePrimitiveHandleLocalPoint(const mfd::ReticleGroup& reticle,
                                           const mfd::Primitive& startPrimitive,
                                           const mfd::Vec2 currentMouseLogical,
                                           const bool snapToGrid,
                                           const float gridStepLogical) noexcept
{
    const mfd::Vec2 snappedWorldPoint =
        SnapWorldPointIfEnabled(currentMouseLogical, snapToGrid, gridStepLogical);
    return InverseTransformPoint(snappedWorldPoint, mfd::ResolvePrimitiveWorldTransform(startPrimitive, reticle));
}
} // namespace editor::app

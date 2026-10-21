/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/** @file @brief Safe geometry adapter used by the editor container gizmo. */

#include "mfd/apilevel1/ContainerTypes.h"

namespace mfd::api1
{
/** @brief Applies bounded authored-geometry changes collected by the host gizmo. */
class ContainerGizmoAdapter final
{
public:
    /**
     * @brief Scales a container around its authored center.
     * @param initial Geometry captured once when the drag starts.
     * @param minimumSize Per-axis minimum authored size.
     * @param factor Finite positive scale factor from the pointer drag.
     * @param sizeMode Content-fitted axes keep their authored fallback size.
     * @return Valid non-negative geometry, or `initial` for an invalid factor.
     */
    [[nodiscard]] ContainerRect ResizeAroundCenter(ContainerRect initial,
                                                   Vec2 minimumSize,
                                                   float factor,
                                                   ContainerSizeMode sizeMode = ContainerSizeMode::Fixed) const noexcept;
};
} // namespace mfd::api1

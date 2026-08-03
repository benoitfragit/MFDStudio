/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal, allocation-free validation of authored runtime model values.
 */

#include "mfd/model/PageDefinition.h"
#include "mfd/model/RuntimeBudgets.h"

namespace mfd::runtime_validation::internal
{
/** @brief Returns whether a transform stays within all runtime numeric budgets. */
inline bool IsValidTransform(const Transform2D& transform) noexcept
{
    return IsValidVec2(transform.position) &&
           IsFiniteAbsWithin(transform.rotationDegrees, kMaxAbsAngleDegrees) &&
           IsValidScale(transform.scale);
}

/** @brief Returns whether optional reticle style values are safe for runtime use. */
inline bool IsValidReticleOverrides(const ReticleStyleOverride& overrides) noexcept
{
    return !overrides.thickness.has_value() ||
           IsPositiveFiniteWithin(*overrides.thickness, kMaxThickness);
}

/** @brief Returns whether reticle clipping metadata stays within runtime budgets. */
inline bool IsValidReticleClipping(const ReticleClipState& clipping) noexcept
{
    return clipping.primitiveId.size() <= kMaxTextBytes;
}

/** @brief Returns whether one complete reticle is safe for authoring and runtime loading. */
inline bool IsValidReticle(const ReticleGroup& reticle) noexcept
{
    if (!IsValidTransform(reticle.transform) ||
        !IsValidReticleOverrides(reticle.overrides) ||
        !IsValidReticleClipping(reticle.clipping))
    {
        return false;
    }

    for (const Primitive& primitive : reticle.primitives)
    {
        if (!IsValidPrimitiveForRuntime(primitive))
        {
            return false;
        }
    }
    return true;
}

/** @brief Returns whether page view values stay finite and bounded. */
inline bool IsValidPageView(const PageViewState& view) noexcept
{
    return IsValidVec2(view.center) && IsPositiveFiniteWithin(view.zoom, kMaxZoom);
}

/** @brief Returns whether strobe capture geometry stays finite and bounded. */
inline bool IsValidStrobeCapture(const StrobeCaptureConfig& capture) noexcept
{
    return IsFiniteAbsWithin(capture.radius, kMaxLogicalSize) &&
           IsValidVec2(capture.size, kMaxLogicalSize);
}

/** @brief Returns whether strobe magnet values stay finite and bounded. */
inline bool IsValidStrobeMagnet(const StrobeMagnetConfig& magnet) noexcept
{
    return IsFiniteAbsWithin(magnet.radius, kMaxLogicalSize) &&
           std::isfinite(magnet.strength) && magnet.strength >= 0.0f && magnet.strength <= 1.0f &&
           IsPositiveFiniteWithin(magnet.visualShapeSize, kMaxLogicalSize);
}

/** @brief Returns whether one complete page is safe for authoring and runtime loading. */
inline bool IsValidPage(const PageDefinition& page) noexcept
{
    if (!IsValidPageView(page.view))
    {
        return false;
    }
    for (const ReticleGroup& reticle : page.staticReticles)
    {
        if (!IsValidReticle(reticle))
        {
            return false;
        }
    }
    for (const PageStrobeDefinition& strobe : page.strobes)
    {
        if (!IsValidReticle(strobe.reticle) ||
            !IsValidStrobeCapture(strobe.capture) ||
            !IsValidStrobeMagnet(strobe.magnet))
        {
            return false;
        }
    }
    return true;
}
} // namespace mfd::runtime_validation::internal

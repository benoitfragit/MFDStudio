/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Stateless helper preparing primitive copy/paste operations inside the reticle studio.
 */

#include "mfd/model/Reticle.h"

namespace editor
{
/**
 * @brief Immutable input describing one primitive paste request in the reticle studio.
 */
struct PrimitivePasteRequest
{
    /** @brief Target reticle receiving the pasted primitive copy. */
    const mfd::ReticleGroup& targetReticle;
    /** @brief Primitive snapshot previously copied into the studio clipboard. */
    const mfd::Primitive& copiedPrimitive;
    /** @brief Primitive index after which the new copy should be inserted, or `-1` to append. */
    int selectedPrimitiveIndex = -1;
    /** @brief Logical local offset applied to the pasted copy so repeated pastes stay visible. */
    mfd::Vec2 positionOffset {};
};

/**
 * @brief Prepared paste result consumed by the editor shell.
 */
struct PrimitivePastePlan
{
    /** @brief Pasted primitive copy with a collision-free id already assigned. */
    mfd::Primitive primitive {};
    /** @brief Zero-based insertion index inside the target reticle primitive array. */
    int insertionIndex = 0;
};

/**
 * @brief Stateless service keeping primitive copy/paste rules consistent and testable.
 */
class PrimitiveClipboardService
{
public:
    /**
     * @brief Builds one paste plan for a copied primitive inside the target reticle.
     * @param request Immutable paste request describing the clipboard snapshot and target reticle.
     * @return Primitive copy plus insertion index ready to apply to the target reticle.
     */
    [[nodiscard]] PrimitivePastePlan BuildPastePlan(const PrimitivePasteRequest& request) const;
};
} // namespace editor

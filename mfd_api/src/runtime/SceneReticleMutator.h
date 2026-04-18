/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal helpers that validate and apply reticle mutations atomically.
 */

#include "mfd/control/CommandTypes.h"
#include "mfd/model/Reticle.h"

namespace mfd::runtime_internal
{
/**
 * @brief Returns whether one scalar value is finite.
 */
bool IsFinite(float value) noexcept;

/**
 * @brief Returns whether both vector coordinates are finite.
 */
bool IsFinite(const Vec2& value) noexcept;

/**
 * @brief Validates patch constraints against one reticle payload.
 */
bool ValidateReticlePatch(const ReticleGroup& reticle, const ReticlePatch& patch) noexcept;

/**
 * @brief Returns the unique text primitive in a reticle, or `nullptr` when missing/ambiguous.
 */
TextGeometry* FindUniqueTextPrimitive(ReticleGroup& reticle) noexcept;

/**
 * @brief Returns the unique text-like letter spacing slot, or `nullptr` when missing/ambiguous.
 */
float* FindUniqueTextLikeLetterSpacing(ReticleGroup& reticle) noexcept;

/**
 * @brief Applies a reticle patch atomically after validation.
 */
bool ApplyReticlePatchAtomic(ReticleGroup& reticle, const ReticlePatch& patch);
} // namespace mfd::runtime_internal

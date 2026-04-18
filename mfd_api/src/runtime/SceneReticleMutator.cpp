/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of internal reticle mutation helpers.
 */

#include "SceneReticleMutator.h"

#include <cmath>
#include <utility>

namespace mfd::runtime_internal
{
namespace
{
/**
 * @brief Returns a pointer to a text-like primitive letter spacing field.
 */
float* FindTextLikeLetterSpacing(Primitive& primitive) noexcept
{
    if (TextGeometry* geometry = std::get_if<TextGeometry>(&primitive.geometry))
    {
        return &geometry->letterSpacing;
    }

    if (TimeGeometry* geometry = std::get_if<TimeGeometry>(&primitive.geometry))
    {
        return &geometry->letterSpacing;
    }

    return nullptr;
}

/**
 * @brief Returns whether a patch has no mutation payload.
 */
bool IsEmptyPatch(const ReticlePatch& patch) noexcept
{
    return !patch.visible.has_value() &&
           !patch.blinkEnabled.has_value() &&
           !patch.blinkType.has_value() &&
           !patch.position.has_value() &&
           !patch.rotationDegrees.has_value() &&
           !patch.color.has_value() &&
           !patch.thickness.has_value() &&
           !patch.text.has_value() &&
           patch.texts.empty() &&
           !patch.letterSpacing.has_value() &&
           patch.letterSpacings.empty();
}
} // namespace

bool IsFinite(const float value) noexcept
{
    return std::isfinite(value);
}

bool IsFinite(const Vec2& value) noexcept
{
    return IsFinite(value.x) && IsFinite(value.y);
}

bool ValidateReticlePatch(const ReticleGroup& reticle, const ReticlePatch& patch) noexcept
{
    if (patch.position.has_value() && !IsFinite(*patch.position))
    {
        return false;
    }

    if (patch.rotationDegrees.has_value() && !IsFinite(*patch.rotationDegrees))
    {
        return false;
    }

    if (patch.thickness.has_value() && (!IsFinite(*patch.thickness) || *patch.thickness <= 0.0f))
    {
        return false;
    }

    if (patch.letterSpacing.has_value() && !IsFinite(*patch.letterSpacing))
    {
        return false;
    }

    for (const auto& [primitiveId, letterSpacing] : patch.letterSpacings)
    {
        const Primitive* primitive = FindPrimitive(reticle, primitiveId);
        if (primitive == nullptr || !IsFinite(letterSpacing))
        {
            return false;
        }

        const bool isTextLike =
            std::holds_alternative<TextGeometry>(primitive->geometry) ||
            std::holds_alternative<TimeGeometry>(primitive->geometry);
        if (!isTextLike)
        {
            return false;
        }
    }

    for (const auto& [primitiveId, text] : patch.texts)
    {
        (void)text;
        const Primitive* primitive = FindPrimitive(reticle, primitiveId);
        if (primitive == nullptr || !std::holds_alternative<TextGeometry>(primitive->geometry))
        {
            return false;
        }
    }

    return true;
}

TextGeometry* FindUniqueTextPrimitive(ReticleGroup& reticle) noexcept
{
    TextGeometry* selected = nullptr;
    for (auto& primitive : reticle.primitives)
    {
        if (TextGeometry* geometry = std::get_if<TextGeometry>(&primitive.geometry))
        {
            if (selected != nullptr)
            {
                return nullptr;
            }

            selected = geometry;
        }
    }

    return selected;
}

float* FindUniqueTextLikeLetterSpacing(ReticleGroup& reticle) noexcept
{
    float* selected = nullptr;
    for (auto& primitive : reticle.primitives)
    {
        if (float* letterSpacing = FindTextLikeLetterSpacing(primitive))
        {
            if (selected != nullptr)
            {
                return nullptr;
            }

            selected = letterSpacing;
        }
    }

    return selected;
}

bool ApplyReticlePatchAtomic(ReticleGroup& reticle, const ReticlePatch& patch)
{
    if (!ValidateReticlePatch(reticle, patch))
    {
        return false;
    }

    ReticleGroup updatedReticle = reticle;
    bool applied = false;

    if (patch.visible.has_value())
    {
        updatedReticle.visible = *patch.visible;
        applied = true;
    }

    if (patch.position.has_value())
    {
        updatedReticle.transform.position = *patch.position;
        applied = true;
    }

    if (patch.rotationDegrees.has_value())
    {
        updatedReticle.transform.rotationDegrees = *patch.rotationDegrees;
        applied = true;
    }

    if (patch.color.has_value())
    {
        updatedReticle.overrides.color = *patch.color;
        applied = true;
    }

    if (patch.thickness.has_value())
    {
        updatedReticle.overrides.thickness = *patch.thickness;
        applied = true;
    }

    if (patch.text.has_value())
    {
        bool hasTextPrimitive = false;
        for (auto& primitive : updatedReticle.primitives)
        {
            if (TextGeometry* geometry = std::get_if<TextGeometry>(&primitive.geometry))
            {
                geometry->text = *patch.text;
                applied = true;
                hasTextPrimitive = true;
                break;
            }
        }

        if (!hasTextPrimitive)
        {
            return false;
        }
    }

    for (const auto& [primitiveId, text] : patch.texts)
    {
        applied = SetTextPrimitive(updatedReticle, primitiveId, text) || applied;
    }

    if (patch.letterSpacing.has_value())
    {
        bool hasTextLikePrimitive = false;
        for (auto& primitive : updatedReticle.primitives)
        {
            if (float* letterSpacing = FindTextLikeLetterSpacing(primitive))
            {
                *letterSpacing = *patch.letterSpacing;
                applied = true;
                hasTextLikePrimitive = true;
                break;
            }
        }

        if (!hasTextLikePrimitive)
        {
            return false;
        }
    }

    for (const auto& [primitiveId, letterSpacing] : patch.letterSpacings)
    {
        applied = SetTextPrimitiveLetterSpacing(updatedReticle, primitiveId, letterSpacing) || applied;
    }

    if (!applied && !IsEmptyPatch(patch))
    {
        return false;
    }

    reticle = std::move(updatedReticle);
    return true;
}
} // namespace mfd::runtime_internal

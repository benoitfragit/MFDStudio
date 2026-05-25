/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for Reticle.
 */

#include "mfd/model/Reticle.h"

#include <algorithm>
#include <cmath>

namespace mfd
{
namespace
{
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
} // namespace

PrimitiveStyle MergeStyle(const PrimitiveStyle& base, const ReticleStyleOverride& overrides) noexcept
{
    PrimitiveStyle merged = base;

    if (overrides.color.has_value())
    {
        merged.color = *overrides.color;
    }

    if (overrides.thickness.has_value())
    {
        merged.thickness = *overrides.thickness;
    }

    if (overrides.fillColor.has_value())
    {
        merged.fillColor = *overrides.fillColor;
    }

    if (overrides.filled.has_value())
    {
        merged.filled = *overrides.filled;
    }

    return merged;
}

ReticleStyleOverride MergeOverrides(const ReticleStyleOverride& base, const ReticleStyleOverride& overrides) noexcept
{
    ReticleStyleOverride merged = base;

    if (overrides.color.has_value())
    {
        merged.color = overrides.color;
    }

    if (overrides.thickness.has_value())
    {
        merged.thickness = overrides.thickness;
    }

    if (overrides.fillColor.has_value())
    {
        merged.fillColor = overrides.fillColor;
    }

    if (overrides.filled.has_value())
    {
        merged.filled = overrides.filled;
    }

    return merged;
}

ReticleGroup InstantiateReticle(const ReticleGroup& templ,
                                std::string instanceId,
                                const Transform2D transform,
                                const ReticleStyleOverride overrides)
{
    ReticleGroup result = templ;
    result.id = std::move(instanceId);
    result.sourceTemplateId = templ.sourceTemplateId.empty() ? templ.id : templ.sourceTemplateId;
    result.layerId.clear();
    result.transform = CombineTransforms(templ.transform, transform);
    result.overrides = MergeOverrides(templ.overrides, overrides);
    return result;
}

Primitive* FindPrimitive(ReticleGroup& reticle, const std::string_view primitiveId) noexcept
{
    const auto iterator = std::find_if(reticle.primitives.begin(),
                                       reticle.primitives.end(),
                                       [primitiveId](const Primitive& primitive)
                                       {
                                           return primitive.id == primitiveId;
                                       });

    return iterator == reticle.primitives.end() ? nullptr : &(*iterator);
}

const Primitive* FindPrimitive(const ReticleGroup& reticle, const std::string_view primitiveId) noexcept
{
    const auto iterator = std::find_if(reticle.primitives.begin(),
                                       reticle.primitives.end(),
                                       [primitiveId](const Primitive& primitive)
                                       {
                                           return primitive.id == primitiveId;
                                       });

    return iterator == reticle.primitives.end() ? nullptr : &(*iterator);
}

Transform2D ResolvePrimitiveWorldTransform(const Primitive& primitive, const ReticleGroup& reticle) noexcept
{
    Transform2D transform;
    transform.position = ApplyTransform(primitive.transform.position, reticle.transform);
    transform.rotationDegrees = primitive.transform.rotationDegrees;
    if (primitive.reticleRotationSensitive)
    {
        transform.rotationDegrees += reticle.transform.rotationDegrees;
    }

    transform.scale = primitive.transform.scale;
    if (primitive.reticleScaleSensitive)
    {
        transform.scale.x *= reticle.transform.scale.x;
        transform.scale.y *= reticle.transform.scale.y;
    }

    return transform;
}

Vec2 ApplyPrimitiveWorldTransform(const Vec2& point,
                                  const Primitive& primitive,
                                  const ReticleGroup& reticle) noexcept
{
    return ApplyTransform(point, ResolvePrimitiveWorldTransform(primitive, reticle));
}

float PrimitiveAverageScale(const Primitive& primitive, const ReticleGroup& reticle) noexcept
{
    const Transform2D transform = ResolvePrimitiveWorldTransform(primitive, reticle);
    return (std::abs(transform.scale.x) + std::abs(transform.scale.y)) * 0.5f;
}

bool SupportsReticleClipPrimitive(const Primitive& primitive) noexcept
{
    switch (primitive.type)
    {
    case PrimitiveType::Circle:
    case PrimitiveType::Rectangle:
    case PrimitiveType::Ellipse:
    case PrimitiveType::Square:
    case PrimitiveType::Triangle:
        return true;
    default:
        return false;
    }
}

Primitive* ResolveClipPrimitive(ReticleGroup& reticle) noexcept
{
    if (reticle.clipping.mode == ReticleClipMode::None || reticle.clipping.primitiveId.empty())
    {
        return nullptr;
    }

    Primitive* primitive = FindPrimitive(reticle, reticle.clipping.primitiveId);
    return primitive != nullptr && SupportsReticleClipPrimitive(*primitive) ? primitive : nullptr;
}

const Primitive* ResolveClipPrimitive(const ReticleGroup& reticle) noexcept
{
    if (reticle.clipping.mode == ReticleClipMode::None || reticle.clipping.primitiveId.empty())
    {
        return nullptr;
    }

    const Primitive* primitive = FindPrimitive(reticle, reticle.clipping.primitiveId);
    return primitive != nullptr && SupportsReticleClipPrimitive(*primitive) ? primitive : nullptr;
}

bool SetTextPrimitive(ReticleGroup& reticle, const std::string_view primitiveId, std::string value)
{
    Primitive* primitive = FindPrimitive(reticle, primitiveId);
    if (primitive == nullptr)
    {
        return false;
    }

    TextGeometry* geometry = std::get_if<TextGeometry>(&primitive->geometry);
    if (geometry == nullptr)
    {
        return false;
    }

    geometry->text = std::move(value);
    return true;
}

bool SetTextPrimitiveLetterSpacing(ReticleGroup& reticle,
                                   const std::string_view primitiveId,
                                   const float letterSpacing) noexcept
{
    Primitive* primitive = FindPrimitive(reticle, primitiveId);
    if (primitive == nullptr)
    {
        return false;
    }

    float* geometryLetterSpacing = FindTextLikeLetterSpacing(*primitive);
    if (geometryLetterSpacing == nullptr)
    {
        return false;
    }

    *geometryLetterSpacing = letterSpacing;
    return true;
}
} // namespace mfd

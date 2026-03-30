/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "mfd/model/Types.h"

#include <cmath>
#include <numbers>

namespace mfd
{
Vec2 operator+(const Vec2& lhs, const Vec2& rhs) noexcept
{
    return {lhs.x + rhs.x, lhs.y + rhs.y};
}

Vec2 operator-(const Vec2& lhs, const Vec2& rhs) noexcept
{
    return {lhs.x - rhs.x, lhs.y - rhs.y};
}

Vec2 operator*(const Vec2& lhs, const float scalar) noexcept
{
    return {lhs.x * scalar, lhs.y * scalar};
}

Vec2 Scale(const Vec2& value, const Vec2& scale) noexcept
{
    return {value.x * scale.x, value.y * scale.y};
}

Vec2 Rotate(const Vec2& value, const float degrees) noexcept
{
    const float radians = degrees * std::numbers::pi_v<float> / 180.0f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);

    return {
        value.x * cosine - value.y * sine,
        value.x * sine + value.y * cosine};
}

Vec2 ApplyTransform(const Vec2& point, const Transform2D& transform) noexcept
{
    return Rotate(Scale(point, transform.scale), transform.rotationDegrees) + transform.position;
}

Vec2 ApplyPageView(const Vec2& point, const PageViewState& view) noexcept
{
    return (point - view.center) * SanitizeZoom(view.zoom);
}

Transform2D CombineTransforms(const Transform2D& parent, const Transform2D& child) noexcept
{
    Transform2D combined;
    combined.position = ApplyTransform(child.position, parent);
    combined.rotationDegrees = parent.rotationDegrees + child.rotationDegrees;
    combined.scale = {parent.scale.x * child.scale.x, parent.scale.y * child.scale.y};
    return combined;
}

float AverageScale(const Transform2D& parent, const Transform2D& child) noexcept
{
    const float sx = std::abs(parent.scale.x * child.scale.x);
    const float sy = std::abs(parent.scale.y * child.scale.y);
    return (sx + sy) * 0.5f;
}

float SanitizeZoom(const float zoom) noexcept
{
    if (!std::isfinite(zoom) || zoom <= 0.0f)
    {
        return 1.0f;
    }

    return zoom;
}
} // namespace mfd

/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for UserSpaceProjector.
 */

#include "mfd/control/UserSpaceProjector.h"

#include <cmath>
#include <utility>

namespace mfd
{
namespace
{
constexpr float kMinBasisLengthSquared = 1.0e-8f;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kRadiansToDegrees = 180.0f / kPi;

float Dot(const Vec2& lhs, const Vec2& rhs) noexcept
{
    return lhs.x * rhs.x + lhs.y * rhs.y;
}

float LengthSquared(const Vec2& value) noexcept
{
    return Dot(value, value);
}

Vec2 Perpendicular(const Vec2& value) noexcept
{
    return {-value.y, value.x};
}

Vec2 NormalizeOrFallback(const Vec2& value, const Vec2& fallback) noexcept
{
    const float lengthSquared = LengthSquared(value);
    if (!std::isfinite(lengthSquared) || lengthSquared < kMinBasisLengthSquared)
    {
        return fallback;
    }

    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    return {value.x * inverseLength, value.y * inverseLength};
}

float SanitizeScale(const float pageUnitsPerUserUnit) noexcept
{
    if (!std::isfinite(pageUnitsPerUserUnit) || pageUnitsPerUserUnit <= 0.0f)
    {
        return 1.0f;
    }

    return pageUnitsPerUserUnit;
}

float SanitizeRadians(const float radians) noexcept
{
    if (!std::isfinite(radians))
    {
        return 0.0f;
    }

    return radians;
}

Vec2 SanitizeVec2(const Vec2& value) noexcept
{
    return {
        std::isfinite(value.x) ? value.x : 0.0f,
        std::isfinite(value.y) ? value.y : 0.0f};
}

Vec2 RotateRadians(const Vec2& value, const float radians) noexcept
{
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    return {
        value.x * cosine - value.y * sine,
        value.x * sine + value.y * cosine};
}

UserSpaceFrame SanitizeFrame(UserSpaceFrame frame) noexcept
{
    frame.userOrigin = SanitizeVec2(frame.userOrigin);
    frame.pageAnchor = SanitizeVec2(frame.pageAnchor);
    frame.originRotationRadians = SanitizeRadians(frame.originRotationRadians);
    frame.pageUnitsPerUserUnit = SanitizeScale(frame.pageUnitsPerUserUnit);

    const Vec2 sanitizedXAxis = NormalizeOrFallback(frame.userXAxisInPage, {1.0f, 0.0f});
    Vec2 orthogonalizedYAxis = SanitizeVec2(frame.userYAxisInPage);
    orthogonalizedYAxis = orthogonalizedYAxis - sanitizedXAxis * Dot(orthogonalizedYAxis, sanitizedXAxis);
    if (LengthSquared(orthogonalizedYAxis) < kMinBasisLengthSquared)
    {
        orthogonalizedYAxis = Perpendicular(sanitizedXAxis);
    }

    frame.userXAxisInPage = sanitizedXAxis;
    frame.userYAxisInPage = NormalizeOrFallback(orthogonalizedYAxis, {0.0f, 1.0f});
    return frame;
}

Vec2 ToPageVector(const UserSpaceFrame& frame, const Vec2& localVector) noexcept
{
    return (frame.userXAxisInPage * localVector.x + frame.userYAxisInPage * localVector.y) *
           frame.pageUnitsPerUserUnit;
}

} // namespace

UserSpaceProjector::UserSpaceProjector(UserSpaceFrame frame)
    : frame_(std::move(frame))
{
}

const UserSpaceFrame& UserSpaceProjector::Frame() const noexcept
{
    return frame_;
}

void UserSpaceProjector::SetFrame(UserSpaceFrame frame) noexcept
{
    frame_ = std::move(frame);
}

Vec2 UserSpaceProjector::ToPageOffset(const Vec2 userOffset) const noexcept
{
    const UserSpaceFrame frame = SanitizeFrame(frame_);
    const Vec2 rotatedOffset = RotateRadians(SanitizeVec2(userOffset), -frame.originRotationRadians);
    return ToPageVector(frame, rotatedOffset);
}

Vec2 UserSpaceProjector::ToPagePosition(const Vec2 userPosition) const noexcept
{
    const UserSpaceFrame frame = SanitizeFrame(frame_);
    const Vec2 userOffset = SanitizeVec2(userPosition) - frame.userOrigin;
    const Vec2 rotatedOffset = RotateRadians(userOffset, -frame.originRotationRadians);
    return frame.pageAnchor + ToPageVector(frame, rotatedOffset);
}

float UserSpaceProjector::ToPageRotationDegrees(const float userRotationRadians) const noexcept
{
    const UserSpaceFrame frame = SanitizeFrame(frame_);
    const float localRotationRadians = SanitizeRadians(userRotationRadians) - frame.originRotationRadians;
    const Vec2 localDirection {std::cos(localRotationRadians), std::sin(localRotationRadians)};
    const Vec2 pageDirection =
        frame.userXAxisInPage * localDirection.x + frame.userYAxisInPage * localDirection.y;

    if (LengthSquared(pageDirection) < kMinBasisLengthSquared)
    {
        return 0.0f;
    }

    return std::atan2(pageDirection.y, pageDirection.x) * kRadiansToDegrees;
}

Transform2D UserSpaceProjector::ToPageTransform(const Vec2 userPosition,
                                                const float userRotationRadians,
                                                const Vec2 scale) const noexcept
{
    Transform2D transform;
    transform.position = ToPagePosition(userPosition);
    transform.rotationDegrees = ToPageRotationDegrees(userRotationRadians);
    transform.scale = scale;
    return transform;
}
} // namespace mfd

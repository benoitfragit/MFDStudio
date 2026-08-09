/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/** @file @brief Private physical-frame transformations shared by HUD
 * projections. */

#include "HudSpatialTransform.h"

#include <algorithm>
#include <cmath>

namespace hud
{
namespace detail
{
namespace
{
constexpr float kMinimumFpmVelocityMps = 0.10f;

float FiniteOr(const float value, const float fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
}
} // namespace

BodyVector RotateNedToBody(const AircraftInputSample& aircraft, const NedVector& vectorNed) noexcept
{
    const float yaw = FiniteOr(aircraft.yawRad, 0.0f);
    const float pitch = FiniteOr(aircraft.pitchRad, 0.0f);
    const float roll = FiniteOr(aircraft.rollRad, 0.0f);
    const float cy = std::cos(yaw);
    const float sy = std::sin(yaw);
    const float cp = std::cos(pitch);
    const float sp = std::sin(pitch);
    const float cr = std::cos(roll);
    const float sr = std::sin(roll);
    const float north = vectorNed.north;
    const float east = vectorNed.east;
    const float down = vectorNed.down;
    return BodyVector {cy * cp * north + sy * cp * east - sp * down,
                       (cy * sp * sr - sy * cr) * north + (sy * sp * sr + cy * cr) * east + cp * sr * down,
                       (cy * sp * cr + sy * sr) * north + (sy * sp * cr - cy * sr) * east + cp * cr * down};
}

BodyAngularDirection ResolveBodyAngularDirection(const AircraftInputSample& aircraft, const NedVector& vectorNed,
                                                 const float minimumLength) noexcept
{
    BodyAngularDirection direction;
    if (!std::isfinite(aircraft.yawRad) || !std::isfinite(aircraft.pitchRad) || !std::isfinite(aircraft.rollRad) ||
        !std::isfinite(vectorNed.north) || !std::isfinite(vectorNed.east) || !std::isfinite(vectorNed.down))
    {
        return direction;
    }

    const float squaredLength =
        vectorNed.north * vectorNed.north + vectorNed.east * vectorNed.east + vectorNed.down * vectorNed.down;
    const float safeMinimumLength = std::max(FiniteOr(minimumLength, 0.0f), 0.0f);
    if (!std::isfinite(squaredLength) || squaredLength <= safeMinimumLength * safeMinimumLength)
    {
        return direction;
    }

    const BodyVector body = RotateNedToBody(aircraft, vectorNed);
    if (!std::isfinite(body.forward) || !std::isfinite(body.right) || !std::isfinite(body.down) || body.forward <= 0.0f)
    {
        return direction;
    }

    direction.azimuthRad = std::atan2(body.right, body.forward);
    direction.elevationRad = std::atan2(-body.down, std::hypot(body.forward, body.right));
    direction.valid = std::isfinite(direction.azimuthRad) && std::isfinite(direction.elevationRad);
    if (!direction.valid)
    {
        direction.azimuthRad = 0.0f;
        direction.elevationRad = 0.0f;
    }
    return direction;
}

BodyAngularDirection ResolveAircraftVelocityDirection(const AircraftInputSample& aircraft) noexcept
{
    return ResolveBodyAngularDirection(aircraft,
                                       NedVector {aircraft.northSpeedMps, aircraft.eastSpeedMps, aircraft.downSpeedMps},
                                       kMinimumFpmVelocityMps);
}
} // namespace detail
} // namespace hud

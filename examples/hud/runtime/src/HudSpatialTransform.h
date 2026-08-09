/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Private physical-frame transformations shared by HUD projections.
 */

#include "hud/HudTypes.h"

namespace hud
{
namespace detail
{
/** @brief Vector in local North-East-Down axes. */
struct NedVector
{
    float north = 0.0f;
    float east = 0.0f;
    float down = 0.0f;
};

/** @brief Vector in aircraft axes: X-forward, Y-right, Z-down. */
struct BodyVector
{
    float forward = 0.0f;
    float right = 0.0f;
    float down = 0.0f;
};

/** @brief Forward-hemisphere angular direction resolved from a physical vector.
 */
struct BodyAngularDirection
{
    float azimuthRad = 0.0f;
    float elevationRad = 0.0f;
    bool valid = false;
};

/**
 * @brief Rotates a local NED vector into aircraft-body axes.
 * @param aircraft Physical aerospace 3-2-1 attitude from the semantic HUD
 * input.
 * @param vectorNed Vector in North-East-Down axes.
 * @return Vector in X-forward, Y-right, Z-down body axes.
 * @note The rotation is the transpose of `Rz(yaw) * Ry(pitch) * Rx(roll)`.
 */
BodyVector RotateNedToBody(const AircraftInputSample& aircraft, const NedVector& vectorNed) noexcept;

/**
 * @brief Resolves azimuth/elevation for a finite forward-facing NED vector.
 * @param aircraft Physical aerospace 3-2-1 attitude.
 * @param vectorNed Physical vector in NED axes.
 * @param minimumLength Minimum vector magnitude accepted as directional.
 * @return Valid body angles, or `valid == false` for a null, non-finite or rear
 * vector.
 */
BodyAngularDirection ResolveBodyAngularDirection(const AircraftInputSample& aircraft, const NedVector& vectorNed,
                                                 float minimumLength) noexcept;

/**
 * @brief Resolves the aircraft ground-velocity direction for FPM projection.
 * @param aircraft Physical attitude and inertial NED ground velocity.
 * @return Valid body angles, or `valid == false` when no forward FPM exists.
 */
BodyAngularDirection ResolveAircraftVelocityDirection(const AircraftInputSample& aircraft) noexcept;
} // namespace detail
} // namespace hud

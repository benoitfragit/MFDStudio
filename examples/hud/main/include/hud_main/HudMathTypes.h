/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Dependency-free spatial value types used by the HUD sample simulation.
 */

namespace hud_main
{
/**
 * @brief Double-precision vector expressed in either local NED or aircraft-body
 * axes.
 *
 * The owning field or parameter name identifies the frame and unit. Keeping the
 * type free of behavior prevents the simulation math from leaking into the HUD
 * runtime contract.
 */
struct Vec3d
{
    /** First axis component: North or body-forward depending on the owning name.
     */
    double x = 0.0;
    /** Second axis component: East or body-right depending on the owning name. */
    double y = 0.0;
    /** Third axis component: NED-down or body-down depending on the owning name.
     */
    double z = 0.0;
};

/**
 * @brief Hamilton quaternion rotating aircraft-body vectors into local NED
 * axes.
 */
struct Quaterniond
{
    /** Scalar component. */
    double w = 1.0;
    /** Body-X vector component. */
    double x = 0.0;
    /** Body-Y vector component. */
    double y = 0.0;
    /** Body-Z vector component. */
    double z = 0.0;
};

/**
 * @brief Aerospace 3-2-1 Euler representation in radians.
 *
 * The equivalent body-to-NED rotation is `Rz(yaw) * Ry(pitch) * Rx(roll)`.
 * This type is a publication/telemetry representation, never the physical
 * attitude state integrated by the sample simulation.
 */
struct EulerAnglesRad
{
    /** Rotation about NED down, radians. */
    double yaw = 0.0;
    /** Positive nose-up rotation, radians. */
    double pitch = 0.0;
    /** Positive right-wing-down rotation, radians. */
    double roll = 0.0;
};
} // namespace hud_main

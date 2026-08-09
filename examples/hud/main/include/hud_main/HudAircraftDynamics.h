/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Standalone six-degree-of-freedom aircraft model for the HUD sample
 * client.
 *
 * This module belongs exclusively to the replaceable example simulation. It has
 * no dependency on `hud_runtime`, generated HUD objects, ImGui or rendering.
 */

#include "hud_main/HudMathTypes.h"

namespace hud_main
{
/** @brief Normalized pilot intent consumed by the aircraft dynamics. */
struct AircraftControlInput
{
    /** Nose-up pitch command in [-1, 1]. */
    double pitchCommand = 0.0;
    /** Right-wing-down roll command in [-1, 1]. */
    double rollCommand = 0.0;
    /** Nose-right yaw command in [-1, 1]. */
    double yawCommand = 0.0;
    /** Requested dry-thrust ratio in [0, 1]. */
    double throttleRatio = 0.62;
    /** Enables augmented thrust when the throttle is above its documented
     * threshold. */
    bool afterburnerRequested = false;
};

/** @brief Atmosphere and air-mass facts sampled for one fixed dynamics step. */
struct AircraftEnvironmentInput
{
    /** Air-mass velocity in local NED axes, meters per second. */
    Vec3d windVelocityNedMps {};
    /** Air density in kilograms per cubic meter. */
    double airDensityKgPerM3 = 1.225;
    /** Down-positive gravitational acceleration in meters per second squared. */
    double gravityMps2 = 9.80665;
};

/**
 * @brief Fixed physical parameters and documented stability bounds of the
 * sample aircraft.
 *
 * The coefficients deliberately describe a generic demonstration aircraft, not
 * a certified or airframe-identical F-16 model. They are grouped here so
 * another simulation can replace this component without changing the HUD
 * runtime.
 */
struct AircraftDynamicsConfig
{
    /** Constant demonstration-aircraft mass in kilograms. */
    double massKg = 12000.0;
    /** Diagonal roll/pitch/yaw inertia in body axes, kilogram-square-meters. */
    Vec3d inertiaBodyKgM2 {25000.0, 90000.0, 110000.0};
    /** Reference wing area in square meters. */
    double wingAreaSquareMeters = 27.87;
    /** Linear pre-stall lift slope per radian of angle of attack. */
    double liftCurveSlopePerRadian = 4.2;
    /** Lift coefficient at zero angle of attack and neutral pitch command. */
    double zeroAngleLiftCoefficient = 0.08;
    /** Lift-coefficient contribution at full pitch command. */
    double pitchCommandLiftCoefficient = 0.48;
    /** Absolute pre-attenuation lift-coefficient bound. */
    double maximumLiftCoefficient = 1.45;
    /** Angle of attack where progressive stall attenuation begins, radians. */
    double stallAngleRad = 0.3141592653589793;
    /** Angle of attack where the residual stalled-lift fraction is reached,
     * radians. */
    double fullyStalledAngleRad = 0.7853981633974483;
    /** Fraction of bounded lift retained beyond the fully stalled angle. */
    double stalledLiftFraction = 0.28;
    /** Parasite drag coefficient at zero lift and zero angle of attack. */
    double zeroLiftDragCoefficient = 0.022;
    /** Multiplier applied to the squared lift coefficient for induced drag. */
    double inducedDragFactor = 0.060;
    /** Additional drag coefficient scaled by squared sine of angle of attack. */
    double highAngleDragCoefficient = 0.82;
    /** Absolute drag-coefficient bound. */
    double maximumDragCoefficient = 1.35;
    /** Restoring side-force coefficient slope per radian of sideslip. */
    double sideslipForceSlopePerRadian = 0.90;
    /** Side-force coefficient contributed by full yaw command. */
    double yawCommandSideForceCoefficient = 0.16;
    /** Absolute side-force coefficient bound. */
    double maximumSideForceCoefficient = 0.65;
    /** Maximum dry thrust in newtons. */
    double maximumDryThrustNewtons = 80000.0;
    /** Maximum augmented thrust in newtons. */
    double maximumAfterburnerThrustNewtons = 130000.0;
    /** Minimum throttle ratio permitting augmented thrust. */
    double afterburnerThrottleThreshold = 0.90;
    /** Maximum throttle-state change per second. */
    double throttleResponseRatePerSecond = 0.85;
    /** Full roll/pitch/yaw control moments in body axes, newton-meters. */
    Vec3d maximumControlMomentBodyNewtonMeters {100000.0, 60000.0, 65000.0};
    /** Roll/pitch/yaw angular damping in body axes, newton-meter-seconds. */
    Vec3d angularDampingBodyNewtonMeterSeconds {45000.0, 150000.0, 115000.0};
    /** Dynamic pressure where control-moment scaling is unity, pascals. */
    double referenceDynamicPressurePascals = 28000.0;
    /** Residual low-speed control-authority multiplier. */
    double minimumControlAuthorityRatio = 0.15;
    /** High-speed control-authority multiplier bound. */
    double maximumControlAuthorityRatio = 1.50;
    /** Absolute roll/pitch/yaw angular-rate bounds, radians per second. */
    Vec3d maximumAngularVelocityBodyRadPerSecond {2.50, 1.20, 1.20};
    /** Airspeed below which aerodynamic forces are disabled, meters per second.
     */
    double minimumAerodynamicSpeedMps = 0.50;
    /** Airspeed used to bound coefficient-derived force magnitude, meters per
     * second. */
    double maximumAerodynamicSpeedMps = 500.0;
    /** Largest accepted integration step; the bundled fixed tick is 0.02 seconds.
     */
    double maximumIntegrationStepSeconds = 0.05;
    /** Absolute bound applied independently to each local-NED position component.
     */
    double maximumAbsolutePositionMeters = 100000000.0;
    /** Absolute bound on the integrated ground-velocity magnitude. */
    double maximumGroundSpeedMps = 1000.0;
};

/** @brief Continuous physical state owned and integrated by the aircraft model.
 */
struct AircraftDynamicState
{
    /** Absolute position in the reset-local North-East-Down frame, meters. */
    Vec3d positionNedMeters {};
    /** Inertial ground velocity in North-East-Down axes, meters per second. */
    Vec3d velocityNedMps {};
    /** Normalized attitude rotating body vectors into local NED axes. */
    Quaterniond attitudeBodyToNed {};
    /** Roll/pitch/yaw angular velocity in body axes, radians per second. */
    Vec3d angularVelocityBodyRadPerSecond {};
    /** Continuous dry-thrust command response in [0, 1]. */
    double throttleRatio = 0.62;
};

/** @brief Air-data and force results from the most recent dynamics step. */
struct AircraftDynamicsTelemetry
{
    /** Air-relative velocity in body axes, meters per second. */
    Vec3d airVelocityBodyMps {};
    /** Lift, drag and side force in body axes, newtons. */
    Vec3d aerodynamicForceBodyNewtons {};
    /** Complete force including gravity in local NED axes, newtons. */
    Vec3d totalForceNedNewtons {};
    /** Control and damping moment in body axes, newton-meters. */
    Vec3d totalMomentBodyNewtonMeters {};
    /** Air-relative speed magnitude in meters per second. */
    double trueAirspeedMps = 0.0;
    /** Angle of attack in radians, positive when air velocity is body-down of the
     * nose. */
    double angleOfAttackRad = 0.0;
    /** Sideslip angle in radians, positive for body-right air velocity. */
    double sideslipAngleRad = 0.0;
    /** Dynamic pressure in pascals. */
    double dynamicPressurePascals = 0.0;
    /** Bounded lift coefficient used by the latest force evaluation. */
    double liftCoefficient = 0.0;
    /** Bounded drag coefficient used by the latest force evaluation. */
    double dragCoefficient = 0.0;
    /** Body-normal specific force divided by configured gravity. */
    double normalLoadFactor = 1.0;
    /** Body-forward non-gravity acceleration in meters per second squared. */
    double longitudinalAccelerationMps2 = 0.0;
    /** True when augmented thrust was selected for the latest step. */
    bool afterburnerActive = false;
};

/**
 * @brief Owns and advances the replaceable sample aircraft physical state.
 */
class HudAircraftDynamics
{
  public:
    /**
     * @brief Creates a dynamics model with an explicit fixed configuration.
     * @param config Physical coefficients and numerical safety bounds.
     */
    explicit HudAircraftDynamics(AircraftDynamicsConfig config = {}) noexcept;

    /**
     * @brief Replaces the complete physical state.
     * @param state Finite initial state. Invalid components are restored to safe
     * defaults.
     */
    void Reset(const AircraftDynamicState& state) noexcept;

    /**
     * @brief Advances forces, moments, velocity, position and quaternion by one
     * step.
     * @param controls Normalized pilot intent.
     * @param environment Wind, density and gravity facts.
     * @param deltaSeconds Positive fixed time step in seconds.
     */
    void Step(const AircraftControlInput& controls, const AircraftEnvironmentInput& environment,
              double deltaSeconds) noexcept;

    /** @brief Returns the current continuous aircraft state. @return Owned state
     * by const reference. */
    const AircraftDynamicState& State() const noexcept;

    /** @brief Returns air data and loads computed during the last step. @return
     * Latest telemetry by const reference. */
    const AircraftDynamicsTelemetry& Telemetry() const noexcept;

    /** @brief Returns the fixed physical configuration. @return Sanitized
     * configuration by const reference. */
    const AircraftDynamicsConfig& Config() const noexcept;

  private:
    AircraftDynamicsConfig config_ {};
    AircraftDynamicState state_ {};
    AircraftDynamicsTelemetry telemetry_ {};
};

/** @brief Returns whether all vector components are finite. @param value Vector
 * to inspect. @return `true` when finite. */
bool IsFinite(const Vec3d& value) noexcept;

/** @brief Returns whether all quaternion components are finite. @param value
 * Quaternion to inspect. @return `true` when finite. */
bool IsFinite(const Quaterniond& value) noexcept;

/** @brief Returns the Euclidean length of a vector. @param value Vector to
 * measure. @return Length in the vector field's unit. */
double VectorLength(const Vec3d& value) noexcept;

/** @brief Returns a normalized quaternion, or identity for an invalid norm.
 * @param value Quaternion to normalize. @return Unit quaternion. */
Quaterniond NormalizeQuaternion(const Quaterniond& value) noexcept;

/**
 * @brief Builds a body-to-NED quaternion from aerospace 3-2-1 Euler angles.
 * @param angles Rotation `Rz(yaw) * Ry(pitch) * Rx(roll)` in radians.
 * @return Normalized body-to-NED quaternion.
 */
Quaterniond QuaternionFromEuler321(const EulerAnglesRad& angles) noexcept;

/**
 * @brief Derives an equivalent 3-2-1 Euler representation nearest a reference.
 * @param attitude Body-to-NED quaternion.
 * @param reference Previous publication angles used only to choose an
 * equivalent representation.
 * @return Euler angles describing exactly `attitude`; no dynamics depend on
 * this result.
 */
EulerAnglesRad Euler321Nearest(const Quaterniond& attitude, const EulerAnglesRad& reference) noexcept;

/** @brief Rotates a body-frame vector into local NED axes. @param
 * attitudeBodyToNed Normalized physical attitude. @param vectorBody Body
 * vector. @return Equivalent NED vector. */
Vec3d RotateBodyToNed(const Quaterniond& attitudeBodyToNed, const Vec3d& vectorBody) noexcept;

/** @brief Rotates a local-NED vector into aircraft-body axes. @param
 * attitudeBodyToNed Normalized physical attitude. @param vectorNed NED vector.
 * @return Equivalent body vector. */
Vec3d RotateNedToBody(const Quaterniond& attitudeBodyToNed, const Vec3d& vectorNed) noexcept;
} // namespace hud_main

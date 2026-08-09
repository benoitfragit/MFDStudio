/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Six-degree-of-freedom dynamics implementation for the HUD sample
 * client.
 */

#include "hud_main/HudAircraftDynamics.h"

#include <algorithm>
#include <cmath>

namespace hud_main
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;
constexpr double kQuaternionNormEpsilon = 1.0e-12;
constexpr double kVectorNormEpsilon = 1.0e-9;
constexpr double kMinimumMassKg = 1.0;
constexpr double kMinimumInertiaKgM2 = 1.0;
constexpr double kMaximumAirDensityKgPerM3 = 2.0;
constexpr double kMaximumGravityMps2 = 20.0;

double Clamp(const double value, const double low, const double high) noexcept
{
    return std::max(low, std::min(value, high));
}

double FiniteOr(const double value, const double fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
}

Vec3d Add(const Vec3d& first, const Vec3d& second) noexcept
{
    return Vec3d {first.x + second.x, first.y + second.y, first.z + second.z};
}

Vec3d Subtract(const Vec3d& first, const Vec3d& second) noexcept
{
    return Vec3d {first.x - second.x, first.y - second.y, first.z - second.z};
}

Vec3d Scale(const Vec3d& value, const double factor) noexcept
{
    return Vec3d {value.x * factor, value.y * factor, value.z * factor};
}

double Dot(const Vec3d& first, const Vec3d& second) noexcept
{
    return first.x * second.x + first.y * second.y + first.z * second.z;
}

Vec3d Cross(const Vec3d& first, const Vec3d& second) noexcept
{
    return Vec3d {first.y * second.z - first.z * second.y, first.z * second.x - first.x * second.z,
                  first.x * second.y - first.y * second.x};
}

Vec3d NormalizeVectorOr(const Vec3d& value, const Vec3d& fallback) noexcept
{
    const double length = VectorLength(value);
    return length > kVectorNormEpsilon ? Scale(value, 1.0 / length) : fallback;
}

Vec3d LimitVectorLength(const Vec3d& value, const double maximumLength) noexcept
{
    const double length = VectorLength(value);
    if (length <= maximumLength || length <= kVectorNormEpsilon)
    {
        return value;
    }

    return Scale(value, maximumLength / length);
}

Vec3d AerodynamicSideDirection(const Vec3d& airDirectionBody) noexcept
{
    const Vec3d bodyRight {0.0, 1.0, 0.0};
    const Vec3d rightProjection = Subtract(bodyRight, Scale(airDirectionBody, Dot(bodyRight, airDirectionBody)));
    if (VectorLength(rightProjection) > kVectorNormEpsilon)
    {
        return NormalizeVectorOr(rightProjection, bodyRight);
    }

    const Vec3d bodyDown {0.0, 0.0, 1.0};
    return NormalizeVectorOr(Subtract(bodyDown, Scale(airDirectionBody, Dot(bodyDown, airDirectionBody))),
                             Vec3d {1.0, 0.0, 0.0});
}

double Approach(const double current, const double target, const double maximumDelta) noexcept
{
    if (current < target)
    {
        return std::min(current + maximumDelta, target);
    }

    return std::max(current - maximumDelta, target);
}

double NormalizeRadiansPi(double value) noexcept
{
    value = std::fmod(value + kPi, kTwoPi);
    if (value < 0.0)
    {
        value += kTwoPi;
    }

    return value - kPi;
}

double AngularDistanceSquared(const EulerAnglesRad& first, const EulerAnglesRad& second) noexcept
{
    const double yaw = NormalizeRadiansPi(FiniteOr(first.yaw, 0.0) - FiniteOr(second.yaw, 0.0));
    const double pitch = NormalizeRadiansPi(FiniteOr(first.pitch, 0.0) - FiniteOr(second.pitch, 0.0));
    const double roll = NormalizeRadiansPi(FiniteOr(first.roll, 0.0) - FiniteOr(second.roll, 0.0));
    return yaw * yaw + pitch * pitch + roll * roll;
}

Quaterniond Multiply(const Quaterniond& first, const Quaterniond& second) noexcept
{
    return Quaterniond {first.w * second.w - first.x * second.x - first.y * second.y - first.z * second.z,
                        first.w * second.x + first.x * second.w + first.y * second.z - first.z * second.y,
                        first.w * second.y - first.x * second.z + first.y * second.w + first.z * second.x,
                        first.w * second.z + first.x * second.y - first.y * second.x + first.z * second.w};
}

Quaterniond IntegrateBodyAngularVelocity(const Quaterniond& attitude, const Vec3d& angularVelocityBodyRadPerSecond,
                                         const double deltaSeconds) noexcept
{
    const double angularSpeed = VectorLength(angularVelocityBodyRadPerSecond);
    if (angularSpeed <= kVectorNormEpsilon)
    {
        return NormalizeQuaternion(attitude);
    }

    const double halfAngle = 0.5 * angularSpeed * deltaSeconds;
    const double vectorScale = std::sin(halfAngle) / angularSpeed;
    const Quaterniond increment {std::cos(halfAngle), angularVelocityBodyRadPerSecond.x * vectorScale,
                                 angularVelocityBodyRadPerSecond.y * vectorScale,
                                 angularVelocityBodyRadPerSecond.z * vectorScale};
    return NormalizeQuaternion(Multiply(attitude, increment));
}

AircraftDynamicsConfig SanitizeConfig(const AircraftDynamicsConfig& source) noexcept
{
    const AircraftDynamicsConfig defaults {};
    AircraftDynamicsConfig config = source;
    config.massKg = std::max(FiniteOr(config.massKg, defaults.massKg), kMinimumMassKg);
    config.inertiaBodyKgM2.x =
        std::max(FiniteOr(config.inertiaBodyKgM2.x, defaults.inertiaBodyKgM2.x), kMinimumInertiaKgM2);
    config.inertiaBodyKgM2.y =
        std::max(FiniteOr(config.inertiaBodyKgM2.y, defaults.inertiaBodyKgM2.y), kMinimumInertiaKgM2);
    config.inertiaBodyKgM2.z =
        std::max(FiniteOr(config.inertiaBodyKgM2.z, defaults.inertiaBodyKgM2.z), kMinimumInertiaKgM2);
    config.wingAreaSquareMeters = std::max(FiniteOr(config.wingAreaSquareMeters, defaults.wingAreaSquareMeters), 0.0);
    config.liftCurveSlopePerRadian = FiniteOr(config.liftCurveSlopePerRadian, defaults.liftCurveSlopePerRadian);
    config.zeroAngleLiftCoefficient = FiniteOr(config.zeroAngleLiftCoefficient, defaults.zeroAngleLiftCoefficient);
    config.pitchCommandLiftCoefficient =
        FiniteOr(config.pitchCommandLiftCoefficient, defaults.pitchCommandLiftCoefficient);
    config.maximumLiftCoefficient =
        std::max(FiniteOr(config.maximumLiftCoefficient, defaults.maximumLiftCoefficient), 0.01);
    config.maximumDragCoefficient =
        std::max(FiniteOr(config.maximumDragCoefficient, defaults.maximumDragCoefficient), 0.01);
    config.maximumSideForceCoefficient =
        std::max(FiniteOr(config.maximumSideForceCoefficient, defaults.maximumSideForceCoefficient), 0.01);
    config.stallAngleRad = std::max(FiniteOr(config.stallAngleRad, defaults.stallAngleRad), 0.0);
    config.fullyStalledAngleRad =
        std::max(FiniteOr(config.fullyStalledAngleRad, defaults.fullyStalledAngleRad), config.stallAngleRad + 0.001);
    config.stalledLiftFraction = Clamp(FiniteOr(config.stalledLiftFraction, defaults.stalledLiftFraction), 0.0, 1.0);
    config.zeroLiftDragCoefficient =
        std::max(FiniteOr(config.zeroLiftDragCoefficient, defaults.zeroLiftDragCoefficient), 0.0);
    config.inducedDragFactor = std::max(FiniteOr(config.inducedDragFactor, defaults.inducedDragFactor), 0.0);
    config.highAngleDragCoefficient =
        std::max(FiniteOr(config.highAngleDragCoefficient, defaults.highAngleDragCoefficient), 0.0);
    config.sideslipForceSlopePerRadian =
        FiniteOr(config.sideslipForceSlopePerRadian, defaults.sideslipForceSlopePerRadian);
    config.yawCommandSideForceCoefficient =
        FiniteOr(config.yawCommandSideForceCoefficient, defaults.yawCommandSideForceCoefficient);
    config.maximumDryThrustNewtons =
        std::max(FiniteOr(config.maximumDryThrustNewtons, defaults.maximumDryThrustNewtons), 0.0);
    config.maximumAfterburnerThrustNewtons =
        std::max(FiniteOr(config.maximumAfterburnerThrustNewtons, defaults.maximumAfterburnerThrustNewtons),
                 config.maximumDryThrustNewtons);
    config.afterburnerThrottleThreshold =
        Clamp(FiniteOr(config.afterburnerThrottleThreshold, defaults.afterburnerThrottleThreshold), 0.0, 1.0);
    config.throttleResponseRatePerSecond =
        std::max(FiniteOr(config.throttleResponseRatePerSecond, defaults.throttleResponseRatePerSecond), 0.0);
    config.maximumControlMomentBodyNewtonMeters.x = std::max(
        FiniteOr(config.maximumControlMomentBodyNewtonMeters.x, defaults.maximumControlMomentBodyNewtonMeters.x), 0.0);
    config.maximumControlMomentBodyNewtonMeters.y = std::max(
        FiniteOr(config.maximumControlMomentBodyNewtonMeters.y, defaults.maximumControlMomentBodyNewtonMeters.y), 0.0);
    config.maximumControlMomentBodyNewtonMeters.z = std::max(
        FiniteOr(config.maximumControlMomentBodyNewtonMeters.z, defaults.maximumControlMomentBodyNewtonMeters.z), 0.0);
    config.angularDampingBodyNewtonMeterSeconds.x = std::max(
        FiniteOr(config.angularDampingBodyNewtonMeterSeconds.x, defaults.angularDampingBodyNewtonMeterSeconds.x), 0.0);
    config.angularDampingBodyNewtonMeterSeconds.y = std::max(
        FiniteOr(config.angularDampingBodyNewtonMeterSeconds.y, defaults.angularDampingBodyNewtonMeterSeconds.y), 0.0);
    config.angularDampingBodyNewtonMeterSeconds.z = std::max(
        FiniteOr(config.angularDampingBodyNewtonMeterSeconds.z, defaults.angularDampingBodyNewtonMeterSeconds.z), 0.0);
    config.referenceDynamicPressurePascals =
        std::max(FiniteOr(config.referenceDynamicPressurePascals, defaults.referenceDynamicPressurePascals), 1.0);
    config.minimumControlAuthorityRatio =
        std::max(FiniteOr(config.minimumControlAuthorityRatio, defaults.minimumControlAuthorityRatio), 0.0);
    config.maximumControlAuthorityRatio =
        std::max(FiniteOr(config.maximumControlAuthorityRatio, defaults.maximumControlAuthorityRatio),
                 config.minimumControlAuthorityRatio);
    config.maximumAngularVelocityBodyRadPerSecond.x =
        std::max(std::fabs(FiniteOr(config.maximumAngularVelocityBodyRadPerSecond.x,
                                    defaults.maximumAngularVelocityBodyRadPerSecond.x)),
                 0.01);
    config.maximumAngularVelocityBodyRadPerSecond.y =
        std::max(std::fabs(FiniteOr(config.maximumAngularVelocityBodyRadPerSecond.y,
                                    defaults.maximumAngularVelocityBodyRadPerSecond.y)),
                 0.01);
    config.maximumAngularVelocityBodyRadPerSecond.z =
        std::max(std::fabs(FiniteOr(config.maximumAngularVelocityBodyRadPerSecond.z,
                                    defaults.maximumAngularVelocityBodyRadPerSecond.z)),
                 0.01);
    config.minimumAerodynamicSpeedMps =
        std::max(FiniteOr(config.minimumAerodynamicSpeedMps, defaults.minimumAerodynamicSpeedMps), 0.0);
    config.maximumAerodynamicSpeedMps =
        std::max(FiniteOr(config.maximumAerodynamicSpeedMps, defaults.maximumAerodynamicSpeedMps),
                 config.minimumAerodynamicSpeedMps + 1.0);
    config.maximumGroundSpeedMps =
        std::max(FiniteOr(config.maximumGroundSpeedMps, defaults.maximumGroundSpeedMps), 1.0);
    config.maximumIntegrationStepSeconds =
        std::max(FiniteOr(config.maximumIntegrationStepSeconds, defaults.maximumIntegrationStepSeconds), 0.001);
    config.maximumAbsolutePositionMeters =
        std::max(FiniteOr(config.maximumAbsolutePositionMeters, defaults.maximumAbsolutePositionMeters), 1.0);
    return config;
}

double StallAttenuation(const AircraftDynamicsConfig& config, const double absoluteAngleOfAttackRad) noexcept
{
    const double stallAngle = std::max(FiniteOr(config.stallAngleRad, 0.0), 0.0);
    const double fullyStalledAngle = std::max(FiniteOr(config.fullyStalledAngleRad, stallAngle), stallAngle + 0.001);
    if (absoluteAngleOfAttackRad <= stallAngle)
    {
        return 1.0;
    }

    const double progress = Clamp((absoluteAngleOfAttackRad - stallAngle) / (fullyStalledAngle - stallAngle), 0.0, 1.0);
    const double residualLift = Clamp(FiniteOr(config.stalledLiftFraction, 0.0), 0.0, 1.0);
    return 1.0 - progress * (1.0 - residualLift);
}

Vec3d ClampAngularVelocity(const Vec3d& value, const Vec3d& maximum) noexcept
{
    return Vec3d {Clamp(value.x, -std::fabs(maximum.x), std::fabs(maximum.x)),
                  Clamp(value.y, -std::fabs(maximum.y), std::fabs(maximum.y)),
                  Clamp(value.z, -std::fabs(maximum.z), std::fabs(maximum.z))};
}
} // namespace

bool IsFinite(const Vec3d& value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool IsFinite(const Quaterniond& value) noexcept
{
    return std::isfinite(value.w) && std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

double VectorLength(const Vec3d& value) noexcept
{
    return std::hypot(value.x, value.y, value.z);
}

Quaterniond NormalizeQuaternion(const Quaterniond& value) noexcept
{
    if (!IsFinite(value))
    {
        return Quaterniond {};
    }

    const double norm = std::hypot(std::hypot(value.w, value.x), std::hypot(value.y, value.z));
    if (norm <= kQuaternionNormEpsilon)
    {
        return Quaterniond {};
    }

    const double inverseNorm = 1.0 / norm;
    return Quaterniond {value.w * inverseNorm, value.x * inverseNorm, value.y * inverseNorm, value.z * inverseNorm};
}

Quaterniond QuaternionFromEuler321(const EulerAnglesRad& angles) noexcept
{
    const double halfYaw = 0.5 * FiniteOr(angles.yaw, 0.0);
    const double halfPitch = 0.5 * FiniteOr(angles.pitch, 0.0);
    const double halfRoll = 0.5 * FiniteOr(angles.roll, 0.0);
    const double cy = std::cos(halfYaw);
    const double sy = std::sin(halfYaw);
    const double cp = std::cos(halfPitch);
    const double sp = std::sin(halfPitch);
    const double cr = std::cos(halfRoll);
    const double sr = std::sin(halfRoll);
    return NormalizeQuaternion(Quaterniond {cy * cp * cr + sy * sp * sr, cy * cp * sr - sy * sp * cr,
                                            cy * sp * cr + sy * cp * sr, sy * cp * cr - cy * sp * sr});
}

EulerAnglesRad Euler321Nearest(const Quaterniond& attitude, const EulerAnglesRad& reference) noexcept
{
    const Quaterniond q = NormalizeQuaternion(attitude);
    const double r00 = 1.0 - 2.0 * (q.y * q.y + q.z * q.z);
    const double r01 = 2.0 * (q.x * q.y - q.w * q.z);
    const double r10 = 2.0 * (q.x * q.y + q.w * q.z);
    const double r11 = 1.0 - 2.0 * (q.x * q.x + q.z * q.z);
    const double r20 = 2.0 * (q.x * q.z - q.w * q.y);
    const double r21 = 2.0 * (q.y * q.z + q.w * q.x);
    const double r22 = 1.0 - 2.0 * (q.x * q.x + q.y * q.y);

    EulerAnglesRad canonical;
    canonical.pitch = std::asin(Clamp(-r20, -1.0, 1.0));
    const double pitchCosine = std::cos(canonical.pitch);
    if (std::fabs(pitchCosine) > 1.0e-7)
    {
        canonical.yaw = std::atan2(r10, r00);
        canonical.roll = std::atan2(r21, r22);
    }
    else
    {
        canonical.yaw = std::atan2(-r01, r11);
        canonical.roll = 0.0;
    }

    EulerAnglesRad alternate;
    alternate.yaw = NormalizeRadiansPi(canonical.yaw + kPi);
    alternate.pitch = canonical.pitch >= 0.0 ? kPi - canonical.pitch : -kPi - canonical.pitch;
    alternate.roll = NormalizeRadiansPi(canonical.roll + kPi);
    return AngularDistanceSquared(canonical, reference) <= AngularDistanceSquared(alternate, reference) ? canonical
                                                                                                        : alternate;
}

Vec3d RotateBodyToNed(const Quaterniond& attitudeBodyToNed, const Vec3d& vectorBody) noexcept
{
    const Quaterniond q = NormalizeQuaternion(attitudeBodyToNed);
    const Vec3d quaternionVector {q.x, q.y, q.z};
    const Vec3d doubledCross = Scale(Cross(quaternionVector, vectorBody), 2.0);
    return Add(Add(vectorBody, Scale(doubledCross, q.w)), Cross(quaternionVector, doubledCross));
}

Vec3d RotateNedToBody(const Quaterniond& attitudeBodyToNed, const Vec3d& vectorNed) noexcept
{
    const Quaterniond q = NormalizeQuaternion(attitudeBodyToNed);
    return RotateBodyToNed(Quaterniond {q.w, -q.x, -q.y, -q.z}, vectorNed);
}

HudAircraftDynamics::HudAircraftDynamics(AircraftDynamicsConfig config) noexcept : config_(SanitizeConfig(config))
{
    Reset(AircraftDynamicState {});
}

void HudAircraftDynamics::Reset(const AircraftDynamicState& state) noexcept
{
    state_ = state;
    if (!IsFinite(state_.positionNedMeters))
    {
        state_.positionNedMeters = {};
    }
    state_.positionNedMeters.x = Clamp(state_.positionNedMeters.x, -config_.maximumAbsolutePositionMeters,
                                       config_.maximumAbsolutePositionMeters);
    state_.positionNedMeters.y = Clamp(state_.positionNedMeters.y, -config_.maximumAbsolutePositionMeters,
                                       config_.maximumAbsolutePositionMeters);
    state_.positionNedMeters.z = Clamp(state_.positionNedMeters.z, -config_.maximumAbsolutePositionMeters,
                                       config_.maximumAbsolutePositionMeters);
    if (!IsFinite(state_.velocityNedMps))
    {
        state_.velocityNedMps = {};
    }
    state_.velocityNedMps = LimitVectorLength(state_.velocityNedMps, config_.maximumGroundSpeedMps);
    state_.attitudeBodyToNed = NormalizeQuaternion(state_.attitudeBodyToNed);
    if (!IsFinite(state_.angularVelocityBodyRadPerSecond))
    {
        state_.angularVelocityBodyRadPerSecond = {};
    }
    state_.angularVelocityBodyRadPerSecond =
        ClampAngularVelocity(state_.angularVelocityBodyRadPerSecond, config_.maximumAngularVelocityBodyRadPerSecond);
    state_.throttleRatio = Clamp(FiniteOr(state_.throttleRatio, 0.0), 0.0, 1.0);
    telemetry_ = {};
}

void HudAircraftDynamics::Step(const AircraftControlInput& controls, const AircraftEnvironmentInput& environment,
                               const double deltaSeconds) noexcept
{
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0)
    {
        return;
    }
    const double stepSeconds = std::min(deltaSeconds, config_.maximumIntegrationStepSeconds);

    const double pitchCommand = Clamp(FiniteOr(controls.pitchCommand, 0.0), -1.0, 1.0);
    const double rollCommand = Clamp(FiniteOr(controls.rollCommand, 0.0), -1.0, 1.0);
    const double yawCommand = Clamp(FiniteOr(controls.yawCommand, 0.0), -1.0, 1.0);
    const double requestedThrottle = Clamp(FiniteOr(controls.throttleRatio, 0.0), 0.0, 1.0);
    state_.throttleRatio =
        Approach(state_.throttleRatio, requestedThrottle, config_.throttleResponseRatePerSecond * stepSeconds);

    const Vec3d windVelocityNedMps =
        IsFinite(environment.windVelocityNedMps) ? environment.windVelocityNedMps : Vec3d {};
    const double airDensityKgPerM3 =
        Clamp(FiniteOr(environment.airDensityKgPerM3, 0.0), 0.0, kMaximumAirDensityKgPerM3);
    const double gravityMps2 = Clamp(FiniteOr(environment.gravityMps2, 0.0), 0.0, kMaximumGravityMps2);
    const Vec3d airVelocityNedMps = Subtract(state_.velocityNedMps, windVelocityNedMps);
    const Vec3d airVelocityBodyMps = RotateNedToBody(state_.attitudeBodyToNed, airVelocityNedMps);
    const double trueAirspeedMps = VectorLength(airVelocityBodyMps);
    const double aerodynamicSpeedMps = Clamp(trueAirspeedMps, 0.0, config_.maximumAerodynamicSpeedMps);

    double angleOfAttackRad = 0.0;
    double sideslipAngleRad = 0.0;
    Vec3d aerodynamicForceBodyNewtons {};
    double liftCoefficient = 0.0;
    double dragCoefficient = 0.0;
    const double dynamicPressurePascals = 0.5 * airDensityKgPerM3 * aerodynamicSpeedMps * aerodynamicSpeedMps;
    if (trueAirspeedMps >= config_.minimumAerodynamicSpeedMps)
    {
        angleOfAttackRad = std::atan2(airVelocityBodyMps.z, airVelocityBodyMps.x);
        sideslipAngleRad = std::asin(Clamp(airVelocityBodyMps.y / trueAirspeedMps, -1.0, 1.0));
        const double linearLiftCoefficient = FiniteOr(config_.zeroAngleLiftCoefficient, 0.0) +
                                             FiniteOr(config_.liftCurveSlopePerRadian, 0.0) * angleOfAttackRad +
                                             FiniteOr(config_.pitchCommandLiftCoefficient, 0.0) * pitchCommand;
        liftCoefficient = Clamp(linearLiftCoefficient, -config_.maximumLiftCoefficient, config_.maximumLiftCoefficient);
        liftCoefficient *= StallAttenuation(config_, std::fabs(angleOfAttackRad));
        dragCoefficient =
            Clamp(std::max(FiniteOr(config_.zeroLiftDragCoefficient, 0.0), 0.0) +
                      std::max(FiniteOr(config_.inducedDragFactor, 0.0), 0.0) * liftCoefficient * liftCoefficient +
                      std::max(FiniteOr(config_.highAngleDragCoefficient, 0.0), 0.0) * std::sin(angleOfAttackRad) *
                          std::sin(angleOfAttackRad),
                  0.0, config_.maximumDragCoefficient);
        const double sideForceCoefficient =
            Clamp(-FiniteOr(config_.sideslipForceSlopePerRadian, 0.0) * sideslipAngleRad +
                      FiniteOr(config_.yawCommandSideForceCoefficient, 0.0) * yawCommand,
                  -config_.maximumSideForceCoefficient, config_.maximumSideForceCoefficient);

        const Vec3d airDirectionBody = Scale(airVelocityBodyMps, 1.0 / trueAirspeedMps);
        const Vec3d sideDirectionBody = AerodynamicSideDirection(airDirectionBody);
        const Vec3d liftDirectionBody =
            NormalizeVectorOr(Cross(sideDirectionBody, airDirectionBody), Vec3d {0.0, 0.0, -1.0});
        const double forceScale = dynamicPressurePascals * config_.wingAreaSquareMeters;
        aerodynamicForceBodyNewtons = Add(Add(Scale(airDirectionBody, -dragCoefficient * forceScale),
                                              Scale(liftDirectionBody, liftCoefficient * forceScale)),
                                          Scale(sideDirectionBody, sideForceCoefficient * forceScale));
    }

    const bool afterburnerActive =
        controls.afterburnerRequested && state_.throttleRatio > FiniteOr(config_.afterburnerThrottleThreshold, 1.0);
    const double maximumThrustNewtons = afterburnerActive
                                            ? std::max(FiniteOr(config_.maximumAfterburnerThrustNewtons, 0.0), 0.0)
                                            : std::max(FiniteOr(config_.maximumDryThrustNewtons, 0.0), 0.0);
    const Vec3d thrustBodyNewtons {state_.throttleRatio * maximumThrustNewtons, 0.0, 0.0};
    const Vec3d nonGravityForceBodyNewtons = Add(aerodynamicForceBodyNewtons, thrustBodyNewtons);
    const Vec3d nonGravityForceNedNewtons = RotateBodyToNed(state_.attitudeBodyToNed, nonGravityForceBodyNewtons);
    const Vec3d totalForceNedNewtons = Add(nonGravityForceNedNewtons, Vec3d {0.0, 0.0, config_.massKg * gravityMps2});
    const Vec3d accelerationNedMps2 = Scale(totalForceNedNewtons, 1.0 / config_.massKg);

    const double controlAuthority =
        Clamp(dynamicPressurePascals / config_.referenceDynamicPressurePascals,
              FiniteOr(config_.minimumControlAuthorityRatio, 0.0), FiniteOr(config_.maximumControlAuthorityRatio, 1.0));
    const Vec3d controlMomentBodyNewtonMeters {
        config_.maximumControlMomentBodyNewtonMeters.x * rollCommand * controlAuthority,
        config_.maximumControlMomentBodyNewtonMeters.y * pitchCommand * controlAuthority,
        config_.maximumControlMomentBodyNewtonMeters.z * yawCommand * controlAuthority};
    const Vec3d dampingMomentBodyNewtonMeters {
        -config_.angularDampingBodyNewtonMeterSeconds.x * state_.angularVelocityBodyRadPerSecond.x * controlAuthority,
        -config_.angularDampingBodyNewtonMeterSeconds.y * state_.angularVelocityBodyRadPerSecond.y * controlAuthority,
        -config_.angularDampingBodyNewtonMeterSeconds.z * state_.angularVelocityBodyRadPerSecond.z * controlAuthority};
    const Vec3d totalMomentBodyNewtonMeters = Add(controlMomentBodyNewtonMeters, dampingMomentBodyNewtonMeters);
    const Vec3d angularMomentumBody {config_.inertiaBodyKgM2.x * state_.angularVelocityBodyRadPerSecond.x,
                                     config_.inertiaBodyKgM2.y * state_.angularVelocityBodyRadPerSecond.y,
                                     config_.inertiaBodyKgM2.z * state_.angularVelocityBodyRadPerSecond.z};
    const Vec3d gyroscopicMoment = Cross(state_.angularVelocityBodyRadPerSecond, angularMomentumBody);
    const Vec3d angularAccelerationBodyRadPerSecondSquared {
        (totalMomentBodyNewtonMeters.x - gyroscopicMoment.x) / config_.inertiaBodyKgM2.x,
        (totalMomentBodyNewtonMeters.y - gyroscopicMoment.y) / config_.inertiaBodyKgM2.y,
        (totalMomentBodyNewtonMeters.z - gyroscopicMoment.z) / config_.inertiaBodyKgM2.z};

    const AircraftDynamicState previousState = state_;
    state_.angularVelocityBodyRadPerSecond = ClampAngularVelocity(
        Add(state_.angularVelocityBodyRadPerSecond, Scale(angularAccelerationBodyRadPerSecondSquared, stepSeconds)),
        config_.maximumAngularVelocityBodyRadPerSecond);
    state_.attitudeBodyToNed =
        IntegrateBodyAngularVelocity(state_.attitudeBodyToNed, state_.angularVelocityBodyRadPerSecond, stepSeconds);
    state_.velocityNedMps = LimitVectorLength(Add(state_.velocityNedMps, Scale(accelerationNedMps2, stepSeconds)),
                                              config_.maximumGroundSpeedMps);
    state_.positionNedMeters = Add(state_.positionNedMeters, Scale(state_.velocityNedMps, stepSeconds));
    state_.positionNedMeters.x = Clamp(state_.positionNedMeters.x, -config_.maximumAbsolutePositionMeters,
                                       config_.maximumAbsolutePositionMeters);
    state_.positionNedMeters.y = Clamp(state_.positionNedMeters.y, -config_.maximumAbsolutePositionMeters,
                                       config_.maximumAbsolutePositionMeters);
    state_.positionNedMeters.z = Clamp(state_.positionNedMeters.z, -config_.maximumAbsolutePositionMeters,
                                       config_.maximumAbsolutePositionMeters);
    if (!IsFinite(state_.positionNedMeters) || !IsFinite(state_.velocityNedMps) ||
        !IsFinite(state_.attitudeBodyToNed) || !IsFinite(state_.angularVelocityBodyRadPerSecond))
    {
        state_ = previousState;
    }

    telemetry_.airVelocityBodyMps = airVelocityBodyMps;
    telemetry_.aerodynamicForceBodyNewtons = aerodynamicForceBodyNewtons;
    telemetry_.totalForceNedNewtons = totalForceNedNewtons;
    telemetry_.totalMomentBodyNewtonMeters = totalMomentBodyNewtonMeters;
    telemetry_.trueAirspeedMps = trueAirspeedMps;
    telemetry_.angleOfAttackRad = angleOfAttackRad;
    telemetry_.sideslipAngleRad = sideslipAngleRad;
    telemetry_.dynamicPressurePascals = dynamicPressurePascals;
    telemetry_.liftCoefficient = liftCoefficient;
    telemetry_.dragCoefficient = dragCoefficient;
    telemetry_.normalLoadFactor =
        gravityMps2 > kVectorNormEpsilon ? -nonGravityForceBodyNewtons.z / (config_.massKg * gravityMps2) : 0.0;
    telemetry_.longitudinalAccelerationMps2 = nonGravityForceBodyNewtons.x / config_.massKg;
    telemetry_.afterburnerActive = afterburnerActive;
}

const AircraftDynamicState& HudAircraftDynamics::State() const noexcept
{
    return state_;
}

const AircraftDynamicsTelemetry& HudAircraftDynamics::Telemetry() const noexcept
{
    return telemetry_;
}

const AircraftDynamicsConfig& HudAircraftDynamics::Config() const noexcept
{
    return config_;
}
} // namespace hud_main

/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of the deterministic HUD mini-simulation.
 */

#include "HudSimulation.h"

#include <algorithm>
#include <cmath>

namespace hud
{
namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kDegreesToRadians = 0.017453292519943295f;
constexpr float kFeetToMeters = 0.3048f;
constexpr float kKnotsToMetersPerSecond = 0.514444444f;
constexpr float kMetersPerSecondToKnots = 1.94384449f;
constexpr float kNauticalMileToMeters = 1852.0f;
constexpr float kSeaLevelMachMetersPerSecond = 340.294f;
constexpr float kGravityMetersPerSecondSquared = 9.80665f;

/**
 * @brief Named tuning parameters for the demo aircraft mini-simulation.
 *
 * All aircraft dynamics constants live here instead of being scattered as magic
 * numbers inside `HudSimulation::Step`. The defaults describe a generic agile
 * fighter. The model bounds rates, accelerations and energy rather than the
 * trajectory shape: a loop may legitimately go more vertical than horizontal.
 * What keeps the flight path coherent after a maneuver is the damped
 * release-to-trim response, which levels the airframe once the pilot stops
 * commanding pitch or roll.
 *
 * @note This is a demo-only tuning block. A real aircraft adapter fills
 * `HudInputSample` directly and never instantiates this configuration.
 */
struct HudMiniSimulationConfig
{
    /** Pitch-stick command low-pass time constant in seconds. */
    float pitchCommandTimeConstantSeconds = 0.70f;
    /** Roll-stick command low-pass time constant in seconds. */
    float rollCommandTimeConstantSeconds = 0.60f;
    /** Base commanded pitch rate at low speed in degrees per second. */
    float pitchRateBaseDegPerSecond = 48.0f;
    /** Extra commanded pitch rate per knot of true airspeed. */
    float pitchRateSpeedGainDegPerSecondPerKnot = 0.018f;
    /** Commanded roll rate in degrees per second. */
    float rollRateDegPerSecond = 118.0f;
    /** Pitch attitude the airframe relaxes toward once the stick is released. */
    float trimPitchDegrees = 2.0f;
    /** Release-to-trim pitch leveling rate in degrees per second. */
    float pitchTrimRateDegPerSecond = 26.0f;
    /** Release-to-trim roll leveling rate in degrees per second. */
    float rollTrimRateDegPerSecond = 55.0f;
    /**
     * @brief Safety bound on the commanded flight-path slope magnitude, degrees.
     *
     * Steep loops legitimately drive the vertical speed above the horizontal
     * speed, so this is intentionally close to 90 and does not enforce any
     * horizontal-dominance invariant. Its only role is to keep the slope away
     * from the exact +/-90 degree singularity where the velocity reconstruction
     * `cos(slope)` collapses. Recovery is produced by the damped release-to-trim
     * convergence below, not by this bound.
     */
    float flightPathAngleMaxDegrees = 85.0f;
    /** Rate at which the flight path tracks its commanded slope, degrees per second. */
    float flightPathResponseRateDegPerSecond = 38.0f;
    /** Flight-path slope gain applied to the pitch attitude. */
    float flightPathPitchGain = 0.62f;
    /** Flight-path slope contribution of excess throttle, degrees at full range. */
    float flightPathThrottleGainDegrees = 9.0f;
    /** Extra commanded climb slope while afterburner is lit, degrees. */
    float flightPathAfterburnerBonusDegrees = 3.2f;
    /** Slope penalty proportional to bank angle, degrees. */
    float flightPathBankPenaltyDegrees = 4.2f;
    /** Slope penalty proportional to load factor above 1 g, degrees. */
    float flightPathLoadPenaltyDegrees = 0.65f;
    /** Minimum sustained true airspeed in knots. */
    float minSpeedKts = 120.0f;
    /** Maximum sustained true airspeed in knots. */
    float maxSpeedKts = 910.0f;
    /** Thrust acceleration at full military throttle in knots per second. */
    float thrustAccelerationKtsPerSecond = 34.0f;
    /** Additional acceleration while afterburner is active, knots per second. */
    float afterburnerAccelerationKtsPerSecond = 22.0f;
    /** Baseline parasite drag deceleration in knots per second. */
    float dragBaseKtsPerSecond = 8.0f;
    /** Divisor turning airspeed squared into speed-dependent drag. */
    float dragSpeedSquaredDivisor = 72000.0f;
    /** Induced-drag deceleration per g of load factor above 1, knots per second. */
    float dragLoadFactorKtsPerSecond = 2.4f;
    /** Energy cost of climbing at full slope in knots per second. */
    float climbCostKtsPerSecond = 24.0f;
    /** Minimum modeled normal load factor in g. */
    float normalLoadMin = -3.0f;
    /** Maximum modeled normal load factor in g. */
    float normalLoadMax = 9.0f;
    /** Commanded load factor gain per unit of filtered pitch command. */
    float commandedLoadPitchGain = 4.6f;
    /** Commanded load factor gain per unit of bank sine. */
    float commandedLoadBankGain = 1.35f;
    /** Throttle-ratio slew rate toward the commanded throttle, per second. */
    float throttleResponseRatePerSecond = 0.85f;
    /** Throttle ratio above which afterburner may light. */
    float afterburnerThrottleThreshold = 0.90f;
    /** Throttle ratio that produces a level (zero-slope) trim contribution. */
    float trimThrottleRatio = 0.54f;
    /** Stick command magnitude below which release-to-trim leveling engages. */
    float commandNeutralDeadband = 0.15f;
    /** Turn-rate gain per knot of true airspeed, degrees per second. */
    float turnRateGainDegPerSecondPerKnot = 0.030f;
    /** Minimum instantaneous turn-rate scale in degrees per second. */
    float turnRateMinDegPerSecond = 5.0f;
    /** Maximum instantaneous turn-rate scale in degrees per second. */
    float turnRateMaxDegPerSecond = 24.0f;
};

constexpr HudMiniSimulationConfig kMiniSimConfig {};

float Clamp(const float value, const float low, const float high) noexcept
{
    return std::max(low, std::min(value, high));
}

float FiniteOr(const float value, const float fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
}

float Approach(const float current, const float target, const float maxDelta) noexcept
{
    if (current < target)
    {
        return std::min(current + maxDelta, target);
    }

    return std::max(current - maxDelta, target);
}

float SmoothCommand(const float current, const float target, const float deltaSeconds, const float timeConstantSeconds) noexcept
{
    const float response = 1.0f - std::exp(-deltaSeconds / std::max(timeConstantSeconds, 0.001f));
    return current + (target - current) * Clamp(response, 0.0f, 1.0f);
}

// Fraction of release-to-trim leveling to apply for a given raw stick command.
// A held command (even a partial one) keeps its commanded attitude; only a
// near-neutral stick engages leveling, and only within a small deadband.
float ReleaseToTrimFactor(const float rawCommand, const float deadband) noexcept
{
    return Clamp(1.0f - std::fabs(rawCommand) / std::max(deadband, 0.001f), 0.0f, 1.0f);
}

float WrapRadiansTwoPi(float value) noexcept
{
    value = std::fmod(value, kTwoPi);
    if (value < 0.0f)
    {
        value += kTwoPi;
    }

    return value;
}

float NormalizeRadiansPi(float value) noexcept
{
    value = std::fmod(value + kPi, kTwoPi);
    if (value < 0.0f)
    {
        value += kTwoPi;
    }

    return value - kPi;
}

// Moves an angle toward a target along the shortest arc, capped by `maxDeltaRad`.
// Used by the release-to-trim logic so the airframe levels out through the
// nearest direction even when it is inverted at the top of a loop.
float ApproachAngleShortestPath(const float currentRad, const float targetRad, const float maxDeltaRad) noexcept
{
    const float delta = NormalizeRadiansPi(targetRad - currentRad);
    const float step = Clamp(delta, -maxDeltaRad, maxDeltaRad);
    return NormalizeRadiansPi(currentRad + step);
}

float SanitizeDeltaSeconds(const float deltaSeconds) noexcept
{
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f)
    {
        return 0.0f;
    }

    return Clamp(deltaSeconds, 0.0f, 0.080f);
}

float HorizontalSpeedMetersPerSecond(const AircraftInputSample& aircraft) noexcept
{
    const float north = FiniteOr(aircraft.northSpeedMps, 0.0f);
    const float east = FiniteOr(aircraft.eastSpeedMps, 0.0f);
    return std::sqrt(north * north + east * east);
}

float TrueSpeedMetersPerSecond(const AircraftInputSample& aircraft) noexcept
{
    const float horizontal = HorizontalSpeedMetersPerSecond(aircraft);
    const float down = FiniteOr(aircraft.downSpeedMps, 0.0f);
    return std::sqrt(horizontal * horizontal + down * down);
}

float FlightPathSlopeRadians(const AircraftInputSample& aircraft) noexcept
{
    const float horizontal = HorizontalSpeedMetersPerSecond(aircraft);
    if (horizontal < 0.01f)
    {
        return 0.0f;
    }

    return std::atan2(-FiniteOr(aircraft.downSpeedMps, 0.0f), horizontal);
}

float TerrainElevationMeters(const float elapsedSeconds, const float headingRad) noexcept
{
    return 128.0f +
           36.5f * std::sin(elapsedSeconds * 0.031f + headingRad) +
           22.8f * std::sin(elapsedSeconds * 0.017f);
}

// Mutable inventory slot accessor used when a launch consumes a round. The
// missile speed, time-of-flight and DLZ tuning live with the demo profiles in
// `HudProjection.cpp`; this simulation reuses `hud::ComputeMissileTimeOfFlight`
// rather than duplicating those constants.
int& InventorySlot(MissileInventory& inventory, const MissileType type) noexcept
{
    return type == MissileType::Aim120C ? inventory.aim120c : inventory.aim9m;
}

bool IsWeaponArmedForHud(const WeaponInputSample& weapon) noexcept
{
    return weapon.masterArm || weapon.simulateMode;
}

bool IsAirToAirMissileMode(const WeaponInputSample& weapon) noexcept
{
    if (weapon.masterMode != HudMasterMode::AirToAir)
    {
        return false;
    }

    return weapon.weaponMode == HudWeaponMode::AirToAirMissile || weapon.weaponMode == HudWeaponMode::None;
}

void SyncWeaponInputFromMissileShots(HudInputSample& inputs, const std::vector<MissileShot>& missileShots) noexcept
{
    if (missileShots.empty())
    {
        inputs.weapon.missileInFlight = false;
        inputs.weapon.activeMissileTimeRemainingSeconds = 0.0f;
        inputs.weapon.activeMissilePhase = MissileFlightPhase::Impact;
        return;
    }

    const MissileShot& shot = missileShots.back();
    inputs.weapon.missileInFlight = shot.timeRemainingSeconds > 0.0f || shot.phase == MissileFlightPhase::Impact;
    inputs.weapon.activeMissileTimeRemainingSeconds = shot.timeRemainingSeconds;
    inputs.weapon.activeMissilePhase = shot.phase;
}
} // namespace

HudSimulation::HudSimulation()
{
    Reset();
}

void HudSimulation::Reset() noexcept
{
    inputs_ = {};
    controls_ = {};
    filteredPitchCommand_ = 0.0f;
    filteredRollCommand_ = 0.0f;
    missileShots_.clear();
}

void HudSimulation::SetControls(const PilotControls& controls) noexcept
{
    controls_ = controls;
    controls_.pitchCommand = Clamp(controls_.pitchCommand, -1.0f, 1.0f);
    controls_.rollCommand = Clamp(controls_.rollCommand, -1.0f, 1.0f);
    controls_.throttle = Clamp(controls_.throttle, 0.0f, 1.0f);
    inputs_.weapon.masterMode = controls_.masterMode;
    inputs_.weapon.weaponMode = controls_.weaponMode;
    inputs_.weapon.gunMode = controls_.gunMode;
    inputs_.weapon.masterArm = controls_.masterArm;
    inputs_.weapon.simulateMode = controls_.simulateMode;
    inputs_.weapon.triggerHeld = controls_.triggerHeld;
    inputs_.weapon.targetLocked = controls_.targetLocked;
    inputs_.approach.landingGearDown = controls_.landingGearDown;
    inputs_.approach.landingModeActive = controls_.landingModeActive;
    inputs_.approach.landingDeclutterActive = controls_.landingDeclutterActive;
    inputs_.ils.powered = controls_.ilsPowered;
    inputs_.ils.selected = controls_.ilsSelected;
    inputs_.ils.signalValid = controls_.ilsSignalValid;
    inputs_.ils.commandSteeringActive = controls_.ilsCommandSteeringActive;
}

void HudSimulation::Step(const float deltaSeconds) noexcept
{
    const float dt = SanitizeDeltaSeconds(deltaSeconds);
    if (dt <= 0.0f)
    {
        return;
    }

    const HudMiniSimulationConfig& cfg = kMiniSimConfig;
    AircraftInputSample& aircraft = inputs_.aircraft;
    aircraft.elapsedSeconds += dt;
    filteredPitchCommand_ =
        SmoothCommand(filteredPitchCommand_, controls_.pitchCommand, dt, cfg.pitchCommandTimeConstantSeconds);
    filteredRollCommand_ =
        SmoothCommand(filteredRollCommand_, controls_.rollCommand, dt, cfg.rollCommandTimeConstantSeconds);

    const float speedKts = TrueSpeedMetersPerSecond(aircraft) * kMetersPerSecondToKnots;
    const float pitchRateDegPerSecond =
        cfg.pitchRateBaseDegPerSecond + speedKts * cfg.pitchRateSpeedGainDegPerSecondPerKnot;
    aircraft.pitchRad = NormalizeRadiansPi(
        aircraft.pitchRad + filteredPitchCommand_ * pitchRateDegPerSecond * kDegreesToRadians * dt);
    aircraft.rollRad =
        NormalizeRadiansPi(aircraft.rollRad + filteredRollCommand_ * cfg.rollRateDegPerSecond * kDegreesToRadians * dt);

    // Release-to-trim: once the stick command relaxes, the airframe levels back
    // toward its trim attitude instead of holding an extreme pitch/roll forever.
    // This is what stops a completed loop from leaving a dominant vertical speed.
    const float pitchReleaseFactor = ReleaseToTrimFactor(controls_.pitchCommand, cfg.commandNeutralDeadband);
    aircraft.pitchRad = ApproachAngleShortestPath(
        aircraft.pitchRad,
        cfg.trimPitchDegrees * kDegreesToRadians,
        pitchReleaseFactor * cfg.pitchTrimRateDegPerSecond * kDegreesToRadians * dt);
    const float rollReleaseFactor = ReleaseToTrimFactor(controls_.rollCommand, cfg.commandNeutralDeadband);
    aircraft.rollRad = ApproachAngleShortestPath(
        aircraft.rollRad,
        0.0f,
        rollReleaseFactor * cfg.rollTrimRateDegPerSecond * kDegreesToRadians * dt);

    aircraft.throttleRatio = Approach(aircraft.throttleRatio, controls_.throttle, cfg.throttleResponseRatePerSecond * dt);
    aircraft.afterburnerActive =
        controls_.afterburnerRequested && aircraft.throttleRatio > cfg.afterburnerThrottleThreshold;

    const float turnRateRadPerSecond =
        std::sin(aircraft.rollRad) *
        Clamp(speedKts * cfg.turnRateGainDegPerSecondPerKnot, cfg.turnRateMinDegPerSecond, cfg.turnRateMaxDegPerSecond) *
        kDegreesToRadians *
        std::cos(aircraft.pitchRad);
    aircraft.headingRad = WrapRadiansTwoPi(aircraft.headingRad + turnRateRadPerSecond * dt);
    aircraft.yawRad = aircraft.headingRad;

    const float commandedLoad =
        1.0f + filteredPitchCommand_ * cfg.commandedLoadPitchGain +
        std::fabs(std::sin(aircraft.rollRad)) * cfg.commandedLoadBankGain;
    const float attitudeLoad = std::cos(aircraft.rollRad) * std::cos(aircraft.pitchRad);
    aircraft.normalLoadFactor = Clamp(attitudeLoad + commandedLoad - 1.0f, cfg.normalLoadMin, cfg.normalLoadMax);

    // The flight-path slope may become steep during a loop (the vertical speed is
    // then allowed to exceed the horizontal one, as in real aerobatics). It is
    // only bounded away from the +/-90 degree reconstruction singularity. What
    // fixed the post-loop FPM saturation is the damped release-to-trim above,
    // which converges the attitude - and therefore the slope - back toward level
    // once the pitch command goes neutral; the slope is never forced small here.
    const float flightPathAngleMaxRad = cfg.flightPathAngleMaxDegrees * kDegreesToRadians;
    const float targetFlightPathSlopeRad = Clamp(
        NormalizeRadiansPi(aircraft.pitchRad) * cfg.flightPathPitchGain +
            (aircraft.throttleRatio - cfg.trimThrottleRatio) * cfg.flightPathThrottleGainDegrees * kDegreesToRadians +
            (aircraft.afterburnerActive ? cfg.flightPathAfterburnerBonusDegrees * kDegreesToRadians : 0.0f) -
            std::fabs(std::sin(aircraft.rollRad)) * cfg.flightPathBankPenaltyDegrees * kDegreesToRadians -
            std::max(aircraft.normalLoadFactor - 1.0f, 0.0f) * cfg.flightPathLoadPenaltyDegrees * kDegreesToRadians,
        -flightPathAngleMaxRad,
        flightPathAngleMaxRad);
    const float flightPathSlopeRad = Approach(
        FlightPathSlopeRadians(aircraft),
        targetFlightPathSlopeRad,
        cfg.flightPathResponseRateDegPerSecond * kDegreesToRadians * dt);

    const float thrustAccelerationKtsPerSecond =
        aircraft.throttleRatio * cfg.thrustAccelerationKtsPerSecond +
        (aircraft.afterburnerActive ? cfg.afterburnerAccelerationKtsPerSecond : 0.0f);
    const float dragAccelerationKtsPerSecond =
        cfg.dragBaseKtsPerSecond + speedKts * speedKts / cfg.dragSpeedSquaredDivisor +
        std::max(aircraft.normalLoadFactor - 1.0f, 0.0f) * cfg.dragLoadFactorKtsPerSecond;
    const float climbCostKtsPerSecond = std::max(0.0f, std::sin(flightPathSlopeRad)) * cfg.climbCostKtsPerSecond;
    const float accelerationMps2 =
        (thrustAccelerationKtsPerSecond - dragAccelerationKtsPerSecond - climbCostKtsPerSecond) *
        kKnotsToMetersPerSecond;
    const float newSpeedMps = Clamp(
        TrueSpeedMetersPerSecond(aircraft) + accelerationMps2 * dt,
        cfg.minSpeedKts * kKnotsToMetersPerSecond,
        cfg.maxSpeedKts * kKnotsToMetersPerSecond);

    const float horizontalSpeedMps = std::max(newSpeedMps * std::cos(flightPathSlopeRad), 0.0f);
    aircraft.northSpeedMps = horizontalSpeedMps * std::cos(aircraft.headingRad);
    aircraft.eastSpeedMps = horizontalSpeedMps * std::sin(aircraft.headingRad);
    aircraft.downSpeedMps = -newSpeedMps * std::sin(flightPathSlopeRad);
    aircraft.altitudeMeters = std::max(18.0f, aircraft.altitudeMeters - aircraft.downSpeedMps * dt);
    aircraft.radioAltitudeMeters =
        std::max(0.0f, aircraft.altitudeMeters - TerrainElevationMeters(aircraft.elapsedSeconds, aircraft.headingRad));
    aircraft.mach = newSpeedMps / kSeaLevelMachMetersPerSecond;
    aircraft.specificEnergyRateMps = -aircraft.downSpeedMps + newSpeedMps * accelerationMps2 / kGravityMetersPerSecondSquared;

    TargetInputSample& target = inputs_.target;
    target.azimuthRad = 7.0f * kDegreesToRadians * std::sin(aircraft.elapsedSeconds * 0.33f);
    target.elevationRad = 3.5f * kDegreesToRadians * std::sin(aircraft.elapsedSeconds * 0.27f + 0.60f);
    target.aspectRad = WrapRadiansTwoPi((125.0f + 34.0f * std::sin(aircraft.elapsedSeconds * 0.19f)) * kDegreesToRadians);
    target.closingSpeedMps =
        360.0f * kKnotsToMetersPerSecond +
        (newSpeedMps - 360.0f * kKnotsToMetersPerSecond) * 0.55f -
        90.0f * kKnotsToMetersPerSecond * std::sin(aircraft.elapsedSeconds * 0.21f);
    target.rangeMeters -= target.closingSpeedMps * dt;
    if (target.rangeMeters < 1.3f * kNauticalMileToMeters)
    {
        target.rangeMeters = 1.3f * kNauticalMileToMeters;
        target.closingSpeedMps = -240.0f * kKnotsToMetersPerSecond;
    }
    else if (target.rangeMeters > 42.0f * kNauticalMileToMeters)
    {
        target.rangeMeters = 42.0f * kNauticalMileToMeters;
    }
    target.altitudeMeters = aircraft.altitudeMeters + 5200.0f * kFeetToMeters *
                                                        std::sin(aircraft.elapsedSeconds * 0.11f + 1.2f);

    AirGroundInputSample& airGround = inputs_.airGround;
    airGround.valid =
        inputs_.weapon.masterMode == HudMasterMode::AirToGround &&
        (inputs_.weapon.weaponMode == HudWeaponMode::AirToGroundCcip ||
         inputs_.weapon.weaponMode == HudWeaponMode::AirToGroundStrafe);
    const float terrainMeters = std::max(aircraft.altitudeMeters - aircraft.radioAltitudeMeters, 0.0f);
    const float heightAboveTargetMeters = std::max(aircraft.altitudeMeters - terrainMeters, 25.0f);
    const float diveAngleRad = Clamp(-FlightPathSlopeRadians(aircraft), 4.0f * kDegreesToRadians, 45.0f * kDegreesToRadians);
    airGround.slantRangeMeters =
        Clamp(heightAboveTargetMeters / std::max(std::sin(diveAngleRad), 0.10f), 250.0f, 7200.0f);
    airGround.pipperAzimuthRad = 0.018f * std::sin(aircraft.elapsedSeconds * 0.23f + aircraft.headingRad);
    airGround.pipperDepressionRad =
        Clamp(diveAngleRad * 0.58f + airGround.slantRangeMeters / 7200.0f * 0.055f, 0.018f, 0.145f);
    airGround.fallLineAzimuthRad = airGround.pipperAzimuthRad * 0.45f;
    airGround.solutionCueValid = inputs_.weapon.weaponMode == HudWeaponMode::AirToGroundCcip;
    airGround.solutionCueDepressionRad = Clamp(airGround.pipperDepressionRad * 0.52f, 0.010f, 0.085f);
    airGround.pullupAnticipationCueValid =
        inputs_.weapon.weaponMode == HudWeaponMode::AirToGroundCcip && aircraft.radioAltitudeMeters < 900.0f;
    airGround.pullupAnticipationCueDepressionRad = Clamp(airGround.pipperDepressionRad * 0.24f, 0.006f, 0.040f);
    airGround.timeToReleaseSeconds =
        inputs_.weapon.weaponMode == HudWeaponMode::AirToGroundCcip
            ? Clamp(airGround.slantRangeMeters / std::max(TrueSpeedMetersPerSecond(aircraft), 1.0f), 0.0f, 99.0f)
            : 0.0f;
    airGround.timeToGoSeconds = airGround.timeToReleaseSeconds > 0.0f
        ? Clamp(airGround.timeToReleaseSeconds + 42.0f, 0.0f, 999.0f)
        : 0.0f;

    inputs_.approach.runwayReferenceValid = inputs_.approach.landingModeActive || inputs_.approach.landingGearDown;
    inputs_.approach.runwayHeadingRad = WrapRadiansTwoPi(aircraft.headingRad + 1.5f * kDegreesToRadians);
    inputs_.approach.runwayDistanceMeters =
        inputs_.approach.runwayReferenceValid ? Clamp(aircraft.radioAltitudeMeters * 6.0f, 450.0f, 30000.0f) : -1.0f;
    inputs_.approach.runwayElevationMeters = terrainMeters;
    if (inputs_.approach.weightOnWheels)
    {
        inputs_.approach.landingDeclutterActive = false;
    }

    if (inputs_.ils.powered && inputs_.ils.selected)
    {
        const float localizerErrorRad = NormalizeRadiansPi(aircraft.headingRad - inputs_.approach.runwayHeadingRad);
        inputs_.ils.localizerDeviationDots = Clamp(localizerErrorRad / (2.5f * kDegreesToRadians), -3.0f, 3.0f);
        const float desiredGlideSlopeRad = -3.0f * kDegreesToRadians;
        inputs_.ils.glideslopeDeviationDots =
            Clamp((FlightPathSlopeRadians(aircraft) - desiredGlideSlopeRad) / (2.5f * kDegreesToRadians), -3.0f, 3.0f);
        inputs_.ils.courseRad = inputs_.approach.runwayHeadingRad;
    }

    for (MissileShot& shot : missileShots_)
    {
        shot.timeRemainingSeconds -= dt;
        if (shot.timeRemainingSeconds <= 0.0f)
        {
            shot.phase = MissileFlightPhase::Impact;
        }
        else
        {
            const float progress = 1.0f - shot.timeRemainingSeconds / std::max(shot.timeOfFlightSeconds, 0.1f);
            if (progress > 0.82f)
            {
                shot.phase = MissileFlightPhase::Terminal;
            }
            else if (progress > 0.55f)
            {
                shot.phase = MissileFlightPhase::Active;
            }
            else if (progress > 0.18f)
            {
                shot.phase = MissileFlightPhase::Midcourse;
            }
            else
            {
                shot.phase = MissileFlightPhase::Boost;
            }
        }
    }

    missileShots_.erase(
        std::remove_if(
            missileShots_.begin(),
            missileShots_.end(),
            [](const MissileShot& shot)
            {
                return shot.phase == MissileFlightPhase::Impact && shot.timeRemainingSeconds <= -8.0f;
            }),
        missileShots_.end());
    SyncWeaponInputFromMissileShots(inputs_, missileShots_);
}

void HudSimulation::SelectMissile(const MissileType type) noexcept
{
    inputs_.weapon.selectedMissile = type;
}

void HudSimulation::CycleSelectedMissile() noexcept
{
    inputs_.weapon.selectedMissile =
        inputs_.weapon.selectedMissile == MissileType::Aim120C ? MissileType::Aim9M : MissileType::Aim120C;
}

bool HudSimulation::FireSelectedMissile() noexcept
{
    if (!IsAirToAirMissileMode(inputs_.weapon) || !IsWeaponArmedForHud(inputs_.weapon) || !inputs_.target.valid)
    {
        return false;
    }

    int& selectedInventory = InventorySlot(inputs_.weapon.inventory, inputs_.weapon.selectedMissile);
    if (selectedInventory <= 0)
    {
        return false;
    }

    const LaunchZone launchZone =
        ComputeLaunchZone(inputs_.aircraft, inputs_.target, inputs_.weapon.selectedMissile);
    const TargetState target = BuildTargetStateForHud(inputs_.target);
    if (launchZone.tooClose || target.rangeNm > launchZone.rmax1Nm * 1.08f)
    {
        return false;
    }

    --selectedInventory;
    MissileShot shot;
    shot.type = inputs_.weapon.selectedMissile;
    shot.timeOfFlightSeconds =
        ComputeMissileTimeOfFlight(inputs_.aircraft, inputs_.target, inputs_.weapon.selectedMissile);
    shot.timeRemainingSeconds = shot.timeOfFlightSeconds;
    shot.launchRangeNm = target.rangeNm;
    shot.phase = MissileFlightPhase::Boost;
    missileShots_.push_back(shot);
    SyncWeaponInputFromMissileShots(inputs_, missileShots_);
    return true;
}

HudFrame HudSimulation::BuildHudFrame() const noexcept
{
    return hud::BuildHudFrame(inputs_);
}

const HudInputSample& HudSimulation::Inputs() const noexcept
{
    return inputs_;
}

AircraftState HudSimulation::Aircraft() const noexcept
{
    return BuildAircraftStateForHud(inputs_.aircraft);
}

TargetState HudSimulation::Target() const noexcept
{
    return BuildTargetStateForHud(inputs_.target);
}

const MissileInventory& HudSimulation::Inventory() const noexcept
{
    return inputs_.weapon.inventory;
}

MissileType HudSimulation::SelectedMissile() const noexcept
{
    return inputs_.weapon.selectedMissile;
}

const std::vector<MissileShot>& HudSimulation::MissileShots() const noexcept
{
    return missileShots_;
}

HudMasterMode HudSimulation::MasterMode() const noexcept
{
    return inputs_.weapon.masterMode;
}
} // namespace hud

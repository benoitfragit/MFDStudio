/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of the deterministic Demo HUD mini-simulation.
 */

#include "DemoHudSimulation.h"

#include <algorithm>
#include <cmath>

namespace demo_hud
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
constexpr float kPitchCommandTimeConstantSeconds = 0.70f;
constexpr float kRollCommandTimeConstantSeconds = 0.60f;

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

float MissileSpeedMetersPerSecond(const MissileType type) noexcept
{
    return type == MissileType::Aim120C ? 1209.0f : 849.0f;
}

int& InventorySlot(MissileInventory& inventory, const MissileType type) noexcept
{
    return type == MissileType::Aim120C ? inventory.aim120c : inventory.aim9m;
}

float ComputeMissileTimeOfFlight(const AircraftInputSample& aircraft,
                                 const TargetInputSample& target,
                                 const MissileType selectedMissile) noexcept
{
    const float closingSpeedMps = std::max(FiniteOr(target.closingSpeedMps, 0.0f), -77.0f);
    const float ownshipSpeedMps = TrueSpeedMetersPerSecond(aircraft);
    const float effectiveSpeedMps =
        MissileSpeedMetersPerSecond(selectedMissile) + closingSpeedMps * 0.40f + ownshipSpeedMps * 0.25f;
    const float seconds = FiniteOr(target.rangeMeters, 0.0f) / std::max(effectiveSpeedMps, 154.0f);
    return Clamp(seconds, 3.0f, selectedMissile == MissileType::Aim120C ? 68.0f : 28.0f);
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

DemoHudSimulation::DemoHudSimulation()
{
    Reset();
}

void DemoHudSimulation::Reset() noexcept
{
    inputs_ = {};
    controls_ = {};
    filteredPitchCommand_ = 0.0f;
    filteredRollCommand_ = 0.0f;
    missileShots_.clear();
}

void DemoHudSimulation::SetControls(const PilotControls& controls) noexcept
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

void DemoHudSimulation::Step(const float deltaSeconds) noexcept
{
    const float dt = SanitizeDeltaSeconds(deltaSeconds);
    if (dt <= 0.0f)
    {
        return;
    }

    AircraftInputSample& aircraft = inputs_.aircraft;
    aircraft.elapsedSeconds += dt;
    filteredPitchCommand_ =
        SmoothCommand(filteredPitchCommand_, controls_.pitchCommand, dt, kPitchCommandTimeConstantSeconds);
    filteredRollCommand_ =
        SmoothCommand(filteredRollCommand_, controls_.rollCommand, dt, kRollCommandTimeConstantSeconds);

    const float speedKts = TrueSpeedMetersPerSecond(aircraft) * kMetersPerSecondToKnots;
    aircraft.pitchRad = NormalizeRadiansPi(
        aircraft.pitchRad + filteredPitchCommand_ * (48.0f + speedKts * 0.018f) * kDegreesToRadians * dt);
    aircraft.rollRad = NormalizeRadiansPi(aircraft.rollRad + filteredRollCommand_ * 118.0f * kDegreesToRadians * dt);
    aircraft.throttleRatio = Approach(aircraft.throttleRatio, controls_.throttle, 0.85f * dt);
    aircraft.afterburnerActive = controls_.afterburnerRequested && aircraft.throttleRatio > 0.90f;

    const float turnRateRadPerSecond =
        std::sin(aircraft.rollRad) *
        Clamp(speedKts * 0.030f, 5.0f, 24.0f) *
        kDegreesToRadians *
        std::cos(aircraft.pitchRad);
    aircraft.headingRad = WrapRadiansTwoPi(aircraft.headingRad + turnRateRadPerSecond * dt);
    aircraft.yawRad = aircraft.headingRad;

    const float commandedLoad =
        1.0f + filteredPitchCommand_ * 4.6f + std::fabs(std::sin(aircraft.rollRad)) * 1.35f;
    const float attitudeLoad = std::cos(aircraft.rollRad) * std::cos(aircraft.pitchRad);
    aircraft.normalLoadFactor = Clamp(attitudeLoad + commandedLoad - 1.0f, -3.0f, 9.0f);

    const float targetFlightPathSlopeRad = Clamp(
        NormalizeRadiansPi(aircraft.pitchRad) * 0.62f +
            (aircraft.throttleRatio - 0.54f) * 9.0f * kDegreesToRadians +
            (aircraft.afterburnerActive ? 3.2f * kDegreesToRadians : 0.0f) -
            std::fabs(std::sin(aircraft.rollRad)) * 4.2f * kDegreesToRadians -
            std::max(aircraft.normalLoadFactor - 1.0f, 0.0f) * 0.65f * kDegreesToRadians,
        -82.0f * kDegreesToRadians,
        82.0f * kDegreesToRadians);
    const float flightPathSlopeRad =
        Approach(FlightPathSlopeRadians(aircraft), targetFlightPathSlopeRad, 32.0f * kDegreesToRadians * dt);

    const float thrustAccelerationKtsPerSecond =
        aircraft.throttleRatio * 34.0f + (aircraft.afterburnerActive ? 22.0f : 0.0f);
    const float dragAccelerationKtsPerSecond =
        8.0f + speedKts * speedKts / 72000.0f +
        std::max(aircraft.normalLoadFactor - 1.0f, 0.0f) * 2.4f;
    const float climbCostKtsPerSecond = std::max(0.0f, std::sin(flightPathSlopeRad)) * 24.0f;
    const float accelerationMps2 =
        (thrustAccelerationKtsPerSecond - dragAccelerationKtsPerSecond - climbCostKtsPerSecond) *
        kKnotsToMetersPerSecond;
    const float newSpeedMps = Clamp(
        TrueSpeedMetersPerSecond(aircraft) + accelerationMps2 * dt,
        120.0f * kKnotsToMetersPerSecond,
        910.0f * kKnotsToMetersPerSecond);

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

void DemoHudSimulation::SelectMissile(const MissileType type) noexcept
{
    inputs_.weapon.selectedMissile = type;
}

void DemoHudSimulation::CycleSelectedMissile() noexcept
{
    inputs_.weapon.selectedMissile =
        inputs_.weapon.selectedMissile == MissileType::Aim120C ? MissileType::Aim9M : MissileType::Aim120C;
}

bool DemoHudSimulation::FireSelectedMissile() noexcept
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

HudFrame DemoHudSimulation::BuildHudFrame() const noexcept
{
    return demo_hud::BuildHudFrame(inputs_);
}

const HudInputSample& DemoHudSimulation::Inputs() const noexcept
{
    return inputs_;
}

AircraftState DemoHudSimulation::Aircraft() const noexcept
{
    return BuildAircraftStateForHud(inputs_.aircraft);
}

TargetState DemoHudSimulation::Target() const noexcept
{
    return BuildTargetStateForHud(inputs_.target);
}

const MissileInventory& DemoHudSimulation::Inventory() const noexcept
{
    return inputs_.weapon.inventory;
}

MissileType DemoHudSimulation::SelectedMissile() const noexcept
{
    return inputs_.weapon.selectedMissile;
}

const std::vector<MissileShot>& DemoHudSimulation::MissileShots() const noexcept
{
    return missileShots_;
}

HudMasterMode DemoHudSimulation::MasterMode() const noexcept
{
    return inputs_.weapon.masterMode;
}
} // namespace demo_hud

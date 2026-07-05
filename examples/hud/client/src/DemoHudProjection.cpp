/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Stateless Demo HUD projection and EEGS funnel implementation.
 */

#include "DemoHudProjection.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace demo_hud
{
namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegreesToRadians = 0.017453292519943295f;
constexpr float kRadiansToDegrees = 57.29577951308232f;
constexpr float kFeetToMeters = 0.3048f;
constexpr float kMetersToFeet = 3.280839895013123f;
constexpr float kMetersPerSecondToKnots = 1.94384449f;
constexpr float kNauticalMileToMeters = 1852.0f;
constexpr float kMetersToNauticalMiles = 1.0f / kNauticalMileToMeters;
constexpr float kMetersPerSecondToFeetPerMinute = 196.850394f;
constexpr float kSeaLevelMachMetersPerSecond = 340.294f;
constexpr float kHudUnitsPerMil = 0.0056f;
constexpr float kDefaultTargetWingspanMeters = 35.0f * kFeetToMeters;
constexpr float kPitchToHudUnits = 0.034f;
constexpr float kHudHorizonLimit = 0.58f;
constexpr float kDlzBottomY = -0.245f;
constexpr float kDlzHeight = 0.49f;
constexpr float kEegsFunnelFarY = 0.205f;
constexpr float kEegsFunnelNearY = 0.765f;
constexpr float kEegsFunnelFarRangeMeters = 1700.0f;
constexpr float kEegsFunnelNearRangeMeters = 450.0f;
constexpr float kEegsFunnelReferenceRangeMeters = 900.0f;
constexpr float kEegsFunnelReferenceHalfWidth = 0.082f;
constexpr float kEegsFunnelFarHalfWidth = 0.038f;
constexpr float kEegsFunnelNearHalfWidth = 0.124f;

float Clamp(const float value, const float low, const float high) noexcept
{
    return std::max(low, std::min(value, high));
}

float FiniteOr(const float value, const float fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
}

float WrapDegrees360(float value) noexcept
{
    value = std::fmod(value, 360.0f);
    if (value < 0.0f)
    {
        value += 360.0f;
    }

    return value;
}

float NormalizeDegrees180(float value) noexcept
{
    value = std::fmod(value + 180.0f, 360.0f);
    if (value < 0.0f)
    {
        value += 360.0f;
    }

    return value - 180.0f;
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

HudVec2 RotateHudVector(const HudVec2 offset, const float degrees) noexcept
{
    const float radians = degrees * kDegreesToRadians;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    return HudVec2 {
        offset.x * cosine - offset.y * sine,
        offset.x * sine + offset.y * cosine};
}

float SmoothStep(const float value) noexcept
{
    const float t = Clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float FunnelSampleRangeMeters(const float travel) noexcept
{
    const float rangeTravel = std::pow(Clamp(travel, 0.0f, 1.0f), 1.18f);
    return kEegsFunnelFarRangeMeters +
           (kEegsFunnelNearRangeMeters - kEegsFunnelFarRangeMeters) * rangeTravel;
}

float FunnelSampleHalfWidth(const float travel) noexcept
{
    const float referenceAngle =
        std::atan((kDefaultTargetWingspanMeters * 0.5f) / kEegsFunnelReferenceRangeMeters);
    const float sampleAngle =
        std::atan((kDefaultTargetWingspanMeters * 0.5f) / FunnelSampleRangeMeters(travel));

    return Clamp(
        kEegsFunnelReferenceHalfWidth * sampleAngle / std::max(referenceAngle, 0.0001f),
        kEegsFunnelFarHalfWidth,
        kEegsFunnelNearHalfWidth);
}

HudVec2 BuildFunnelSpinePoint(const float travel,
                              const float heightScale,
                              const HudVec2 center,
                              const float curvature,
                              const float skew) noexcept
{
    const float screenTravel = Clamp(travel, 0.0f, 1.0f);
    const float shapedTravel = SmoothStep(screenTravel);
    const float leadBend = curvature * screenTravel * screenTravel * 0.52f;

    return HudVec2 {
        center.x + skew * (0.20f + 0.80f * shapedTravel) + leadBend * 0.55f,
        center.y + kEegsFunnelFarY + (kEegsFunnelNearY - kEegsFunnelFarY) * screenTravel * heightScale +
            leadBend};
}

/**
 * @brief Builds one base range sample of an EEGS funnel rail.
 * @param side Rail side, negative for left and positive for right.
 * @param travel Normalized range travel, 0 at the far/narrow end and 1 at the
 * near/wider end.
 * @param widthScale Runtime range and target-wingspan width correction.
 * @param heightScale Runtime airspeed and load-factor vertical stretch.
 * @param center HUD-space drift of the funnel spine.
 * @param curvature Load and speed driven lead curvature.
 * @param skew Lateral lead offset shared by both rails.
 * @return HUD-space wall point after range width and lead correction.
 *
 * The wall is built around a central spine, then offset by the angular
 * half-wingspan for the sample range. This follows the same principle as
 * practical EEGS implementations: the target should fit between the walls at
 * the corresponding range instead of seeing a decorative V shape.
 */
HudVec2 BuildFunnelWallPoint(const float side,
                             const float travel,
                             const float widthScale,
                             const float heightScale,
                             const HudVec2 center,
                             const float curvature,
                             const float skew) noexcept
{
    const HudVec2 firstSpine = BuildFunnelSpinePoint(0.0f, heightScale, center, curvature, skew);
    const HudVec2 lastSpine = BuildFunnelSpinePoint(1.0f, heightScale, center, curvature, skew);
    const float tangentX = lastSpine.x - firstSpine.x;
    const float tangentY = lastSpine.y - firstSpine.y;
    const float tangentLength = std::sqrt(tangentX * tangentX + tangentY * tangentY);
    const float perpendicularX = tangentLength > 0.0001f ? tangentY / tangentLength : 1.0f;
    const float perpendicularY = tangentLength > 0.0001f ? -tangentX / tangentLength : 0.0f;

    const float halfWidth = FunnelSampleHalfWidth(travel) * widthScale;
    const HudVec2 spine = BuildFunnelSpinePoint(travel, heightScale, center, curvature, skew);
    return HudVec2 {spine.x + side * perpendicularX * halfWidth, spine.y + side * perpendicularY * halfWidth};
}

HudFunnelControlPoints BuildEegsFunnelRail(const float side,
                                           const float widthScale,
                                           const float heightScale,
                                           const HudVec2 center,
                                           const float curvature,
                                           const float skew) noexcept
{
    HudFunnelControlPoints rail {};
    for (std::size_t index = 0; index < rail.size(); ++index)
    {
        const float travel = static_cast<float>(index) / static_cast<float>(rail.size() - 1U);
        rail[index] = BuildFunnelWallPoint(
            side,
            travel,
            widthScale,
            heightScale,
            center,
            curvature,
            skew);
    }

    return rail;
}

float MissileSpeedMetersPerSecond(const MissileType type) noexcept
{
    return type == MissileType::Aim120C ? 1209.0f : 849.0f;
}

int InventoryCount(const MissileInventory& inventory, const MissileType type) noexcept
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

HudAttitudeFrame ResolveHudAttitude(const AircraftInputSample& aircraftInput) noexcept
{
    const AircraftState aircraft = BuildAircraftStateForHud(aircraftInput);
    HudAttitudeFrame frame;

    float displayPitch = NormalizeDegrees180(aircraft.pitchDegrees);
    float displayRoll = NormalizeDegrees180(aircraft.rollDegrees);
    if (displayPitch > 90.0f)
    {
        displayPitch = 180.0f - displayPitch;
        displayRoll = NormalizeDegrees180(displayRoll + 180.0f);
        frame.inverted = true;
    }
    else if (displayPitch < -90.0f)
    {
        displayPitch = -180.0f - displayPitch;
        displayRoll = NormalizeDegrees180(displayRoll + 180.0f);
        frame.inverted = true;
    }

    const float horizonRawY = -displayPitch * kPitchToHudUnits;
    const float horizonY = Clamp(horizonRawY, -kHudHorizonLimit, kHudHorizonLimit);
    const float rollRotationDegrees = -displayRoll;
    frame.displayPitchDegrees = displayPitch;
    frame.displayRollDegrees = displayRoll;
    frame.ladderPosition = RotateHudVector(HudVec2 {0.0f, horizonRawY}, rollRotationDegrees);
    frame.ladderRotationDegrees = rollRotationDegrees;
    frame.trueHorizonPosition = RotateHudVector(HudVec2 {0.0f, horizonY}, rollRotationDegrees);
    frame.trueHorizonRotationDegrees = rollRotationDegrees;
    frame.trueHorizonVisible = std::fabs(horizonRawY) <= kHudHorizonLimit;
    frame.ghostHorizonVisible = !frame.trueHorizonVisible;
    const float ghostHorizonY = horizonRawY < 0.0f ? -kHudHorizonLimit : kHudHorizonLimit;
    frame.ghostHorizonPosition = RotateHudVector(HudVec2 {0.0f, ghostHorizonY}, rollRotationDegrees);
    frame.ghostHorizonRotationDegrees = rollRotationDegrees;

    const float fpmRawX = std::sin(displayRoll * kDegreesToRadians) * 0.08f;
    const float fpmRawY = (aircraft.flightPathAngleDegrees - displayPitch) * kPitchToHudUnits;
    frame.fpmPosition = HudVec2 {Clamp(fpmRawX, -0.54f, 0.54f), Clamp(fpmRawY, -0.52f, 0.52f)};
    frame.fpmLimited = std::fabs(fpmRawX) > 0.54f || std::fabs(fpmRawY) > 0.52f;
    frame.bankAngleIndicatorPosition = frame.fpmPosition;
    frame.bankAngleIndicatorRotationDegrees = -displayRoll;
    frame.zenithVisible = displayPitch > 68.0f;
    frame.zenithPosition = RotateHudVector(HudVec2 {0.0f, 0.48f}, rollRotationDegrees);
    frame.nadirVisible = displayPitch < -68.0f;
    frame.nadirPosition = RotateHudVector(HudVec2 {0.0f, -0.48f}, rollRotationDegrees);
    frame.levelRecoveryVisible =
        frame.inverted ||
        (aircraft.radarAltitudeFeet < 1500.0f && aircraft.verticalSpeedFpm < -2500.0f);

    return frame;
}

bool IsRadarAltitudeVisible(const AircraftState& aircraft, const HudAttitudeFrame& attitude) noexcept
{
    if (aircraft.radarAltitudeFeet <= 0.0f)
    {
        return false;
    }

    const float absPitch = std::fabs(attitude.displayPitchDegrees);
    const float absRoll = std::fabs(attitude.displayRollDegrees);
    if (aircraft.radarAltitudeFeet < 5000.0f)
    {
        return absPitch <= 30.0f && absRoll <= 90.0f;
    }

    return absPitch <= 10.0f && absRoll <= 75.0f;
}

HudAirDataFrame BuildAirDataFrame(const AircraftInputSample& aircraftInput, const HudAttitudeFrame& attitude) noexcept
{
    const AircraftState aircraft = BuildAircraftStateForHud(aircraftInput);
    HudAirDataFrame frame;
    frame.radarAltitudeVisible = IsRadarAltitudeVisible(aircraft, attitude);
    frame.verticalVelocityCueY = Clamp(aircraft.verticalSpeedFpm / 6000.0f * 0.40f, -0.40f, 0.40f);

    const float energyCueOffset = Clamp(std::fabs(aircraft.energyRate) * 0.028f, 0.075f, 0.155f);
    frame.energyChevronUpVisible = aircraft.energyRate > 0.45f;
    frame.energyChevronDownVisible = aircraft.energyRate < -0.45f;
    frame.energyChevronUpPosition =
        HudVec2 {attitude.fpmPosition.x, Clamp(attitude.fpmPosition.y + energyCueOffset, -0.50f, 0.56f)};
    frame.energyChevronDownPosition =
        HudVec2 {attitude.fpmPosition.x, Clamp(attitude.fpmPosition.y - energyCueOffset, -0.56f, 0.50f)};
    return frame;
}

HudVec2 ProjectTargetToHud(const TargetInputSample& target) noexcept
{
    return HudVec2 {
        Clamp(FiniteOr(target.azimuthRad, 0.0f) / (18.0f * kDegreesToRadians) * 0.46f, -0.52f, 0.52f),
        Clamp(FiniteOr(target.elevationRad, 0.0f) / (14.0f * kDegreesToRadians) * 0.36f, -0.40f, 0.42f)};
}

HudVec2 ProjectAngularOffsetToHud(const float azimuthRad, const float elevationRad) noexcept
{
    return HudVec2 {
        Clamp(FiniteOr(azimuthRad, 0.0f) / (18.0f * kDegreesToRadians) * 0.46f, -0.52f, 0.52f),
        Clamp(FiniteOr(elevationRad, 0.0f) / (14.0f * kDegreesToRadians) * 0.36f, -0.50f, 0.50f)};
}

float DefaultStrafeInRangeFeet(const HudAmmoType ammoType) noexcept
{
    return ammoType == HudAmmoType::M56 ? 4000.0f : 12000.0f;
}

float EffectiveStrafeInRangeFeet(const WeaponInputSample& weapon) noexcept
{
    const float configuredRange = FiniteOr(weapon.strafeInRangeFeet, 0.0f);
    return configuredRange > 0.0f ? configuredRange : DefaultStrafeInRangeFeet(weapon.ammoType);
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

HudWeaponFrame BuildWeaponFrame(const HudInputSample& input) noexcept
{
    HudWeaponFrame frame;
    frame.airToAirVisible = IsAirToAirMissileMode(input.weapon) && IsWeaponArmedForHud(input.weapon);
    frame.targetVisible = frame.airToAirVisible && input.target.valid;
    frame.targetPosition = ProjectTargetToHud(input.target);
    frame.missileDiamondPosition = frame.targetPosition;
    frame.missileDiamondScale = input.weapon.selectedMissile == MissileType::Aim120C ? 0.92f : 1.15f;
    frame.attackSteeringCueVisible = frame.targetVisible;
    frame.attackSteeringCuePosition = HudVec2 {
        Clamp(frame.targetPosition.x * 0.55f, -0.30f, 0.30f),
        Clamp(frame.targetPosition.y * 0.45f - 0.04f, -0.26f, 0.26f)};
    frame.launchZone = ComputeLaunchZone(input.aircraft, input.target, input.weapon.selectedMissile);
    frame.dynamicLaunchZoneVisible = frame.targetVisible;
    frame.rangeCueVisible = frame.targetVisible;
    const TargetState target = BuildTargetStateForHud(input.target);
    const float rangeFactor = Clamp(target.rangeNm / std::max(frame.launchZone.rmax1Nm, 0.1f), 0.0f, 1.0f);
    frame.rangeCuePosition = HudVec2 {0.0f, kDlzBottomY + rangeFactor * kDlzHeight};
    frame.breakXVisible = frame.airToAirVisible && input.target.valid && frame.launchZone.tooClose;
    frame.missileCircleVisible = frame.airToAirVisible;
    frame.selectedMissileTimeOfFlightSeconds =
        ComputeMissileTimeOfFlight(input.aircraft, input.target, input.weapon.selectedMissile);

    frame.missileInFlight = input.weapon.missileInFlight;
    frame.activeMissileTimeRemainingSeconds = input.weapon.activeMissileTimeRemainingSeconds;
    frame.activeMissilePhase = input.weapon.activeMissilePhase;

    if (InventoryCount(input.weapon.inventory, input.weapon.selectedMissile) <= 0)
    {
        frame.rangeCueVisible = false;
    }

    return frame;
}

HudGunFrame BuildGunFrame(const HudInputSample& input, const HudAttitudeFrame& attitude) noexcept
{
    HudGunFrame frame;
    const AircraftState aircraft = BuildAircraftStateForHud(input.aircraft);
    const bool armed = IsWeaponArmedForHud(input.weapon);
    const bool airToAirGun =
        input.weapon.masterMode == HudMasterMode::AirToAir &&
        input.weapon.weaponMode == HudWeaponMode::AirToAirGun &&
        input.weapon.gunMode == HudGunMode::Eegs &&
        armed &&
        input.weapon.gunRoundsRemaining > 0;
    const bool strafeGun =
        input.weapon.masterMode == HudMasterMode::AirToGround &&
        input.weapon.weaponMode == HudWeaponMode::AirToGroundStrafe &&
        input.weapon.gunMode == HudGunMode::Strafe &&
        armed &&
        input.weapon.gunRoundsRemaining > 0 &&
        input.airGround.valid;

    const HudVec2 targetPosition = ProjectTargetToHud(input.target);
    frame.airToAirGunVisible = airToAirGun;
    frame.eegsFunnelVisible = airToAirGun && !input.weapon.targetLocked;
    frame.mrgsVisible = frame.eegsFunnelVisible;
    frame.fedsVisible = frame.eegsFunnelVisible && input.weapon.triggerHeld;
    frame.tdCircleVisible = airToAirGun && input.weapon.targetLocked && input.target.valid;
    frame.tdCirclePosition = targetPosition;
    frame.targetRangeFeet = std::max(FiniteOr(input.target.rangeMeters, 0.0f), 0.0f) * kMetersToFeet;

    const float targetRangeMeters = std::max(FiniteOr(input.target.rangeMeters, 0.0f), 500.0f);
    const float targetWingspanMeters =
        std::max(FiniteOr(input.weapon.targetWingspanMeters, kDefaultTargetWingspanMeters), kFeetToMeters);
    const float halfAngleRad = std::atan((targetWingspanMeters * 0.5f) / targetRangeMeters);
    const float referenceHalfAngleRad = std::atan((kDefaultTargetWingspanMeters * 0.5f) / (2500.0f * kFeetToMeters));
    const float rangeScaleX = halfAngleRad / std::max(referenceHalfAngleRad, 0.0001f);
    const float loadFactor = Clamp(FiniteOr(aircraft.normalLoadG, 1.0f), -1.5f, 9.0f);
    const float speedScale = Clamp(430.0f / std::max(FiniteOr(aircraft.speedKts, 430.0f), 160.0f), 0.78f, 1.28f);
    const float targetAcceleration = FiniteOr(input.weapon.targetAccelerationMps2, 0.0f);
    const float ballisticDrop = Clamp((loadFactor - 1.0f) * 0.012f + (speedScale - 1.0f) * 0.045f, -0.035f, 0.080f);
    const float sightDriftX = Clamp(
        attitude.fpmPosition.x * 0.35f +
            (input.target.valid ? targetPosition.x * 0.24f : 0.0f) +
            Clamp(targetAcceleration / 120.0f, -0.055f, 0.055f),
        -0.18f,
        0.18f);
    const float sightDriftY = Clamp(
        attitude.fpmPosition.y * 0.30f +
            (input.target.valid ? targetPosition.y * 0.16f : 0.0f) -
            ballisticDrop,
        -0.015f,
        0.145f);
    const float rollCouplingDegrees = Clamp(
        -attitude.displayRollDegrees * 0.18f +
            (input.target.valid ? targetPosition.x * 28.0f : 0.0f) +
            Clamp(targetAcceleration * 0.18f, -12.0f, 12.0f),
        -30.0f,
        30.0f);
    frame.eegsFunnelScaleX = Clamp(rangeScaleX * (1.0f + std::fabs(sightDriftX) * 0.35f), 0.52f, 1.72f);
    frame.eegsFunnelScaleY = Clamp(speedScale * (1.0f + std::max(loadFactor - 1.0f, 0.0f) * 0.030f), 0.76f, 1.36f);
    frame.eegsFunnelPosition = HudVec2 {sightDriftX, sightDriftY};
    frame.eegsFunnelRotationDegrees = rollCouplingDegrees;

    const float funnelCurvature = Clamp(
        (loadFactor - 1.0f) * 0.014f +
            std::fabs(attitude.displayRollDegrees) * 0.00075f +
            (speedScale - 1.0f) * 0.055f,
        -0.018f,
        0.115f);
    const float funnelSkew =
        Clamp(sightDriftX * 0.45f + targetAcceleration / 620.0f - attitude.displayRollDegrees * 0.0016f, -0.105f, 0.105f);
    frame.eegsFunnelLeftControlPoints = BuildEegsFunnelRail(
        -1.0f,
        frame.eegsFunnelScaleX,
        frame.eegsFunnelScaleY,
        frame.eegsFunnelPosition,
        funnelCurvature,
        funnelSkew);
    frame.eegsFunnelRightControlPoints = BuildEegsFunnelRail(
        1.0f,
        frame.eegsFunnelScaleX,
        frame.eegsFunnelScaleY,
        frame.eegsFunnelPosition,
        funnelCurvature,
        funnelSkew);

    frame.mrgsPosition = HudVec2 {
        Clamp(frame.eegsFunnelPosition.x * 1.12f, -0.20f, 0.20f),
        Clamp(frame.eegsFunnelPosition.y + 0.065f, -0.040f, 0.16f)};
    frame.mrgsRotationDegrees = frame.eegsFunnelRotationDegrees;

    if (frame.tdCircleVisible)
    {
        const float settlingOffset = 0.018f;
        frame.oneGPipperVisible = true;
        frame.oneGPipperPosition = HudVec2 {
            Clamp(targetPosition.x * 0.82f, -0.48f, 0.48f),
            Clamp(targetPosition.y - settlingOffset, -0.46f, 0.46f)};
        frame.maxGPipperVisible = true;
        frame.maxGPipperPosition = HudVec2 {
            Clamp(targetPosition.x * 0.68f, -0.48f, 0.48f),
            Clamp(targetPosition.y - 0.12f, -0.48f, 0.42f)};
        frame.outOfPlaneBarsVisible = true;
        frame.solutionCircleVisible = true;
        frame.solutionCirclePosition = HudVec2 {
            Clamp(targetPosition.x * 0.76f, -0.48f, 0.48f),
            Clamp(targetPosition.y - 0.070f, -0.48f, 0.44f)};
        frame.batrVisible = input.weapon.triggerHeld;
        frame.batrPosition = frame.solutionCirclePosition;
    }

    frame.strafeVisible = strafeGun;
    frame.strafeSlantRangeFeet = std::max(FiniteOr(input.airGround.slantRangeMeters, 0.0f), 0.0f) * kMetersToFeet;
    frame.strafePipperPosition =
        ProjectAngularOffsetToHud(input.airGround.pipperAzimuthRad, -FiniteOr(input.airGround.pipperDepressionRad, 0.0f));
    frame.strafeInRangeCueVisible = strafeGun && frame.strafeSlantRangeFeet <= EffectiveStrafeInRangeFeet(input.weapon);
    frame.bulletTrackEndPosition = HudVec2 {
        Clamp(frame.strafePipperPosition.x * 0.72f, -0.42f, 0.42f),
        Clamp(frame.strafePipperPosition.y + 0.22f, -0.42f, 0.52f)};
    return frame;
}

HudAirGroundFrame BuildAirGroundFrame(const HudInputSample& input) noexcept
{
    HudAirGroundFrame frame;
    const bool ccipMode =
        input.weapon.masterMode == HudMasterMode::AirToGround &&
        input.weapon.weaponMode == HudWeaponMode::AirToGroundCcip &&
        IsWeaponArmedForHud(input.weapon) &&
        input.airGround.valid;
    frame.ccipVisible = ccipMode;
    frame.ccipPipperPosition =
        ProjectAngularOffsetToHud(input.airGround.pipperAzimuthRad, -FiniteOr(input.airGround.pipperDepressionRad, 0.0f));
    frame.bombFallLineX =
        Clamp(FiniteOr(input.airGround.fallLineAzimuthRad, 0.0f) / (18.0f * kDegreesToRadians) * 0.46f, -0.42f, 0.42f);
    frame.solutionCueVisible = ccipMode && input.airGround.solutionCueValid;
    frame.solutionCuePosition = ProjectAngularOffsetToHud(
        input.airGround.fallLineAzimuthRad,
        -FiniteOr(input.airGround.solutionCueDepressionRad, 0.0f));
    frame.pullupAnticipationCueVisible = ccipMode && input.airGround.pullupAnticipationCueValid;
    frame.pullupAnticipationCuePosition = ProjectAngularOffsetToHud(
        input.airGround.fallLineAzimuthRad,
        -FiniteOr(input.airGround.pullupAnticipationCueDepressionRad, 0.0f));
    frame.slantRangeFeet = std::max(FiniteOr(input.airGround.slantRangeMeters, 0.0f), 0.0f) * kMetersToFeet;
    frame.timeToReleaseSeconds = std::max(FiniteOr(input.airGround.timeToReleaseSeconds, 0.0f), 0.0f);
    frame.timeToGoSeconds = std::max(FiniteOr(input.airGround.timeToGoSeconds, 0.0f), 0.0f);
    return frame;
}

HudApproachFrame BuildApproachFrame(const HudInputSample& input, const HudAttitudeFrame& attitude) noexcept
{
    HudApproachFrame frame;
    frame.landingVisible =
        input.approach.landingModeActive ||
        input.approach.landingGearDown ||
        input.weapon.masterMode == HudMasterMode::Landing;
    frame.forceCalibratedAirspeed = input.approach.landingGearDown;
    frame.fineAltitudeScale = frame.landingVisible;
    frame.minusTwoPointFivePitchLineVisible = frame.landingVisible;
    const float minusTwoPointFiveOffsetY =
        (-2.5f - attitude.displayPitchDegrees) * kPitchToHudUnits;
    frame.minusTwoPointFivePitchLinePosition =
        RotateHudVector(HudVec2 {0.0f, Clamp(minusTwoPointFiveOffsetY, -0.52f, 0.52f)}, -attitude.displayRollDegrees);
    frame.declutterActive =
        frame.landingVisible &&
        input.approach.landingDeclutterActive &&
        !input.approach.weightOnWheels;
    frame.rollIndicatorVisible = !frame.declutterActive;
    frame.headingTapeShiftedUp = frame.landingVisible;
    return frame;
}

HudIlsFrame BuildIlsFrame(const HudInputSample& input, const HudApproachFrame& approach) noexcept
{
    HudIlsFrame frame;
    const bool ilsAvailable =
        input.ils.powered &&
        input.ils.selected &&
        input.ils.signalValid;
    frame.barsVisible = ilsAvailable && !approach.declutterActive;
    frame.localizerDeviationDots = Clamp(FiniteOr(input.ils.localizerDeviationDots, 0.0f), -2.5f, 2.5f);
    frame.glideslopeDeviationDots = Clamp(FiniteOr(input.ils.glideslopeDeviationDots, 0.0f), -2.5f, 2.5f);
    frame.localizerBarPosition = HudVec2 {frame.localizerDeviationDots * 0.075f, 0.0f};
    frame.glideslopeBarPosition = HudVec2 {0.0f, frame.glideslopeDeviationDots * 0.075f};
    frame.commandSteeringVisible =
        ilsAvailable &&
        input.ils.commandSteeringActive &&
        !approach.declutterActive &&
        input.approach.flightPathMarkerAvailable;
    frame.commandSteeringPosition = HudVec2 {
        Clamp(frame.localizerDeviationDots * 0.065f, -0.22f, 0.22f),
        Clamp(frame.glideslopeDeviationDots * 0.065f, -0.22f, 0.22f)};
    frame.wSteeringVisible =
        ilsAvailable &&
        !approach.declutterActive &&
        !input.approach.flightPathMarkerAvailable;
    frame.wSteeringPosition = HudVec2 {0.0f, 0.90f - 11.0f * kHudUnitsPerMil};
    return frame;
}
} // namespace

AircraftState BuildAircraftStateForHud(const AircraftInputSample& aircraft) noexcept
{
    AircraftState state;
    const float trueSpeedMps = TrueSpeedMetersPerSecond(aircraft);
    state.elapsedSeconds = FiniteOr(aircraft.elapsedSeconds, 0.0f);
    state.pitchDegrees = NormalizeDegrees180(FiniteOr(aircraft.pitchRad, 0.0f) * kRadiansToDegrees);
    state.rollDegrees = NormalizeDegrees180(FiniteOr(aircraft.rollRad, 0.0f) * kRadiansToDegrees);
    state.headingDegrees = WrapDegrees360(FiniteOr(aircraft.headingRad, 0.0f) * kRadiansToDegrees);
    state.flightPathAngleDegrees = FlightPathSlopeRadians(aircraft) * kRadiansToDegrees;
    state.speedKts = std::max(trueSpeedMps, 0.0f) * kMetersPerSecondToKnots;
    const float inputMach = FiniteOr(aircraft.mach, 0.0f);
    state.mach = inputMach > 0.01f ? inputMach : trueSpeedMps / kSeaLevelMachMetersPerSecond;
    state.altitudeFeet = std::max(FiniteOr(aircraft.altitudeMeters, 0.0f), 0.0f) * kMetersToFeet;
    state.radarAltitudeFeet = std::max(FiniteOr(aircraft.radioAltitudeMeters, 0.0f), 0.0f) * kMetersToFeet;
    state.verticalSpeedFpm = -FiniteOr(aircraft.downSpeedMps, 0.0f) * kMetersPerSecondToFeetPerMinute;
    state.normalLoadG = FiniteOr(aircraft.normalLoadFactor, 1.0f);
    state.throttle = Clamp(FiniteOr(aircraft.throttleRatio, 0.0f), 0.0f, 1.0f);
    state.energyRate = FiniteOr(aircraft.specificEnergyRateMps, 0.0f) / 12.0f;
    state.afterburnerActive = aircraft.afterburnerActive;
    return state;
}

TargetState BuildTargetStateForHud(const TargetInputSample& target) noexcept
{
    TargetState state;
    state.valid = target.valid;
    state.rangeNm = std::max(FiniteOr(target.rangeMeters, 0.0f), 0.0f) * kMetersToNauticalMiles;
    state.closureKts = FiniteOr(target.closingSpeedMps, 0.0f) * kMetersPerSecondToKnots;
    state.aspectDegrees = WrapDegrees360(FiniteOr(target.aspectRad, 0.0f) * kRadiansToDegrees);
    state.azimuthDegrees = FiniteOr(target.azimuthRad, 0.0f) * kRadiansToDegrees;
    state.elevationDegrees = FiniteOr(target.elevationRad, 0.0f) * kRadiansToDegrees;
    state.altitudeFeet = std::max(FiniteOr(target.altitudeMeters, 0.0f), 0.0f) * kMetersToFeet;
    return state;
}

LaunchZone ComputeLaunchZone(const AircraftInputSample& aircraft,
                             const TargetInputSample& target,
                             const MissileType selectedMissile) noexcept
{
    const float altitudeBonus = Clamp(FiniteOr(aircraft.altitudeMeters, 0.0f) / 12192.0f, 0.0f, 1.0f);
    const float closureBonus = Clamp(FiniteOr(target.closingSpeedMps, 0.0f) / 463.0f, -0.35f, 0.65f);
    const float energyBonus = Clamp(FiniteOr(aircraft.specificEnergyRateMps, 0.0f) / 72.0f, -0.25f, 0.45f);
    const float afterburnerBonus = aircraft.afterburnerActive ? 0.09f : 0.0f;
    const float targetAspectRad = FiniteOr(target.aspectRad, 0.0f);
    const float aspectPenalty = Clamp(std::fabs(targetAspectRad - kPi) / kPi, 0.0f, 1.0f) * 0.18f;
    const float baseRangeNm = selectedMissile == MissileType::Aim120C ? 24.0f : 8.2f;
    const float scale =
        1.0f + altitudeBonus * 0.35f + closureBonus * 0.28f + energyBonus + afterburnerBonus - aspectPenalty;

    LaunchZone zone;
    zone.rmax1Nm = std::max(baseRangeNm * scale, selectedMissile == MissileType::Aim120C ? 10.0f : 3.0f);
    zone.rmax2Nm = zone.rmax1Nm * (selectedMissile == MissileType::Aim120C ? 0.68f : 0.62f);
    zone.rmin1Nm = selectedMissile == MissileType::Aim120C ? 1.15f : 0.42f;
    zone.rmin2Nm = zone.rmin1Nm + (selectedMissile == MissileType::Aim120C ? 1.15f : 0.55f);
    const TargetState displayTarget = BuildTargetStateForHud(target);
    zone.inNoEscapeZone = displayTarget.rangeNm >= zone.rmin2Nm && displayTarget.rangeNm <= zone.rmax2Nm;
    zone.tooClose = displayTarget.rangeNm < zone.rmin1Nm;
    return zone;
}

HudFrame BuildHudFrame(const HudInputSample& input) noexcept
{
    HudFrame frame;
    frame.attitude = ResolveHudAttitude(input.aircraft);
    frame.airData = BuildAirDataFrame(input.aircraft, frame.attitude);
    frame.weapon = BuildWeaponFrame(input);
    frame.gun = BuildGunFrame(input, frame.attitude);
    frame.airGround = BuildAirGroundFrame(input);
    frame.approach = BuildApproachFrame(input, frame.attitude);
    frame.ils = BuildIlsFrame(input, frame.approach);
    return frame;
}

std::string FormatHeading(const float headingDegrees)
{
    char buffer[16] {};
    std::snprintf(buffer, sizeof(buffer), "%03d", static_cast<int>(std::lround(WrapDegrees360(headingDegrees))) % 360);
    return buffer;
}

std::string FormatMissileInventory(const MissileType selectedMissile, const MissileInventory& inventory)
{
    char buffer[32] {};
    std::snprintf(
        buffer,
        sizeof(buffer),
        "%s %d",
        selectedMissile == MissileType::Aim120C ? "AIM-120C" : "AIM-9M",
        InventoryCount(inventory, selectedMissile));
    return buffer;
}

const char* MissileMnemonic(const MissileType selectedMissile) noexcept
{
    return selectedMissile == MissileType::Aim120C ? "MRM" : "SRM";
}

const char* MissilePhaseMnemonic(const MissileFlightPhase phase) noexcept
{
    switch (phase)
    {
    case MissileFlightPhase::Boost:
        return "BST";
    case MissileFlightPhase::Midcourse:
        return "MID";
    case MissileFlightPhase::Active:
        return "ACT";
    case MissileFlightPhase::Terminal:
        return "TERM";
    case MissileFlightPhase::Impact:
        return "HIT";
    }

    return "UNK";
}
} // namespace demo_hud

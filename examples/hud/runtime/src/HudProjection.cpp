/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Stateless HUD projection and EEGS funnel implementation.
 */

#include "hud/HudProjection.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace hud
{
namespace
{
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

// --- Single HUD angular projection model -------------------------------------
// One explicit field of view drives every conformal symbol (pitch ladder,
// target/radar cues, missile cues and A-G cues). Changing the HUD field of view
// is done here and nowhere else.
//
// Product decision: a 30-degree total vertical field of view for the pitch
// ladder reduces the visual saturation of the previous ~58-degree scale. At
// level flight only the +/-15 degree bars reach the aperture, so the +/-20,
// +/-25 and +/-30 bars are no longer drawn.
constexpr float kHudPitchLadderVerticalFovDeg = 30.0f;
constexpr float kHudConformalVerticalFovDeg = 30.0f;
constexpr float kHudConformalHorizontalFovDeg = 30.0f;
// The authored HUD page spans [-1, +1]; half the field of view maps to this
// half-extent, so a symbol at the field-of-view edge sits at the page edge.
constexpr float kHudConformalHalfWidthUnits = 1.0f;
constexpr float kHudConformalHalfHeightUnits = 1.0f;
// HUD units per degree. Horizontal and vertical are equal, giving conformal
// symbols the 1:1 angular scale required by the BMS attitude bars.
constexpr float kHudUnitsPerHorizontalDegree =
    kHudConformalHalfWidthUnits / (kHudConformalHorizontalFovDeg * 0.5f);
constexpr float kHudUnitsPerVerticalDegree =
    kHudConformalHalfHeightUnits / (kHudConformalVerticalFovDeg * 0.5f);
// The pitch ladder shares the vertical conformal scale over its 2.0-unit height.
constexpr float kPitchToHudUnits =
    (2.0f * kHudConformalHalfHeightUnits) / kHudPitchLadderVerticalFovDeg;

// Pitch beyond which the true horizon leaves the visible aperture and ghosts to
// the edge. Derived from the pitch ladder scale so it tracks the HUD field of
// view instead of being a magic HUD-unit constant.
constexpr float kGhostHorizonLimitDeg = 8.7f;
constexpr float kHudHorizonLimit = kGhostHorizonLimitDeg * kPitchToHudUnits;
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

// Weapon-specific facts (launch zone, time of flight, labels, quantities and
// per-weapon symbol scale) are caller-resolved inputs of `WeaponInputSample`.
// The projection only sanitizes them before use, so the HUD runtime never
// embeds airframe- or client-specific armament models.
float SanitizedMissileDiamondScale(const float scale) noexcept
{
    const float finiteScale = FiniteOr(scale, 1.0f);
    return finiteScale > 0.0f ? finiteScale : 1.0f;
}

float SanitizedNonNegative(const float value) noexcept
{
    return std::max(FiniteOr(value, 0.0f), 0.0f);
}

LaunchZone SanitizedLaunchZone(const LaunchZone& launchZone) noexcept
{
    LaunchZone zone = launchZone;
    zone.rmax1Nm = SanitizedNonNegative(zone.rmax1Nm);
    zone.rmax2Nm = SanitizedNonNegative(zone.rmax2Nm);
    zone.rmin2Nm = SanitizedNonNegative(zone.rmin2Nm);
    zone.rmin1Nm = SanitizedNonNegative(zone.rmin1Nm);
    return zone;
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

// Single conformal projector: every boresight-relative symbol goes through this
// helper, so the HUD keeps one angular scale and one field-of-view rule.
ProjectedHudPoint ProjectConformalPoint(const float azimuthRad, const float elevationRad) noexcept
{
    const float azimuthDeg = FiniteOr(azimuthRad, 0.0f) * kRadiansToDegrees;
    const float elevationDeg = FiniteOr(elevationRad, 0.0f) * kRadiansToDegrees;
    const float rawX = azimuthDeg * kHudUnitsPerHorizontalDegree;
    const float rawY = elevationDeg * kHudUnitsPerVerticalDegree;
    const float x = Clamp(rawX, -kHudConformalHalfWidthUnits, kHudConformalHalfWidthUnits);
    const float y = Clamp(rawY, -kHudConformalHalfHeightUnits, kHudConformalHalfHeightUnits);

    ProjectedHudPoint point;
    point.position = HudVec2 {x, y};
    point.insideFov =
        std::fabs(azimuthDeg) <= kHudConformalHorizontalFovDeg * 0.5f &&
        std::fabs(elevationDeg) <= kHudConformalVerticalFovDeg * 0.5f;
    point.limited = x != rawX || y != rawY;
    return point;
}

ProjectedHudPoint ProjectTargetToHud(const TargetInputSample& target) noexcept
{
    return ProjectConformalPoint(target.azimuthRad, target.elevationRad);
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
    // A target outside the HUD field of view is clamped to the edge and flagged
    // as limited, not hidden: BMS keeps the AIM-120 diamond at the FOV edge and
    // overlays a geometric limit-X on it.
    const ProjectedHudPoint targetPoint = ProjectTargetToHud(input.target);
    frame.targetVisible = frame.airToAirVisible && input.target.valid;
    frame.targetPosition = targetPoint.position;
    frame.targetLimited = frame.targetVisible && !targetPoint.insideFov;
    frame.missileDiamondLimited = frame.targetLimited;
    frame.missileLimitXVisible = frame.targetLimited;
    frame.missileLimitXPosition = frame.targetPosition;
    frame.missileDiamondPosition = frame.targetPosition;
    frame.missileDiamondScale = SanitizedMissileDiamondScale(input.weapon.missileDiamondScale);
    frame.attackSteeringCueVisible = frame.targetVisible;
    frame.attackSteeringCuePosition = HudVec2 {
        Clamp(frame.targetPosition.x * 0.55f, -0.30f, 0.30f),
        Clamp(frame.targetPosition.y * 0.45f - 0.04f, -0.26f, 0.26f)};
    // The launch zone and time of flight are avionics facts computed by the
    // caller; the projection only positions the range cue on the DLZ scale.
    frame.launchZone = SanitizedLaunchZone(input.weapon.launchZone);
    frame.dynamicLaunchZoneVisible = frame.targetVisible;
    frame.rangeCueVisible = frame.targetVisible;
    const TargetState target = BuildTargetStateForHud(input.target);
    const float rangeFactor = Clamp(target.rangeNm / std::max(frame.launchZone.rmax1Nm, 0.1f), 0.0f, 1.0f);
    frame.rangeCuePosition = HudVec2 {0.0f, kDlzBottomY + rangeFactor * kDlzHeight};
    frame.breakXVisible = frame.airToAirVisible && input.target.valid && frame.launchZone.tooClose;
    frame.missileCircleVisible = frame.airToAirVisible;
    frame.selectedMissileTimeOfFlightSeconds =
        SanitizedNonNegative(input.weapon.selectedMissileTimeOfFlightSeconds);

    frame.missileInFlight = input.weapon.missileInFlight;
    frame.activeMissileTimeRemainingSeconds = input.weapon.activeMissileTimeRemainingSeconds;
    frame.activeMissilePhase = input.weapon.activeMissilePhase;

    if (input.weapon.selectedWeaponQuantity <= 0)
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

    const ProjectedHudPoint targetPoint = ProjectTargetToHud(input.target);
    const HudVec2 targetPosition = targetPoint.position;
    frame.airToAirGunVisible = airToAirGun;
    frame.eegsFunnelVisible = airToAirGun && !input.weapon.targetLocked;
    frame.mrgsVisible = frame.eegsFunnelVisible;
    frame.fedsVisible = frame.eegsFunnelVisible && input.weapon.triggerHeld;
    // The locked target designator circle is conformal: when the track leaves the
    // field of view it is clamped to the edge and flagged, not hidden.
    frame.tdCircleVisible = airToAirGun && input.weapon.targetLocked && input.target.valid;
    frame.tdCirclePosition = targetPosition;
    frame.tdCircleLimited = frame.tdCircleVisible && !targetPoint.insideFov;
    frame.tdCircleLimitXVisible = frame.tdCircleLimited;
    frame.tdCircleLimitXPosition = targetPosition;
    frame.targetRangeFeet = std::max(FiniteOr(input.target.rangeMeters, 0.0f), 0.0f) * kMetersToFeet;

    const float targetRangeMeters = std::max(FiniteOr(input.target.rangeMeters, 0.0f), 500.0f);
    // A neutral or non-finite wingspan input falls back to the authored HUD
    // default; the fallback is a projection concern, not part of the contract.
    const float providedWingspanMeters = FiniteOr(input.weapon.targetWingspanMeters, 0.0f);
    const float targetWingspanMeters =
        providedWingspanMeters > 0.0f ? providedWingspanMeters : kDefaultTargetWingspanMeters;
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

    const ProjectedHudPoint strafePipper = ProjectConformalPoint(
        input.airGround.pipperAzimuthRad, -FiniteOr(input.airGround.pipperDepressionRad, 0.0f));
    // The strafe pipper is conformal; when the aim point leaves the HUD field of
    // view it is clamped to the edge and flagged, not hidden. The current model
    // has no dedicated strafe limit-X reticle, so only the flag is exposed.
    frame.strafeVisible = strafeGun;
    frame.strafeSlantRangeFeet = std::max(FiniteOr(input.airGround.slantRangeMeters, 0.0f), 0.0f) * kMetersToFeet;
    frame.strafePipperPosition = strafePipper.position;
    frame.strafePipperLimited = frame.strafeVisible && !strafePipper.insideFov;
    // The in-range threshold is a caller-resolved fact; the runtime never
    // invents an ammunition default. A non-positive value hides the cue.
    const float strafeInRangeFeet = FiniteOr(input.weapon.strafeInRangeFeet, 0.0f);
    frame.strafeInRangeCueVisible =
        frame.strafeVisible && strafeInRangeFeet > 0.0f && frame.strafeSlantRangeFeet <= strafeInRangeFeet;
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
    // The CCIP pipper is conformal; when it reaches the total field-of-view edge
    // it is clamped there and a limit-X is overlaid, matching BMS, instead of the
    // pipper disappearing.
    const ProjectedHudPoint ccipPipper = ProjectConformalPoint(
        input.airGround.pipperAzimuthRad, -FiniteOr(input.airGround.pipperDepressionRad, 0.0f));
    frame.ccipVisible = ccipMode;
    frame.ccipPipperPosition = ccipPipper.position;
    frame.ccipPipperLimited = frame.ccipVisible && !ccipPipper.insideFov;
    frame.ccipLimitXVisible = frame.ccipPipperLimited;
    frame.ccipLimitXPosition = ccipPipper.position;
    frame.bombFallLineX = ProjectConformalPoint(input.airGround.fallLineAzimuthRad, 0.0f).position.x;
    const ProjectedHudPoint solutionCue = ProjectConformalPoint(
        input.airGround.fallLineAzimuthRad, -FiniteOr(input.airGround.solutionCueDepressionRad, 0.0f));
    frame.solutionCueVisible = frame.ccipVisible && input.airGround.solutionCueValid;
    frame.solutionCuePosition = solutionCue.position;
    frame.solutionCueLimited = frame.solutionCueVisible && !solutionCue.insideFov;
    const ProjectedHudPoint pullupCue = ProjectConformalPoint(
        input.airGround.fallLineAzimuthRad, -FiniteOr(input.airGround.pullupAnticipationCueDepressionRad, 0.0f));
    frame.pullupAnticipationCueVisible = frame.ccipVisible && input.airGround.pullupAnticipationCueValid;
    frame.pullupAnticipationCuePosition = pullupCue.position;
    frame.pullupAnticipationCueLimited = frame.pullupAnticipationCueVisible && !pullupCue.insideFov;
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

HudAngularProjection HudConformalProjection() noexcept
{
    return HudAngularProjection {
        kHudConformalHorizontalFovDeg,
        kHudConformalVerticalFovDeg,
        kHudConformalHalfWidthUnits,
        kHudConformalHalfHeightUnits};
}

float HudPitchLadderVerticalFovDegrees() noexcept
{
    return kHudPitchLadderVerticalFovDeg;
}

ProjectedHudPoint ProjectBoresightAngularOffsetToHud(const float azimuthRad, const float elevationRad) noexcept
{
    return ProjectConformalPoint(azimuthRad, elevationRad);
}

HudVec2 ProjectPitchOffsetToHud(const float pitchOffsetDeg) noexcept
{
    return HudVec2 {0.0f, FiniteOr(pitchOffsetDeg, 0.0f) * kPitchToHudUnits};
}

bool IsInsideHudFov(const float azimuthRad, const float elevationRad) noexcept
{
    return ProjectConformalPoint(azimuthRad, elevationRad).insideFov;
}

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
} // namespace hud

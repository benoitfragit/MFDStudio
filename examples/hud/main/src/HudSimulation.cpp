/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of the deterministic HUD mini-simulation.
 */

#include "hud_main/HudSimulation.h"

#include "hud_main/HudPhysics.h"
#include "hud_main/HudSimulationTime.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace hud_main
{
// The simulation body works on the generic HUD contract types; importing them
// keeps the sample readable without pulling main-client types into `hud`.
using hud::AircraftInputSample;
using hud::AirGroundInputSample;
using hud::HudInputSample;
using hud::HudMasterMode;
using hud::HudWeaponMode;
using hud::LaunchZone;
using hud::MissileFlightPhase;
using hud::TargetInputSample;
using hud::TargetState;
using hud::WeaponInputSample;

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kDegreesToRadians = 0.017453292519943295f;
constexpr float kFeetToMeters = 0.3048f;
constexpr float kKnotsToMetersPerSecond = 0.514444444f;
constexpr float kNauticalMileToMeters = 1852.0f;

/** Wind-speed sanitization bound in knots for the environment controls. */
constexpr float kWindSpeedMaxKts = 200.0f;
/** Terrain-elevation sanitization bounds in meters for the environment controls. */
constexpr float kTerrainElevationMinMeters = 0.0f;
constexpr float kTerrainElevationMaxMeters = 9000.0f;
/** Outside-air-temperature sanitization bounds in kelvin. */
constexpr float kOutsideAirTemperatureMinKelvin = 180.0f;
constexpr float kOutsideAirTemperatureMaxKelvin = 350.0f;
/** Static-pressure sanitization bounds in hectopascals. */
constexpr float kPressureMinHpa = 300.0f;
constexpr float kPressureMaxHpa = 1100.0f;
float Clamp(const float value, const float low, const float high) noexcept
{
    return std::max(low, std::min(value, high));
}

float FiniteOr(const float value, const float fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
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

float HorizontalSpeedMetersPerSecond(const AircraftInputSample& aircraft) noexcept
{
    const float north = FiniteOr(aircraft.northSpeedMps, 0.0f);
    const float east = FiniteOr(aircraft.eastSpeedMps, 0.0f);
    return std::sqrt(north * north + east * east);
}

// Magnitude of the published NED velocity. Since `Step()` writes the ground
// velocity (air velocity NED + wind NED) into the sample, this is the ground
// speed, not the true airspeed. With zero wind both speeds are identical.
float GroundSpeedMetersPerSecond(const AircraftInputSample& aircraft) noexcept
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

// Sanitizes the raw environment panel values into their supported ranges.
void SanitizeEnvironmentControls(EnvironmentControls& environment) noexcept
{
    const EnvironmentControls defaults {};
    environment.windSpeedKts = Clamp(FiniteOr(environment.windSpeedKts, 0.0f), 0.0f, kWindSpeedMaxKts);
    environment.windDirectionRad = WrapRadiansTwoPi(environment.windDirectionRad);
    environment.turbulenceIntensity = Clamp(FiniteOr(environment.turbulenceIntensity, 0.0f), 0.0f, 1.0f);
    environment.terrainElevationMeters = Clamp(
        FiniteOr(environment.terrainElevationMeters, defaults.terrainElevationMeters),
        kTerrainElevationMinMeters,
        kTerrainElevationMaxMeters);
    environment.outsideAirTemperatureKelvin = Clamp(
        FiniteOr(environment.outsideAirTemperatureKelvin, defaults.outsideAirTemperatureKelvin),
        kOutsideAirTemperatureMinKelvin,
        kOutsideAirTemperatureMaxKelvin);
    environment.pressureHpa = Clamp(
        FiniteOr(environment.pressureHpa, defaults.pressureHpa),
        kPressureMinHpa,
        kPressureMaxHpa);
}

// --- Sample armament and launch-zone tuning ----------------------------------
// Main-client armament data. The reusable HUD runtime never sees these
// profiles: the simulation resolves them into the generic label, mnemonic,
// quantity, launch-zone and time-of-flight fields of `hud::WeaponInputSample`.
struct MissileProfile
{
    /** HUD inventory label such as "AIM-120C". */
    const char* label = "";
    /** Short HUD mnemonic such as "MRM". */
    const char* mnemonic = "";
    /** Nominal missile speed in meters per second. */
    float nominalSpeedMps = 0.0f;
    /** Lower bound on effective closing speed used for time-of-flight, m/s. */
    float minEffectiveSpeedMps = 0.0f;
    /** Minimum displayed time of flight in seconds. */
    float minTimeOfFlightSeconds = 0.0f;
    /** Maximum displayed time of flight in seconds. */
    float maxTimeOfFlightSeconds = 0.0f;
    /** Base aerodynamic range before DLZ scaling, nautical miles. */
    float baseRangeNm = 0.0f;
    /** Floor applied to the scaled maximum range, nautical miles. */
    float minRmax1Nm = 0.0f;
    /** No-escape maximum range as a fraction of rmax1. */
    float rmax2Factor = 0.0f;
    /** Minimum launch range in nautical miles. */
    float rmin1Nm = 0.0f;
    /** No-escape minimum range offset above rmin1, nautical miles. */
    float rmin2OffsetNm = 0.0f;
    /** Missile diamond scale multiplier for this weapon. */
    float diamondScale = 1.0f;
};

// Coefficients shared by every sample weapon's launch-zone and time-of-flight math.
struct LaunchZoneTuning
{
    float altitudeBonusDivisorMeters = 12192.0f;
    float altitudeBonusScale = 0.35f;
    float closureBonusDivisorMps = 463.0f;
    float closureBonusMin = -0.35f;
    float closureBonusMax = 0.65f;
    float closureBonusScale = 0.28f;
    float energyBonusDivisorMps = 72.0f;
    float energyBonusMin = -0.25f;
    float energyBonusMax = 0.45f;
    float afterburnerBonus = 0.09f;
    float aspectPenaltyScale = 0.18f;
    float closingSpeedFloorMps = -77.0f;
    float closingSpeedContribution = 0.40f;
    float ownshipSpeedContribution = 0.25f;
};

constexpr MissileProfile kAim120CProfile {
    "AIM-120C", // label
    "MRM",      // mnemonic
    1209.0f,    // nominalSpeedMps
    154.0f,     // minEffectiveSpeedMps
    3.0f,       // minTimeOfFlightSeconds
    68.0f,      // maxTimeOfFlightSeconds
    24.0f,      // baseRangeNm
    10.0f,      // minRmax1Nm
    0.68f,      // rmax2Factor
    1.15f,      // rmin1Nm
    1.15f,      // rmin2OffsetNm
    0.92f};     // diamondScale
constexpr MissileProfile kAim9MProfile {
    "AIM-9M", // label
    "SRM",    // mnemonic
    849.0f,   // nominalSpeedMps
    154.0f,   // minEffectiveSpeedMps
    3.0f,     // minTimeOfFlightSeconds
    28.0f,    // maxTimeOfFlightSeconds
    8.2f,     // baseRangeNm
    3.0f,     // minRmax1Nm
    0.62f,    // rmax2Factor
    0.42f,    // rmin1Nm
    0.55f,    // rmin2OffsetNm
    1.15f};   // diamondScale
constexpr LaunchZoneTuning kLaunchZoneTuning {};

const MissileProfile& MissileProfileFor(const MissileType type) noexcept
{
    return type == MissileType::Aim120C ? kAim120CProfile : kAim9MProfile;
}

int InventoryCount(const MissileInventory& inventory, const MissileType type) noexcept
{
    return type == MissileType::Aim120C ? inventory.aim120c : inventory.aim9m;
}

// Mutable inventory slot accessor used when a launch consumes a round.
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

// Initial scene of the bundled client. `hud::HudInputSample{}` is a neutral
// runtime contract (grounded, no target, no weapon), so the client writes the
// airborne scene it previously inherited from the runtime defaults explicitly.
struct ClientSceneSample
{
    /** Aircraft pitch in radians (~2 degrees nose-up cruise). */
    float pitchRad = 0.034906585f;
    /** Mean sea level altitude in meters (~15000 ft). */
    float altitudeMeters = 4572.0f;
    /** Radar altitude above ground level in meters. */
    float radioAltitudeMeters = 4419.6f;
    /** North velocity component in meters per second (~430 kts cruise). */
    float northSpeedMps = 221.21f;
    /** Down velocity component in meters per second; negative while climbing. */
    float downSpeedMps = -3.86f;
    /** Cruise Mach number. */
    float mach = 0.65f;
    /** Cruise throttle ratio. */
    float throttleRatio = 0.62f;
    /** Slant range to the scripted target in meters (5.4 NM). */
    float targetRangeMeters = 5.4f * kNauticalMileToMeters;
    /** Target closing speed in meters per second (360 kts). */
    float targetClosingSpeedMps = 360.0f * kKnotsToMetersPerSecond;
    /** Target aspect angle in radians (~125 degrees). */
    float targetAspectRad = 2.1816616f;
    /** Target altitude in meters MSL (~20000 ft). */
    float targetAltitudeMeters = 6096.0f;
    /** Remaining gun rounds carried by the client aircraft. */
    int gunRoundsRemaining = 510;
    /** Target wingspan in meters used for the EEGS funnel width (~35 ft). */
    float targetWingspanMeters = 10.668f;
    /**
     * @brief Client-resolved STRF in-range threshold in feet.
     *
     * The bundled client carries a single PGU-28 loadout, so it resolves the
     * ammunition threshold itself (M56 would be 4000 ft, PGU-28 is 12000 ft)
     * and hands the already-resolved value to the runtime.
     */
    float strafeInRangeFeet = 12000.0f;
    /** Initial A-G slant range to the computed impact point in meters. */
    float airGroundSlantRangeMeters = 1828.8f;
    /** Initial A-G pipper depression in radians below boresight. */
    float airGroundPipperDepressionRad = 0.045f;
    /** Initial CCIP solution cue depression in radians below boresight. */
    float airGroundSolutionCueDepressionRad = 0.020f;
    /** Initial pull-up anticipation cue depression in radians below boresight. */
    float airGroundPullupAnticipationCueDepressionRad = 0.006f;
};

constexpr ClientSceneSample kClientSceneSample {};

// Writes the client's airborne scene into a neutral HUD input sample.
void ApplyClientSceneSample(HudInputSample& inputs) noexcept
{
    const ClientSceneSample& scene = kClientSceneSample;
    inputs.aircraft.pitchRad = scene.pitchRad;
    inputs.aircraft.altitudeMeters = scene.altitudeMeters;
    inputs.aircraft.radioAltitudeMeters = scene.radioAltitudeMeters;
    inputs.aircraft.northSpeedMps = scene.northSpeedMps;
    inputs.aircraft.downSpeedMps = scene.downSpeedMps;
    inputs.aircraft.mach = scene.mach;
    inputs.aircraft.throttleRatio = scene.throttleRatio;

    inputs.target.valid = true;
    inputs.target.rangeMeters = scene.targetRangeMeters;
    inputs.target.closingSpeedMps = scene.targetClosingSpeedMps;
    inputs.target.aspectRad = scene.targetAspectRad;
    inputs.target.altitudeMeters = scene.targetAltitudeMeters;

    inputs.weapon.gunRoundsRemaining = scene.gunRoundsRemaining;
    inputs.weapon.targetWingspanMeters = scene.targetWingspanMeters;
    inputs.weapon.strafeInRangeFeet = scene.strafeInRangeFeet;

    // A-G delivery data stays invalid until a delivery mode is selected; only
    // the geometric seed values are primed. `Step()` recomputes them per frame.
    inputs.airGround.slantRangeMeters = scene.airGroundSlantRangeMeters;
    inputs.airGround.pipperDepressionRad = scene.airGroundPipperDepressionRad;
    inputs.airGround.solutionCueValid = true;
    inputs.airGround.solutionCueDepressionRad = scene.airGroundSolutionCueDepressionRad;
    inputs.airGround.pullupAnticipationCueDepressionRad = scene.airGroundPullupAnticipationCueDepressionRad;
}
} // namespace

LaunchZone ComputeLaunchZone(const AircraftInputSample& aircraft,
                             const TargetInputSample& target,
                             const MissileType selectedMissile) noexcept
{
    const MissileProfile& profile = MissileProfileFor(selectedMissile);
    const LaunchZoneTuning& tuning = kLaunchZoneTuning;
    const float altitudeBonus =
        Clamp(FiniteOr(aircraft.altitudeMeters, 0.0f) / tuning.altitudeBonusDivisorMeters, 0.0f, 1.0f);
    const float closureBonus = Clamp(
        FiniteOr(target.closingSpeedMps, 0.0f) / tuning.closureBonusDivisorMps,
        tuning.closureBonusMin,
        tuning.closureBonusMax);
    const float energyBonus = Clamp(
        FiniteOr(aircraft.specificEnergyRateMps, 0.0f) / tuning.energyBonusDivisorMps,
        tuning.energyBonusMin,
        tuning.energyBonusMax);
    const float afterburnerBonus = aircraft.afterburnerActive ? tuning.afterburnerBonus : 0.0f;
    const float targetAspectRad = FiniteOr(target.aspectRad, 0.0f);
    const float aspectPenalty = Clamp(std::fabs(targetAspectRad - kPi) / kPi, 0.0f, 1.0f) * tuning.aspectPenaltyScale;
    const float scale = 1.0f + altitudeBonus * tuning.altitudeBonusScale + closureBonus * tuning.closureBonusScale +
                        energyBonus + afterburnerBonus - aspectPenalty;

    LaunchZone zone;
    zone.rmax1Nm = std::max(profile.baseRangeNm * scale, profile.minRmax1Nm);
    zone.rmax2Nm = zone.rmax1Nm * profile.rmax2Factor;
    zone.rmin1Nm = profile.rmin1Nm;
    zone.rmin2Nm = zone.rmin1Nm + profile.rmin2OffsetNm;
    const TargetState displayTarget = hud::BuildTargetStateForHud(target);
    zone.inNoEscapeZone = displayTarget.rangeNm >= zone.rmin2Nm && displayTarget.rangeNm <= zone.rmax2Nm;
    zone.tooClose = displayTarget.rangeNm < zone.rmin1Nm;
    return zone;
}

float ComputeMissileTimeOfFlight(const AircraftInputSample& aircraft,
                                 const TargetInputSample& target,
                                 const MissileType selectedMissile) noexcept
{
    const MissileProfile& profile = MissileProfileFor(selectedMissile);
    const LaunchZoneTuning& tuning = kLaunchZoneTuning;
    const float closingSpeedMps = std::max(FiniteOr(target.closingSpeedMps, 0.0f), tuning.closingSpeedFloorMps);
    const float ownshipSpeedMps = GroundSpeedMetersPerSecond(aircraft);
    const float effectiveSpeedMps =
        profile.nominalSpeedMps +
        closingSpeedMps * tuning.closingSpeedContribution +
        ownshipSpeedMps * tuning.ownshipSpeedContribution;
    const float seconds =
        FiniteOr(target.rangeMeters, 0.0f) / std::max(effectiveSpeedMps, profile.minEffectiveSpeedMps);
    return Clamp(seconds, profile.minTimeOfFlightSeconds, profile.maxTimeOfFlightSeconds);
}

const char* MissileLabel(const MissileType selectedMissile) noexcept
{
    return MissileProfileFor(selectedMissile).label;
}

const char* MissileMnemonic(const MissileType selectedMissile) noexcept
{
    return MissileProfileFor(selectedMissile).mnemonic;
}

float MissileDiamondScale(const MissileType selectedMissile) noexcept
{
    return MissileProfileFor(selectedMissile).diamondScale;
}

std::string FormatMissileInventory(const MissileType selectedMissile, const MissileInventory& inventory)
{
    char buffer[32] {};
    std::snprintf(
        buffer,
        sizeof(buffer),
        "%s %d",
        MissileLabel(selectedMissile),
        InventoryCount(inventory, selectedMissile));
    return buffer;
}

HudSimulation::HudSimulation()
{
    Reset();
}

void HudSimulation::Reset()
{
    inputs_ = {};
    // The runtime contract defaults are neutral, so the client restores its own
    // airborne scene explicitly instead of relying on runtime scene defaults.
    ApplyClientSceneSample(inputs_);
    controls_ = {};
    selectedMissile_ = MissileType::Aim120C;
    inventory_ = {};
    publishedAttitudeEulerRad_ = EulerAnglesRad {
        inputs_.aircraft.yawRad,
        inputs_.aircraft.pitchRad,
        inputs_.aircraft.rollRad};
    AircraftDynamicState initialAircraftState;
    initialAircraftState.positionNedMeters = Vec3d {
        0.0,
        0.0,
        -static_cast<double>(inputs_.aircraft.altitudeMeters)};
    initialAircraftState.velocityNedMps = Vec3d {
        inputs_.aircraft.northSpeedMps,
        inputs_.aircraft.eastSpeedMps,
        inputs_.aircraft.downSpeedMps};
    initialAircraftState.attitudeBodyToNed = QuaternionFromEuler321(publishedAttitudeEulerRad_);
    initialAircraftState.throttleRatio = inputs_.aircraft.throttleRatio;
    aircraftDynamics_.Reset(initialAircraftState);
    missileShots_.clear();
    simulationTickCount_ = 0U;
    const AircraftDynamicState& aircraftState = aircraftDynamics_.State();
    const GunLaunchState launchState {
        aircraftState.positionNedMeters,
        aircraftState.velocityNedMps,
        inputs_.aircraft.yawRad,
        inputs_.aircraft.pitchRad,
        inputs_.aircraft.rollRad};
    gunProjectiles_.Reset(launchState, controls_.environment);
    inputs_.gunTrajectory = gunProjectiles_.BuildSnapshot(aircraftState.positionNedMeters);
    RefreshWeaponPresentation();
}

void HudSimulation::SetSimulationControls(const SimulationControls& controls) noexcept
{
    controls_ = controls;
    PilotControls& pilot = controls_.pilot;
    pilot.pitchCommand = Clamp(FiniteOr(pilot.pitchCommand, 0.0f), -1.0f, 1.0f);
    pilot.rollCommand = Clamp(FiniteOr(pilot.rollCommand, 0.0f), -1.0f, 1.0f);
    pilot.yawCommand = Clamp(FiniteOr(pilot.yawCommand, 0.0f), -1.0f, 1.0f);
    pilot.throttle = Clamp(FiniteOr(pilot.throttle, 0.0f), 0.0f, 1.0f);
    SanitizeEnvironmentControls(controls_.environment);
    inputs_.weapon.masterMode = pilot.masterMode;
    inputs_.weapon.weaponMode = pilot.weaponMode;
    inputs_.weapon.gunMode = pilot.gunMode;
    inputs_.weapon.masterArm = pilot.masterArm;
    inputs_.weapon.simulateMode = pilot.simulateMode;
    inputs_.weapon.triggerHeld = pilot.triggerHeld;
    inputs_.weapon.targetLocked = pilot.targetLocked;
    inputs_.approach.landingGearDown = pilot.landingGearDown;
    inputs_.approach.landingModeActive = pilot.landingModeActive;
    inputs_.approach.landingDeclutterActive = pilot.landingDeclutterActive;
    inputs_.ils.powered = pilot.ilsPowered;
    inputs_.ils.selected = pilot.ilsSelected;
    inputs_.ils.signalValid = pilot.ilsSignalValid;
    inputs_.ils.commandSteeringActive = pilot.ilsCommandSteeringActive;
}

void HudSimulation::Step()
{
    constexpr float dt = static_cast<float>(kHudSimulationStepSeconds);
    const PilotControls& pilot = controls_.pilot;
    const EnvironmentControls& environment = controls_.environment;
    AircraftInputSample& aircraft = inputs_.aircraft;
    ++simulationTickCount_;
    aircraft.elapsedSeconds = static_cast<float>(
        static_cast<double>(simulationTickCount_) * kHudSimulationStepSeconds);
    const WindVectorNed steadyWind = ComputeWindVectorNed(environment.windSpeedKts, environment.windDirectionRad);
    const WindVectorNed gust = ComputeTurbulenceGustNed(environment.turbulenceIntensity, aircraft.elapsedSeconds);
    AircraftEnvironmentInput dynamicsEnvironment;
    dynamicsEnvironment.windVelocityNedMps = Vec3d {
        static_cast<double>(steadyWind.northMps + gust.northMps),
        static_cast<double>(steadyWind.eastMps + gust.eastMps),
        static_cast<double>(steadyWind.downMps + gust.downMps)};
    dynamicsEnvironment.airDensityKgPerM3 = ComputeAirDensityKgPerM3(
        environment.pressureHpa,
        environment.outsideAirTemperatureKelvin);
    AircraftControlInput dynamicsControls;
    dynamicsControls.pitchCommand = pilot.pitchCommand;
    dynamicsControls.rollCommand = pilot.rollCommand;
    dynamicsControls.yawCommand = pilot.yawCommand;
    dynamicsControls.throttleRatio = pilot.throttle;
    dynamicsControls.afterburnerRequested = pilot.afterburnerRequested;
    aircraftDynamics_.Step(dynamicsControls, dynamicsEnvironment, kHudSimulationStepSeconds);

    const AircraftDynamicState& aircraftState = aircraftDynamics_.State();
    const AircraftDynamicsTelemetry& dynamicsTelemetry = aircraftDynamics_.Telemetry();
    publishedAttitudeEulerRad_ = Euler321Nearest(
        aircraftState.attitudeBodyToNed,
        publishedAttitudeEulerRad_);
    aircraft.yawRad = static_cast<float>(publishedAttitudeEulerRad_.yaw);
    aircraft.pitchRad = static_cast<float>(publishedAttitudeEulerRad_.pitch);
    aircraft.rollRad = static_cast<float>(publishedAttitudeEulerRad_.roll);
    aircraft.headingRad = WrapRadiansTwoPi(aircraft.yawRad);
    aircraft.northSpeedMps = static_cast<float>(aircraftState.velocityNedMps.x);
    aircraft.eastSpeedMps = static_cast<float>(aircraftState.velocityNedMps.y);
    aircraft.downSpeedMps = static_cast<float>(aircraftState.velocityNedMps.z);
    aircraft.throttleRatio = static_cast<float>(aircraftState.throttleRatio);
    aircraft.afterburnerActive = dynamicsTelemetry.afterburnerActive;
    aircraft.normalLoadFactor = static_cast<float>(dynamicsTelemetry.normalLoadFactor);
    const Vec3d currentAirVelocityNedMps {
        aircraftState.velocityNedMps.x - dynamicsEnvironment.windVelocityNedMps.x,
        aircraftState.velocityNedMps.y - dynamicsEnvironment.windVelocityNedMps.y,
        aircraftState.velocityNedMps.z - dynamicsEnvironment.windVelocityNedMps.z};
    const float newSpeedMps = static_cast<float>(VectorLength(currentAirVelocityNedMps));
    const GunLaunchState launchState {
        aircraftState.positionNedMeters,
        aircraftState.velocityNedMps,
        aircraft.yawRad,
        aircraft.pitchRad,
        aircraft.rollRad};
    gunProjectiles_.Step(launchState, environment);
    inputs_.gunTrajectory = gunProjectiles_.BuildSnapshot(aircraftState.positionNedMeters);
    aircraft.altitudeMeters = std::max(
        18.0f,
        static_cast<float>(-aircraftState.positionNedMeters.z));
    aircraft.radioAltitudeMeters = ComputeRadioAltitudeMeters(
        aircraft.altitudeMeters,
        ComputeTerrainElevationMeters(environment, aircraft.elapsedSeconds, aircraft.headingRad));
    aircraft.mach =
        ComputeMach(newSpeedMps, ComputeSpeedOfSoundMps(environment.outsideAirTemperatureKelvin));
    aircraft.specificEnergyRateMps =
        ComputeSpecificEnergyRateMps(
            newSpeedMps,
            -aircraft.downSpeedMps,
            static_cast<float>(dynamicsTelemetry.longitudinalAccelerationMps2));

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
            ? Clamp(airGround.slantRangeMeters / std::max(GroundSpeedMetersPerSecond(aircraft), 1.0f), 0.0f, 99.0f)
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
    // Aircraft and target moved this step, so the caller-resolved launch zone
    // and time of flight published to the HUD must be refreshed too.
    RefreshWeaponPresentation();
}

void HudSimulation::SelectMissile(const MissileType type)
{
    selectedMissile_ = type;
    RefreshWeaponPresentation();
}

void HudSimulation::CycleSelectedMissile()
{
    SelectMissile(selectedMissile_ == MissileType::Aim120C ? MissileType::Aim9M : MissileType::Aim120C);
}

bool HudSimulation::FireSelectedMissile()
{
    if (!IsAirToAirMissileMode(inputs_.weapon) || !IsWeaponArmedForHud(inputs_.weapon) || !inputs_.target.valid)
    {
        return false;
    }

    int& selectedInventory = InventorySlot(inventory_, selectedMissile_);
    if (selectedInventory <= 0)
    {
        return false;
    }

    const LaunchZone launchZone = ComputeLaunchZone(inputs_.aircraft, inputs_.target, selectedMissile_);
    const TargetState target = hud::BuildTargetStateForHud(inputs_.target);
    if (launchZone.tooClose || target.rangeNm > launchZone.rmax1Nm * 1.08f)
    {
        return false;
    }

    --selectedInventory;
    MissileShot shot;
    shot.type = selectedMissile_;
    shot.timeOfFlightSeconds = ComputeMissileTimeOfFlight(inputs_.aircraft, inputs_.target, selectedMissile_);
    shot.timeRemainingSeconds = shot.timeOfFlightSeconds;
    shot.launchRangeNm = target.rangeNm;
    shot.phase = MissileFlightPhase::Boost;
    missileShots_.push_back(shot);
    SyncWeaponInputFromMissileShots(inputs_, missileShots_);
    RefreshWeaponPresentation();
    return true;
}

void HudSimulation::RefreshWeaponPresentation()
{
    // This is the sample equivalent of a real avionics resolver: concrete
    // armament facts become the generic weapon presentation of the HUD input.
    WeaponInputSample& weapon = inputs_.weapon;
    weapon.selectedWeaponLabel = MissileLabel(selectedMissile_);
    weapon.selectedWeaponMnemonic = MissileMnemonic(selectedMissile_);
    weapon.selectedWeaponQuantity = InventoryCount(inventory_, selectedMissile_);
    weapon.missileDiamondScale = MissileDiamondScale(selectedMissile_);
    weapon.launchZone = ComputeLaunchZone(inputs_.aircraft, inputs_.target, selectedMissile_);
    weapon.selectedMissileTimeOfFlightSeconds =
        ComputeMissileTimeOfFlight(inputs_.aircraft, inputs_.target, selectedMissile_);
}

hud::HudFrame HudSimulation::BuildHudFrame() const noexcept
{
    return hud::BuildHudFrame(inputs_);
}

const HudInputSample& HudSimulation::Inputs() const noexcept
{
    return inputs_;
}

hud::AircraftState HudSimulation::Aircraft() const noexcept
{
    return hud::BuildAircraftStateForHud(inputs_.aircraft);
}

hud::TargetState HudSimulation::Target() const noexcept
{
    return hud::BuildTargetStateForHud(inputs_.target);
}

const MissileInventory& HudSimulation::Inventory() const noexcept
{
    return inventory_;
}

MissileType HudSimulation::SelectedMissile() const noexcept
{
    return selectedMissile_;
}

const std::vector<MissileShot>& HudSimulation::MissileShots() const noexcept
{
    return missileShots_;
}

hud::HudMasterMode HudSimulation::MasterMode() const noexcept
{
    return inputs_.weapon.masterMode;
}
} // namespace hud_main
